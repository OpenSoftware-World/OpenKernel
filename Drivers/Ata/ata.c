#include "ata.h"
#include <OpenKernel/SystemLib/SystemIO/io.h>

static void ata_wait_busy() {
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
}

static void ata_wait_drq() {
    while(!(inb(ATA_STATUS) & ATA_STATUS_DRQ));
}

void ata_sys_disk_init() {

}

void ata_sys_disk_read(uint32_t lba, uint8_t* buffer) {
    ata_wait_busy();

    outb(ATA_DRIVE_SELECT, 0x0E | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_COUNT, 1);

    outb(ATA_LBA_LOW, (uint8_t)(lba));
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));

    outb(ATA_COMMAND, ATA_READ);

    ata_wait_busy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buffer)[i] = inw(ATA_DATA);
    }
}

void ata_sys_disk_write(uint32_t lba, const uint8_t* buffer) {
    ata_wait_busy();

    outb(ATA_DRIVE_SELECT, 0x0E | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_COUNT, 1);

    outb(ATA_LBA_LOW, (uint8_t)(lba));
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));

    outb(ATA_COMMAND, ATA_WRITE);

    ata_wait_busy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, ((uint16_t*)buffer)[i]);
    }
}

void ata_soft_reset() {
    outb(ATA_CONTROL, ATA_SRST);

    for (int i = 0; i < 1000; i++) {
        asm volatile ("nop");
    }
    outb(ATA_CONTROL, 0x00);

    for (int i = 0; i < 10000; i++) {
        asm volatile("nop");
    }
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
}

void ata_reinit(uint8_t drive, uint16_t* identify_buf) {
    ata_wait_busy();

    outb(ATA_DRIVE_SELECT, drive);

    for (volatile int i = 0; i < 10000; i++) {
        asm volatile ("nop");
    }
    ata_identify(drive, identify_buf);
}

void ata_identify(uint8_t drive, uint16_t* buf) {
    ata_wait_busy();
    outb(ATA_DRIVE_SELECT, drive);
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);

    outb(ATA_COMMAND, ATA_IDENTIFY);

    ata_wait_busy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++) {
        buf[i] = inw(ATA_DATA);
    }
}