#include "vfs.c"
#include <OpenKernel/FAT12/fat12.h>
#include <OpenKernel/FAT16/fat16.h>
#include <OpenKernel/FAT32/fat32.h>
#include <OpenKernel/OFS/ofs.h>

void vfs_init(fs_t *fsname) {
    switch (fsname) {
        case OFS:
        default:
            ofs_init();
            break;
        case FAT12:
            fat12_init();
            break;
        case FAT16:
            fat16_init();
            break;
        case FAT32:
            fat32_init();
            break;
    }
}

void vfs_list(fs_t *fsname) {
    switch (fsname) {
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