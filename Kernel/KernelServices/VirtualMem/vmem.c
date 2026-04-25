#include "vmem.h"

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t first_page_table[1024] __attribute__((aligned(4096)));

void paging_init() {
    for (int i = 0; i < 1024; i++) {
        first_page_table[i] = 0;
    }
    for (int i = 0; i < 1024; i++) {
        first_page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_RW;
    }
    for (int i = 0; i < 1024; i++) {
        page_dir[i] = 0;
    }
    page_dir[0] = ((uint32_t)first_page_table) | PAGE_PRESENT | PAGE_RW;
    load_page_dir(page_dir);
    enable_paging();
}

void load_page_dir(uint32_t* pd) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(pd));
}

void enable_paging() {
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0":"=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ __volatile__("mov %0, %%cr0"::"r"(cr0));
}