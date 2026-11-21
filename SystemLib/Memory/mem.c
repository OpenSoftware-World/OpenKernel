#include "mem.h"
#include <OpenKernel/Drivers/Cpu/cpu.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>

static uint32_t heap_start;
static uint32_t heap_end;
static uint32_t heap_current;

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

void heap_init(uint32_t start, uint32_t size) {
    heap_start = start;
    heap_end = start + size;
    heap_current = start;
}

void* kmalloc(uint32_t size) {
    uint32_t flags;
    void* addr;

    if (size > 0xFFFFFFFCu) {
        return (void*)0;
    }

    size = (size + 3) & ~3u;
    flags = eflags_r();
    cli();

    addr = (void*)0;
    if (heap_current + size >= heap_current && heap_current + size < heap_end) {
        addr = (void*)heap_current;
        heap_current += size;
    }

    if (flags & EFLAGS_IF) sti();

    return addr;
}

void kfree(void* ptr) {
    (void)ptr;
}

/*
Example:

heap_init(0x100000, 1024 * 1024); // 1 MB heap

int* a = (int*)kmalloc(4);
int* b = (int*)kmalloc(16);

*a = 10;
*b = 20;
*/