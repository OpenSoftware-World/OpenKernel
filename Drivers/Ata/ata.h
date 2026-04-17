#ifndef ATA_H
#define ATA_H

/*
 * ATA Driver for OpenKernel
*/

#include <OpenKernel/SystemLib/Std/types.h>

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE_SELECT 0x1F6
#define ATA_COMMAND 0x1F7
#define ATA_STATUS 0x1F7
#define ATA_IDENTIFY 0xEC

#define ATA_READ 0x20
#define ATA_WRITE 0x30

#define ATA_STATUS_BSY 0x80
#define ATA_STATUS_DRQ 0x08

#define ATA_CONTROL 0x3F6
#define ATA_SRST 0x04

void ata_sys_disk_init();
void ata_sys_disk_read(uint32_t lba, uint8_t* buffer);
void ata_sys_disk_write(uint32_t lba, const uint8_t* buffer);
void ata_soft_reset();
void ata_reinit(uint8_t drive, uint16_t* identify_buf);
void ata_identify(uint8_t drive, uint16_t* buf);

#define ATA_DRIVER_NAME "OpenKernel ATA Disk Driver"
#define ATA_DRIVER_VER "0.3"
#define ATA_DRIVER_DESC "A simple ATA Disk Driver for OpenKernel"
#define ATA_DRIVER_AUTHOR "OpenSoftware-World"
#define ATA_DRIVER_KRNL_VER "2.0"

#endif