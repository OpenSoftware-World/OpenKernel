#ifndef FAT32_H
#define FAT32_H

#include <OpenKernel/SystemLib/Std/types.h>

#define FAT32_EOC 0x0FFFFFF8
#define FAT32_BAD_CLUSTER 0x0FFFFFF7

typedef struct {
    uint8_t jmp[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat;
    uint16_t flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved2;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
} __attribute__((packed)) fat32_boot_sector_t;

typedef struct {
    fat32_boot_sector_t bs;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t root_cluster;
} fat32_fs_t;

void fat32_init(uint32_t boot_lba);
uint32_t fat32_get_next_cluster(uint32_t cluster);
int fat32_read_file(const char* filename, void* buffer, uint32_t size);
void fat32_list_dir(uint32_t cluster);
void fat32_list_root(void);

#endif