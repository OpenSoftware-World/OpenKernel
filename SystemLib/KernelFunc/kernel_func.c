#include "kernel_func.h"
#include <OpenKernel/SystemLib/Memory/mem.h>

#define MAX_FUNC 256

typedef struct {
    const char *name;
    kfunc_t func;
} kfunc_entry_t;

static kfunc_entry_t ftable[MAX_FUNC];
static int kfunc_count = 0;
static int kfunc_s = 0;
static bool_t kernel_func_enabled = false;

void register_func(const char *name, kfunc_t func) {
    if (kernel_func_enabled == false) return;
    if (kfunc_count >= MAX_FUNC) {
        return;
    }
    ftable[kfunc_count].name = name;
    ftable[kfunc_count].func = func;
    kfunc_count++;
}

void exec_func(const char *func_name) {
    if (kernel_func_enabled == false) return;
    for (int i = 0; i < kfunc_count; i++) {
        if (strcmp(ftable[i].name, func_name) == 0) {
            ftable[i].func();
            return;
        }
    }
}

void exec_command(const char *command_name) {
    if (kernel_func_enabled == false) return;
    exec_func(command_name);
}

void kernel_func_init() {
    memset(ftable, 0, sizeof(ftable));
    kfunc_count = 0;
}

void KernelFunc_O() {
    if (kernel_func_enabled == true) return;
    kernel_func_enabled = true;
}

void KernelFunc_C() {
    if (kernel_func_enabled == false) return;
    kernel_func_enabled = false;
}