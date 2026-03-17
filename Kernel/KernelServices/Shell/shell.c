#include "shell.h"
#include "command_ls.h"
#include "../../../Drivers/Vga/vga.h"
#include "../../../SystemLib/SysCalls/basic_syscall.h"
#include "../../../SystemLib/TimeMng/time.h"
#include "../../../SystemLib/SystemIO/io.h"
#include "../../../Drivers/Keyboard/keyboard.h"
#include "../../../SystemLib/Std/std.h"
#include "../SystemManagement/sysmng.h"

void shell_inp() {
    vga_set_text_color(VGA_COLOR_GREEN);
    vga_print_scr("root");
    vga_set_text_color(VGA_COLOR_WHITE);
    vga_print_scr("@");
    vga_set_text_color(VGA_COLOR_CYAN);
    vga_print_scr("shell");
    vga_set_text_color(VGA_COLOR_WHITE);
    vga_print_scr("> ");
}

void shell_init() {
    char cmd[MAX_CMD_LEN];
    uint8_t idx = 0;

    shell_inp();

    while (1) {
        char c = get_char();

        if (c == '\b') {
            if (idx > 0) {
                idx--;
                vga_bckspc();
            }
        } else if (c == '\n') {
            vga_newline();
            cmd[idx] = 0;
            idx = 0;

            if (strcmp(cmd, "list") == 0) {
                vga_print_scr("Commands: clear, cls, list, seqlist, ver, shellinfo, osfetch, shutdown, reboot, time, version, echo\n");
            } else if (strcmp(cmd, SEQLIST_STRING) == 0) {
                vga_print_scr_nw("Commands:");
                vga_print_scr_nw("clear");
                vga_print_scr_nw("cls");
                vga_print_scr_nw("list");
                vga_print_scr_nw("seqlist");
                vga_print_scr_nw("ver");
                vga_print_scr_nw("shellinfo");
                vga_print_scr_nw("osfetch");
                vga_print_scr_nw("shutdown");
                vga_print_scr_nw("reboot");
                vga_print_scr_nw("time");
                vga_print_scr_nw("version");
                vga_print_scr_nw("echo");
            } else if (strcmp(cmd, OSFETCH_STRING) == 0) {
                
                vga_print_scr_nw("  ____   _____      root@shell");
                vga_print_scr_nw(" / __ \\ / ___/      ----------");
                vga_print_scr_nw("| |  | | \\__ \\      OS: OpenKernel 1.0");
                vga_print_scr_nw("| |  | |___/  /     Kernel: OpenKernel 1.0");
                vga_print_scr_nw("| |  | |___/  /     Bootloader Name: GNU GRUB Bootloader");
                vga_print_scr_nw("| |   | ___/  /     Bootloader version: 2.12");
                vga_print_scr_nw(" \\____/|_____/      Architecture: x86 (32-bit)");
                vga_print_scr_nw("                    Shell: OpenKernel Shell 1.0");
            } else if (strcmp(cmd, CLEAR_STRING) == 0 || strcmp(cmd, CLS_STRING) == 0) {
                vga_screen_clear();
            } else if (strcmp(cmd, TIME_STRING) == 0) {
                print_time();
            } else if (strncmp(cmd, ECHO_STRING, 5) == 0) {
                vga_print_scr(cmd + 5);
                vga_newline();
            } else if (strcmp(cmd, SHELLINFO_STRING) == 0) {
                vga_print_scr_nw("OpenKernel Shell 1.0");
            } else if (strcmp(cmd, SHUTDOWN_STRING) == 0) {
                sys_next_status("S");
            } else if (strcmp(cmd, REBOOT_STRING) == 0) {
                sys_next_status("R");
            } else if (strcmp(cmd, VERSION_STRING) == 0 || strcmp(cmd, VER_STRING) == 0) {
                vga_print_scr("OpenKernel v1.0");
                vga_newline();
            } else {
                vga_print_scr("The command you entered could not be found. Please type list for a list of all commands.");
                vga_newline();
            }

            shell_inp();
        } else {
            if (idx < MAX_CMD_LEN - 1) {
                cmd[idx++] = c;
                ptchar(c);
            }
        }
    }
}