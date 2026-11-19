#include "fat32.h"
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/FS/FATfs/FATLib/fatlib.h>

static fat32_fs_t fs;

static void read_sector(uint32_t lba, void* buffer) {
    ata_sys_disk_read(lba, buffer);
}

void fat32_init(uint32_t boot_lba) {
    uint8_t sector[512];

    read_sector(boot_lba, sector);
    memcpy(&fs.bs, sector, sizeof(fs.bs));

    if (fs.bs.bytes_per_sector != 512 || fs.bs.sectors_per_cluster == 0) {
        memset(&fs, 0, sizeof(fs));
        fs.bs.bytes_per_sector = 512;
        fs.bs.sectors_per_cluster = 1;
        fs.root_cluster = 0;
        return;
    }

    fs.fat_start_lba = boot_lba + fs.bs.reserved_sectors;
    fs.data_start_lba = fs.fat_start_lba + (fs.bs.fat_count * fs.bs.sectors_per_fat);
    fs.root_cluster = fs.bs.root_cluster;
}

uint32_t fat32_get_next_cluster(uint32_t cluster) {
    if (fs.bs.bytes_per_sector == 0 || fs.bs.bytes_per_sector > 512) {
        return FAT32_EOC;
    }

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs.fat_start_lba + (fat_offset / fs.bs.bytes_per_sector);
    uint32_t ent_offset = fat_offset % fs.bs.bytes_per_sector;

    uint8_t sector[512];
    read_sector(fat_sector, sector);

    uint32_t next = 0;
    if (ent_offset + 3 < fs.bs.bytes_per_sector) {
        next = *(uint32_t*)&sector[ent_offset];
    } else {
        uint8_t next_sector[512];
        uint32_t in_sector = fs.bs.bytes_per_sector - ent_offset;
        uint32_t carry = 0;

        read_sector(fat_sector + 1, next_sector);

        for (uint32_t b = 0; b < 4; b++) {
            if (b < in_sector) {
                carry |= (uint32_t)sector[ent_offset + b] << (8 * b);
            } else {
                carry |= (uint32_t)next_sector[b - in_sector] << (8 * b);
            }
        }
        next = carry;
    }
    return next & 0x0FFFFFFF;
}

void fat32_list_dir(uint32_t cluster) {
    uint8_t sector[512];
    uint32_t current = cluster;
    uint32_t steps = 0;

    if (fs.bs.sectors_per_cluster == 0) return;

    while (current >= 2 && current < FAT32_EOC) {

        uint32_t lba = fs.data_start_lba + (current - 2) * fs.bs.sectors_per_cluster;

        for (uint8_t i = 0; i < fs.bs.sectors_per_cluster; i++) {
            read_sector(lba + i, sector);
            fatfs_dir_entry_t* entry = (fatfs_dir_entry_t*)sector;

            for (int j = 0; j < 16; j++) {
                if (entry[j].file_name[0] == 0x00)
                    return;

                if ((uint8_t)entry[j].file_name[0] == 0xE5)
                    continue;

                char name[12];
                int k = 0;

                for (int x = 0; x < 11; x++) {
                    if (entry[j].file_name[x] == ' ')
                        continue;

                    name[k++] = entry[j].file_name[x];
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
        if (++steps > FAT_MAX_CLUSTER_STEPS) return;

        current = fat32_get_next_cluster(current);
    }
}

int fat32_read_file(const char* filename, void* buffer, uint32_t size) {
    uint8_t sector[512];
    uint32_t cluster = fs.root_cluster;
    uint32_t steps = 0;

    if (!filename || !buffer || size == 0) return -1;
    if (fs.bs.sectors_per_cluster == 0) return -1;

    while (cluster >= 2 && cluster < FAT32_EOC) {
        uint32_t lba = fs.data_start_lba + (cluster - 2) * fs.bs.sectors_per_cluster;

        for (uint8_t i = 0; i < fs.bs.sectors_per_cluster; i++) {
            read_sector(lba + i, sector);
            fatfs_dir_entry_t* entry = (fatfs_dir_entry_t*)sector;

            for (int j = 0; j < 16; j++) {
                if (entry[j].file_name[0] == 0x00)
                    return -1;

                if (strncmp(entry[j].file_name, filename, 11) == 0) {
                    uint32_t file_cluster = ((uint32_t)entry[j].first_cluster_high << 16) | entry[j].first_cluster_low;
                    uint32_t read_bytes = 0;

                    while (file_cluster >= 2 && file_cluster < FAT32_EOC && read_bytes < size) {
                        uint32_t file_lba = fs.data_start_lba + (file_cluster - 2) * fs.bs.sectors_per_cluster;

                        for (uint32_t s = 0;
                             s < fs.bs.sectors_per_cluster && read_bytes < size;
                             s++) {
                            read_sector(file_lba + s, sector);

                            uint32_t n = fs.bs.bytes_per_sector;
                            if (n > size - read_bytes) n = size - read_bytes;

                            memcpy((uint8_t*)buffer + read_bytes, sector, n);
                            read_bytes += n;
                        }

                        file_cluster = fat32_get_next_cluster(file_cluster);
                    }
                    return (int)read_bytes;
                }
            }
        }
        if (++steps > FAT_MAX_CLUSTER_STEPS) return -1;

        cluster = fat32_get_next_cluster(cluster);
    }
    return -1;
}

void fat32_list_root(void) {
    fat32_list_dir(fs.root_cluster);
}
