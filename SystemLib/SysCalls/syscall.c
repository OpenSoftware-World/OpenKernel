#include "syscall.h"

static securchk_result_t g_sec_result;
static int g_sec_initialized = 0;

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

        case SYSCALL_SECURCHK_INIT:
            securchk_init();
            g_sec_initialized = 1;
            return 0;

        case SYSCALL_SECURCHK_RUN:
            if (!g_sec_initialized) securchk_init();
            if (arg)
                securchk_run((securchk_result_t*)arg);
            else
                securchk_run(&g_sec_result);
            return 0;

        case SYSCALL_SECURCHK_STATUS:
            if (!g_sec_initialized) return -1;
            if (arg)
                *(securchk_result_t*)arg = g_sec_result;
            return securchk_get_overall_status(&g_sec_result);

        default:
            return -1;
    }
}
