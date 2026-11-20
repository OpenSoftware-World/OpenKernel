#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <OpenKernel/SystemLib/Std/types.h>

#define KEY_BF_SIZE 256

/* IRQ line the 8042 raises for keyboard bytes (PIC remaps it to vector 33). */
#define KBD_IRQ_LINE 1

void kbd_init();
char get_char();
uint8_t kb_check();
void kbd_hndlr(uint8_t scancode);
void kbd_close();

#define KBD_DRIVER_NAME "OpenKernel Keyboard Driver"
#define KBD_DRIVER_VER "1.0"
#define KBD_DRIVER_DESC "A simple Keyboard driver for OpenKernel"
#define KBD_DRIVER_AUTHOR "OpenSoftware-World"
#define KBD_DRIVER_KRNL_VER "4.0"

#endif