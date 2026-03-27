#include "basic_syscall.h"
#include "../../Drivers/Vga/vga.h"

void exit_program(const char *mode, const char *message) {
    /*
    D: Default
    C: Custom
    */
    if (mode[0] == 'D') {
        vga_print_scr_nw("Exiting program...");
    } else if (mode[0] == 'C') {
        vga_print_scr_nw(message);
    }

    while (true) {
        __asm__ __volatile__ ("hlt");
    }
}

void log_message(const char *mode, const char *message) {
    /*
    S: Success
    E: Error
    W: Warning
    I: Info
    U: Unknown
    */
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
    } else if (mode[0] == 'U') {
        vga_set_text_color(VGA_COLOR_LIGHT_GREY);
        vga_print_scr("[UNKNOWN]: ");
    }
    vga_print_scr_nw(message);
}