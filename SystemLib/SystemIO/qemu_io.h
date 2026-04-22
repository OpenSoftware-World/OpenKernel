#ifndef QEMU_IO_H
#define QEMU_IO_H

#include "io.h"
#include <OpenKernel/Kernel/KernelServices/LogService/log.h>

static inline void qemu_print_dbg(char c) {
    outb(0xE9, c);
}

static inline void qemu_str_print_dbg(const char* str) {
    while (*str) {
        outb(0xE9, *str++);
    }
}

static inline void qemu_log_message(klog_level_t level, const char *message) {
    /*
    S: Success
    E: Error
    W: Warning
    I: Info
    U: Unknown
    */
    switch (level) {
        case LOG_SUCCESS:
            qemu_str_print_dbg("[SUCCESS]: ");
            break;
        case LOG_ERROR:
            qemu_str_print_dbg("[ERROR]: ");
            break;
        case LOG_WARNING:
            qemu_str_print_dbg("[WARNING]: ");
            break;
        case LOG_INFO:
            qemu_str_print_dbg("[INFO]: ");
            break;
        case LOG_UNKNOWN:
        default:
            qemu_str_print_dbg("[UNKNOWN]: ");
            break;
    }
    qemu_str_print_dbg(message);
}

#define QEMU_IO_MODULE_NAME "QEMU I/O Library"
#define QEMU_IO_MODULE_DESC "QEMU I/O Library for OpenKernel"
#define QEMU_IO_MODULE_VER "1.0"
#define QEMU_IO_MODULE_AUTHOR "OpenSoftware-World"
#define QEMU_IO_MODULE_FILE_NAME "qemu_io.lib"
#define QEMU_IO_MODULE_KRNL_VER "2.0"

#endif