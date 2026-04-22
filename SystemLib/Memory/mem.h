#ifndef MEM_H
#define MEM_H

#include <OpenKernel/SystemLib/Std/types.h>

void* memset(void* dst, uint8_t val, uint32_t size);
void* memcpy(void* dst, const void* src, uint32_t size);
int memcmp(const void* a, const void* b, uint32_t size);

void heap_init(uint32_t start, uint32_t size);
void* kmalloc(uint32_t size);
void kfree(void* ptr);

#define MEM_MODULE_NAME "Memory Library"
#define MEM_MODULE_DESC "Memory Library for OpenKernel"
#define MEM_MODULE_VER "1.0"
#define MEM_MODULE_AUTHOR "OpenSoftware-World"
#define MEM_MODULE_FILE_NAME "mem.lib"
#define MEM_MODULE_KRNL_VER "2.0"

#endif