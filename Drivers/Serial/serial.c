#include "serial.h"

#define SERIAL_LINE_STATUS_DATA_READY 0x01
#define SERIAL_LINE_STATUS_THRE 0x20

void serial_init() {
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
    outb(COM1_PORT + 4, 0x1E);
    outb(COM1_PORT + 0, 0xAE);
    if (inb(COM1_PORT + 0) != 0xAE) {
        return;
    }
    outb(COM1_PORT + 4, 0x0F);
}

int serial_is_transmit_empty() {
    return inb(COM1_PORT + 5) & SERIAL_LINE_STATUS_THRE;
}

void serial_print_char(char c) {
    while (!serial_is_transmit_empty());
    outb(COM1_PORT, c);
}

void serial_print_string(const char *string) {
    while (*string) {
        if (*string == '\n') {
            serial_print_char('\r');
        }
        serial_print_char(*string++);
    }
}

char serial_read_char() {
    while (!(inb(COM1_PORT + 5) & SERIAL_LINE_STATUS_DATA_READY));
    return inb(COM1_PORT);
}