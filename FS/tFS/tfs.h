#ifndef TFS_H
#define TFS_H

#include <OpenKernel/SystemLib/Std/types.h>

/*
 * tFS (tempFS) - a RAM-backed file system.
 *
 * Every file lives in a static pool of memory, so nothing is ever written to
 * disk and nothing survives a reboot. It is meant for scratch data: logs,
 * buffers, files a downstream OS wants to hand between subsystems.
 */

#define TFS_NAME_MAX_LENGTH 32
#define TFS_MAX_FILES 64

/* Storage geometry. Change these to resize the file system. */
#define TFS_BLOCK_SIZE 512
#define TFS_POOL_SIZE (64 * 1024)
#define TFS_BLOCK_COUNT (TFS_POOL_SIZE / TFS_BLOCK_SIZE)

#define TFS_NO_BLOCK 0xFFFF

typedef struct {
    char name[TFS_NAME_MAX_LENGTH];
    uint32_t size_bytes;
    uint16_t first_block;
    uint8_t used;
    uint8_t flags;
    uint8_t reserved[2];
} tfs_file_t;

void tfs_init(void);
bool_t tfs_create_file(const char* name, uint32_t size);
bool_t tfs_read_file(const char* name, uint8_t* buffer, uint32_t size);
bool_t tfs_write_file(const char* name, const uint8_t* data, uint32_t size);
bool_t tfs_delete_file(const char* name);
void tfs_list_files(void);
uint32_t tfs_get_file_size(const char* name);

#endif
