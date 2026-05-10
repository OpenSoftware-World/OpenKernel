#include "panic.h"
#include <OpenKernel/Drivers/Cpu/cpu.h>

void kernel_panic(const char *msg, const char *error_code) {
    vga_clear_screen("F");
    vga_cursor_mode("D", 0, 0);
    vga_set_color_scheme(VGA_COLOR_RED, VGA_COLOR_WHITE);
    vga_print_scr_nw("                            == OpenKernel PANIC ==");
    vga_print_scr_nw("KERNEL PANIC: ");
    vga_newline();
    vga_print_scr("Error Details: ");
    vga_print_scr_nw(msg);
    vga_print_scr("Error code: ");
    vga_print_scr_nw(error_code);
    
    while (1) {
        hlt();
    } 
}