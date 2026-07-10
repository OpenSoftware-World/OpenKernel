#include "fat32.h"
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/Drivers/Vga/vga.h>

static fat32_fs_t fs;

static void read_sector(uint32_t lba, void* buffer) {
    ata_read_sector(lba, buffer);
}

void fat32_init(uint32_t boot_lba) {
    read_sector(boot_lba, &fs.bs);
    fs.fat_start_lba = boot_lba + fs.bs.reserved_sectors;
    fs.data_start_lba = fs.fat_start_lba + (fs.bs.fat_count * fs.bs.sectors_per_fat);
    fs.root_cluster = fs.bs.root_cluster;
}

uint32_t fat32_get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs.fat_start_lba + (fat_offset / fs.bs.bytes_per_sector);
    uint32_t ent_offset = fat_offset % fs.bs.bytes_per_sector;

    uint8_t sector[512];
    read_sector(fat_sector, sector);

    uint32_t next = *(uint32_t*)&sector[ent_offset];
    return next & 0x0FFFFFFF;
}

void fat32_list_dir(uint32_t cluster) {
    uint8_t sector[512];
    uint32_t current = cluster;

    while (current < FAT32_EOC) {

        uint32_t lba = fs.data_start_lba + (current - 2) * fs.bs.sectors_per_cluster;

        for (uint8_t i = 0; i < fs.bs.sectors_per_cluster; i++) {
            read_sector(lba + i, sector);
            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)sector;

            for (int j = 0; j < 16; j++) {
                if (entry[j].filename[0] == 0x00)
                    return;

                if ((uint8_t)entry[j].filename[0] == 0xE5)
                    continue;

                char name[12];
                int k = 0;

                for (int x = 0; x < 11; x++) {
                    if (entry[j].filename[x] == ' ')
                        continue;

                    name[k++] = entry[j].filename[x];
                }

                name[k] = '\0';

                if (entry[j].attr & 0x10) {
                    vga_print_scr("[DIR] ");
                } else {
                    vga_print_scr("      ");
                }
                vga_print_scr_nw(name);
            }
        }
        current = fat32_get_next_cluster(current);
    }
}

int fat32_read_file(const char* filename, void* buffer, uint32_t size) {
    uint8_t sector[512];
    uint32_t cluster = fs.root_cluster;

    while (cluster < FAT32_EOC) {
        uint32_t lba = fs.data_start_lba + (cluster - 2) * fs.bs.sectors_per_cluster;

        for (uint8_t i = 0; i < fs.bs.sectors_per_cluster; i++) {
            read_sector(lba + i, sector);
            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)sector;

            for (int j = 0; j < 16; j++) {
                if (entry[j].filename[0] == 0x00)
                    return -1;

                if (strncmp(entry[j].filename, filename, 11) == 0) {
                    uint32_t file_cluster = ((uint32_t)entry[j].first_cluster_high << 16) | entry[j].first_cluster_low;
                    uint32_t offset = 0;

                    while (file_cluster < FAT32_EOC && offset < size) {
                        uint32_t file_lba = fs.data_start_lba + (file_cluster - 2) * fs.bs.sectors_per_cluster;
                        read_sector(file_lba, buffer + offset);
                        offset += fs.bs.sectors_per_cluster * fs.bs.bytes_per_sector;
                        file_cluster = fat32_get_next_cluster(file_cluster);
                    }
                    return offset;
                }
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    return -1;
}