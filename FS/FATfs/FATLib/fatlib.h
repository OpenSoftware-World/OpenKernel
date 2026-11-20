#ifndef FATLIB_H
#define FATLIB_H

#include <OpenKernel/SystemLib/Std/types.h>

/*
 * Cap on a cluster chain walk. A FAT whose entries point back at themselves
 * would otherwise spin a directory listing forever; 65536 clusters is 32 MiB
 * at one sector per cluster, far more than this driver can address anyway.
 */
#define FAT_MAX_CLUSTER_STEPS 65536u

/* On-disk geometry shared by every FAT variant. */
#define FATFS_SECTOR_SIZE 512u
#define FATFS_ENTRY_SIZE 32u
#define FATFS_ENTRIES_PER_SECTOR (FATFS_SECTOR_SIZE / FATFS_ENTRY_SIZE)

/* fatfs_dir_entry_t.attr bits. */
#define FATFS_ATTR_READONLY  0x01
#define FATFS_ATTR_HIDDEN    0x02
#define FATFS_ATTR_SYSTEM    0x04
#define FATFS_ATTR_VOLUME    0x08
#define FATFS_ATTR_DIRECTORY 0x10
#define FATFS_ATTR_ARCHIVE   0x20
#define FATFS_ATTR_LFN       0x0F

/* 1980-01-01, the FAT epoch, stamped on the date fields of files we create. */
#define FATFS_DATE_EPOCH 0x0021u

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

typedef enum {
    FATFS_TYPE_FAT12 = 12,
    FATFS_TYPE_FAT16 = 16,
    FATFS_TYPE_FAT32 = 32
} fatfs_type_t;

/*
 * Everything a FAT variant needs in order to reach its own on-disk
 * structures. Each driver fills one of these during *_init() and passes it
 * to the shared core, so FAT12, FAT16 and FAT32 differ only in how the
 * context is built, never in how files are read or written.
 */
typedef struct {
    fatfs_type_t type;
    uint32_t fat_start_lba;     /* first sector of FAT copy 0 */
    uint32_t fat_sectors;       /* size of one FAT copy */
    uint8_t fat_count;          /* number of FAT copies to keep in sync */
    uint32_t root_dir_lba;      /* FAT12/FAT16 fixed root directory */
    uint32_t root_dir_sectors;  /* FAT12/FAT16 only, 0 for FAT32 */
    uint32_t data_start_lba;    /* first sector of cluster 2 */
    uint32_t root_cluster;      /* FAT32 root directory cluster, 0 otherwise */
    uint16_t bytes_per_sector;  /* 512 once the volume is usable */
    uint8_t sectors_per_cluster;
    uint32_t total_clusters;    /* data clusters, 0 when the geometry is unknown */
    uint32_t alloc_hint;        /* cluster a free-space scan starts from */
} fatfs_ctx_t;

/* Sector I/O through the ATA driver. */
void fatfs_read_sector(uint32_t lba, uint8_t* buffer);
void fatfs_write_sector(uint32_t lba, const uint8_t* buffer);

/* Geometry. A context that is not ready rejects every operation. */
bool_t fatfs_ready(const fatfs_ctx_t* ctx);
void fatfs_finalize_geometry(fatfs_ctx_t* ctx, uint32_t boot_lba, uint32_t total_sectors);
uint32_t fatfs_cluster_size(const fatfs_ctx_t* ctx);
uint32_t fatfs_cluster_lba(const fatfs_ctx_t* ctx, uint32_t cluster);

/* FAT table access. All copies of the FAT are updated by fatfs_fat_set(). */
uint32_t fatfs_fat_get(const fatfs_ctx_t* ctx, uint32_t cluster);
void fatfs_fat_set(fatfs_ctx_t* ctx, uint32_t cluster, uint32_t value);

/* Cluster chain maintenance. */
uint32_t fatfs_alloc_cluster(fatfs_ctx_t* ctx);
void fatfs_free_chain(fatfs_ctx_t* ctx, uint32_t first);
uint32_t fatfs_chain_length(const fatfs_ctx_t* ctx, uint32_t first);
bool_t fatfs_chain_extend(fatfs_ctx_t* ctx, uint32_t* first, uint32_t count);
bool_t fatfs_chain_truncate(fatfs_ctx_t* ctx, uint32_t* first, uint32_t keep);

/* 8.3 name conversion. Both directions accept the on-disk form. */
bool_t fatfs_name_to_83(const char* name, char out[11]);
void fatfs_name_to_display(const char raw[11], char out[12]);

/* File operations, all of them rooted at the volume root directory. */
int fatfs_read_file(const fatfs_ctx_t* ctx, const char* name, void* buf, uint32_t size);
bool_t fatfs_create_file(fatfs_ctx_t* ctx, const char* name, uint32_t size);
bool_t fatfs_write_file(fatfs_ctx_t* ctx, const char* name, const uint8_t* data, uint32_t size);
bool_t fatfs_delete_file(fatfs_ctx_t* ctx, const char* name);
uint32_t fatfs_get_file_size(const fatfs_ctx_t* ctx, const char* name);
void fatfs_list(const fatfs_ctx_t* ctx);
void fatfs_list_dir(const fatfs_ctx_t* ctx, uint32_t cluster);

#endif
