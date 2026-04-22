#ifndef STD_H
#define STD_H

#include "types.h"

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, unsigned int n);
int strlen(const char *str);
void strcpy(char *dst, const char *src);
void strncpy(char *dst, const char *src, unsigned int n);

#define STD_MODULE_NAME "Std Library"
#define STD_MODULE_DESC "Std Library for OpenKernel"
#define STD_MODULE_VER "1.0"
#define STD_MODULE_AUTHOR "OpenSoftware-World"
#define STD_MODULE_FILE_NAME "std.lib"
#define STD_MODULE_KRNL_VER "2.0"

#endif