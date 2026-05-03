#include "serial.h"

#define SERIAL_LINE_STATUS_DATA_READY 0x01
#define SERIAL_LINE_STATUS_THRE 0x20

static bool_t serial_status = true;

void serial_init() {
    serial_status = true;
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
    if (!serial_status) return;
    while (!serial_is_transmit_empty());
    outb(COM1_PORT, c);
}

void serial_print_string(const char *string) {
    if (!serial_status) return;
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

int serial_received() {
    return inb(COM1_LSR) & 1;
}

char serial_read_char() {
    while (serial_received() == 0);
    return inb(COM1_PORT);
}

int serial_try_read_char(char *c) {
    if (!(inb(COM1_LSR) & 1)) {
        return 0;
    }
    *c = inb(COM1_PORT);
    return 1;
}

int serial_has_error() {
    uint8_t stat = inb(COM1_LSR);
    return stat & (0x0E | 0x80);
}

int serial_get_errors() {
    return inb(COM1_LSR) & (SERIAL_ERR_OVERRUN | SERIAL_ERR_PARITY | SERIAL_ERR_FRAME | SERIAL_ERROR_FIFO);
}

void serial_clear_errors() {
    inb(COM1_LSR);
}

void serial_log(const char* tag, const char* msg) {
    serial_print_string("[");
    serial_print_string(tag);
    serial_print_string("]");
    serial_print_string(msg);
    serial_print_string("\n");
}

void serial_success(const char* msg) {
    serial_log("SUCCESS", msg);
}

void serial_error(const char* msg) {
    serial_log("ERROR", msg);
}

void serial_warn(const char* msg) {
    serial_log("WARNING", msg);
}

void serial_info(const char* msg) {
    serial_log("INFO", msg);
}

void serial_unknown(const char* msg) {
    serial_log("UNKNOWN", msg);
}

void serial_set_status(bool_t val) {
    serial_status = val;
}

void serial_loopback_test() {
    outb(COM1_MCR, 0x1E);
    outb(COM1_PORT, 0xAE);
    if (inb(COM1_PORT) != 0xAE) {
        serial_error("Loopback test FAILED.");
    }
    else {
        serial_success("Loopback test OK.");
    }
    outb(COMR_MCR, 0x0);
}