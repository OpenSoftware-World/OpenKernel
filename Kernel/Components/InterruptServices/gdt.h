#ifndef GDT_H
#define GDT_H

#include <OpenKernel/SystemLib/Std/types.h>

/*
 * Selector values. The IDT gates and the isr.asm stubs hard-code these, so
 * they must stay in sync with gdt_init()'s layout:
 *   0x08 - kernel code (GDT index 1)
 *   0x10 - kernel data (GDT index 2)
 *   0x18 - user code   (GDT index 3)
 *   0x20 - user data   (GDT index 4)
 */
#define GDT_KERNEL_CODE_SELECTOR 0x08
#define GDT_KERNEL_DATA_SELECTOR 0x10
#define GDT_USER_CODE_SELECTOR   0x18
#define GDT_USER_DATA_SELECTOR   0x20

/* Number of descriptors installed, including the null descriptor. */
#define GDT_ENTRIES 5

/*
 * Load a flat GDT covering ring 0 and ring 3 and reload every segment
 * register from it. Must run before interrupt_init(): the IDT gates name
 * selector 0x08, so an IDT installed over someone else's GDT will #GP on
 * the first interrupt if that GDT does not describe the same segments.
 */
void gdt_init(void);

#endif
