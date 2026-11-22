#include "panic.h"
#include <OpenKernel/Drivers/Cpu/cpu.h>

/*
This file is used to display a message on the screen when an operating system or kernel error occurs.

How does it work?: First, the screen is completely cleared; then, the cursor is hidden, the color scheme is set to red and white, the relevant text is displayed on the screen, and the system is frozen.
*/

void kernel_panic(const char *msg, const char *error_code) {
    cli();
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