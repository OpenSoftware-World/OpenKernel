#include "fat16.h"
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/Drivers/Vga/vga.h>

static fat16_fs_t fs;

static void read_sector(uint32_t lba, void* buffer) {
    ata_read_sector(lba, buffer);
}

void fat16_init(uint32_t boot_lba) {
    read_sector(boot_lba, &fs.bs);
    fs.fat_start_lba = boot_lba + fs.bs.reserved_sectors;
    fs.root_dir_lba = fs.fat_start_lba + (fs.bs.fat_count * fs.bs.sectors_per_fat);
    fs.root_dir_sectors = ((fs.bs.root_entry_count * 32) + (fs.bs.bytes_per_sector - 1)) / fs.bs.bytes_per_sector;
    fs.data_start_lba = fs.root_dir_lba + fs.root_dir_sectors;
}

uint16_t fat16_get_next_cluster(uint16_t cluster) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fs.fat_start_lba + (fat_offset / fs.bs.bytes_per_sector);
    uint32_t ent_offset = fat_offset % fs.bs.bytes_per_sector;
    uint8_t sector[512];
    read_sector(fat_sector, sector);
    return *(uint16_t*)&sector[ent_offset];
}

void fat16_list_root() {
    uint8_t sector[512];

    for (uint32_t i = 0; i < fs.root_dir_sectors; i++) {
        read_sector(fs.root_dir_lba + i, sector);
        fat16_dir_entry_t* entry = (fat16_dir_entry_t*)sector;

        for (int j = 0; j < 16; j++) {
            if (entry[j].filename[0] == 0x00)
                return;

            if ((uint8_t)entry[j].filename[0] == 0xE5)
                continue;
            char name[12];
            int k = 0;
            for (int i = 0; i < 11; i++) {
                if (entry[j].filename[i] == ' ')
                    continue;
                name[k++] = entry[j].filename[i];
            }
            name[k] = '\0';
            vga_print_scr_nw(name);
        }
    }
}

int fat16_read_file(const char* filename, void* buffer, uint32_t size) {
    uint8_t sector[512];
    for (uint32_t i = 0; i < fs.root_dir_sectors; i++) {
        read_sector(fs.root_dir_lba + i, sector);
        fat16_dir_entry_t* entry = (fat16_dir_entry_t*)sector;

        for (int j = 0; j < 16; j++) {
            if (entry[j].filename[0] == 0x00)
                return -1;

            if (strncmp(entry[j].filename, filename, 11) == 0) {
                uint16_t cluster = entry[j].first_cluster;
                uint32_t offset = 0;

                while (cluster < FAT16_EOC && offset < size) {

                    uint32_t lba = fs.data_start_lba + (cluster - 2) * fs.bs.sectors_per_cluster;
                    read_sector(lba, buffer + offset);
                    offset += fs.bs.sectors_per_cluster * fs.bs.bytes_per_sector;
                    cluster = fat16_get_next_cluster(cluster);
                }
                return offset;
            }
        }
    }
    return -1;
}