#ifndef FATLIB_H
#define FATLIB_H

#include <OpenKernel/SystemLib/Std/types.h>

typedef struct {
    char file_name[11];
    uint8_t attr;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fatfs_dir_entry_t;

#endif