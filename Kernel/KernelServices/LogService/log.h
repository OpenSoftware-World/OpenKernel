#ifndef LOG_H
#define LOG_H

#include "../../../Drivers/Vga/vga.h"

enum kernel_log_level {
    LOG_SUCCESS,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_UNKNOWN
} klog_level_t;

void log_message(klog_level_t level, const char *message);

#endif