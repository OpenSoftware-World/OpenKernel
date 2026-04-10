#include "std.h"

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char*s2, unsigned int n) {
    unsigned int i;
    for (i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == 0 || s2[i] == 0) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

int strlen(const char *str) {
    int len = 0;
    while (*str++) len++;
    return len;
}

void strcpy(char* dst, const char *src) {
    while (*src) {
        *dst++ = *src++;
    }
    *dst = 0;
}

void strncpy(char* dst, const char *src, unsigned int n) {
    unsigned int i;
    for (i = 0; i < n && src[i]; i++) {
        dst[i] = src[i];
    }
    if (i < n) {
        dst[i] = 0;
    }
}