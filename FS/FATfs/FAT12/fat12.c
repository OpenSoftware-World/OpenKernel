#include "fat12.h"
#include <OpenKernel/SystemLib/Memory/mem.h>

static fat12_fs_t fs;

/*
 * Parse the boot sector and build the context the shared FAT core needs.
 * An unusable boot sector leaves the context unready, so every later call
 * fails cleanly instead of dividing by zero or reading the wrong sectors.
 */
void fat12_init(uint32_t boot_lba) {
    uint8_t sector[FATFS_SECTOR_SIZE];
    uint32_t total_sectors;

    memset(&fs, 0, sizeof(fs));
    fatfs_read_sector(boot_lba, sector);
    memcpy(&fs.bs, sector, sizeof(fs.bs));

    if (fs.bs.bytes_per_sector != FATFS_SECTOR_SIZE || fs.bs.sectors_per_cluster == 0 ||
        fs.bs.sectors_per_fat == 0 || fs.bs.fat_count == 0) {
        return;
    }

    fs.ctx.type = FATFS_TYPE_FAT12;
    fs.ctx.bytes_per_sector = fs.bs.bytes_per_sector;
    fs.ctx.sectors_per_cluster = fs.bs.sectors_per_cluster;
    fs.ctx.fat_start_lba = boot_lba + fs.bs.reserved_sectors;
    fs.ctx.fat_sectors = fs.bs.sectors_per_fat;
    fs.ctx.fat_count = fs.bs.fat_count;
    fs.ctx.root_dir_lba = fs.ctx.fat_start_lba +
                          (uint32_t)fs.bs.fat_count * fs.bs.sectors_per_fat;
    fs.ctx.root_dir_sectors = ((uint32_t)fs.bs.root_entry_count * FATFS_ENTRY_SIZE +
                               FATFS_SECTOR_SIZE - 1) / FATFS_SECTOR_SIZE;
    fs.ctx.data_start_lba = fs.ctx.root_dir_lba + fs.ctx.root_dir_sectors;
    fs.ctx.root_cluster = 0;

    total_sectors = fs.bs.total_sectors_16 ? fs.bs.total_sectors_16 : fs.bs.total_sectors_32;
    fatfs_finalize_geometry(&fs.ctx, boot_lba, total_sectors);
}

uint16_t fat12_get_next_cluster(uint16_t cluster) {
    if (!fatfs_ready(&fs.ctx)) return FAT12_EOC;
    return (uint16_t)fatfs_fat_get(&fs.ctx, cluster);
}

void fat12_list_root() {
    fatfs_list(&fs.ctx);
}

int fat12_read_file(const char* file_name, void* buffer, uint32_t size) {
    return fatfs_read_file(&fs.ctx, file_name, buffer, size);
}

bool_t fat12_create_file(const char* file_name, uint32_t size) {
    return fatfs_create_file(&fs.ctx, file_name, size);
}

bool_t fat12_write_file(const char* file_name, const uint8_t* data, uint32_t size) {
    return fatfs_write_file(&fs.ctx, file_name, data, size);
}

bool_t fat12_delete_file(const char* file_name) {
    return fatfs_delete_file(&fs.ctx, file_name);
}

uint32_t fat12_get_file_size(const char* file_name) {
    return fatfs_get_file_size(&fs.ctx, file_name);
}
