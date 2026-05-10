#include "kernel_func.h"

#define MAX_FUNC 256

typedef struct {
    const char *name;
    kfunc_t func;
} kfunc_entry_t;

static kfunc_entry_t ftable[MAX_FUNC];
static int kfunc_count = 0;

void register_func(const char *name, kfunc_t func) {
    if (kfunc_count >= MAX_FUNC) {
        return;
    }
    ftable[kfunc_count].name = name;
    ftable[kfunc_count].func = func;
    kfunc_count++;
}

void exec_func(const char *func_name) {
    for (int i = 0; i < kfunc_count; i++) {
        if (strcmp(ftable[i].name, func_name) == 0) {
            ftable[i].func();
            return;
        }
    }
}

void exec_command(const char *command_name) {
    exec_func(command_name);
}