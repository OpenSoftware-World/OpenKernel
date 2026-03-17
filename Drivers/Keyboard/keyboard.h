#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../../SystemLib/Std/types.h"

#define KEY_BF_SIZE 256

void kbd_init();
char get_char();
uint8_t kb_check();
void kbd_hndlr();

#define DRIVER_NAME "OpenKernel Keyboard Driver"
#define DRIVER_VER "0.1"
#define DRIVER_DESC "A simple Keyboard driver for OpenKernel"
#define DRIVER_KRNL_VER "1.0"

#endif