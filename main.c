#include "SystemLib/Std/types.h"
#include "Drivers/Vga/vga.h"
#include "SystemLib/SysCalls/basic_syscall.h"
#include "SystemLib/TimeMng/time.h"
#include "Kernel/KernelServices/Shell/shell.h"

void kernel_main()
{
    // The system is starting up and displaying log messages
    vga_init();
    sleep(30);
    log_message("S", "System disk read successful; loading the kernel...");
    sleep(30);
    log_message("I", "Kernel initialized successfully.");
    sleep(20);
    log_message("I", "VGA Driver initalized successfully.");
    sleep(10);
    log_message("I", "RTC Services initalized successfully.");
    sleep(10);
    log_message("I", "Shell initalized successfully.");
    sleep(5);
    log_message("W", "Since this system is still new, there may be some glitches.");
    sleep(15);
    log_message("U", "The hardware status is unknown.");
    sleep(25);
    log_message("S", "The system is ready to start; the system is starting.");
    sleep(100);

    // Main menu
    vga_set_color_scheme(VGA_COLOR_BLUE, VGA_COLOR_WHITE);
    vga_screen_clear();
    vga_print_scr_nw("Welcome to the OpenKernel!");
    vga_print_scr_nw("OpenKernel version 1.0 (It was created for future use in OpenSoftware-World OS.)");

    // Shell is starting
    vga_newline();
    vga_set_cursor(12, 4);
    shell_init();
}