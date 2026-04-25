#ifndef VMEM_H
#define VMEM_H

#include <OpenKernel/SystemLib/Std/types.h>

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4

void paging_init();
void load_page_dir(uint32_t* pd);
void enable_paging();

#endif