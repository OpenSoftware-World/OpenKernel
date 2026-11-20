#include "mouse.h"
#include <OpenKernel/Drivers/Keyboard/keyboard.h>
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>
#include <OpenKernel/SystemLib/TimeMng/time.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define MOUSE_TOP_LIMIT 1
#define MOUSE_BOTTOM_LIMIT (VGA_HEIGHT - 2)
#define MOUSE_LEFT_LIMIT 0
#define MOUSE_RIGHT_LIMIT (VGA_WIDTH - 1)

/* Safety bound for the drain loop: a stuck status bit must not trap the
   handler in interrupt context. One packet is three bytes and the 8042
   output buffer is a byte deep, so a pass always empties it in practice. */
#define MOUSE_DRAIN_LIMIT 16

#define MOUSE_STAT_DATA 0x01   /* output buffer holds a byte */
#define MOUSE_STAT_AUX  0x20   /* that byte came from the auxiliary device */

extern uint16_t *vgabuffer;
extern uint16_t screen_buffer[VGA_WIDTH * VGA_HEIGHT];

static bool_t mouse_enabled_driver = false;

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

int mouse_x = 40;
int mouse_y = 12;

int prev_mouse_x = 40;
int prev_mouse_y = 12;

/* Defined below; referenced by mouse_init(). */
static void mouse_irq_handler(registers_t *regs);

void mouse_init() {
    outb(0x64, 0xA8);
    outb(0x64, 0x20);

    uint8_t status = inb(0x60);
    status |= 2;

    outb(0x64, 0x60);
    outb(0x60, status);

    outb(0x64, 0xD4);
    outb(0x60, 0xF6);
    inb(0x60);

    outb(0x64, 0xD4);
    outb(0x60, 0xF4);
    inb(0x60);

    for (int i = 0; i < 100; i++) inb(0x60);

    mouse_cycle = 0;

    mouse_x = 40;
    mouse_y = 12;
    prev_mouse_x = mouse_x;
    prev_mouse_y = mouse_y;
    mouse_enabled_driver = true;
    
    interrupt_register_handler((uint8_t)(PIC_IRQ_BASE + MOUSE_IRQ_LINE), mouse_irq_handler);
    pic_clear_mask(MOUSE_IRQ_LINE);
}

/*
 * IRQ12 handler. The 8042 output buffer is shared with the keyboard, so a
 * byte without the auxiliary flag belongs to IRQ1's device and is routed
 * there instead of being dropped. The EOI is sent by interrupt_dispatch().
 */
static void mouse_irq_handler(registers_t *regs) {
    (void)regs;

    for (int i = 0; i < MOUSE_DRAIN_LIMIT; i++) {
        uint8_t stat = inb(0x64);
        uint8_t data;

        if (!(stat & MOUSE_STAT_DATA)) break;
        data = inb(0x60);

        if (stat & MOUSE_STAT_AUX) mouse_handle_byte(data);
        else kbd_hndlr(data);
    }
}

/*
 * Assemble one byte of the standard 3-byte PS/2 packet and, once it is
 * complete, move the cursor. Called from IRQ12 (and from IRQ1 when the
 * shared controller surfaces an auxiliary byte on the keyboard line).
 */
void mouse_handle_byte(uint8_t data) {
    if (!mouse_enabled_driver) return;

    switch (mouse_cycle)
    {
        case 0:
            if (!(data & 0x08)) {
                mouse_cycle = 0;
                return;
            }
            mouse_byte[0] = data;
            mouse_cycle++;
            break;

        case 1:
            mouse_byte[1] = data;
            mouse_cycle++;
            break;

        case 2:
            mouse_byte[2] = data;

            prev_mouse_x = mouse_x;
            prev_mouse_y = mouse_y;

            mouse_x += (int8_t)mouse_byte[1];
            mouse_y -= (int8_t)mouse_byte[2];

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= VGA_WIDTH) mouse_x = VGA_WIDTH - 1;

            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= VGA_HEIGHT) mouse_y = VGA_HEIGHT - 1;

            mouse_cycle = 0;
            break;
    }
}

void mouse_draw() {
    if (!mouse_enabled_driver) return;
    uint16_t index = mouse_y * VGA_WIDTH + mouse_x;

    vgabuffer[index] = (0x1F << 8) | 0xDB;
}

void mouse_restore() {
    if (!mouse_enabled_driver) return;
    uint16_t index = prev_mouse_y * VGA_WIDTH + prev_mouse_x;

    vgabuffer[index] = screen_buffer[index] ? screen_buffer[index] : (' ' | (0x1F << 8));
}

/*
 * Redraw the cursor when IRQ12 has moved it. There is nothing left to
 * poll: the packet assembly already happened in the interrupt handler.
 */
void mouse_update() {
    if (!mouse_enabled_driver) return;

    if (mouse_x != prev_mouse_x || mouse_y != prev_mouse_y) {

        mouse_restore();
        mouse_draw();

        prev_mouse_x = mouse_x;
        prev_mouse_y = mouse_y;
    }
}

void mouse_close() {
    pic_set_mask(MOUSE_IRQ_LINE);
    sleep(1);
    mouse_cycle = 0;
    mouse_x = 0;
    mouse_y = 0;
    sleep(1);
    prev_mouse_x = mouse_x;
    prev_mouse_y = mouse_y;
    mouse_enabled_driver = false;
}