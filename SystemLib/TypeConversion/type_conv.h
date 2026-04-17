#ifndef TYPE_CONV_H
#define TYPE_CONV_H

#include <OpenKernel/SystemLib/Std/types.h>

char* itoa(int val, char* str);
char* htoa(uint32_t val, char* str);
char* htoan(uint32_t val, char* str, int width);
int atoi(const char* str);
uint8_t bcd_to_bin(uint8_t val);

#endif