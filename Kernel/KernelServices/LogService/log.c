#include "log.h"

void log_message(klog_level_t level, const char *message) {
    /*
    S: Success
    E: Error
    W: Warning
    I: Info
    U: Unknown
    */
    switch (level) {
        case LOG_SUCCESS:
            vga_set_text_color(VGA_COLOR_GREEN);
            vga_print_scr("[SUCCESS]: ");
            break;
        case LOG_ERROR:
            vga_set_text_color(VGA_COLOR_RED);
            vga_print_scr("[ERROR]: ");
            break;
        case LOG_WARNING:
            vga_set_text_color(VGA_COLOR_LIGHT_BROWN);
            vga_print_scr("[WARNING]: ");
            break;
        case LOG_INFO:
            vga_set_text_color(VGA_COLOR_LIGHT_BLUE);
            vga_print_scr("[INFO]: ");
            break;
        case LOG_UNKNOWN:
        default:
            vga_set_text_color(VGA_COLOR_LIGHT_GREY);
            vga_print_scr("[UNKNOWN]: ");
            break;
    }
    vga_print_scr_nw(message);
}