#include "vfs.h"
#include <OpenKernel/FAT12/fat12.h>
#include <OpenKernel/FAT16/fat16.h>
#include <OpenKernel/FAT32/fat32.h>
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
            fat12_init();
            set_fs(FAT12);
            break;
        case FAT16:
            fat16_init();
            set_fs(FAT16);
            break;
        case FAT32:
            fat32_init();
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
            fat32_list_dir();
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