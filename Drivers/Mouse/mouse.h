#ifndef MOUSE_H
#define MOUSE_H

#include <OpenKernel/SystemLib/Std/types.h>

extern int mouse_x;
extern int mouse_y;
extern int prev_mouse_x;
extern int prev_mouse_y;
extern uint16_t prev_cell;

void mouse_init();
void mouse_poll();
void mouse_draw();
void mouse_clear();
void mouse_put_char(int x, int y, char c, uint8_t color);
void mouse_restore();
void mouse_update();
void mouse_close();

#define MOUSE_DRIVER_NAME "OpenKernel Mouse Driver"
#define MOUSE_DRIVER_VER "0.2"
#define MOUSE_DRIVER_DESC "A simple mouse driver for OpenKernel"
#define MOUSE_DRIVER_AUTHOR "OpenSoftware-World"
#define MOUSE_DRIVER_KRNL_VER "3.0"

#endif