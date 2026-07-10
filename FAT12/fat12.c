#include "fat12.h"
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/Drivers/Vga/vga.h>

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

uint16_t fat12_get_next_cluster(uint16_t cluster) {
    uint32_t fat_offset = cluster + (cluster / 2);
    uint32_t fat_sector = fs.fat_start_lba + (fat_offset / fs.bs.bytes_per_sector);
    uint32_t end_offset = fat_offset % fs.bs.bytes_per_sector;

    uint8_t sector[512];
    read_sector(fat_sector, sector);
    uint16_t value = *(uint16_t*)&sector[end_offset];

    if (cluster & 0x0001) {
        value >>= 4;
    }
    else {
        value &= 0x0FFF;
    }
    return value;
}

void fat12_list_root() {
    uint8_t sector[512];

    for (uint32_t i = 0; i < fs.root_dir_size; i++) {
        read_sector(fs.root_dir_lba + i, sector);

        fat12_dir_entry_t* entry = (fat12_dir_entry_t*)sector;

        for (int j = 0; j < 16; j++) {

            if (entry[j].filename[0] == 0x00) {
                return;
            }

            if ((uint8_t)entry[j].filename[0] == 0xE5) {
                continue;
            }

            char name[12];
            for (int k = 0; k < 11; k++) {
                name[k] = entry[j].filename[k];
            }
            name[11] = '\0';
            vga_print_scr_nw(name);
        }
    }
}

int fat12_read_file(const char* filename, void* buffer, uint32_t size) {
    uint8_t sector[512];

    for (uint32_t i = 0; i < fs.root_dir_size; i++) {
        read_sector(fs.root_dir_lba + i, sector);
        fat12_dir_entry_t* entry = (fat12_dir_entry_t*)sector;

        for (int j = 0; j < 16; j++) {
            if (entry[j].filename[0] == 0x00)
                return -1;

            if (strncmp(entry[j].filename, filename, 11) == 0) {
                uint16_t cluster = entry[j].first_cluster;
                uint32_t read_bytes = 0;

                while (cluster < FAT12_EOC && read_bytes < size) {
                    uint32_t lba = fs.data_start_lba + (cluster - 2) * fs.bs.sectors_per_cluster;
                    read_sector(lba, buffer + read_bytes);
                    read_bytes += fs.bs.sectors_per_cluster * fs.bs.bytes_per_sector;
                    cluster = fat12_get_next_cluster(cluster);
                }
                return read_bytes;
            }
        }
    }
    return -1;
}