#include "fatlib.h"
#include <OpenKernel/Drivers/Ata/ata.h>
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/SystemLib/Std/std.h>

/*
 * FATLib - the shared core behind the FAT12, FAT16 and FAT32 drivers.
 *
 * The three drivers used to carry three near identical copies of the same
 * code. Everything that does not depend on the entry width now lives here:
 * the caller builds a fatfs_ctx_t describing its volume and hands it over.
 * Only the BPB parsing stays in fat12.c / fat16.c / fat32.c.
 *
 * Scope: root directory only, 8.3 names only, no formatter.
 */

/* ------------------------------------------------------------------------ */
/* Sector I/O                                                               */
/* ------------------------------------------------------------------------ */

void fatfs_read_sector(uint32_t lba, uint8_t* buffer) {
    ata_sys_disk_read(lba, buffer);
}

void fatfs_write_sector(uint32_t lba, const uint8_t* buffer) {
    ata_sys_disk_write(lba, buffer);
}

/* ------------------------------------------------------------------------ */
/* Geometry                                                                 */
/* ------------------------------------------------------------------------ */

/* A context is usable only when the boot sector parsed into sane values. */
bool_t fatfs_ready(const fatfs_ctx_t* ctx) {
    return ctx != NULL && ctx->bytes_per_sector == FATFS_SECTOR_SIZE &&
           ctx->sectors_per_cluster != 0 && ctx->fat_count != 0 &&
           ctx->fat_sectors != 0;
}

uint32_t fatfs_cluster_size(const fatfs_ctx_t* ctx) {
    if (!fatfs_ready(ctx)) return 0;
    return (uint32_t)ctx->bytes_per_sector * ctx->sectors_per_cluster;
}

uint32_t fatfs_cluster_lba(const fatfs_ctx_t* ctx, uint32_t cluster) {
    if (!fatfs_ready(ctx) || cluster < 2) return 0;
    return ctx->data_start_lba + (cluster - 2) * ctx->sectors_per_cluster;
}

/* Number of data clusters the FAT itself can address, from its byte size. */
static uint32_t fat_capacity(const fatfs_ctx_t* ctx) {
    uint32_t fat_bytes = (uint32_t)ctx->fat_sectors * FATFS_SECTOR_SIZE;
    uint32_t entries;

    switch (ctx->type) {
        case FATFS_TYPE_FAT12: entries = (fat_bytes / 3u) * 2u; break;
        case FATFS_TYPE_FAT32: entries = fat_bytes / 4u; break;
        case FATFS_TYPE_FAT16:
        default:               entries = fat_bytes / 2u; break;
    }
    /* Entries 0 and 1 are reserved, data clusters start at 2. */
    return entries > 2 ? entries - 2 : 0;
}

/* Data cluster count that keeps clear of the reserved/bad/EOC markers. */
static uint32_t fat_type_limit(fatfs_type_t type) {
    switch (type) {
        case FATFS_TYPE_FAT12: return 0xFEFu - 1u;      /* 4078 clusters */
        case FATFS_TYPE_FAT32: return 0x0FFFFFF5u - 1u;
        case FATFS_TYPE_FAT16:
        default:               return 0xFFF5u - 1u;     /* 65524 clusters */
    }
}

/*
 * Derive total_clusters from the BPB totals, then clamp it to what the FAT
 * and the variant's own marker range can hold. A wrong BPB can therefore
 * only shrink the allocatable space, never walk off the end of the FAT.
 * total_sectors of 0 leaves total_clusters at 0, which disables allocation
 * but keeps reads working.
 */
void fatfs_finalize_geometry(fatfs_ctx_t* ctx, uint32_t boot_lba, uint32_t total_sectors) {
    uint32_t used, avail, cap;

    if (!fatfs_ready(ctx)) return;

    used = (ctx->data_start_lba > boot_lba) ? (ctx->data_start_lba - boot_lba) : 0;
    avail = (total_sectors > used) ? (total_sectors - used) : 0;
    cap = fat_capacity(ctx);

    ctx->total_clusters = avail / ctx->sectors_per_cluster;
    if (ctx->total_clusters > cap) ctx->total_clusters = cap;
    if (ctx->total_clusters > fat_type_limit(ctx->type)) ctx->total_clusters = fat_type_limit(ctx->type);

    ctx->alloc_hint = 2;
}

/* ------------------------------------------------------------------------ */
/* FAT table                                                                */
/* ------------------------------------------------------------------------ */

static uint32_t fat_entry_offset(const fatfs_ctx_t* ctx, uint32_t cluster) {
    switch (ctx->type) {
        case FATFS_TYPE_FAT12: return cluster + (cluster / 2u);
        case FATFS_TYPE_FAT32: return cluster * 4u;
        case FATFS_TYPE_FAT16:
        default:               return cluster * 2u;
    }
}

/* Bytes one entry occupies on disk; FAT12 packs two entries in three bytes. */
static uint32_t fat_entry_width(const fatfs_ctx_t* ctx) {
    return (ctx->type == FATFS_TYPE_FAT32) ? 4u : 2u;
}

/* First end-of-chain marker of the variant. */
static uint32_t fat_eoc(const fatfs_ctx_t* ctx) {
    switch (ctx->type) {
        case FATFS_TYPE_FAT12: return 0x0FF8u;
        case FATFS_TYPE_FAT32: return 0x0FFFFFF8u;
        case FATFS_TYPE_FAT16:
        default:               return 0xFFF8u;
    }
}

/*
 * First value that is no longer a usable data cluster: the reserved range,
 * the bad-cluster marker and the EOC markers all sit at or above it.
 */
static uint32_t fat_first_non_data(const fatfs_ctx_t* ctx) {
    switch (ctx->type) {
        case FATFS_TYPE_FAT12: return 0x0FF7u;
        case FATFS_TYPE_FAT32: return 0x0FFFFFF7u;
        case FATFS_TYPE_FAT16:
        default:               return 0xFFF7u;
    }
}

/* Read n bytes out of the FAT, tolerating a sector boundary. */
static void fat_region_read(const fatfs_ctx_t* ctx, uint32_t off, uint8_t* out, uint32_t n) {
    uint8_t sector[FATFS_SECTOR_SIZE];
    uint32_t sec = off / FATFS_SECTOR_SIZE;
    uint32_t idx = off % FATFS_SECTOR_SIZE;
    uint32_t first = FATFS_SECTOR_SIZE - idx;

    if (first > n) first = n;

    if (sec >= ctx->fat_sectors) {
        memset(out, 0, n);
        return;
    }

    fatfs_read_sector(ctx->fat_start_lba + sec, sector);
    memcpy(out, sector + idx, first);

    if (first < n) {
        if (sec + 1 >= ctx->fat_sectors) {
            memset(out + first, 0, n - first);
            return;
        }
        fatfs_read_sector(ctx->fat_start_lba + sec + 1, sector);
        memcpy(out + first, sector, n - first);
    }
}

/* Write n bytes into every copy of the FAT so the mirrors stay identical. */
static void fat_region_write(const fatfs_ctx_t* ctx, uint32_t off, const uint8_t* data, uint32_t n) {
    uint8_t sector[FATFS_SECTOR_SIZE];
    uint32_t sec = off / FATFS_SECTOR_SIZE;
    uint32_t idx = off % FATFS_SECTOR_SIZE;
    uint32_t first = FATFS_SECTOR_SIZE - idx;
    uint32_t copy;

    if (first > n) first = n;
    if (sec >= ctx->fat_sectors) return;
    if (first < n && sec + 1 >= ctx->fat_sectors) return;

    for (copy = 0; copy < ctx->fat_count; copy++) {
        uint32_t base = ctx->fat_start_lba + copy * ctx->fat_sectors;

        fatfs_read_sector(base + sec, sector);
        memcpy(sector + idx, data, first);
        fatfs_write_sector(base + sec, sector);

        if (first < n) {
            fatfs_read_sector(base + sec + 1, sector);
            memcpy(sector, data + first, n - first);
            fatfs_write_sector(base + sec + 1, sector);
        }
    }
}

/* Next cluster recorded for a cluster, or the EOC marker when unreadable. */
uint32_t fatfs_fat_get(const fatfs_ctx_t* ctx, uint32_t cluster) {
    uint8_t raw[4];
    uint32_t value;

    if (!fatfs_ready(ctx) || cluster < 2) return fat_eoc(ctx);

    fat_region_read(ctx, fat_entry_offset(ctx, cluster), raw, fat_entry_width(ctx));

    switch (ctx->type) {
        case FATFS_TYPE_FAT12:
            value = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8);
            value = (cluster & 1u) ? (value >> 4) : (value & 0x0FFFu);
            break;
        case FATFS_TYPE_FAT32:
            value = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) |
                    ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24);
            value &= 0x0FFFFFFFu;
            break;
        case FATFS_TYPE_FAT16:
        default:
            value = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8);
            break;
    }
    return value;
}

/* Store a cluster link in every copy of the FAT. */
void fatfs_fat_set(fatfs_ctx_t* ctx, uint32_t cluster, uint32_t value) {
    uint8_t raw[4];

    if (!fatfs_ready(ctx) || cluster < 2) return;

    fat_region_read(ctx, fat_entry_offset(ctx, cluster), raw, fat_entry_width(ctx));

    switch (ctx->type) {
        case FATFS_TYPE_FAT12:
            value &= 0x0FFFu;
            if (cluster & 1u) {
                /* Odd entries share a byte with their predecessor. */
                raw[0] = (uint8_t)((raw[0] & 0x0Fu) | ((value << 4) & 0xF0u));
                raw[1] = (uint8_t)((value >> 4) & 0xFFu);
            } else {
                raw[0] = (uint8_t)(value & 0xFFu);
                raw[1] = (uint8_t)((raw[1] & 0xF0u) | ((value >> 8) & 0x0Fu));
            }
            break;
        case FATFS_TYPE_FAT32:
            value &= 0x0FFFFFFFu;
            raw[0] = (uint8_t)(value & 0xFFu);
            raw[1] = (uint8_t)((value >> 8) & 0xFFu);
            raw[2] = (uint8_t)((value >> 16) & 0xFFu);
            raw[3] = (uint8_t)((value >> 24) & 0xFFu);
            break;
        case FATFS_TYPE_FAT16:
        default:
            value &= 0xFFFFu;
            raw[0] = (uint8_t)(value & 0xFFu);
            raw[1] = (uint8_t)((value >> 8) & 0xFFu);
            break;
    }

    fat_region_write(ctx, fat_entry_offset(ctx, cluster), raw, fat_entry_width(ctx));
}

/* Is this cluster a member of the data area rather than a marker? */
static bool_t fat_is_data_cluster(const fatfs_ctx_t* ctx, uint32_t cluster) {
    if (!fatfs_ready(ctx) || cluster < 2) return false;
    if (cluster >= fat_first_non_data(ctx)) return false;
    if (ctx->total_clusters != 0 && cluster > ctx->total_clusters + 1) return false;
    return true;
}

/* Follow a chain one step; 0 means the chain ends here. */
static uint32_t fat_chain_next(const fatfs_ctx_t* ctx, uint32_t cluster) {
    uint32_t next;

    if (!fat_is_data_cluster(ctx, cluster)) return 0;
    next = fatfs_fat_get(ctx, cluster);
    return fat_is_data_cluster(ctx, next) ? next : 0;
}

/* ------------------------------------------------------------------------ */
/* Cluster chains                                                           */
/* ------------------------------------------------------------------------ */

/*
 * Claim one free cluster and mark it end-of-chain so a later allocation
 * cannot hand out the same cluster. Returns 0 when the volume is full or
 * the geometry is unknown. The scan resumes from alloc_hint, which keeps
 * allocating a large file linear instead of quadratic.
 */
uint32_t fatfs_alloc_cluster(fatfs_ctx_t* ctx) {
    uint32_t last, cluster, scanned;

    if (!fatfs_ready(ctx) || ctx->total_clusters == 0) return 0;

    last = ctx->total_clusters + 1;
    if (ctx->alloc_hint < 2 || ctx->alloc_hint > last) ctx->alloc_hint = 2;

    cluster = ctx->alloc_hint;
    for (scanned = 0; scanned < ctx->total_clusters; scanned++) {
        if (cluster > last) cluster = 2;

        if (fatfs_fat_get(ctx, cluster) == 0) {
            fatfs_fat_set(ctx, cluster, fat_eoc(ctx));
            ctx->alloc_hint = (cluster >= last) ? 2 : cluster + 1;
            return cluster;
        }
        cluster++;
    }
    return 0;
}

/* Return a whole chain to the free space. */
void fatfs_free_chain(fatfs_ctx_t* ctx, uint32_t first) {
    uint32_t steps = 0;

    while (fat_is_data_cluster(ctx, first)) {
        uint32_t next = fatfs_fat_get(ctx, first);

        fatfs_fat_set(ctx, first, 0);
        if (!fat_is_data_cluster(ctx, next)) break;

        first = next;
        if (++steps > FAT_MAX_CLUSTER_STEPS) break;
    }
}

/* How many clusters a chain currently holds. */
uint32_t fatfs_chain_length(const fatfs_ctx_t* ctx, uint32_t first) {
    uint32_t count = 0;
    uint32_t steps = 0;

    while (fat_is_data_cluster(ctx, first)) {
        count++;
        if (++steps > FAT_MAX_CLUSTER_STEPS) break;
        first = fat_chain_next(ctx, first);
    }
    return count;
}

/*
 * Append count clusters to a chain. The new clusters are linked together
 * first and spliced on only when every one of them was allocated, so a
 * full volume leaves the chain exactly as it was.
 */
bool_t fatfs_chain_extend(fatfs_ctx_t* ctx, uint32_t* first, uint32_t count) {
    uint32_t tail = 0;
    uint32_t head = 0;
    uint32_t prev = 0;
    uint32_t steps = 0;
    uint32_t i;

    if (!ctx || !first) return false;
    if (count == 0) return true;

    if (fat_is_data_cluster(ctx, *first)) {
        tail = *first;
        for (;;) {
            uint32_t next = fat_chain_next(ctx, tail);
            if (next == 0) break;
            tail = next;
            if (++steps > FAT_MAX_CLUSTER_STEPS) return false;
        }
    }

    for (i = 0; i < count; i++) {
        uint32_t fresh = fatfs_alloc_cluster(ctx);
        if (!fresh) {
            fatfs_free_chain(ctx, head);
            return false;
        }
        if (head == 0) head = fresh;
        if (prev != 0) fatfs_fat_set(ctx, prev, fresh);
        prev = fresh;
    }

    if (tail != 0) fatfs_fat_set(ctx, tail, head);
    else *first = head;
    return true;
}

/* Drop every cluster after the first keep ones, returning them to the free space. */
bool_t fatfs_chain_truncate(fatfs_ctx_t* ctx, uint32_t* first, uint32_t keep) {
    uint32_t cur;
    uint32_t steps = 0;
    uint32_t i;

    if (!ctx || !first) return false;

    if (keep == 0) {
        fatfs_free_chain(ctx, *first);
        *first = 0;
        return true;
    }
    if (!fat_is_data_cluster(ctx, *first)) return true;

    cur = *first;
    for (i = 1; i < keep; i++) {
        uint32_t next = fat_chain_next(ctx, cur);
        if (next == 0) return true;   /* chain is already shorter than keep */
        cur = next;
        if (++steps > FAT_MAX_CLUSTER_STEPS) return false;
    }

    {
        uint32_t rest = fat_chain_next(ctx, cur);
        fatfs_fat_set(ctx, cur, fat_eoc(ctx));
        fatfs_free_chain(ctx, rest);
    }
    return true;
}

/* Zero every sector of a chain so a fresh file cannot expose stale data. */
static bool_t fat_zero_chain(fatfs_ctx_t* ctx, uint32_t first) {
    uint8_t sector[FATFS_SECTOR_SIZE];
    uint32_t cluster = first;
    uint32_t steps = 0;

    while (fat_is_data_cluster(ctx, cluster)) {
        uint32_t lba = fatfs_cluster_lba(ctx, cluster);
        uint32_t s;

        for (s = 0; s < ctx->sectors_per_cluster; s++) {
            memset(sector, 0, FATFS_SECTOR_SIZE);
            fatfs_write_sector(lba + s, sector);
        }

        if (++steps > FAT_MAX_CLUSTER_STEPS) return false;
        cluster = fat_chain_next(ctx, cluster);
    }
    return true;
}

/* ------------------------------------------------------------------------ */
/* 8.3 names                                                                */
/* ------------------------------------------------------------------------ */

static char fat_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

/* Characters FAT forbids in an 8.3 name. Everything else passes through. */
static bool_t fat_char_ok(char c) {
    unsigned char u = (unsigned char)c;

    if (u < 0x20 || u == 0x7F) return false;

    switch (c) {
        case '"': case '*': case ',': case '/': case ':': case ';':
        case '<': case '=': case '>': case '?': case '[': case '\\':
        case ']': case '|':
            return false;
        default:
            return true;
    }
}

/*
 * Turn a caller supplied name into the raw 11 byte directory field.
 * "readme.txt" becomes "README  TXT"; a name that already is the padded
 * 11 byte form and carries no dot is taken verbatim. out is only valid
 * when true is returned. Names are ASCII only.
 */
bool_t fatfs_name_to_83(const char* name, char out[11]) {
    const char* base;
    const char* ext;
    uint32_t len, base_len, ext_len, dot, i;

    if (!name || !out) return false;

    while (*name == ' ') name++;

    len = (uint32_t)strlen(name);
    while (len > 0 && name[len - 1] == ' ') len--;
    if (len == 0) return false;

    dot = len;
    for (i = 0; i < len; i++) {
        if (name[i] == '.') dot = i;
    }

    /* No dot and exactly eleven bytes: this is already the on-disk form. */
    if (dot == len && len == 11) {
        memcpy(out, name, 11);
        return true;
    }

    base = name;
    base_len = (dot < len) ? dot : len;
    ext = (dot < len) ? name + dot + 1 : name + len;
    ext_len = (dot < len) ? (len - dot - 1) : 0;

    if (base_len == 0 || base_len > 8 || ext_len > 3) return false;

    for (i = 0; i < base_len; i++) {
        if (base[i] == '.' || !fat_char_ok(base[i])) return false;
        out[i] = fat_upper(base[i]);
    }
    for (; i < 8; i++) out[i] = ' ';

    for (i = 0; i < ext_len; i++) {
        if (ext[i] == '.' || !fat_char_ok(ext[i])) return false;
        out[8 + i] = fat_upper(ext[i]);
    }
    for (; i < 3; i++) out[8 + i] = ' ';

    return true;
}

/* Render the raw 11 byte field as "NAME.EXT" for listings. */
void fatfs_name_to_display(const char raw[11], char out[12]) {
    uint32_t i;
    uint32_t k = 0;

    for (i = 0; i < 8 && raw[i] != ' '; i++) out[k++] = raw[i];

    if (raw[8] != ' ') {
        out[k++] = '.';
        for (i = 8; i < 11 && raw[i] != ' '; i++) out[k++] = raw[i];
    }
    out[k] = '\0';
}

/* ------------------------------------------------------------------------ */
/* Root directory entries                                                   */
/* ------------------------------------------------------------------------ */

/* Address of one32 byte entry inside a sector. */
typedef struct {
    uint32_t lba;
    uint8_t index;
} fatfs_entry_loc_t;

/*
 * Walks the root directory one sector at a time: the fixed region for
 * FAT12/FAT16, the cluster chain of root_cluster for FAT32.
 */
typedef struct {
    uint32_t lba;      /* next sector to hand out (fixed root) */
    uint32_t left;     /* sectors left in the fixed root */
    uint32_t cluster;  /* current directory cluster (FAT32) */
    uint32_t sector;   /* sector index inside that cluster (FAT32) */
    uint32_t steps;
    bool_t active;
} fatfs_dir_iter_t;

static void fatfs_dir_iter_init(const fatfs_ctx_t* ctx, fatfs_dir_iter_t* it, uint32_t start_cluster) {
    memset(it, 0, sizeof(*it));

    if (!fatfs_ready(ctx)) return;

    if (ctx->type == FATFS_TYPE_FAT32) {
        it->cluster = start_cluster;
        it->active = fat_is_data_cluster(ctx, it->cluster);
    } else {
        /* FAT12/FAT16 expose a single fixed root directory. */
        it->lba = ctx->root_dir_lba;
        it->left = ctx->root_dir_sectors;
        it->active = ctx->root_dir_sectors != 0;
    }
}

static bool_t fatfs_dir_iter_next(const fatfs_ctx_t* ctx, fatfs_dir_iter_t* it, uint32_t* lba) {
    if (!it->active) return false;

    if (ctx->type == FATFS_TYPE_FAT32) {
        while (fat_is_data_cluster(ctx, it->cluster)) {
            if (it->sector < ctx->sectors_per_cluster) {
                *lba = fatfs_cluster_lba(ctx, it->cluster) + it->sector;
                it->sector++;
                return true;
            }
            if (++it->steps > FAT_MAX_CLUSTER_STEPS) break;
            it->cluster = fat_chain_next(ctx, it->cluster);
            it->sector = 0;
        }
        it->active = false;
        return false;
    }

    if (it->left == 0) {
        it->active = false;
        return false;
    }
    *lba = it->lba++;
    it->left--;
    return true;
}

static bool_t fatfs_entry_is_free(const fatfs_dir_entry_t* entry) {
    uint8_t first = (uint8_t)entry->file_name[0];
    return first == 0x00 || first == 0xE5;
}

/*
 * Locate a directory entry by its raw name. Stops at the 0x00 end marker,
 * so a miss costs one directory scan at most.
 */
static bool_t fatfs_dir_find(const fatfs_ctx_t* ctx, const char name83[11],
                             fatfs_entry_loc_t* loc, fatfs_dir_entry_t* out) {
    uint8_t sector[FATFS_SECTOR_SIZE];
    fatfs_dir_iter_t it;
    uint32_t lba;

    if (!fatfs_ready(ctx) || !name83) return false;

    fatfs_dir_iter_init(ctx, &it, ctx->root_cluster);
    while (fatfs_dir_iter_next(ctx, &it, &lba)) {
        fatfs_dir_entry_t* entry = (fatfs_dir_entry_t*)sector;
        uint32_t i;

        fatfs_read_sector(lba, sector);

        for (i = 0; i < FATFS_ENTRIES_PER_SECTOR; i++) {
            if ((uint8_t)entry[i].file_name[0] == 0x00) return false;
            if ((uint8_t)entry[i].file_name[0] == 0xE5) continue;
            if ((entry[i].attr & FATFS_ATTR_LFN) == FATFS_ATTR_LFN) continue;
            if (memcmp(entry[i].file_name, name83, 11) != 0) continue;

            if (loc) {
                loc->lba = lba;
                loc->index = (uint8_t)i;
            }
            if (out) *out = entry[i];
            return true;
        }
    }
    return false;
}

/* Grow the FAT32 root directory by one zeroed cluster. */
static bool_t fatfs_root_grow(fatfs_ctx_t* ctx, fatfs_entry_loc_t* loc) {
    uint8_t sector[FATFS_SECTOR_SIZE];
    uint32_t root = ctx->root_cluster;
    uint32_t tail = 0;
    uint32_t steps = 0;
    uint32_t fresh, lba, s;

    /* Walk to the end of the existing root chain, when there is one. */
    if (fat_is_data_cluster(ctx, root)) {
        tail = root;
        for (;;) {
            uint32_t next = fat_chain_next(ctx, tail);
            if (next == 0) break;
            tail = next;
            if (++steps > FAT_MAX_CLUSTER_STEPS) return false;
        }
    }

    if (!fatfs_chain_extend(ctx, &root, 1)) return false;
    ctx->root_cluster = root;

    /* With no previous chain the extension became the root itself. */
    fresh = tail ? fat_chain_next(ctx, tail) : root;
    if (!fat_is_data_cluster(ctx, fresh)) return false;

    lba = fatfs_cluster_lba(ctx, fresh);
    for (s = 0; s < ctx->sectors_per_cluster; s++) {
        memset(sector, 0, FATFS_SECTOR_SIZE);
        fatfs_write_sector(lba + s, sector);
    }

    loc->lba = lba;
    loc->index = 0;
    return true;
}

/* Find a free directory slot. The FAT32 root grows when it runs out. */
static bool_t fatfs_dir_alloc(fatfs_ctx_t* ctx, fatfs_entry_loc_t* loc) {
    uint8_t sector[FATFS_SECTOR_SIZE];
    fatfs_dir_iter_t it;
    uint32_t lba;

    if (!fatfs_ready(ctx) || !loc) return false;

    fatfs_dir_iter_init(ctx, &it, ctx->root_cluster);
    while (fatfs_dir_iter_next(ctx, &it, &lba)) {
        fatfs_dir_entry_t* entry = (fatfs_dir_entry_t*)sector;
        uint32_t i;

        fatfs_read_sector(lba, sector);

        for (i = 0; i < FATFS_ENTRIES_PER_SECTOR; i++) {
            if (!fatfs_entry_is_free(&entry[i])) continue;
            loc->lba = lba;
            loc->index = (uint8_t)i;
            return true;
        }
    }

    if (ctx->type == FATFS_TYPE_FAT32) return fatfs_root_grow(ctx, loc);
    return false;   /* FAT12/FAT16 root directory is fixed in size */
}

static void fatfs_dir_write_entry(const fatfs_entry_loc_t* loc, const fatfs_dir_entry_t* entry) {
    uint8_t sector[FATFS_SECTOR_SIZE];

    fatfs_read_sector(loc->lba, sector);
    memcpy(sector + (uint32_t)loc->index * FATFS_ENTRY_SIZE, entry, FATFS_ENTRY_SIZE);
    fatfs_write_sector(loc->lba, sector);
}

/* Starting cluster of an entry, masked the way the variant stores it. */
static uint32_t fatfs_entry_cluster(const fatfs_ctx_t* ctx, const fatfs_dir_entry_t* entry) {
    uint32_t cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;

    return (ctx->type == FATFS_TYPE_FAT32) ? (cluster & 0x0FFFFFFFu) : cluster;
}

static void fatfs_entry_set_cluster(fatfs_dir_entry_t* entry, uint32_t cluster) {
    entry->first_cluster_high = (uint16_t)((cluster >> 16) & 0xFFFFu);
    entry->first_cluster_low = (uint16_t)(cluster & 0xFFFFu);
}

/* Directories and volume labels are not files. */
static bool_t fatfs_entry_is_file(const fatfs_dir_entry_t* entry) {
    return (entry->attr & (FATFS_ATTR_DIRECTORY | FATFS_ATTR_VOLUME)) == 0;
}

/* Round a byte count up to whole clusters without overflowing. */
static uint32_t fatfs_clusters_for(const fatfs_ctx_t* ctx, uint32_t size) {
    uint32_t cluster_bytes = fatfs_cluster_size(ctx);

    if (cluster_bytes == 0) return 0;
    return size / cluster_bytes + ((size % cluster_bytes) ? 1u : 0u);
}

/* ------------------------------------------------------------------------ */
/* File operations                                                          */
/* ------------------------------------------------------------------------ */

/*
 * Copy at most size bytes of the named file into buf and return how many
 * bytes were copied, or -1 when the file is missing or unusable. The read
 * never runs past the size stored in the directory entry.
 */
int fatfs_read_file(const fatfs_ctx_t* ctx, const char* name, void* buf, uint32_t size) {
    char raw[11];
    fatfs_dir_entry_t entry;
    uint8_t sector[FATFS_SECTOR_SIZE];
    uint32_t cluster, done = 0, limit, steps = 0;

    if (!fatfs_ready(ctx) || !name || !buf || size == 0) return -1;
    if (!fatfs_name_to_83(name, raw)) return -1;
    if (!fatfs_dir_find(ctx, raw, NULL, &entry)) return -1;
    if (!fatfs_entry_is_file(&entry)) return -1;

    limit = entry.file_size;
    if (limit > size) limit = size;

    cluster = fatfs_entry_cluster(ctx, &entry);
    while (done < limit) {
        uint32_t lba;
        uint32_t s;

        if (!fat_is_data_cluster(ctx, cluster)) break;

        lba = fatfs_cluster_lba(ctx, cluster);
        for (s = 0; s < ctx->sectors_per_cluster && done < limit; s++) {
            uint32_t n = FATFS_SECTOR_SIZE;

            fatfs_read_sector(lba + s, sector);
            if (n > limit - done) n = limit - done;

            memcpy((uint8_t*)buf + done, sector, n);
            done += n;
        }

        if (++steps > FAT_MAX_CLUSTER_STEPS) break;
        cluster = fat_chain_next(ctx, cluster);
    }
    return (int)done;
}

/*
 * Create an empty file holding size bytes of zeroed storage. Fails when
 * the name is unusable, already taken, or neither the directory nor the
 * free space can take it; nothing is modified in that case.
 */
bool_t fatfs_create_file(fatfs_ctx_t* ctx, const char* name, uint32_t size) {
    char raw[11];
    fatfs_entry_loc_t loc;
    fatfs_dir_entry_t entry;
    uint32_t clusters, first = 0;

    if (!fatfs_ready(ctx) || !name) return false;
    if (!fatfs_name_to_83(name, raw)) return false;
    if (fatfs_dir_find(ctx, raw, NULL, NULL)) return false;
    if (!fatfs_dir_alloc(ctx, &loc)) return false;

    clusters = fatfs_clusters_for(ctx, size);
    if (clusters != 0 && !fatfs_chain_extend(ctx, &first, clusters)) return false;
    if (!fat_zero_chain(ctx, first)) {
        fatfs_free_chain(ctx, first);
        return false;
    }

    memset(&entry, 0, sizeof(entry));
    memcpy(entry.file_name, raw, 11);
    entry.attr = FATFS_ATTR_ARCHIVE;
    fatfs_entry_set_cluster(&entry, first);
    entry.file_size = size;
    entry.creation_date = FATFS_DATE_EPOCH;
    entry.last_access_date = FATFS_DATE_EPOCH;
    entry.write_date = FATFS_DATE_EPOCH;

    fatfs_dir_write_entry(&loc, &entry);
    return true;
}

/*
 * Replace the contents of a file with size bytes from data, growing or
 * shrinking its cluster chain as needed. The file is created when it does
 * not exist yet. When the free space runs out the old contents are left
 * untouched and false is returned.
 */
bool_t fatfs_write_file(fatfs_ctx_t* ctx, const char* name, const uint8_t* data, uint32_t size) {
    char raw[11];
    fatfs_entry_loc_t loc;
    fatfs_dir_entry_t entry;
    uint8_t sector[FATFS_SECTOR_SIZE];
    uint32_t first, needed, have, cluster, done = 0, steps = 0;

    if (!fatfs_ready(ctx) || !name || (data == NULL && size != 0)) return false;
    if (!fatfs_name_to_83(name, raw)) return false;

    if (!fatfs_dir_find(ctx, raw, &loc, &entry)) {
        if (!fatfs_create_file(ctx, name, size)) return false;
        if (!fatfs_dir_find(ctx, raw, &loc, &entry)) return false;
    }
    if (!fatfs_entry_is_file(&entry)) return false;

    needed = fatfs_clusters_for(ctx, size);
    first = fatfs_entry_cluster(ctx, &entry);
    have = fatfs_chain_length(ctx, first);

    if (needed > have && !fatfs_chain_extend(ctx, &first, needed - have)) return false;

    cluster = first;
    while (done < size) {
        uint32_t lba;
        uint32_t s;

        if (!fat_is_data_cluster(ctx, cluster)) return false;

        lba = fatfs_cluster_lba(ctx, cluster);
        for (s = 0; s < ctx->sectors_per_cluster && done < size; s++) {
            uint32_t n = FATFS_SECTOR_SIZE;

            if (n > size - done) n = size - done;

            /* Zero first so the tail of a partial sector cannot leak. */
            memset(sector, 0, FATFS_SECTOR_SIZE);
            memcpy(sector, data + done, n);
            fatfs_write_sector(lba + s, sector);
            done += n;
        }

        if (++steps > FAT_MAX_CLUSTER_STEPS) return false;
        cluster = fat_chain_next(ctx, cluster);
    }

    if (needed < have) fatfs_chain_truncate(ctx, &first, needed);

    fatfs_entry_set_cluster(&entry, first);
    entry.file_size = size;
    entry.write_date = FATFS_DATE_EPOCH;

    fatfs_dir_write_entry(&loc, &entry);
    return true;
}

/* Remove a file, return its clusters to the free space and free the slot. */
bool_t fatfs_delete_file(fatfs_ctx_t* ctx, const char* name) {
    char raw[11];
    fatfs_entry_loc_t loc;
    fatfs_dir_entry_t entry;

    if (!fatfs_ready(ctx) || !name) return false;
    if (!fatfs_name_to_83(name, raw)) return false;
    if (!fatfs_dir_find(ctx, raw, &loc, &entry)) return false;
    if (!fatfs_entry_is_file(&entry)) return false;

    fatfs_free_chain(ctx, fatfs_entry_cluster(ctx, &entry));

    memset(&entry, 0, sizeof(entry));
    entry.file_name[0] = (char)0xE5;
    fatfs_dir_write_entry(&loc, &entry);
    return true;
}

/* Size of a file in bytes, or 0 when it is missing or a directory. */
uint32_t fatfs_get_file_size(const fatfs_ctx_t* ctx, const char* name) {
    char raw[11];
    fatfs_dir_entry_t entry;

    if (!fatfs_ready(ctx) || !name) return 0;
    if (!fatfs_name_to_83(name, raw)) return 0;
    if (!fatfs_dir_find(ctx, raw, NULL, &entry)) return 0;
    if (!fatfs_entry_is_file(&entry)) return 0;

    return entry.file_size;
}

/* Print every entry of a directory, one per line. */
void fatfs_list_dir(const fatfs_ctx_t* ctx, uint32_t cluster) {
    uint8_t sector[FATFS_SECTOR_SIZE];
    fatfs_dir_iter_t it;
    uint32_t lba;

    if (!fatfs_ready(ctx)) return;

    fatfs_dir_iter_init(ctx, &it, cluster);
    while (fatfs_dir_iter_next(ctx, &it, &lba)) {
        fatfs_dir_entry_t* entry = (fatfs_dir_entry_t*)sector;
        uint32_t i;

        fatfs_read_sector(lba, sector);

        for (i = 0; i < FATFS_ENTRIES_PER_SECTOR; i++) {
            char name[12];

            if ((uint8_t)entry[i].file_name[0] == 0x00) return;
            if ((uint8_t)entry[i].file_name[0] == 0xE5) continue;
            if ((entry[i].attr & FATFS_ATTR_LFN) == FATFS_ATTR_LFN) continue;

            fatfs_name_to_display(entry[i].file_name, name);

            if (entry[i].attr & FATFS_ATTR_DIRECTORY) vga_print_scr("[DIR] ");
            else if (entry[i].attr & FATFS_ATTR_VOLUME) vga_print_scr("[VOL] ");
            else vga_print_scr("      ");

            vga_print_scr_nw(name);
        }
    }
}

/* Print the volume root directory. */
void fatfs_list(const fatfs_ctx_t* ctx) {
    fatfs_list_dir(ctx, ctx->root_cluster);
}
