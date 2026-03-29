#include "kernel_func.h"

/*
extern void example_func(void);
*/

func_table_t ftable[] = {
    {"", },
    {0, 0}
}

void exec_func(const char *func_name) {
    for (int i = 0; ftable[i].name != 0; i++) {
        if (strcmp(ftable[i].name, func_name) == 0) {
            __asm__ __volatile__ (
                "call *%0"
                :
                : "r"(ftable[i].call_func)
            );
            return;
        }
    }
}