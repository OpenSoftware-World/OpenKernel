#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <OpenKernel/SystemLib/Std/types.h>

#define KEY_BF_SIZE 256

void kbd_init();
char get_char();
uint8_t kb_check();
void kbd_hndlr(uint8_t scancode);

#define KBD_DRIVER_NAME "OpenKernel Keyboard Driver"
#define KBD_DRIVER_VER "0.2"
#define KBD_DRIVER_DESC "A simple Keyboard driver for OpenKernel"
#define KBD_DRIVER_AUTHOR "OpenSoftware-World"
#define KBD_DRIVER_KRNL_VER "1.0"

#endif