#include "mouse.h"
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define MOUSE_TOP_LIMIT 1
#define MOUSE_BOTTOM_LIMIT (VGA_HEIGHT - 2)
#define MOUSE_LEFT_LIMIT 0
#define MOUSE_RIGHT_LIMIT (VGA_WIDTH - 1)

extern uint16_t *vgabuffer = (uint16_t *)0xB8000;
extern uint16_t screen_buffer[VGA_WIDTH * VGA_HEIGHT];

static bool_t mouse_enabled_driver = false;

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

int mouse_x = 40;
int mouse_y = 12;

int prev_mouse_x = 40;
int prev_mouse_y = 12;

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
}

void mouse_poll() {
    if (!mouse_enabled_driver) return;
    uint8_t status = inb(0x64);

    if (!(status & 1)) return;
    if (!(status & 0x20)) return;

    uint8_t data = inb(0x60);

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

    if (!(status & 1) || !(status & 0x20)) {
        mouse_cycle = 0;
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

void mouse_update() {
    if (!mouse_enabled_driver) return;
    mouse_poll();

    if (mouse_x != prev_mouse_x || mouse_y != prev_mouse_y) {

        mouse_restore();
        mouse_draw();

        prev_mouse_x = mouse_x;
        prev_mouse_y = mouse_y;
    }
}

void mouse_close() {
    sleep(1);
    mouse_cycle = 0;
    mouse_x = 0;
    mouse_y = 0;
    sleep(1);
    prev_mouse_x = mouse_x;
    prev_mouse_y = mouse_y;
    mouse_enabled_driver = false;
}