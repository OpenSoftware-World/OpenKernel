# 07 - File Systems

[← Drivers](06-drivers.md) | [Documentation Index](README.md) | [Next: Kernel API →](08-kernel-api.md)

---

OpenKernel ships four file system implementations and one dispatch layer:

| Component | Path | Capabilities |
|---|---|---|
| OFS (Open File System) | `OFS/` | Create, read, write, delete, list |
| FAT12 | `FAT12/` | Create, read, write, delete, list (root directory) |
| FAT16 | `FAT16/` | Create, read, write, delete, list (root directory) |
| FAT32 | `FAT32/` | Create, read, write, delete, list (root or any directory cluster) |
| VFS | `VFS/` | Selects one active file system |

The FAT drivers can write files but they do not format: there is no FAT formatter anywhere in the project, so the volume still has to be created with host tools such as `mkfs.fat`.

## 1. OFS - Open File System

A deliberately simple flat file system designed for this project.

### On-disk layout

| Region | Sectors | Contents |
|---|---|---|
| Superblock | 0 | Signature `"OFS1"`, `total_sectors`, `file_table_start`, `data_start`, `version` |
| File table | 1 to 11 | 11 sectors (5,632 bytes), exactly enough for 128 x 44-byte entries |
| Data | from 12 | Each file occupies a fixed 8-sector (4 KiB) slot |

Each file entry is:

```c
typedef struct {
    char     name[32];        /* OFS_NAME_MAX_LENGTH */
    uint32_t start_sector;
    uint32_t size_bytes;
    uint8_t  used;
    uint8_t  flags;
    uint8_t  reserved;
} ofs_file_t;
```

Because slots are fixed size, a file's sector index is simply `data_start + index * 8`. There is no free-space map, no allocation bitmap, and no way for a file to grow beyond 4 KiB.

### Functions

```c
void ofs_init(void);
void ofs_create_file(const char* name, uint32_t size);
void ofs_read_file(const char* name, uint8_t* buffer);
void ofs_write_file(const char* name, const uint8_t* data, uint32_t size);
void ofs_delete_file(const char* name);
void ofs_list_files(void);
```

### `ofs_init()` behaviour

1. Reads sector 0 into a 512-byte buffer and copies the 20-byte superblock out of it (the old code read 512 bytes straight into the 20-byte struct, overwriting the first 11 table entries).
2. Validates the superblock: signature `"OFS1"`, a non-zero `file_table_start`, and `data_start >= file_table_start + 11`.
3. If validation fails, it formats: writes a fresh superblock, zeroes the table in memory, and flushes the 11 table sectors to disk. A status message is printed through the VGA layer (which does nothing while the VGA driver is disabled).
4. Otherwise it copies the superblock in and loads the 11 table sectors.

So the first use on a blank disk automatically formats it, and an image written by the previous 8-sector layout is reformatted rather than silently overlapping the table with the data area.

Every mutation (`ofs_create_file()`, `ofs_write_file()`, `ofs_delete_file()`) now calls the same flush helper, so metadata survives a reboot.

### Usage

```c
ofs_init();

ofs_create_file("hello.txt", 128);

const char* msg = "Hello, OFS!";
ofs_write_file("hello.txt", (const uint8_t*)msg, 11);

uint8_t buf[128];
ofs_read_file("hello.txt", buf);

ofs_list_files();
ofs_delete_file("hello.txt");
```

### Known defects

Read these before relying on OFS:

1. **`ofs_read_file()` takes no buffer length.** The sector count comes from `size_bytes` alone (capped at the 8-sector slot), so the caller must guarantee the buffer is at least `size_bytes` large. The FAT readers take a `size`; this one does not.
2. **No create-on-write.** `ofs_write_file()` only updates files that already exist.
3. **`ofs_create_file()` does not zero the data area**, so a newly created file contains stale bytes until written.
4. **`total_sectors` in the superblock is never populated.**
5. **The on-disk layout changed.** The table area went from 8 sectors (4,096 bytes - too small for 128 x 44-byte entries) to the derived 11 sectors. `ofs_init()` reformats an image that does not match `data_start >= file_table_start + 11`, so a disk written by the previous code loses its contents on first mount.
6. **No directories, no free-space map, no file larger than 4 KiB.**

Fixed in this revision, kept here so the history is not lost:

- The table used to be addressed as `ofs.file_table + (i * SECTOR_SIZE)`, which advances `i * 512 * sizeof(ofs_file_t)` bytes (about 22 KiB per iteration) and wrote far outside the object. It is byte addressing now.
- The superblock used to be filled by a 512-byte read into a 20-byte struct, and written back the same way. Both directions now go through a sector-sized buffer.
- `ofs_create_file()`, `ofs_write_file()` and `ofs_delete_file()` used to modify only the in-memory table; nothing was ever written back except during format. They flush now.
- `ofs_write_file()` used to transfer full 512-byte sectors straight from the caller's data, so a size that was not a multiple of 512 caused an over-read past the end of the caller's buffer. Partial sectors are now built in a scratch buffer and zero-filled, and the transfer is capped at the 8-sector slot.

## 2. FAT12, FAT16, FAT32

All three follow the same design: read the BIOS Parameter Block, locate the FAT and root directory, then walk cluster chains.

Only the BPB parsing is per variant. Everything that does not depend on the entry width lives in the shared core `FATLib/fatlib.c`, which works on a `fatfs_ctx_t` describing the volume:

```c
typedef struct {
    fatfs_type_t type;          /* FATFS_TYPE_FAT12 / _FAT16 / _FAT32 */
    uint32_t fat_start_lba;
    uint32_t fat_sectors;
    uint8_t  fat_count;         /* every copy is kept in sync on write */
    uint32_t root_dir_lba;      /* FAT12/FAT16 fixed root */
    uint32_t root_dir_sectors;  /* FAT12/FAT16 only, 0 for FAT32 */
    uint32_t data_start_lba;
    uint32_t root_cluster;      /* FAT32 root, 0 otherwise */
    uint16_t bytes_per_sector;  /* 512 once the volume is usable */
    uint8_t  sectors_per_cluster;
    uint32_t total_clusters;    /* 0 when the BPB does not say */
    uint32_t alloc_hint;        /* where the next free-space scan starts */
} fatfs_ctx_t;
```

`fat12_fs_t`, `fat16_fs_t` and `fat32_fs_t` now hold the parsed boot sector plus this context; the old `fat_start_lba` / `root_dir_lba` / `data_start_lba` fields were folded into it.

### Common functions

```c
void     fat12_init(uint32_t boot_lba);     /* same shape for fat16, fat32 */
int      fat12_read_file(const char* file_name, void* buf, uint32_t size);
bool_t   fat12_create_file(const char* file_name, uint32_t size);
bool_t   fat12_write_file(const char* file_name, const uint8_t* data, uint32_t size);
bool_t   fat12_delete_file(const char* file_name);
uint32_t fat12_get_file_size(const char* file_name);
uint16_t fat12_get_next_cluster(uint16_t cluster);
void     fat12_list_root(void);
```

`fat16_*` is identical. `fat32_*` uses `uint32_t` clusters and adds `fat32_list_dir(uint32_t cluster)`, which lists an arbitrary directory cluster instead of only the root.

### How reading works

1. `*_init(boot_lba)` reads the boot sector into a sector-sized buffer, validates `bytes_per_sector == 512`, a non-zero `sectors_per_cluster`, `sectors_per_fat` and `fat_count`, then fills the context. A boot sector that fails validation leaves the context *not ready*, so every later call returns without touching the disk rather than dividing by zero.
2. `fatfs_finalize_geometry()` derives `total_clusters` from the BPB totals and clamps it to what the FAT size and the variant's reserved/bad/EOC marker range can address, so a wrong BPB can only shrink the allocatable space.
3. `fatfs_read_file()` converts the caller's name to the raw 11-byte 8.3 field, scans the root directory for an exact match, follows the cluster chain and copies each cluster into the caller's buffer. The copy stops at `min(size, file_size)`, so a read can no longer run past the end of the file.
4. Cluster chain decoding differs per variant:

| Variant | Entry width | Formula |
|---|---|---|
| FAT12 | 12 bits, packed | index `cluster + cluster/2`, then mask `0x0FFF` |
| FAT16 | 16 bits | index `cluster * 2` |
| FAT32 | 32 bits, high 4 bits reserved | index `cluster * 4`, mask `0x0FFFFFFF` |

### Writing

All three variants share one implementation:

- **`fatfs_create_file()`** converts the name, refuses it when it already exists, reserves a directory slot, allocates and **zeroes** the cluster chain, then writes an archive entry with `FATFS_DATE_EPOCH` (1980-01-01) in the date fields. A zero-length file gets cluster 0, as the FAT spec requires.
- **`fatfs_write_file()`** creates the file when it does not exist (the same contract OFS and tFS use), grows the chain when the new size needs more clusters, writes the payload sector by sector, truncates the surplus clusters and finally updates `file_size`. Every partial sector is zero-filled first, so bytes past `file_size` cannot leak stale data from a previous occupant. When the volume is full the old contents are left untouched and `false` is returned.
- **`fatfs_delete_file()`** frees the whole chain, then marks the entry `0xE5`.
- **`fatfs_alloc_cluster()`** marks a claimed cluster end-of-chain immediately (no window where it looks free) and resumes each scan from `alloc_hint`, so filling a volume stays linear. `fatfs_fat_set()` writes **every** copy of the FAT, and its read-modify-write path tolerates an entry that straddles a sector boundary.
- **Names** go through `fatfs_name_to_83()`: `"readme.txt"` becomes `README  TXT`, an already padded 11-byte field with no dot is taken verbatim, and the forbidden 8.3 characters plus control characters are rejected. Names are ASCII only; there is no LFN writer.

### Limitations of all three

- **Root directory only.** FAT12 and FAT16 cannot create files anywhere else. FAT32 can *list* a subdirectory by cluster (`fat32_list_dir()`), but create/write/delete/read always target the root.
- **8.3 names only.** No long file name support: LFN entries are skipped on read and never generated on write.
- **No formatter.** The drivers mount an existing volume; they never write a boot sector.
- **No timestamps from the clock.** Files get the FAT epoch date (1980-01-01) and midnight as their time, because nothing in the project converts the RTC reading into FAT date/time fields yet.
- **Cycle guard.** Directory and chain walks stop after `FAT_MAX_CLUSTER_STEPS` (65,536) steps. The per-file data walk terminates on its own because the byte counter strictly increases.
- **Bounded transfers.** Every sector is read into a local 512-byte buffer and only the bytes that still fit are copied, so a `size` that is not a multiple of the sector size cannot write past the caller's buffer.
- **Geometry is validated**, but the declared variant is not checked against the cluster count: calling `fat12_init()` on a FAT16 image is still user error and will misread the FAT.
- `boot_lba` is hard-coded to `0` wherever the VFS calls them.

### Compile status

All four files (`fatlib.c` plus the three drivers) compile clean with `-Wall -Wextra`, and the tree links with no undefined `fatfs_*` symbols. Runtime behaviour has not been executed.

### Testing a FAT filesystem

Since the project has no formatter, create the image on the host:

```bash
qemu-img create fatdisk.img 32M
mkfs.fat -F 16 fatdisk.img
mcopy -i fatdisk.img myfile.txt ::
```

Then attach it in QEMU and point `fat16_init(0)` at it.

## 3. VFS - `VFS/vfs.c`

### What it is

A thin dispatch layer over one *globally selected* file system.

```c
typedef enum filesystem_list { OFS, FAT12, FAT16, FAT32, tFS } fs_t;

void vfs_init(fs_t fsname);
void vfs_list(void);
void vfs_read_file(const char* name, uint8_t* buffer, void* buf, uint32_t size);
void vfs_create_file(const char* name, uint32_t size);
void vfs_write_file(const char* name, const uint8_t* data, uint32_t size);
void vfs_delete_file(const char* name);
```

`vfs_init()` stores the choice in a static `current_fs` and calls that file system's `*_init(0)` with a boot LBA of 0. From then on `vfs_list()` and `vfs_read_file()` switch on `current_fs` and forward to the selected implementation.

```c
vfs_init(FAT16);
vfs_list();

uint8_t buffer[4096];
vfs_read_file("README.TXT", buffer, buffer, 4096);
```

### What it is not

Do not mistake this for a real virtual file system:

- **No mount table.** One file system is active for the whole kernel at a time.
- **No mount points, no paths, no directories.** You pass a bare filename.
- **No file handles, no offsets, no seek.** Reads always start at the beginning.
- **No per-mount state** beyond the single `current_fs` global.
- **No backend registration mechanism.** Adding a file system means editing the enum and the switch statements.

### The dual-buffer signature

```c
void vfs_read_file(const char* name, uint8_t* buffer, void* buf, uint32_t size);
```

`buffer` is used for OFS and `buf` for the FAT variants. This exists because `ofs_read_file()` takes no size argument while the FAT readers do. In practice most callers pass the same pointer twice, as shown above. It is a design smell caused by inconsistent underlying signatures.

### Write support

`vfs_create_file()`, `vfs_write_file()` and `vfs_delete_file()` are declared in `vfs.h` and defined in `vfs.c`. Each of them switches on `current_fs` and forwards to the selected implementation, for OFS, tFS **and** the three FAT variants alike:

```c
vfs_init(FAT16);

vfs_create_file("hello.txt", 64);
vfs_write_file("hello.txt", (const uint8_t*)"Hello, FAT!", 11);

uint8_t buffer[64];
vfs_read_file("hello.txt", buffer, buffer, 64);   /* 11 bytes are copied */

vfs_delete_file("hello.txt");
```

On a FAT volume the calls therefore reach `fat16_create_file()` and friends. The FAT backends are root-directory-only and 8.3-only (see section 2), and none of the three `void` wrappers report failure: a full volume or an invalid name is silently ignored. Use the `fat*_` functions directly when you need the `bool_t` result.

### Compile status

`vfs.c`, `vfs.h` and the FAT backends compile clean. `vfs_read_file()` still has no error return path: the return values of the underlying `fat*_read_file()` calls are discarded, and `ofs_read_file()` ignores `size` entirely (it has no length parameter).

## 4. Choosing a file system

| You want | Use | Why |
|---|---|---|
| A small self-contained volume you format yourself | OFS | It formats a blank disk on first mount. Remember the fixed 4 KiB per-file slot and the missing buffer-length parameter on read. |
| Files bigger than 4 KiB, or an image built with host tools | FAT12/16/32 | Standard formats you can build with `mkfs.fat`; files are limited by the volume, not by a fixed slot. |
| A single switch point for your OS code | VFS | Hides which file system is active, at the cost of flexibility. |
| Subdirectories you can create through VFS | None of the above | OFS and tFS have no directories, and the FAT drivers only write into the root (FAT32 can *list* a subdirectory by cluster). |

## 5. How the file systems reach storage

Every file system performs raw sector I/O through the ATA driver (`ata_sys_disk_read` / `ata_sys_disk_write`) on port `0x1F0`. Consequences:

- The disk must be on the primary IDE channel, which is what `make run` provides via `-drive file=disk.img,format=raw,if=ide`.
- Sector 0 must contain either an OFS superblock or a FAT boot sector.
- The AHCI driver is not wired into any file system; SATA-only machines are unsupported.
- Because `heap_init()` and `vga_init()` are prerequisites for other subsystems, initialise those first or file system operations that print will silently do nothing.

---

[← Drivers](06-drivers.md) | [Documentation Index](README.md) | [Next: Kernel API →](08-kernel-api.md)
