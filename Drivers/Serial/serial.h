#ifndef SERIAL_H
#define SERIAL_H

#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>

#define COM1_PORT 0x3F8
#define COM1_LSR (COM1_PORT + 5)
#define SERIAL_ERR_OVERRUN 0x02
#define SERIAL_ERR_PARITY 0x04
#define SERIAL_ERR_FRAME 0x08
#define SERIAL_ERROR_FIFO 0x80
#define COM1_MCR (COM1_PORT + 4)

void serial_init();
int serial_is_transmit_empty();
void serial_print_char(char c);
void serial_print_string(const char *string);
char serial_read_char();
int serial_received();
int serial_try_read_char(char *c);
int serial_has_error();
int serial_get_error();
void serial_clear_errors();
void serial_log(const char* tag, const char* msg);
void serial_success(const char* msg);
void serial_error(const char* msg);
void serial_warn(const char* msg);
void serial_info(const char* msg);
void serial_unknown(const char* msg);
void serial_set_status(bool_t val);
void serial_loopback_test();

#define SERIAL_DRIVER_NAME "OpenKernel Serial (COM1_PORT) Driver"
#define SERIAL_DRIVER_VER "0.2"
#define SERIAL_DRIVER_DESC "A Serial (COM1_PORT) driver for OpenKernel"
#define SERIAL_DRIVER_AUTHOR "OpenSoftware-World"
#define SERIAL_DRIVER_KRNL_VER "3.0"

#endif