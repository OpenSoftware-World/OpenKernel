#ifndef MEM_H
#define MEM_H

#include "../Std/types.h"

void* memset(void* dst, uint8_t val, uint32_t size);
void* memcpy(void* dst, const void* src, uint32_t size);
int memcmp(const void* a, const void* b, uint32_t size);

#endif