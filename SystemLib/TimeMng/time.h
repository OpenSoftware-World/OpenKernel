#ifndef TIME_H
#define TIME_H

#include <OpenKernel/SystemLib/Std/types.h>

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} systime;

extern systime sys_time;

void uptime();
systime get_time();
void print_time();
void sleep(uint32_t seconds);

#define TIME_MODULE_NAME "Time Library"
#define TIME_MODULE_DESC "Time Library for OpenKernel"
#define TIME_MODULE_VER "1.0"
#define TIME_MODULE_AUTHOR "OpenSoftware-World"
#define TIME_MODULE_FILE_NAME "time.lib"
#define TIME_MODULE_KRNL_VER "2.0"

#endif