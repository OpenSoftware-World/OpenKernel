#include "ofs.h"
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Vga/vga.h>

ofs_t ofs;

#define SECTOR_SIZE 512
#define FTABLE_SECTORS 8

void ofs_init() {
    ata_sys_disk_read(0, (uint8_t*)&ofs.superb);

    if (memcmp(ofs.superb.signature, "OFS1", 4) != 0) {
        memset(&ofs, 0, sizeof(ofs));

        ofs.superb.signature[0] = 'O';
        ofs.superb.signature[1] = 'F';
        ofs.superb.signature[2] = 'S';
        ofs.superb.signature[3] = '1';

        ofs.superb.version = 1;
        ofs.superb.total_sectors = 0;
        ofs.superb.file_table_start = 1;
        ofs.superb.data_start = 1 + FTABLE_SECTORS;

        ata_sys_disk_write(0, (uint8_t*)&ofs.superb);

        memset(ofs.file_table, 0, sizeof(ofs.file_table));

        for (int i = 0; i < FTABLE_SECTORS; i++) {
            ata_sys_disk_write(1 + i, (uint8_t*)(ofs.file_table + (i * SECTOR_SIZE)));
        }
    }
    for (int i = 0; i < FTABLE_SECTORS; i++) {
        ata_sys_disk_read(1 + i, (uint8_t*)(ofs.file_table + (i * SECTOR_SIZE)));
    }
}

void ofs_create_file(const char* name, uint32_t size) {

    for (int i = 0; i < OFS_MAX_FILES; i++) {

        if (!ofs.file_table[i].used) {

            memset(ofs.file_table[i].name, 0, OFS_NAME_MAX_LENGTH);
            strncpy(ofs.file_table[i].name, name, OFS_NAME_MAX_LENGTH - 1);

            ofs.file_table[i].size_bytes = size;
            ofs.file_table[i].used = 1;
            ofs.file_table[i].flags = 0;

            ofs.file_table[i].start_sector =
                ofs.superb.data_start + (i * 8);

            return;
        }
    }
}

void ofs_read_file(const char* name, uint8_t* buffer) {
    for (int i = 0; i < OFS_MAX_FILES; i++) {
        if (ofs.file_table[i].used &&
            strncmp(ofs.file_table[i].name, name, OFS_NAME_MAX_LENGTH) == 0) {

            uint32_t sectors_to_read =
                (ofs.file_table[i].size_bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;

            for (uint32_t s = 0; s < sectors_to_read; s++) {
                ata_sys_disk_read(ofs.file_table[i].start_sector + s,
                                  buffer + (s * SECTOR_SIZE));
            }
            return;
        }
    }
}

void ofs_write_file(const char* name, const uint8_t* data, uint32_t size) {

    for (int i = 0; i < OFS_MAX_FILES; i++) {

        if (ofs.file_table[i].used && strncmp(ofs.file_table[i].name, name, OFS_NAME_MAX_LENGTH) == 0) {

            ofs.file_table[i].size_bytes = size;

            uint32_t sectors =
                (size + SECTOR_SIZE - 1) / SECTOR_SIZE;

            for (uint32_t s = 0; s < sectors; s++) {
                ata_sys_disk_write(ofs.file_table[i].start_sector + s, data + (s * SECTOR_SIZE));
            }
            return;
        }
    }
}

void ofs_delete_file(const char* name) {
    for (int i = 0; i < OFS_MAX_FILES; i++) {
        if (ofs.file_table[i].used && strncmp(ofs.file_table[i].name, name, OFS_NAME_MAX_LENGTH) == 0) {
            ofs.file_table[i].used = 0;
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