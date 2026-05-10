#include "syscall.h"

int syscall(int id, void* arg) {
    return syscall_handler(id, arg);
}

int syscall_handler(int id, void* arg) {
    switch (id) {
        case SYSCALL_PRINT:
            if (!arg) return -1;
            vga_print_scr((char*)arg);
            return 0;

        case SYSCALL_CLEAR_SCREEN:
            vga_clear_screen("C");
            return 0;

        case SYSCALL_SHUTDOWN:
            sys_next_status("S", 0);
            return 0;

        case SYSCALL_REBOOT:
            sys_next_status("R", 0);
            return 0;
        default:
            return -1;
    }
}
