#include "time.h"
#include "../../Drivers/Vga/vga.h"
#include "../SystemIO/io.h"

systime sys_time = {0, 0, 0, 0, 0, 0};

#define RTC_ADDR 0x70
#define RTC_DATA 0x71

static uint8_t rtc_read(uint8_t reg) {
    outb(RTC_ADDR, reg);
    return inb(RTC_DATA);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return ((val / 16) * 10) + (val & 0x0F);
}

void uptime() {
    uint8_t sec, min, hour, day, month, year;
    sec = rtc_read(0x00);
    min = rtc_read(0x02);
    hour = rtc_read(0x04);
    day = rtc_read(0x07);
    month = rtc_read(0x08);
    year = rtc_read(0x09);

    sec = bcd_to_bin(sec);
    min = bcd_to_bin(min);
    hour = bcd_to_bin(hour);
    day = bcd_to_bin(day);
    month = bcd_to_bin(month);
    year = bcd_to_bin(year);

    sys_time.hours = hour;
    sys_time.minutes = min;
    sys_time.seconds = sec;
    sys_time.day = day;
    sys_time.month = month;
    sys_time.year = year + 2000;
}

systime get_time() {
    uptime();
    return sys_time;
}

void print_time() {
    uptime();
    char buf[20]; // DD/MM/YYYY HH:MM:SS
    buf[0]  = '0' + (sys_time.day / 10);
    buf[1]  = '0' + (sys_time.day % 10);
    buf[2]  = '/';
    buf[3]  = '0' + (sys_time.month / 10);
    buf[4]  = '0' + (sys_time.month % 10);
    buf[5]  = '/';
    buf[6]  = '0' + ((sys_time.year / 1000) % 10);
    buf[7]  = '0' + ((sys_time.year / 100) % 10);
    buf[8]  = '0' + ((sys_time.year / 10) % 10);
    buf[9]  = '0' + (sys_time.year % 10);
    buf[10] = ' ';
    buf[11] = '0' + (sys_time.hours / 10);
    buf[12] = '0' + (sys_time.hours % 10);
    buf[13] = ':';
    buf[14] = '0' + (sys_time.minutes / 10);
    buf[15] = '0' + (sys_time.minutes % 10);
    buf[16] = ':';
    buf[17] = '0' + (sys_time.seconds / 10);
    buf[18] = '0' + (sys_time.seconds % 10);
    buf[19] = 0;

    vga_print_scr(buf);
    vga_newline();
}

void sleep(uint32_t seconds) {
    for (uint64_t i = 0; i < (uint64_t)seconds * 50000000ULL; i++) {
        __asm__ __volatile__ ("nop");
    }
}