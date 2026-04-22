#ifndef KERNEL_FUNC_H
#define KERNEL_FUNC_H

#include <OpenKernel/SystemLib/Std/std.h>
#include "extern_func.h"

/*
Function table
*/

typedef void (*kfunc_table_t)(void);

typedef struct {
    const char *name;
    kfunc_table_t call_func;
} func_table_t;

void exec_func(const char *func_name);

#define KERNEL_FUNC_MODULE_NAME "Kernel Func Library"
#define KERNEL_FUNC_MODULE_DESC "Kernel Func Library for OpenKernel"
#define KERNEL_FUNC_MODULE_VER "1.0"
#define KERNEL_FUNC_MODULE_AUTHOR "OpenSoftware-World"
#define KERNEL_FUNC_MODULE_FILE_NAME "kernel_func.lib"
#define KERNEL_FUNC_MODULE_KRNL_VER "2.0"

#endif