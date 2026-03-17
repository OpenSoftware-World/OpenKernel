#ifndef TIME_H
#define TIME_H

#include "../Std/types.h"

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

#endif