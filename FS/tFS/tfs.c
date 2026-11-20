#include "tfs.h"
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Vga/vga.h>

/*
 * tFS (tempFS) - a RAM-backed file system.
 *
 * Layout of the pool: every file owns a chain of TFS_BLOCK_SIZE blocks. The
 * chain links live in tfs_next[], which doubles as the free list: a block is
 * either linked into a file's chain or linked into the free list, never both.
 */

static uint8_t tfs_pool[TFS_POOL_SIZE];
static tfs_file_t tfs_table[TFS_MAX_FILES];
static uint16_t tfs_next[TFS_BLOCK_COUNT];
static uint16_t tfs_free_head;

/* Block indices are stored in uint16_t, so the sentinel must stay reachable. */
STATIC_ASSERT(TFS_BLOCK_COUNT <= TFS_NO_BLOCK, tfs_block_count_must_fit_uint16);

/* Walk a block chain and return how many blocks it holds. */
static uint32_t tfs_chain_length(uint16_t blk) {
    uint32_t count = 0;

    while (blk != TFS_NO_BLOCK) {
        count++;
        blk = tfs_next[blk];
    }
    return count;
}

/* Release a whole block chain back onto the free list. */
static void tfs_free_chain(uint16_t blk) {
    while (blk != TFS_NO_BLOCK) {
        uint16_t next = tfs_next[blk];
        tfs_next[blk] = tfs_free_head;
        tfs_free_head = blk;
        blk = next;
    }
}

/* Pop one block off the free list, or TFS_NO_BLOCK when the pool is empty. */
static uint16_t tfs_alloc_block(void) {
    uint16_t blk;

    if (tfs_free_head == TFS_NO_BLOCK) return TFS_NO_BLOCK;

    blk = tfs_free_head;
    tfs_free_head = tfs_next[blk];
    tfs_next[blk] = TFS_NO_BLOCK;
    return blk;
}

/* Allocate a chain of count blocks; rolls back and returns TFS_NO_BLOCK on failure. */
static uint16_t tfs_alloc_chain(uint32_t count) {
    uint16_t first = TFS_NO_BLOCK;
    uint16_t prev = TFS_NO_BLOCK;

    for (uint32_t i = 0; i < count; i++) {
        uint16_t blk = tfs_alloc_block();

        if (blk == TFS_NO_BLOCK) {
            tfs_free_chain(first);
            return TFS_NO_BLOCK;
        }

        if (prev == TFS_NO_BLOCK) first = blk;
        else tfs_next[prev] = blk;

        prev = blk;
    }
    return first;
}

/* Drop every block after the first keep ones, returning them to the free list. */
static void tfs_truncate_chain(uint16_t* first, uint32_t keep) {
    uint16_t blk = *first;
    uint16_t rest;

    if (blk == TFS_NO_BLOCK) return;

    if (keep == 0) {
        tfs_free_chain(blk);
        *first = TFS_NO_BLOCK;
        return;
    }

    for (uint32_t i = 1; i < keep; i++) {
        if (tfs_next[blk] == TFS_NO_BLOCK) return;
        blk = tfs_next[blk];
    }

    rest = tfs_next[blk];
    tfs_next[blk] = TFS_NO_BLOCK;
    tfs_free_chain(rest);
}

/* Return the table index of name, or -1 when it is not present. */
static int tfs_find(const char* name) {
    for (int i = 0; i < TFS_MAX_FILES; i++) {
        if (tfs_table[i].used &&
            strncmp(tfs_table[i].name, name, TFS_NAME_MAX_LENGTH) == 0) {
            return i;
        }
    }
    return -1;
}

/* Return the index of a free table slot, or -1 when the table is full. */
static int tfs_free_slot(void) {
    for (int i = 0; i < TFS_MAX_FILES; i++) {
        if (!tfs_table[i].used) return i;
    }
    return -1;
}

/* Round a byte count up to a whole number of blocks. */
static uint32_t tfs_blocks_for(uint32_t size_bytes) {
    return (size_bytes + TFS_BLOCK_SIZE - 1) / TFS_BLOCK_SIZE;
}

/* Zero every block of a freshly allocated chain so no stale bytes leak out. */
static void tfs_zero_chain(uint16_t first) {
    for (uint16_t blk = first; blk != TFS_NO_BLOCK; blk = tfs_next[blk]) {
        memset(tfs_pool + (uint32_t)blk * TFS_BLOCK_SIZE, 0, TFS_BLOCK_SIZE);
    }
}

/*
 * Reset the file system to an empty state. Everything written before is lost,
 * which is the point of a temporary file system.
 */
void tfs_init(void) {
    memset(tfs_table, 0, sizeof(tfs_table));
    memset(tfs_pool, 0, sizeof(tfs_pool));

    for (uint32_t i = 0; i < TFS_BLOCK_COUNT; i++) {
        tfs_next[i] = (uint16_t)(i + 1);
    }
    tfs_next[TFS_BLOCK_COUNT - 1] = TFS_NO_BLOCK;
    tfs_free_head = 0;

    vga_print_scr_nw("tFS: RAM file system ready");
}

/*
 * Create an empty file holding size_bytes of zeroed storage.
 * Fails when the name is unusable, already taken, or the pool cannot fit it.
 */
bool_t tfs_create_file(const char* name, uint32_t size) {
    uint32_t blocks;
    uint16_t first;
    int slot;

    if (!name || !name[0]) return false;
    if (strlen(name) >= TFS_NAME_MAX_LENGTH) return false;
    if (tfs_find(name) >= 0) return false;

    slot = tfs_free_slot();
    if (slot < 0) return false;

    blocks = tfs_blocks_for(size);
    first = TFS_NO_BLOCK;

    if (blocks > 0) {
        first = tfs_alloc_chain(blocks);
        if (first == TFS_NO_BLOCK) return false;
        tfs_zero_chain(first);
    }

    memset(tfs_table[slot].name, 0, TFS_NAME_MAX_LENGTH);
    strncpy(tfs_table[slot].name, name, TFS_NAME_MAX_LENGTH - 1);

    tfs_table[slot].size_bytes = size;
    tfs_table[slot].first_block = first;
    tfs_table[slot].flags = 0;
    tfs_table[slot].used = 1;
    return true;
}

/*
 * Copy at most size bytes of the file into buffer.
 * size is the caller's buffer capacity, so a short buffer can never overrun.
 */
bool_t tfs_read_file(const char* name, uint8_t* buffer, uint32_t size) {
    uint32_t offset = 0;
    uint32_t to_copy;
    uint16_t blk;
    int idx;

    if (!name || !buffer) return false;

    idx = tfs_find(name);
    if (idx < 0) return false;

    to_copy = tfs_table[idx].size_bytes;
    if (to_copy > size) to_copy = size;

    blk = tfs_table[idx].first_block;
    while (offset < to_copy && blk != TFS_NO_BLOCK) {
        uint32_t chunk = to_copy - offset;

        if (chunk > TFS_BLOCK_SIZE) chunk = TFS_BLOCK_SIZE;

        memcpy(buffer + offset, tfs_pool + (uint32_t)blk * TFS_BLOCK_SIZE, chunk);
        offset += chunk;
        blk = tfs_next[blk];
    }
    return true;
}

/*
 * Replace the file's contents with size bytes from data, resizing it as needed.
 * The file is created when it does not exist yet. On a pool exhaustion the
 * old contents are left untouched and false is returned.
 */
bool_t tfs_write_file(const char* name, const uint8_t* data, uint32_t size) {
    uint32_t needed;
    uint32_t have;
    uint32_t offset;
    uint16_t orig_tail = TFS_NO_BLOCK;
    uint16_t added_head = TFS_NO_BLOCK;
    uint16_t prev;
    uint16_t blk;
    int idx;

    if (!name || !data) return false;

    idx = tfs_find(name);
    if (idx < 0) {
        if (!tfs_create_file(name, size)) return false;
        idx = tfs_find(name);
        if (idx < 0) return false;
    }

    needed = tfs_blocks_for(size);
    have = tfs_chain_length(tfs_table[idx].first_block);

    if (needed > have) {
        /* Keep the existing blocks and append only the missing ones. */
        prev = tfs_table[idx].first_block;

        if (prev != TFS_NO_BLOCK) {
            while (tfs_next[prev] != TFS_NO_BLOCK) prev = tfs_next[prev];
            orig_tail = prev;
        }

        for (uint32_t i = have; i < needed; i++) {
            uint16_t fresh = tfs_alloc_block();

            if (fresh == TFS_NO_BLOCK) {
                tfs_free_chain(added_head);
                if (orig_tail == TFS_NO_BLOCK) tfs_table[idx].first_block = TFS_NO_BLOCK;
                else tfs_next[orig_tail] = TFS_NO_BLOCK;
                return false;
            }

            if (added_head == TFS_NO_BLOCK) added_head = fresh;

            if (prev == TFS_NO_BLOCK) tfs_table[idx].first_block = fresh;
            else tfs_next[prev] = fresh;

            tfs_next[fresh] = TFS_NO_BLOCK;
            prev = fresh;
        }
    } else if (needed < have) {
        tfs_truncate_chain(&tfs_table[idx].first_block, needed);
    }

    /* Copy the payload block by block, zeroing each one first. */
    offset = 0;
    blk = tfs_table[idx].first_block;
    while (offset < size && blk != TFS_NO_BLOCK) {
        uint32_t chunk = size - offset;
        uint8_t* dst = tfs_pool + (uint32_t)blk * TFS_BLOCK_SIZE;

        if (chunk > TFS_BLOCK_SIZE) chunk = TFS_BLOCK_SIZE;

        memset(dst, 0, TFS_BLOCK_SIZE);
        memcpy(dst, data + offset, chunk);
        offset += chunk;
        blk = tfs_next[blk];
    }

    tfs_table[idx].size_bytes = size;
    return true;
}

/* Remove a file and return its blocks to the pool. */
bool_t tfs_delete_file(const char* name) {
    int idx;

    if (!name) return false;

    idx = tfs_find(name);
    if (idx < 0) return false;

    tfs_free_chain(tfs_table[idx].first_block);
    memset(&tfs_table[idx], 0, sizeof(tfs_file_t));
    return true;
}

/* Print every file name currently held in RAM. */
void tfs_list_files(void) {
    for (int i = 0; i < TFS_MAX_FILES; i++) {
        if (!tfs_table[i].used) continue;

        vga_print_scr("FILE: ");
        vga_print_scr(tfs_table[i].name);
        vga_print_scr("\n");
    }
}

/* Size of a file in bytes, or 0 when it is missing or empty. */
uint32_t tfs_get_file_size(const char* name) {
    int idx;

    if (!name) return 0;

    idx = tfs_find(name);
    return idx < 0 ? 0 : tfs_table[idx].size_bytes;
}
