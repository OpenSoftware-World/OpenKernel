#ifndef QEMU_IO_H
#define QEMU_IO_H

#include "io.h"

static inline void qemu_print_dbg(char c) {
    outb(0xE9, c);
}

static inline void qemu_str_print_dbg(const char* str) {
    while (*str) {
        outb(0xE9, *str++);
    }
}

#endif