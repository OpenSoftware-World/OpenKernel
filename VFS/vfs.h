#ifndef VFS_H
#define VFS_H

typedef enum filesystem_list {
    OFS,
    FAT12,
    FAT16,
    FAT32
} fs_t;

void vfs_init(fs_t fsname);
void vfs_list();
void vfs_read_file(const char* name, uint8_t* buffer, void* buf, uint32_t size);

/* Coming soon...
void vfs_create_file(const char *name, uint32_t size);
void vfs_write_file(const char *name, const uint8_t* data, uint32_t size);
void vfs_delete_file(const char *fname);
*/

#endif