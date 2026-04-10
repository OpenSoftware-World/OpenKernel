#include "mem.h"

void* memset(void* dst, uint8_t val, uint32_t size) {
    uint8_t* p = (uint8_t*)dst;

    for (uint32_t i = 0; i < size; i++) {
        p[i] = (uint8_t)val;
    }
    return dst;
}

void* memcpy(void* dst, const void* src, uint32_t size) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dst;
}

int memcmp(const void* a, const void* b, uint32_t size) {
    const uint8_t* p1 = (const uint8_t*)a;
    const uint8_t* p2 = (const uint8_t*)b;

    for (uint32_t i = 0; i < size; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}