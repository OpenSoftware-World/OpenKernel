#ifndef MOUSE_H
#define MOUSE_H

#include <OpenKernel/SystemLib/Std/types.h>

/* IRQ line the 8042 raises for auxiliary bytes (vector 44 after the remap). */
#define MOUSE_IRQ_LINE 12

extern int mouse_x;
extern int mouse_y;
extern int prev_mouse_x;
extern int prev_mouse_y;
extern uint16_t prev_cell;

void mouse_init();
void mouse_handle_byte(uint8_t data);
void mouse_draw();
void mouse_clear();
void mouse_put_char(int x, int y, char c, uint8_t color);
void mouse_restore();
void mouse_update();
void mouse_close();

#define MOUSE_DRIVER_NAME "OpenKernel Mouse Driver"
#define MOUSE_DRIVER_VER "1.0"
#define MOUSE_DRIVER_DESC "A simple mouse driver for OpenKernel"
#define MOUSE_DRIVER_AUTHOR "OpenSoftware-World"
#define MOUSE_DRIVER_KRNL_VER "4.0"

#endif