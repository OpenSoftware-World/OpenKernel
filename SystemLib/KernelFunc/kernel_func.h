#ifndef KERNEL_FUNC_H
#define KERNEL_FUNC_H

#include "../Std/std.h"

/*
Function table
*/
typedef void (*kfunc_table_t)(void);

typedef struct {
    const char *name;
    kfunc_table_t call_func;
} func_table_t;

void exec_func(const char *func_name);

#endif