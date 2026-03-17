#ifndef VGA_H
#define VGA_H

#include "../../SystemLib/Std/types.h"

#define VWIDTH 80
#define VHEIGHT 25
#define VBUFFER 0xB8000

void vga_init();
void vga_screen_clear();
void vga_set_bg_color(uint8_t color);
void vga_set_text_color(uint8_t color);
void vga_print_scr(const char *str);
void vga_newline();
void vga_bckspc();
void vga_set_cursor(uint8_t x, uint8_t y);
void ptchar(char c);
void vga_print_scr_nw(const char *str);
void vga_set_color_scheme(uint8_t bg_color, uint8_t text_color);

#define VGA_COLOR_BLACK 0x0
#define VGA_COLOR_BLUE 0x1
#define VGA_COLOR_GREEN 0x2
#define VGA_COLOR_CYAN 0x3
#define VGA_COLOR_RED 0x4
#define VGA_COLOR_MAGENTA 0x5
#define VGA_COLOR_BROWN 0x6
#define VGA_COLOR_LIGHT_GREY 0x7
#define VGA_COLOR_DARK_GREY 0x8
#define VGA_COLOR_LIGHT_BLUE 0x9
#define VGA_COLOR_LIGHT_GREEN 0xA
#define VGA_COLOR_LIGHT_CYAN 0xB
#define VGA_COLOR_LIGHT_RED 0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_LIGHT_BROWN 0xE
#define VGA_COLOR_WHITE 0xF

#define VGA_COLOR(bg, fg) ((bg << 4) | (fg))
#define VGA_ENTRY(ch, color) ((uint16_t)ch | (color << 8))

#define DRIVER_NAME "OpenKernel VGA Driver"
#define DRIVER_VER "0.1"
#define DRIVER_DESC "A simple VGA text mode driver for OpenKernel"
#define DRIVER_KRNL_VER "1.0"

#endif