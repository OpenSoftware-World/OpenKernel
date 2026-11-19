#include "ofs.h"
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Vga/vga.h>

ofs_t ofs;

#define SECTOR_SIZE 512
#define FTABLE_BYTES   (sizeof(ofs_file_t) * OFS_MAX_FILES)
#define FTABLE_SECTORS ((FTABLE_BYTES + SECTOR_SIZE - 1) / SECTOR_SIZE)
#define OFS_FILE_SECTORS 8
#define OFS_MAX_FILE_SIZE (OFS_FILE_SECTORS * SECTOR_SIZE)

static void ofs_load_table(void) {
    uint8_t *table = (uint8_t *)ofs.file_table;

    for (uint32_t i = 0; i < FTABLE_SECTORS; i++) {
        ata_sys_disk_read(ofs.superb.file_table_start + i, table + (i * SECTOR_SIZE));
    }
}

static void ofs_flush_table(void) {
    uint8_t *table = (uint8_t *)ofs.file_table;

    for (uint32_t i = 0; i < FTABLE_SECTORS; i++) {
        ata_sys_disk_write(ofs.superb.file_table_start + i, table + (i * SECTOR_SIZE));
    }
}

static bool_t ofs_superblock_valid(const ofs_superblock_t *sb) {
    if (memcmp(sb->signature, "OFS1", 4) != 0) return false;
    if (sb->file_table_start == 0) return false;
    if (sb->data_start < sb->file_table_start + FTABLE_SECTORS) return false;
    return true;
}

void ofs_init() {
    uint8_t sector[SECTOR_SIZE];
    ofs_superblock_t disk_sb;

    ata_sys_disk_read(0, sector);
    memcpy(&disk_sb, sector, sizeof(disk_sb));

    if (!ofs_superblock_valid(&disk_sb)) {
        memset(&ofs, 0, sizeof(ofs));

        ofs.superb.signature[0] = 'O';
        ofs.superb.signature[1] = 'F';
        ofs.superb.signature[2] = 'S';
        ofs.superb.signature[3] = '1';

        ofs.superb.version = 1;
        ofs.superb.total_sectors = 0;
        ofs.superb.file_table_start = 1;
        ofs.superb.data_start = 1 + FTABLE_SECTORS;

        memset(sector, 0, sizeof(sector));
        memcpy(sector, &ofs.superb, sizeof(ofs.superb));
        ata_sys_disk_write(0, sector);

        memset(ofs.file_table, 0, sizeof(ofs.file_table));
        ofs_flush_table();

        vga_print_scr_nw("OFS: disk formatted");
    } else {
        ofs.superb = disk_sb;
        ofs_load_table();
    }
}

void ofs_create_file(const char* name, uint32_t size) {
    if (!name) return;
    if (size > OFS_MAX_FILE_SIZE) size = OFS_MAX_FILE_SIZE;

    for (int i = 0; i < OFS_MAX_FILES; i++) {

        if (!ofs.file_table[i].used) {

            memset(ofs.file_table[i].name, 0, OFS_NAME_MAX_LENGTH);
            strncpy(ofs.file_table[i].name, name, OFS_NAME_MAX_LENGTH - 1);

            ofs.file_table[i].size_bytes = size;
            ofs.file_table[i].used = 1;
            ofs.file_table[i].flags = 0;

            ofs.file_table[i].start_sector =
                ofs.superb.data_start + (i * OFS_FILE_SECTORS);

            ofs_flush_table();
            return;
        }
    }
}

void ofs_read_file(const char* name, uint8_t* buffer) {
    if (!name || !buffer) return;

    for (int i = 0; i < OFS_MAX_FILES; i++) {
        if (ofs.file_table[i].used &&
            strncmp(ofs.file_table[i].name, name, OFS_NAME_MAX_LENGTH) == 0) {

            uint32_t sectors_to_read =
                (ofs.file_table[i].size_bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;

            if (sectors_to_read > OFS_FILE_SECTORS) sectors_to_read = OFS_FILE_SECTORS;

            for (uint32_t s = 0; s < sectors_to_read; s++) {
                ata_sys_disk_read(ofs.file_table[i].start_sector + s,
                                  buffer + (s * SECTOR_SIZE));
            }
            return;
        }
    }
}

void ofs_write_file(const char* name, const uint8_t* data, uint32_t size) {
    if (!name || !data) return;

    for (int i = 0; i < OFS_MAX_FILES; i++) {

        if (ofs.file_table[i].used && strncmp(ofs.file_table[i].name, name, OFS_NAME_MAX_LENGTH) == 0) {

            if (size > OFS_MAX_FILE_SIZE) size = OFS_MAX_FILE_SIZE;

            ofs.file_table[i].size_bytes = size;

            uint32_t sectors =
                (size + SECTOR_SIZE - 1) / SECTOR_SIZE;

            uint8_t sector[SECTOR_SIZE];

            for (uint32_t s = 0; s < sectors; s++) {
                uint32_t offset = s * SECTOR_SIZE;
                uint32_t avail = (size > offset) ? (size - offset) : 0;

                memset(sector, 0, sizeof(sector));
                if (avail > 0) memcpy(sector, data + offset, avail);

                ata_sys_disk_write(ofs.file_table[i].start_sector + s, sector);
            }

            ofs_flush_table();
            return;
        }
    }
}

void ofs_delete_file(const char* name) {
    if (!name) return;

    for (int i = 0; i < OFS_MAX_FILES; i++) {
        if (ofs.file_table[i].used && strncmp(ofs.file_table[i].name, name, OFS_NAME_MAX_LENGTH) == 0) {
            ofs.file_table[i].used = 0;
            ofs_flush_table();
            return;
        }
    }
}

void ofs_list_files() {
    for (int i = 0; i < OFS_MAX_FILES; i++) {
        if (ofs.file_table[i].used) {
            vga_print_scr("FILE: ");
            vga_print_scr(ofs.file_table[i].name);
            vga_print_scr("\n");
        }
    }
}
