#ifndef SERIAL_H
#define SERIAL_H

#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>

#define COM1_PORT 0x3F8

void serial_init();
int serial_is_transmit_empty();
void serial_print_char(char c);
void serial_print_string(const char *string);

#endif