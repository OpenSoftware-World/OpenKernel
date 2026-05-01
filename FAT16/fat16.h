#ifndef FAT16_H
#define FAT16_H

#include <OpenKernel/SystemLib/Std/types.h>

#define FAT16_EOC 0xFFF8
#define FAT16_BAD_CLUSTER 0xFFF7

typedef struct {
    char filename[11];
    uint8_t attr;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster;
    uint32_t file_size;
} __attribute__((packed)) fat16_dir_entry_t;

/* Boot sector */
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
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
} __attribute__((packed)) fat16_boot_sector_t;

typedef struct {
    fat16_boot_sector_t bs;
    uint32_t fat_start_lba;
    uint32_t root_dir_lba;
    uint32_t data_start_lba;
    uint32_t root_dir_sectors;
} fat16_fs_t;

void fat16_init(uint32_t boot_lba);
void fat16_list_root();
int fat16_read_file(const char* filename, void* buffer, uint32_t size);
uint16_t fat16_get_next_cluster(uint16_t cluster);

#endif