#ifndef UNIVERSAL_API_DEFS_H
#define UNIVERSAL_API_DEFS_H

#include <OpenKernel/KernelAPI/OpenKernelEnvironment/SysCalls/syscalls.h>
#include <OpenKernel/KernelAPI/OpenKernelEnvironment/types.h>

void openkernel_universal_api_vga_set_bg_color(uint8_t color);
void openkernel_universal_api_vga_set_text_color(uint8_t color);
void openkernel_universal_api_vga_newline();
void openkernel_universal_api_vga_set_cursor(uint8_t x, uint8_t y);
void openkernel_universal_api_ptchar(char c);
void openkernel_universal_api_vga_print_scr_nw(const char *str);
void openkernel_universal_api_vga_set_color_scheme(uint8_t bg_color, uint8_t text_color);
void openkernel_universal_api_get_char();
void openkernel_universal_api_sleep(uint32_t seconds);

#endif