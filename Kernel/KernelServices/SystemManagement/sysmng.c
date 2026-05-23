#include "sysmng.h"
#include <OpenKernel/SystemLib/Std/std.h>

void sys_next_status(const char *mode, uint32_t seconds) {
    /*
    S: Shutdown
    R: Reboot
    */
    sleep(seconds);
    if (mode[0] == 'S' && strcmp(mode, "shutdown") == 0) {
        outb(0x604, 0x00);
        sys_wait();
        outb(0x605, 0x20);
    }
    else if(mode[0] == 'R' && strcmp(mode, "reboot") == 0) {
        cli();
        while (inb(0x64) & 0x02);
        outb(0x64, 0xFE);
    }
}

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
        hlt();
    }
}