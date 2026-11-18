#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <OpenKernel/SystemLib/Std/types.h>

#define IDT_ENTRIES     256
#define IDT_GATE_ATTR   0x8E    /* present, DPL 0, 32-bit interrupt gate */

/* 32-bit IDT gate descriptor (Intel SDV volume 3, figure 6-4). */
typedef struct {
    uint16_t offset_low;    /* ISR address, bits 0..15 */
    uint16_t selector;      /* code segment selector (kernel: 0x08) */
    uint8_t  zero;          /* reserved, must be 0 */
    uint8_t  type_attr;     /* gate type, DPL and present bit */
    uint16_t offset_high;   /* ISR address, bits 16..31 */
} PACKED idt_gate_t;

/* Value loaded into the IDTR by lidt. */
typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED idtr_t;

/*
 * Register frame handed to C handlers. The layout must match the pushes
 * performed in isr.asm exactly: the lowest address holds what the stub
 * pushed last (ds), then the pusha block, then the vector/error pair and
 * finally the CPU-pushed eip/cs/eflags (useresp/ss only exist after a
 * privilege level change).
 */
typedef struct {
    uint32_t ds;                    /* data segment observed at entry */
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;  /* pusha order */
    uint32_t vector;                /* interrupt vector number */
    uint32_t error_code;            /* 0 when the CPU did not push one */
    uint32_t eip, cs, eflags;       /* pushed by the CPU */
    uint32_t useresp, ss;           /* only after a privilege level change */
} PACKED registers_t;

typedef void (*isr_handler_t)(registers_t *regs);

/* Frame layout sanity checks against the pushes in isr.asm. */
STATIC_ASSERT(sizeof(registers_t) == 64, registers_frame_size);
STATIC_ASSERT(__builtin_offsetof(registers_t, eip) == 44, registers_eip_offset);

/*
 * Build the IDT, install the exception/IRQ stubs and remap the PIC.
 * IRQ lines stay masked until a driver unmasks the one it handles.
 */
void interrupt_init(void);

/* Install the C handler for an exception (0-31) or IRQ vector (32-47). */
void interrupt_register_handler(uint8_t vector, isr_handler_t handler);

/* Shared entry point called by the isr.asm stubs. */
void interrupt_dispatch(registers_t *regs);

#endif
