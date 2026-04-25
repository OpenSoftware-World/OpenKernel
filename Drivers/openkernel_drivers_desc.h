#ifndef OPENKERNEL_DRIVERS_DESC_H
#define OPENKERNEL_DRIVERS_DESC_H

#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/Drivers/Cpu/cpu.h>
#include <OpenKernel/Drivers/Keyboard/keyboard.h>
#include <OpenKernel/Drivers/Serial/serial.h>
#include <OpenKernel/Drivers/Vga/vga.h>

typedef struct {
    const char *driver_name;
    const char *driver_desc;
    const char *driver_ver;
    const char *driver_author;
    const char *driver_krnl_ver;
} openkernel_drivers_information_t;

openkernel_drivers_information_t ata_driver;
openkernel_drivers_information_t cpu_driver;
openkernel_drivers_information_t keyboard_driver;
openkernel_drivers_information_t serial_driver;
openkernel_drivers_information_t vga_driver;

ata_driver = (openkernel_drivers_information_t) {
    .driver_name = ATA_DRIVER_NAME,
    .driver_desc = ATA_DRIVER_DESC,
    .driver_ver = ATA_DRIVER_VER,
    .driver_author = ATA_DRIVER_AUTHOR,
    .driver_krnl_ver = ATA_DRIVER_KRNL_VER,
};

cpu_driver = (openkernel_drivers_information_t) {
    .driver_name = CPU_DRIVER_NAME,
    .driver_desc = CPU_DRIVER_DESC,
    .driver_ver = CPU_DRIVER_VER,
    .driver_author = CPU_DRIVER_AUTHOR,
    .driver_krnl_ver = CPU_DRIVER_KRNL_VER,
};

keyboard_driver = (openkernel_drivers_information_t) {
    .driver_name = KBD_DRIVER_NAME,
    .driver_desc = KBD_DRIVER_DESC,
    .driver_ver = KBD_DRIVER_VER,
    .driver_author = KBD_DRIVER_AUTHOR,
    .driver_krnl_ver = KBD_DRIVER_KRNL_VER,
};

serial_driver = (openkernel_drivers_information_t) {
    .driver_name = SERIAL_DRIVER_NAME,
    .driver_desc = SERIAL_DRIVER_DESC,
    .driver_ver = SERIAL_DRIVER_VER,
    .driver_author = SERIAL_DRIVER_AUTHOR,
    .driver_krnl_ver = SERIAL_DRIVER_KRNL_VER,
};

vga_driver = (openkernel_drivers_information_t) {
    .driver_name = VGA_DRIVER_NAME,
    .driver_desc = VGA_DRIVER_DESC,
    .driver_ver = VGA_DRIVER_VER,
    .driver_author = VGA_DRIVER_AUTHOR,
    .driver_krnl_ver = VGA_DRIVER_KRNL_VER,
};

#endif
