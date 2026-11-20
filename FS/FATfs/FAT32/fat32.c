#include "fat32.h"
#include <OpenKernel/SystemLib/Memory/mem.h>

static fat32_fs_t fs;

/*
 * Parse the boot sector and build the context the shared FAT core needs.
 * FAT32 has no fixed root directory, so root_dir_sectors stays 0 and the
 * root lives in bs.root_cluster. An unusable boot sector leaves the
 * context unready, so every later call fails cleanly.
 */
void fat32_init(uint32_t boot_lba) {
    uint8_t sector[FATFS_SECTOR_SIZE];
    uint32_t total_sectors;

    memset(&fs, 0, sizeof(fs));
    fatfs_read_sector(boot_lba, sector);
    memcpy(&fs.bs, sector, sizeof(fs.bs));

    if (fs.bs.bytes_per_sector != FATFS_SECTOR_SIZE || fs.bs.sectors_per_cluster == 0 ||
        fs.bs.sectors_per_fat == 0 || fs.bs.fat_count == 0) {
        return;
    }

    fs.ctx.type = FATFS_TYPE_FAT32;
    fs.ctx.bytes_per_sector = fs.bs.bytes_per_sector;
    fs.ctx.sectors_per_cluster = fs.bs.sectors_per_cluster;
    fs.ctx.fat_start_lba = boot_lba + fs.bs.reserved_sectors;
    fs.ctx.fat_sectors = fs.bs.sectors_per_fat;
    fs.ctx.fat_count = fs.bs.fat_count;
    fs.ctx.root_dir_lba = 0;
    fs.ctx.root_dir_sectors = 0;
    fs.ctx.data_start_lba = fs.ctx.fat_start_lba +
                            (uint32_t)fs.bs.fat_count * fs.bs.sectors_per_fat;
    fs.ctx.root_cluster = fs.bs.root_cluster;

    total_sectors = fs.bs.total_sectors_16 ? fs.bs.total_sectors_16 : fs.bs.total_sectors_32;
    fatfs_finalize_geometry(&fs.ctx, boot_lba, total_sectors);
}

uint32_t fat32_get_next_cluster(uint32_t cluster) {
    if (!fatfs_ready(&fs.ctx)) return FAT32_EOC;
    return fatfs_fat_get(&fs.ctx, cluster);
}

void fat32_list_dir(uint32_t cluster) {
    fatfs_list_dir(&fs.ctx, cluster);
}

void fat32_list_root(void) {
    fatfs_list(&fs.ctx);
}

int fat32_read_file(const char* filename, void* buffer, uint32_t size) {
    return fatfs_read_file(&fs.ctx, filename, buffer, size);
}

bool_t fat32_create_file(const char* filename, uint32_t size) {
    return fatfs_create_file(&fs.ctx, filename, size);
}

bool_t fat32_write_file(const char* filename, const uint8_t* data, uint32_t size) {
    return fatfs_write_file(&fs.ctx, filename, data, size);
}

bool_t fat32_delete_file(const char* filename) {
    return fatfs_delete_file(&fs.ctx, filename);
}

uint32_t fat32_get_file_size(const char* filename) {
    return fatfs_get_file_size(&fs.ctx, filename);
}
