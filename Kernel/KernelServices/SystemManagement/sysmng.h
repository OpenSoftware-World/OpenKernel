#ifndef SYSMNG_H
#define SYSMNG_H

#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>
#include <OpenKernel/SystemLib/TimeMng/time.h>
#include <OpenKernel/Drivers/Vga/vga.h>

void sys_next_status(const char *mode, uint32_t seconds);
void exit_program(const char *mode, const char *message);

#endif