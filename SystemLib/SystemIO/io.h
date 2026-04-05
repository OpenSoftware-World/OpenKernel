#ifndef IO_H
#define IO_H

#include "../Std/types.h"

/*
outb:

Data width: 8-bit
Data direction: CPU -> Port
Description: 1 byte is sent from the CPU to the specified hardware port.
*/
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__ ("outb %0, %1": :"a"(val), "Nd"(port));
}

/*
inb:

Data width: 8-bit
Data direction: Port -> CPU
Description: Reads 1 byte of data from the specified CPU port.
*/
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
outw:

Data width: 16-bit
Data direction: CPU -> Port
Description: 2 byte is sent from the CPU to the specified hardware port.
*/
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__ ("outw %0, %1": :"a"(val), "Nd"(port));
}

/*
inw:

Data width: 16-bit
Data direction: Port -> CPU
Description: Reads 2 byte of data from the specified CPU port.
*/
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__ ("inw %1, %0": "=a"(ret): "Nd"(port));
    return ret;
}

/*
outl:

Data width: 32-bit
Data direction: CPU -> Port
Description: 4 byte is sent from the CPU to the specified hardware port.
*/
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__ ("outl %0, %1": : "a"(val), "Nd"(port));
}

/*
inl:

Data width: 32-bit
Data direction: Port -> CPU
Description: Reads 4 byte of data from the specified CPU port.
*/
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ __volatile__ ("inl %1, %0": "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insw(uint16_t port, void* addr, uint32_t count) {
    __asm__ __volatile__ ("rep insw"
                            : "+D"(addr), "+c"(count)
                            : "d"(port)
                            : "memory");
}

static inline void outsw(uint16_t port, const void* addr, uint32_t count) {
    __asm__ __volatile__ ("rep outsw"
                            : "+S"(addr), "+c"(count)
                        : "d"(port));
}

static inline void sys_wait(void) {
    __asm__ __volatile__ ("outb %%al, $0x80": :"a"(0): "memory");
}

static inline void sys_barrier(void) {
    __asm__ __volatile__ ("" ::: "memory");
}

#endif