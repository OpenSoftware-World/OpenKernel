#ifndef CPU_H
#define CPU_H

#include <OpenKernel/SystemLib/Std/types.h>

typedef struct {
    char sys_cpu_brand[49];
    char sys_cpu_vendor[13];
    uint8_t sse;
    uint8_t sse2;
    uint8_t avx;
    uint8_t cores;
    uint8_t mmx;
    uint8_t sse3;
    uint8_t ssse3;
    uint8_t sse41;
    uint8_t sse42;
    uint8_t osxsave;
    uint8_t vmx;
    uint8_t svm;
    uint8_t hyper_threading;
    uint8_t avx2;
    uint8_t bmi1;
    uint8_t bmi2;
    uint8_t sgx;
} sys_cpuinfo_t;

uint32_t cpuinfo_data(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx, uint32_t function_id);
void get_sys_cpuinfo(const char *mode);
void cpu_status(const char *mode);
void cli(void);
void sti(void);
void hlt(void);
void nop(void);

#define CPU_DRIVER_NAME "OpenKernel CPU Driver"
#define CPU_DRIVER_VER "0.3"
#define CPU_DRIVER_DESC "A simple CPU Driver for OpenKernel"
#define CPU_DRIVER_AUTHOR "OpenSoftware-World"
#define CPU_DRIVER_KRNL_VER "2.0"

#endif