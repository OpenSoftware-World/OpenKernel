#ifndef PIC_H
#define PIC_H

#include <OpenKernel/SystemLib/Std/types.h>

/* 8259A ports: master at 0x20/0x21, slave at 0xA0/0xA1. */
#define PIC_MASTER_CMD      0x20
#define PIC_MASTER_DATA     0x21
#define PIC_SLAVE_CMD       0xA0
#define PIC_SLAVE_DATA      0xA1

#define PIC_EOI             0x20    /* end-of-interrupt command */
#define PIC_IRQ_BASE        32      /* IRQ0-15 remapped to vectors 32-47 */

/*
 * Remap IRQ0-15 to vectors 32-47 (they would otherwise collide with the
 * CPU exceptions) and mask every line.
 */
void pic_init(void);

/* Mask (pic_set_mask) or unmask (pic_clear_mask) one IRQ line (0-15). */
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

/* Acknowledge an IRQ (0-15) so the controller can deliver the next one. */
void pic_send_eoi(uint8_t irq);

#endif
