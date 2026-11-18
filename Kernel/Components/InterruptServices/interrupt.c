/* IDT construction and the C half of the interrupt dispatch path. */

#include <OpenKernel/Kernel/ErrorHandler/panic.h>
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>

#define KERNEL_CODE_SELECTOR 0x08   /* GDT index 1: kernel code segment */

/* Entry stubs generated in isr.asm. */
#define DECLARE_ISR(n) extern void isr##n(void);
#define DECLARE_IRQ(n) extern void irq##n(void);

DECLARE_ISR(0)
DECLARE_ISR(1)
DECLARE_ISR(2)
DECLARE_ISR(3)
DECLARE_ISR(4)
DECLARE_ISR(5)
DECLARE_ISR(6)
DECLARE_ISR(7)
DECLARE_ISR(8)
DECLARE_ISR(9)
DECLARE_ISR(10)
DECLARE_ISR(11)
DECLARE_ISR(12)
DECLARE_ISR(13)
DECLARE_ISR(14)
DECLARE_ISR(15)
DECLARE_ISR(16)
DECLARE_ISR(17)
DECLARE_ISR(18)
DECLARE_ISR(19)
DECLARE_ISR(20)
DECLARE_ISR(21)
DECLARE_ISR(22)
DECLARE_ISR(23)
DECLARE_ISR(24)
DECLARE_ISR(25)
DECLARE_ISR(26)
DECLARE_ISR(27)
DECLARE_ISR(28)
DECLARE_ISR(29)
DECLARE_ISR(30)
DECLARE_ISR(31)

DECLARE_IRQ(0)
DECLARE_IRQ(1)
DECLARE_IRQ(2)
DECLARE_IRQ(3)
DECLARE_IRQ(4)
DECLARE_IRQ(5)
DECLARE_IRQ(6)
DECLARE_IRQ(7)
DECLARE_IRQ(8)
DECLARE_IRQ(9)
DECLARE_IRQ(10)
DECLARE_IRQ(11)
DECLARE_IRQ(12)
DECLARE_IRQ(13)
DECLARE_IRQ(14)
DECLARE_IRQ(15)

/* Stubs in vector order, generated in isr.asm. */
static void (*const isr_stubs[32])(void) = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

static void (*const irq_stubs[16])(void) = {
    irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
    irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
};

static idt_gate_t idt[IDT_ENTRIES];
static isr_handler_t handlers[IDT_ENTRIES];

/* CPU exception mnemonics (Intel SDV volume 3, table 6-1). */
static const char *exception_names[32] = {
    "#DE divide-by-zero",       "#DB debug",
    "NMI",                      "#BP breakpoint",
    "#OF overflow",             "#BR bound-range-exceeded",
    "#UD invalid-opcode",       "#NM device-not-available",
    "#DF double-fault",         "coprocessor-segment-overrun",
    "#TS invalid-TSS",          "#NP segment-not-present",
    "#SS stack-fault",          "#GP general-protection",
    "#PF page-fault",           "reserved (15)",
    "#MF x87-FPU",              "#AC alignment-check",
    "#MC machine-check",        "#XM SIMD-floating-point",
    "#VE virtualisation",       "#CP control-protection",
    "reserved (22)",            "reserved (23)",
    "reserved (24)",            "reserved (25)",
    "reserved (26)",            "reserved (27)",
    "reserved (28)",            "reserved (29)",
    "reserved (30)",            "reserved (31)"
};

/* Point one IDT entry at an ISR stub. */
static void idt_set_gate(uint8_t vector, uint32_t handler) {
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector = KERNEL_CODE_SELECTOR;
    idt[vector].zero = 0;
    idt[vector].type_attr = IDT_GATE_ATTR;
    idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}

void interrupt_init(void) {
    uint8_t i;
    idtr_t idtr;

    /* CPU exceptions 0-31, hardware IRQs 0-15 at vectors 32-47. */
    for (i = 0; i < 32; i++)
        idt_set_gate(i, (uint32_t)isr_stubs[i]);
    for (i = 0; i < 16; i++)
        idt_set_gate((uint8_t)(PIC_IRQ_BASE + i), (uint32_t)irq_stubs[i]);

    /*
     * Vectors 48-255 stay zeroed: a stray software INT to them raises
     * #GP instead of jumping through a garbage gate.
     */
    pic_init();

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint32_t)&idt[0];
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

void interrupt_register_handler(uint8_t vector, isr_handler_t handler) {
    handlers[vector] = handler;
}

void interrupt_dispatch(registers_t *regs) {
    uint32_t vector = regs->vector;
    isr_handler_t handler = (vector < IDT_ENTRIES) ? handlers[vector] : NULL;

    /* CPU exceptions: a registered handler (e.g. a page fault manager)
       takes the blame, otherwise stop the kernel with a report. */
    if (vector < 32) {
        if (handler != NULL) handler(regs);
        else kernel_panic("Unhandled CPU exception", exception_names[vector]);
        return;
    }

    if (handler != NULL) handler(regs);

    /* IRQs must be acknowledged even when nobody listens, otherwise the
       controller stops delivering further interrupts. */
    if (vector >= PIC_IRQ_BASE && vector < PIC_IRQ_BASE + 16)
        pic_send_eoi((uint8_t)(vector - PIC_IRQ_BASE));
}
