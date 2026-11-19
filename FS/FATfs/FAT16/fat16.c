#include "fat16.h"
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/FS/FATfs/FATLib/fatlib.h>

static fat16_fs_t fs;

static void read_sector(uint32_t lba, void* buffer) {
    ata_sys_disk_read(lba, buffer);
}

void fat16_init(uint32_t boot_lba) {
    uint8_t sector[512];

    read_sector(boot_lba, sector);
    memcpy(&fs.bs, sector, sizeof(fs.bs));

    if (fs.bs.bytes_per_sector != 512 || fs.bs.sectors_per_cluster == 0) {
        memset(&fs, 0, sizeof(fs));
        fs.bs.bytes_per_sector = 512;
        fs.bs.sectors_per_cluster = 1;
        fs.root_dir_sectors = 0;   
        return;
    }

    fs.fat_start_lba = boot_lba + fs.bs.reserved_sectors;
    fs.root_dir_lba = fs.fat_start_lba + (fs.bs.fat_count * fs.bs.sectors_per_fat);
    fs.root_dir_sectors = ((fs.bs.root_entry_count * 32) + (fs.bs.bytes_per_sector - 1)) / fs.bs.bytes_per_sector;
    fs.data_start_lba = fs.root_dir_lba + fs.root_dir_sectors;
}

uint16_t fat16_get_next_cluster(uint16_t cluster) {
    if (fs.bs.bytes_per_sector == 0 || fs.bs.bytes_per_sector > 512) {
        return FAT16_EOC;
    }

    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fs.fat_start_lba + (fat_offset / fs.bs.bytes_per_sector);
    uint32_t ent_offset = fat_offset % fs.bs.bytes_per_sector;
    uint8_t sector[512];
    read_sector(fat_sector, sector);

    if (ent_offset + 1 < fs.bs.bytes_per_sector) {
        return *(uint16_t*)&sector[ent_offset];
    }

    uint8_t next_sector[512];
    read_sector(fat_sector + 1, next_sector);
    return (uint16_t)sector[ent_offset] | ((uint16_t)next_sector[0] << 8);
}

void fat16_list_root() {
    uint8_t sector[512];

    for (uint32_t i = 0; i < fs.root_dir_sectors; i++) {
        read_sector(fs.root_dir_lba + i, sector);
        fatfs_dir_entry_t* entry = (fatfs_dir_entry_t*)sector;

        for (int j = 0; j < 16; j++) {
            if (entry[j].file_name[0] == 0x00)
                return;

            if ((uint8_t)entry[j].file_name[0] == 0xE5)
                continue;
            char name[12];
            int k = 0;
            for (int i = 0; i < 11; i++) {
                if (entry[j].file_name[i] == ' ')
                    continue;
                name[k++] = entry[j].file_name[i];
            }
            name[k] = '\0';
            vga_print_scr_nw(name);
        }
    }
}

int fat16_read_file(const char* filename, void* buffer, uint32_t size) {
    uint8_t sector[512];

    if (!filename || !buffer || size == 0) return -1;

    for (uint32_t i = 0; i < fs.root_dir_sectors; i++) {
        read_sector(fs.root_dir_lba + i, sector);
        fatfs_dir_entry_t* entry = (fatfs_dir_entry_t*)sector;

        for (int j = 0; j < 16; j++) {
            if (entry[j].file_name[0] == 0x00)
                return -1;

            if (strncmp(entry[j].file_name, filename, 11) == 0) {
                uint16_t cluster = entry[j].first_cluster_low;
                uint32_t read_bytes = 0;

                while (cluster >= 2 && cluster < FAT16_EOC && read_bytes < size) {
                    uint32_t lba = fs.data_start_lba + (cluster - 2) * fs.bs.sectors_per_cluster;

                    for (uint32_t s = 0;
                         s < fs.bs.sectors_per_cluster && read_bytes < size;
                         s++) {
                        read_sector(lba + s, sector);
                        uint32_t n = fs.bs.bytes_per_sector;
                        if (n > size - read_bytes) n = size - read_bytes;

                        memcpy((uint8_t*)buffer + read_bytes, sector, n);
                        read_bytes += n;
                    }

                    cluster = fat16_get_next_cluster(cluster);
                }
                return (int)read_bytes;
            }
        }
    }
    return -1;
}
