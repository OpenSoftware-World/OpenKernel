#ifndef OFS_H
#define OFS_H

#include <OpenKernel/SystemLib/Std/types.h>

#define OFS_NAME_MAX_LENGTH 32
#define OFS_MAX_FILES 128

typedef struct {
    char signature[4];
    uint32_t total_sectors;
    uint32_t file_table_start;
    uint32_t data_start;
    uint32_t version;
} ofs_superblock_t;

typedef struct {
    char name[OFS_NAME_MAX_LENGTH];
    uint32_t start_sector;
    uint32_t size_bytes;
    uint8_t used;
    uint8_t flags;
    uint8_t reserved;
} ofs_file_t;

typedef struct {
    ofs_superblock_t superb;
    ofs_file_t file_table[OFS_MAX_FILES];
} ofs_t;

extern ofs_t ofs;

void ofs_init();
void ofs_create_file(const char* name, uint32_t size);
void ofs_read_file(const char* name, uint8_t* buffer);
void ofs_write_file(const char* name, const uint8_t* data, uint32_t size);
void ofs_delete_file(const char* name);
void ofs_list_files();

#endif