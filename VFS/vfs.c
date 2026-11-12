#include "vfs.h"
#include <OpenKernel/FATfs/FAT12/fat12.h>
#include <OpenKernel/FATfs/FAT16/fat16.h>
#include <OpenKernel/FATfs/FAT32/fat32.h>
#include <OpenKernel/OFS/ofs.h>

fs_t current_fs;

static void set_fs(fs_t fs) {
    current_fs = fs;
}

void vfs_init(fs_t fsname) {
    switch (fsname) {
        case OFS:
        default:
            ofs_init();
            set_fs(OFS);
            break;
        case FAT12:
            fat12_init(0);
            set_fs(FAT12);
            break;
        case FAT16:
            fat16_init(0);
            set_fs(FAT16);
            break;
        case FAT32:
            fat32_init(0);
            set_fs(FAT32);
            break;
    }
}

void vfs_list() {
    switch (current_fs) {
        case OFS:
        default:
            ofs_list_files();
            break;
        case FAT12:
            fat12_list_root();
            break;
        case FAT16:
            fat16_list_root();
            break;
        case FAT32:
            fat32_list_root();
            break;
    }
}

void vfs_read_file(const char* name, uint8_t* buffer, void* buf, uint32_t size) {
    switch (current_fs) {
        case OFS:
        default:
            ofs_read_file(name, buffer);
            break;
        case FAT12:
            fat12_read_file(name, buf, size);
            break;
        case FAT16:
            fat16_read_file(name, buf, size);
            break;
        case FAT32:
            fat32_read_file(name, buf, size);
            break;
    }
}

void vfs_create_file(const char *name, uint32_t size) {
    switch (current_fs) {
        case OFS:
        default:
            ofs_create_file(name, size);
            break;
        case FAT12:
            // The ability to create files on a FAT12 drive is not currently available, but I plan to add it in the future.
            break;
        case FAT16:
            // The ability to create files on a FAT16 drive is not currently available, but I plan to add it in the future.
            break;
        case FAT32:
            // The ability to create files on a FAT32 drive is not currently available, but I plan to add it in the future.
            break;
    }
}

void vfs_write_file(const char *name, const uint8_t* data, uint32_t size) {
    switch (current_fs) {
        case OFS:
        default:
            ofs_write_file(name, data, size);
            break;
        case FAT12:
            // The ability to write files on a FAT12 drive is not currently available, but I plan to add it in the future.
            break;
        case FAT16:
            // The ability to write files on a FAT16 drive is not currently available, but I plan to add it in the future.
            break;
        case FAT32:
            // The ability to write files on a FAT32 drive is not currently available, but I plan to add it in the future.
            break;
    }
}

void vfs_delete_file(const char *fname) {
    switch (current_fs) {
        case OFS:
        default:
            ofs_delete_file(fname);
            break;
        case FAT12:
            // The ability to delete files on a FAT12 drive is not currently available, but I plan to add it in the future.
            break;
        case FAT16:
            // The ability to delete files on a FAT16 drive is not currently available, but I plan to add it in the future.
            break;
        case FAT32:
            // The ability to delete files on a FAT32 drive is not currently available, but I plan to add it in the future.
            break;
    }
}