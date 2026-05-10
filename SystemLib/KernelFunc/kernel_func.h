#ifndef KERNEL_FUNC_H
#define KERNEL_FUNC_H

#include <OpenKernel/SystemLib/Std/std.h>

/*
Function type
*/
typedef void (*kfunc_t)(void);

/*
Function register
*/
void register_func(const char *name, kfunc_t func);

/*
Function Run
*/
void exec_func(const char *func_name);

#define KERNEL_FUNC_MODULE_NAME "Kernel Func Library"
#define KERNEL_FUNC_MODULE_DESC "Kernel Func Library for OpenKernel"
#define KERNEL_FUNC_MODULE_VER "1.1"
#define KERNEL_FUNC_MODULE_AUTHOR "OpenSoftware-World"
#define KERNEL_FUNC_MODULE_FILE_NAME "kernel_func.lib"
#define KERNEL_FUNC_MODULE_KRNL_VER "3.0"

#endif