#include "sysmng.h"
#include "../../../SystemLib/SystemIO/io.h"

void sys_next_status(const char *mode) {
    if (mode[0] == 'S') {
        outb(0x604, 0x00);
        outb(0x605, 0x20);
    }
    else if(mode[0] == 'R') {
        __asm__ volatile ("cli");
        while (inb(0x64) & 0x02);
        outb(0x64, 0xFE);
    }
}