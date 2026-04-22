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

#define QEMU_IO_MODULE_NAME "QEMU_IO Library"
#define QEMU_IO_MODULE_DESC "QEMU_IO Library for OpenKernel"
#define QEMU_IO_MODULE_VER "1.0"
#define QEMU_IO_MODULE_AUTHOR "OpenSoftware-World"
#define QEMU_IO_MODULE_FILE_NAME "qemu_io.lib"
#define QEMU_IO_MODULE_KRNL_VER "2.0"

#endif