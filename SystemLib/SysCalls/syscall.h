#ifndef SYSCALL_H
#define SYSCALL_H

#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/Kernel/KernelServices/SystemManagement/sysmng.h>
#include <OpenKernel/Kernel/KernelServices/SecurityChecker/securchk.h>

#define SYSCALL_PRINT           0
#define SYSCALL_CLEAR_SCREEN    1
#define SYSCALL_SHUTDOWN        2
#define SYSCALL_REBOOT          3
#define SYSCALL_SECURCHK_INIT   4
#define SYSCALL_SECURCHK_RUN    5
#define SYSCALL_SECURCHK_STATUS 6
#define SYSCALL_MAX_ID          31

int syscall(int id, void* arg);
int syscall_handler(int id, void* arg);

#define SYSCALL_MODULE_NAME "Syscall Library"
#define SYSCALL_MODULE_DESC "Syscall Library for OpenKernel"
#define SYSCALL_MODULE_VER "1.0"
#define SYSCALL_MODULE_AUTHOR "OpenSoftware-World"
#define SYSCALL_MODULE_FILE_NAME "syscall.lib"
#define SYSCALL_MODULE_KRNL_VER "2.0"

#endif
