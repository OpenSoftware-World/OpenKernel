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

#define BASIC_SYSCALL_MODULE_NAME "Syscall Library"
#define BASIC_SYSCALL_MODULE_DESC "Syscall Library for OpenKernel"
#define BASIC_SYSCALL_MODULE_VER "1.0"
#define BASIC_SYSCALL_MODULE_AUTHOR "OpenSoftware-World"
#define BASIC_SYSCALL_MODULE_FILE_NAME "basic_syscall.lib"
#define BASIC_SYSCALL_MODULE_KRNL_VER "2.0"

#endif