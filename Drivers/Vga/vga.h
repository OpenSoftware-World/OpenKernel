#ifndef VGA_H
#define VGA_H

#include <OpenKernel/SystemLib/Std/types.h>

#define VWIDTH 80
#define VHEIGHT 25
#define VBUFFER 0xB8000

void vga_init();
void vga_clear_screen(const char *mode);
void vga_set_bg_color(uint8_t color);
void vga_set_text_color(uint8_t color);
void vga_print_scr(const char *str);
void vga_newline();
void vga_bckspc();
void vga_set_cursor(uint8_t x, uint8_t y);
void ptchar(char c);
void vga_print_scr_nw(const char *str);
void vga_set_color_scheme(uint8_t bg_color, uint8_t text_color);
void vga_print_hex(uint32_t n);
void vga_print_dec(int num);
void vga_print_bin(unsigned int num);
void vga_cursor_mode(const char *mode, uint8_t start, uint8_t end);

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

/*
Example: VGA_COLOR_BLUE 0x1 -> 1: Blue background color
0x10 -> 1: Blue background color, 0: Black text color
*/

#define VGA_COLOR(bg, fg) ((bg << 4) | (fg))
#define VGA_ENTRY(ch, color) ((uint16_t)ch | (color << 8))

#define VGA_DRIVER_NAME "OpenKernel VGA Driver"
#define VGA_DRIVER_VER "0.3"
#define VGA_DRIVER_DESC "A simple VGA text mode driver for OpenKernel"
#define VGA_DRIVER_AUTHOR "OpenSoftware-World"
#define VGA_DRIVER_KRNL_VER "2.0"

#endif