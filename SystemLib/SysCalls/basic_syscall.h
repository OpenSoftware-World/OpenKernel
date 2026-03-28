#ifndef BASIC_SYSCALL_H
#define BASIC_SYSCALL_H

#include "../../SystemLib/Std/types.h"

void exit_program(const char *mode, const char *message);
void log_message(const char *mode, const char *message);

#endif