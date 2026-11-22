/* Flat global descriptor table for ring 0 and ring 3. */

#include <OpenKernel/Kernel/Components/InterruptServices/gdt.h>

/* One 8-byte segment descriptor (Intel SDV volume 3, figure 3-8). */
typedef struct {
    uint16_t limit_low;     /* limit bits 0..15 */
    uint16_t base_low;      /* base bits 0..15 */
    uint8_t  base_mid;      /* base bits 16..23 */
    uint8_t  access;        /* P, DPL, S, type */
    uint8_t  granularity;   /* limit bits 16..19, G, D/B, AVL */
    uint8_t  base_high;     /* base bits 24..31 */
} PACKED gdt_entry_t;

/* Value loaded into the GDTR by lgdt. */
typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED gdt_ptr_t;

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

/* Fill one descriptor: flat base, the given limit and access byte. */
static void gdt_set_gate(uint8_t index, uint32_t limit, uint8_t access) {
    gdt[index].base_low    = 0;
    gdt[index].base_mid    = 0;
    gdt[index].base_high   = 0;
    gdt[index].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[index].granularity = (uint8_t)(((limit >> 16) & 0x0F) | 0xC0);
    gdt[index].access      = access;
}

/*
 * Install the table and reload the segments. The far return is what makes
 * the new CS take effect: the CPU caches the descriptor behind a selector,
 * so reassigning the GDT alone leaves CS pointing at the old one.
 *
 * 0x08 is reloaded through the return address on the stack rather than a
 * ljmp, which keeps the whole sequence in C inline assembly.
 */
static void gdt_flush(void) {
    __asm__ __volatile__(
        "lgdt %0\n\t"
        "movw %1, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        "pushl %2\n\t"
        "pushl $1f\n\t"
        "lret\n\t"
        "1:\n\t"
        :
        : "m"(gdt_ptr), "i"(GDT_KERNEL_DATA_SELECTOR),
          "i"(GDT_KERNEL_CODE_SELECTOR)
        : "eax", "memory");
}

void gdt_init(void) {
    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base  = (uint32_t)&gdt[0];

    /* Null descriptor - a required empty slot at index 0. */
    gdt_set_gate(0, 0, 0x00);
    /* Kernel code: present, ring 0, executable, readable, 4 GiB, 4K pages. */
    gdt_set_gate(1, 0xFFFFF, 0x9A);
    /* Kernel data: present, ring 0, writable. */
    gdt_set_gate(2, 0xFFFFF, 0x92);
    /* User code: same but ring 3, so a future syscall gate can enter it. */
    gdt_set_gate(3, 0xFFFFF, 0xFA);
    /* User data: ring 3. */
    gdt_set_gate(4, 0xFFFFF, 0xF2);

    gdt_flush();
}
