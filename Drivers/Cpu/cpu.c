#include "cpu.h"
#include "../Vga/vga.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"

uint32_t cpuinfo_data(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx, uint32_t function_id) {
    __asm__ __volatile__ ("cpuid": "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx): "a"(function_id));
}

#pragma GCC diagnostic pop

void get_sys_cpuinfo(const char *mode) {
    if (mode[0] == 'B') {
        sys_cpuinfo_t cpuinfo;
        uint32_t *p = (uint32_t*)cpuinfo.sys_cpu_brand;
        
        for (uint32_t i = 0; i < 3; i++) {
            cpuinfo_data(&p[i*4], &p[i*4+1], &p[i*4+2], &p[i*4+3], 0x80000002 + i);
        }
        cpuinfo.sys_cpu_brand[48] = '\0';
        vga_print_scr("CPU Brand: ");
        vga_print_scr_nw(cpuinfo.sys_cpu_brand);
    }
}

void cpu_status(const char *mode) {
    if (mode[0] == 'S') {
        __asm__ __volatile__ ("hlt");
    }
    if (mode[0] == 'A') {
        __asm__ __volatile__ ("sti");
    }
}

void cli(void) {
    __asm__ __volatile__ ("cli" ::: "memory");
}

void sti(void) {
    __asm__ __volatile__ ("sti" ::: "memory");
}

void hlt(void) {
    __asm__ __volatile__ ("hlt");
}

void nop(void) {
    __asm__ __volatile__ ("nop");
}