#ifndef STD_H
#define STD_H

#include "types.h"

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, unsigned int n);
int strlen(const char *str);
void strcpy(char *dst, const char *src);
void strncpy(char *dst, const char *src, unsigned int n);

#endif