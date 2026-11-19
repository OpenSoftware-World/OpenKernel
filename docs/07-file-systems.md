# 07 - File Systems

[← Drivers](06-drivers.md) | [Documentation Index](README.md) | [Next: Kernel API →](08-kernel-api.md)

---

OpenKernel ships four file system implementations and one dispatch layer:

| Component | Path | Capabilities |
|---|---|---|
| OFS (Open File System) | `OFS/` | Create, read, write, delete, list |
| FAT12 | `FAT12/` | Read only |
| FAT16 | `FAT16/` | Read only |
| FAT32 | `FAT32/` | Read only |
| VFS | `VFS/` | Selects one active file system |

None of the FAT implementations can write or format. There is no FAT formatter anywhere in the project.

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

### Common functions

```c
void     fat12_init(uint32_t boot_lba);     /* same shape for fat16, fat32 */
int      fat12_read_file(const char* file_name, void* buf, uint32_t size);
uint16_t fat12_get_next_cluster(uint16_t cluster);
void     fat16_list_root(void);
void     fat32_list_dir(uint32_t cluster);
```

### How reading works

1. `*_init(boot_lba)` reads the boot sector, parses `reserved_sectors`, FAT count, root entry count, and sectors per cluster, then computes `fat_start_lba`, `root_dir_lba`, and `data_start_lba`.
2. `*_read_file()` linearly scans the root directory for an exact 11-byte 8.3 filename match.
3. It follows the cluster chain with `*_get_next_cluster()` and copies each cluster into the caller's buffer.

Cluster chain decoding differs per variant:

| Variant | Entry width | Formula |
|---|---|---|
| FAT12 | 12 bits, packed | index `cluster + cluster/2`, then mask `0x0FFF` |
| FAT16 | 16 bits | index `cluster * 2` |
| FAT32 | 32 bits, high 4 bits reserved | index `cluster * 4`, mask `0x0FFFFFFF` |

### Limitations of all three

- **Read only.** No write, no create, no delete, no format.
- **8.3 names only.** Long file name (LFN) entries are not skipped, so a FAT32 directory containing LFN entries will print garbage in `fat32_list_dir()`.
- **Root directory only** for FAT12 and FAT16 reads. FAT32 can list a subdirectory by cluster but `fat32_read_file()` still targets the root.
- **Cycle guard.** `fat32_list_dir()` and the root walk in `fat32_read_file()` stop after `FAT_MAX_CLUSTER_STEPS` (65,536) clusters. The per-file data walk terminates on its own because the byte counter strictly increases.
- **Bounded reads.** Each sector is read into a local 512-byte buffer and only the bytes that still fit are copied, so a `size` that is not a multiple of the sector size can no longer write past the caller's buffer.
- **Geometry is validated.** `*_init()` reads the boot sector into a sector-sized buffer (the old code read 512 bytes into a 36 / 62 / 90-byte struct and ran hundreds of bytes past it), then requires `bytes_per_sector == 512` and a non-zero `sectors_per_cluster`. Anything else leaves the root directory empty so every lookup misses, instead of dividing by zero later.
- `boot_lba` is hard-coded to `0` wherever the VFS calls them.

### Compile status

All three compile clean with `-Wall -Wextra`. Older revisions of this document listed an `entry[j].filename` typo in `fat12.c`, a missing `fat12_list_root()` declaration, and two calls to the non-existent `ata_read_sector()`; none of those are present any more.

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
typedef enum filesystem_list { OFS, FAT12, FAT16, FAT32 } fs_t;

void vfs_init(fs_t fsname);
void vfs_list(void);
void vfs_read_file(const char* name, uint8_t* buffer, void* buf, uint32_t size);
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

`vfs_create_file()`, `vfs_write_file()` and `vfs_delete_file()` are declared in `vfs.h` and defined in `vfs.c`. They forward to the OFS implementation and are empty `case` branches for FAT12, FAT16 and FAT32 (the source carries a comment saying write support there is planned).

They therefore only work when `vfs_init(OFS)` selected OFS. On a FAT file system the calls return without doing anything.

### Compile status

`vfs.c` and `vfs.h` compile clean. Older revisions reported three problems, none of which remain: `vfs.h` now includes `Std/types.h`, `fat12_list_root()` is declared in `fat12.h`, and `vfs_list()` calls `fat32_list_root()` rather than the `fat32_list_dir(uint32_t cluster)` entry point.

Note that `vfs_read_file()` has no error return path: the return values of the underlying `fat*_read_file()` calls are discarded.

## 4. Choosing a file system

| You want | Use | Why |
|---|---|---|
| To create and modify files inside your kernel | OFS | It is the only read/write implementation. Remember the fixed 4 KiB per-file slot and the missing buffer-length parameter on read. |
| To read files from an image made with host tools | FAT12/16/32 | Standard formats you can build with `mkfs.fat`. |
| A single switch point for your OS code | VFS | Hides which file system is active, at the cost of flexibility. |
| Subdirectories or files larger than 4 KiB | None of the above | Not supported. You would need to extend OFS or complete the FAT drivers. |

## 5. How the file systems reach storage

Every file system performs raw sector I/O through the ATA driver (`ata_sys_disk_read` / `ata_sys_disk_write`) on port `0x1F0`. Consequences:

- The disk must be on the primary IDE channel, which is what `make run` provides via `-drive file=disk.img,format=raw,if=ide`.
- Sector 0 must contain either an OFS superblock or a FAT boot sector.
- The AHCI driver is not wired into any file system; SATA-only machines are unsupported.
- Because `heap_init()` and `vga_init()` are prerequisites for other subsystems, initialise those first or file system operations that print will silently do nothing.

---

[← Drivers](06-drivers.md) | [Documentation Index](README.md) | [Next: Kernel API →](08-kernel-api.md)
