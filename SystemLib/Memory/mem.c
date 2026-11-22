#include "mem.h"
#include <OpenKernel/Drivers/Cpu/cpu.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>

static uint32_t heap_start;
static uint32_t heap_end;

/*
 * Every block - allocated or free - starts with an 8-byte header. While
 * the block is allocated, `u.magic` marks it; while it is free, the same
 * slot holds the `next` link of the address-ordered free list. Address
 * order lets kfree() merge a block with both of its neighbours.
 */
#define HEAP_BLOCK_MAGIC 0x48454150u   /* "HEAP" */
#define HEAP_HDR_SIZE    8u
#define HEAP_MIN_PAYLOAD 4u            /* a free block must hold its link */

typedef struct block_hdr {
    uint32_t size;                     /* payload bytes, multiple of 4 */
    union {
        uint32_t magic;
        struct block_hdr *next;
    } u;
} block_hdr_t;

static block_hdr_t *free_list;

void* memset(void* dst, uint8_t val, uint32_t size) {
    uint8_t* p = (uint8_t*)dst;

    for (uint32_t i = 0; i < size; i++) {
        p[i] = (uint8_t)val;
    }
    return dst;
}

void* memcpy(void* dst, const void* src, uint32_t size) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dst;
}

int memcmp(const void* a, const void* b, uint32_t size) {
    const uint8_t* p1 = (const uint8_t*)a;
    const uint8_t* p2 = (const uint8_t*)b;

    for (uint32_t i = 0; i < size; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

/*
 * Arm the heap as one large free block. The caller's region is never
 * written past: the start is rounded up to a dword boundary, but the end
 * stays where the caller put it.
 */
void heap_init(uint32_t start, uint32_t size) {
    uint32_t raw_end = start + size;
    block_hdr_t *first;

    start = (start + 3) & ~3u;
    heap_start = start;
    heap_end = (raw_end < start) ? start : raw_end;   /* also catches wrap */
    free_list = 0;

    if (heap_end - heap_start < HEAP_HDR_SIZE + HEAP_MIN_PAYLOAD) return;

    first = (block_hdr_t*)heap_start;
    first->size = (heap_end - heap_start - HEAP_HDR_SIZE) & ~3u;
    if (first->size < HEAP_MIN_PAYLOAD) return;
    first->u.next = 0;
    free_list = first;
}

/*
 * First-fit allocation from the free list. Blocks are split when the
 * remainder can still hold a header plus a payload; the remainder goes
 * back to the list. The list is protected the same way the old bump
 * allocator was: interrupts off while the heap state is touched.
 */
void* kmalloc(uint32_t size) {
    uint32_t flags;
    block_hdr_t *b, *prev;
    void *addr = 0;

    if (size > 0xFFFFFFFCu) {
        return (void*)0;
    }

    size = (size + 3) & ~3u;
    if (size < HEAP_MIN_PAYLOAD) size = HEAP_MIN_PAYLOAD;

    flags = eflags_r();
    cli();

    /* First fit. On the break, `prev` is b's predecessor (0 when b is the
       list head); when nothing fits, b is NULL and nothing is touched. */
    prev = 0;
    for (b = free_list; b != 0; prev = b, b = b->u.next) {
        if (b->size >= size) break;
    }

    if (b != 0) {
        if (b->size >= size + HEAP_HDR_SIZE + HEAP_MIN_PAYLOAD) {
            /* Split: the tail becomes a free block of its own. */
            block_hdr_t *rest = (block_hdr_t*)((uint8_t*)b + HEAP_HDR_SIZE + size);
            rest->size = b->size - size - HEAP_HDR_SIZE;
            rest->u.next = b->u.next;
            b->size = size;
            if (prev) prev->u.next = rest;
            else      free_list = rest;
        } else {
            /* Too small to split: hand out the whole block. */
            if (prev) prev->u.next = b->u.next;
            else      free_list = b->u.next;
        }
        b->u.magic = HEAP_BLOCK_MAGIC;
        addr = (uint8_t*)b + HEAP_HDR_SIZE;
    }

    if (flags & EFLAGS_IF) sti();

    return addr;
}

/*
 * Return a block to the heap and merge it with any free neighbours, so a
 * alloc/free pattern does not fragment the heap into unusable slivers.
 * NULL, out-of-range and non-allocated pointers are ignored instead of
 * corrupting the free list.
 */
void kfree(void* ptr) {
    uint32_t flags;
    block_hdr_t *b, *prev, *cur;

    if (ptr == 0) return;
    if ((uint32_t)ptr < heap_start + HEAP_HDR_SIZE) return;
    if ((uint32_t)ptr >= heap_end) return;

    b = (block_hdr_t*)((uint8_t*)ptr - HEAP_HDR_SIZE);
    if (b->u.magic != HEAP_BLOCK_MAGIC) return;   /* never allocated */

    flags = eflags_r();
    cli();

    /* Insert in address order so both neighbours are adjacent in the
       walk: prev just below b, cur just above it. */
    prev = 0;
    for (cur = free_list; cur != 0 && (uint32_t)cur < (uint32_t)b;
         prev = cur, cur = cur->u.next) ;
    b->u.next = cur;                               /* clears the magic */
    if (prev) prev->u.next = b;
    else      free_list = b;

    /* Merge the block above, then the block below (the second merge may
       swallow b into prev, so it must run second). */
    if (cur != 0 && (uint8_t*)b + HEAP_HDR_SIZE + b->size == (uint8_t*)cur) {
        b->size += HEAP_HDR_SIZE + cur->size;
        b->u.next = cur->u.next;
    }
    if (prev != 0 &&
        (uint8_t*)prev + HEAP_HDR_SIZE + prev->size == (uint8_t*)b) {
        prev->size += HEAP_HDR_SIZE + b->size;
        prev->u.next = b->u.next;
    }

    if (flags & EFLAGS_IF) sti();
}

/*
Example:

heap_init(0x100000, 1024 * 1024); // 1 MB heap

int* a = (int*)kmalloc(4);
int* b = (int*)kmalloc(16);

*a = 10;
*b = 20;
*/