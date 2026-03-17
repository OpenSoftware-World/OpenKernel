#include "basic_syscall.h"
#include "../../Drivers/Vga/vga.h"

void exit_program() {
    vga_print_scr("Exiting program...");
    vga_newline();

    while (true) {
        __asm__ __volatile__ ("hlt");
    }
}

void log_message(const char *mode, const char *message) {
    if (mode[0] == 'S') {
        vga_set_text_color(VGA_COLOR_GREEN);
        vga_print_scr("[SUCCESS]: ");
    } else if (mode[0] == 'E') {
        vga_set_text_color(VGA_COLOR_RED);
        vga_print_scr("[ERROR]: ");
    } else if (mode[0] == 'W') {
        vga_set_text_color(VGA_COLOR_LIGHT_BROWN);
        vga_print_scr("[WARNING]: ");
    } else if (mode[0] == 'I') {
        vga_set_text_color(VGA_COLOR_LIGHT_BLUE);
        vga_print_scr("[INFO]: ");
    } else {
        vga_set_text_color(VGA_COLOR_LIGHT_GREY);
        vga_print_scr("[UNKNOWN]: ");
    }
    vga_print_scr_nw(message);
}