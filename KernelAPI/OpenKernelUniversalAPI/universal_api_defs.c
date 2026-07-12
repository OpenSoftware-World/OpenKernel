#include "universal_api_defs.h"
#include <OpenKernel/SystemLib/SysCalls/Structs/structs.h>

void openkernel_universal_api_vga_set_bg_color(uint8_t color) {
    syscall(4, &color);
}

void openkernel_universal_api_vga_set_text_color(uint8_t color) {
    syscall(5, &color);
}

void openkernel_universal_api_vga_newline() {
    syscall(6, NULL);
}

void openkernel_universal_api_vga_set_cursor(uint8_t x, uint8_t y) {
    vga_set_cursor_t args = {
        .x = x,
        .y = y
    };
    syscall(7, &args);
}

void openkernel_universal_api_ptchar(char c) {
    syscall(8, &c);
}

void openkernel_universal_api_vga_print_scr_nw(const char *str) {
    syscall(9, &str);
}

void openkernel_universal_api_vga_set_color_scheme(uint8_t bg_color, uint8_t text_color) {
    vga_set_color_scheme_t args = {
        .bg_color = bg_color,
        .text_color = text_color
    };
    syscall(10, &args);
}

void openkernel_universal_api_get_char() {
    syscall(11, NULL);
}

void openkernel_universal_api_sleep(uint32_t seconds) {
    syscall(12, &seconds);
}