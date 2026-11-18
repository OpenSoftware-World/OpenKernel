/* 8259A programmable interrupt controller: initialisation, masking, EOI. */

#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>
#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>

#define ICW1_INIT   0x11    /* ICW1: initialisation, ICW4 follows */
#define ICW4_8086   0x01    /* ICW4: 8086/88 mode, non-automatic EOI */

/*
 * Remap IRQ0-15 to vectors 32-47. Without the remap IRQ0 would fire on
 * vector 0x08 (double fault) and IRQ8 on vector 0x70.
 */
void pic_init(void) {
    /* ICW1 on both controllers starts the initialisation sequence. */
    outb(PIC_MASTER_CMD, ICW1_INIT);
    sys_wait();
    outb(PIC_SLAVE_CMD, ICW1_INIT);
    sys_wait();

    /* ICW2: vector offset each controller uses for its first IRQ line. */
    outb(PIC_MASTER_DATA, PIC_IRQ_BASE);        /* IRQ0 -> vector 32 */
    sys_wait();
    outb(PIC_SLAVE_DATA, PIC_IRQ_BASE + 8);     /* IRQ8 -> vector 40 */
    sys_wait();

    /* ICW3: the slave is cascaded on IRQ2 and identifies itself as 2. */
    outb(PIC_MASTER_DATA, 0x04);
    sys_wait();
    outb(PIC_SLAVE_DATA, 0x02);
    sys_wait();

    /* ICW4: 8086 mode. */
    outb(PIC_MASTER_DATA, ICW4_8086);
    sys_wait();
    outb(PIC_SLAVE_DATA, ICW4_8086);
    sys_wait();

    /* Mask every line; drivers unmask the IRQs they handle. */
    outb(PIC_MASTER_DATA, 0xFF);
    outb(PIC_SLAVE_DATA, 0xFF);
}

/* Set or clear the mask bit of one IRQ line. */
static void pic_update_mask(uint8_t irq, bool_t masked) {
    uint16_t port;
    uint8_t mask;

    if (irq > 15) return;

    port = (irq < 8) ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
    mask = inb(port);
    if (masked) mask |= (uint8_t)(1u << (irq & 7));
    else        mask &= (uint8_t)~(1u << (irq & 7));
    outb(port, mask);
}

void pic_set_mask(uint8_t irq) {
    pic_update_mask(irq, true);
}

void pic_clear_mask(uint8_t irq) {
    pic_update_mask(irq, false);
}

void pic_send_eoi(uint8_t irq) {
    if (irq > 15) return;

    /* An IRQ from the slave also has to acknowledge the cascade line. */
    if (irq >= 8) outb(PIC_SLAVE_CMD, PIC_EOI);
    outb(PIC_MASTER_CMD, PIC_EOI);
}
