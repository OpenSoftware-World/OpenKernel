#include "fat12.h"
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/SystemLib/Std/std.h>

static fat12_fs_t fs;

static void read_sector(uint32_t lba, void* buf) {
    ata_sys_disk_read(lba, buf);
}

void fat12_init(uint32_t boot_lba) {
    read_sector(boot_lba, &fs.bs);
    fs.fat_start_lba = boot_lba + fs.bs.reserved_sectors;
    fs.root_dir_lba = fs.fat_start_lba + (fs.bs.fat_count * fs.bs.sectors_per_fat);
    fs.root_dir_size = (fs.bs.root_entry_count * 32) / fs.bs.bytes_per_sector;
    fs.data_start_lba = fs.root_dir_lba + fs.root_dir_size;
}

