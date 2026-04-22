#ifndef TYPE_CONV_H
#define TYPE_CONV_H

#include <OpenKernel/SystemLib/Std/types.h>

char* itoa(int val, char* str);
char* htoa(uint32_t val, char* str);
char* htoan(uint32_t val, char* str, int width);
int atoi(const char* str);
uint8_t bcd_to_bin(uint8_t val);

#define TYPE_CONV_MODULE_NAME "Type Conversion Library"
#define TYPE_CONV_MODULE_DESC "Type Conversion Library for OpenKernel"
#define TYPE_CONV_MODULE_VER "1.0"
#define TYPE_CONV_MODULE_AUTHOR "OpenSoftware-World"
#define TYPE_CONV_MODULE_FILE_NAME "type_conv.lib"
#define TYPE_CONV_MODULE_KRNL_VER "2.0"

#endif