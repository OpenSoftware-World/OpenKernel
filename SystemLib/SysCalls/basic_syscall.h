#ifndef BASIC_SYSCALL_H
#define BASIC_SYSCALL_H

#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/Kernel/KernelServices/SystemManagement/sysmng.h>

#define SYSCALL_PRINT 0
#define SYSCALL_CLEAR_SCREEN 1
#define SYSCALL_SHUTDOWN 2
#define SYSCALL_REBOOT 3

int syscall(int id, void* arg);
int syscall_handler(int id, void* arg);

#endif