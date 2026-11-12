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
| File table | 1 to 8 | 8 sectors holding up to 128 file entries |
| Data | from 9 | Each file occupies a fixed 8-sector (4 KiB) slot |

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

1. Reads sector 0.
2. If the signature is not `"OFS1"`, it formats: writes a fresh superblock, zeroes the table in memory, and writes the 8 table sectors to disk.
3. Reads the 8 table sectors into the in-memory `ofs.file_table` array.

So the first use on a blank disk automatically formats it.

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

1. **Out-of-bounds pointer arithmetic during init and format.** The code writes and reads the table with:
   ```c
   ata_sys_disk_write(1 + i, (uint8_t*)(ofs.file_table + (i * SECTOR_SIZE)));
   ```
   `ofs.file_table` is an `ofs_file_t*`, so `+ i * 512` advances by `512 * sizeof(ofs_file_t)` bytes (roughly 22 KiB per iteration) instead of 512 bytes. For every iteration after the first this reads and writes far outside the object and corrupts other global data. The table is therefore never persisted correctly.
2. **The table does not fit.** 128 entries at roughly 44 bytes each is about 5,632 bytes, but only 8 sectors (4,096 bytes) are reserved. Even with correct arithmetic, the last entries would spill into the data area.
3. **Changes are never flushed.** `ofs_create_file()`, `ofs_write_file()`, and `ofs_delete_file()` modify only the in-memory table. It is written back to disk only during the format step of `ofs_init()`. All metadata changes are lost on reboot.
4. **No read-size limit.** `ofs_read_file()` derives the sector count from `size_bytes` with no cap, so a corrupt or oversized entry reads into the following file's slot.
5. `ofs_write_file()` only updates files that already exist; there is no create-on-write.
6. `ofs_create_file()` does not zero the data area, so a newly created file contains stale bytes until written.
7. `total_sectors` in the superblock is never populated.

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
- **No cycle guard** in the cluster chain walk. A corrupted FAT loops forever.
- **No buffer overrun protection.** `size` limits the loop, but each `read_sector` writes a full 512 bytes, so a partial final sector can write past the end of the caller's buffer.
- `boot_lba` is hard-coded to `0` wherever the VFS calls them.

### Compile status

| File | Problem |
|---|---|
| `FAT12/fat12.c` | Uses `entry[j].filename` but the struct member is `file_name`. Also, `fat12_list_root()` is used by the VFS but never declared in `fat12.h`. |
| `FAT16/fat16.c` | Calls `ata_read_sector()`, which does not exist. The real function is `ata_sys_disk_read()`. |
| `FAT32/fat32.c` | Same missing `ata_read_sector()` call. |

None of the three compiles as written.

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

Create, write, and delete are commented out in `vfs.h` under `/* Coming soon... */`:

```c
/* Coming soon...
void vfs_create_file(const char *name, uint32_t size);
void vfs_write_file(const char *name, const uint8_t* data, uint32_t size);
void vfs_delete_file(const char *fname);
*/
```

They cannot work generically today, because only OFS implements writes and the FAT drivers are read-only.

### Compile status

`vfs.c` and `vfs.h` do not compile:

- `vfs.h` includes no type header, so `uint8_t` and `uint32_t` are unknown.
- `fat12_list_root()` is called implicitly and is not declared in `fat12.h`.
- `fat32_list_dir()` is called with no arguments although it requires a `uint32_t cluster`.

## 4. Choosing a file system

| You want | Use | Why |
|---|---|---|
| To create and modify files inside your kernel | OFS | It is the only read/write implementation. Work around the table-persistence bug first. |
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
