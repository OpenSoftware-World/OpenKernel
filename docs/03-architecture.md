# 03 - Architecture

[← Getting Started](02-getting-started.md) | [Documentation Index](README.md) | [Next: Kernel Services →](04-kernel-services.md)

---

This document describes how the repository is organised, how the build works, and how memory is laid out once the kernel is linked.

## 1. Directory structure

```
OpenKernel/
├── Kernel/                          Kernel core
│   ├── ErrorHandler/panic.c         kernel_panic()
│   ├── KernelServices/
│   │   ├── LogService/log.c         Coloured log output
│   │   ├── SecurityChecker/         Runtime self-checks (PSC)
│   │   ├── SystemManagement/        Shutdown / reboot / exit
│   │   └── VirtualMem/              Paging setup
│   └── openkernel_information.h     Name, version, architecture
│
├── SystemLib/                       Reusable kernel-side library
│   ├── Memory/mem.c                 memset/memcpy/memcmp, kmalloc/kfree
│   ├── Std/std.c                    String functions (no printf)
│   ├── SystemIO/io.h                Port I/O inline functions
│   ├── SystemIO/qemu_io.h           QEMU debug port 0xE9 (unused)
│   ├── TimeMng/time.c               RTC reading, sleep, uptime
│   ├── TaskMng/task.c               Task list structures
│   ├── TypeConversion/type_conv.c   itoa, atoi, hex, BCD
│   ├── KernelFunc/kernel_func.c     Named kernel function registry
│   ├── SysCalls/syscall.c           Internal dispatch table
│   └── openkernel_systemlib_desc.h  Module metadata (unused)
│
├── Drivers/                         Hardware drivers
│   ├── Vga/                         Text mode 80x25
│   ├── Cpu/                         CPUID, cli/sti/hlt
│   ├── Keyboard/                    PS/2 polling
│   ├── Mouse/                       PS/2 polling
│   ├── Serial/                      COM1 16550
│   ├── Sound/sb16/                  Sound Blaster 16
│   ├── Ata/                         PIO mode disk access
│   ├── Ahci/                        SATA controller
│   ├── PCIBus/                      PCI configuration space
│   ├── Network/Ethernet/            Ethernet frame layer
│   ├── Network/e1000/               Intel e1000
│   ├── Network/RTL8139/             Realtek RTL8139
│   └── openkernel_drivers_desc.h    Module metadata (unused)
│
├── KernelAPI/                       Stable API for downstream OSes
│   ├── OpenKernelUniversalAPI/      Always-available functions
│   ├── nonuniversal_api_defs.*      Optional/subsystem functions
│   ├── OpenKernelEnvironment/       Types and syscall declarations
│   └── Information/                 API version metadata
│
├── OFS/                             Open File System (read/write, flat)
├── FAT12/ FAT16/ FAT32/             FAT readers (read-only)
├── VFS/                             Single active-file-system switch
│
├── Makefile                         Build rules
├── link.ld                          Linker script
├── install_pkg.sh                   Host setup (Linux)
├── notes.txt                        Bootstrapping instructions
├── version.txt                      Release version (4.0)
└── .github/workflows/               Release packaging and CodeQL
```

Rough size distribution (C source plus headers, about 5,350 lines total):

| Area | Lines | Share |
|---|---:|---:|
| `Drivers/` | 2,158 | 40% |
| `SystemLib/` | 1,463 | 27% |
| `Kernel/` | 694 | 13% |
| `KernelAPI/` | 341 | 6% |
| File systems (`OFS`, `FAT*`, `VFS`) | 596 | 11% |

The largest single files are `Kernel/KernelServices/SecurityChecker/security.c` (602 lines) and `Drivers/Ahci/ahci.c` (500 lines).

## 2. Include convention

Almost every file includes headers with an absolute-style path rooted at the package directory:

```c
#include <OpenKernel/SystemLib/Std/types.h>
```

This works because the compiler is invoked with `-I .` from the directory that *contains* `OpenKernel/`. If you compile from inside the repository root you must use `-I ..` instead.

A handful of files (`vga.c`, `ahci.c`, `mouse.c`, `pci.c`, `ata.c`, `fat16.c`, `fat32.c`) mix in relative includes such as `"../../SystemLib/SystemIO/io.h"`. Both conventions coexist; be careful if you move files.

## 3. The Makefile

### Tool variables

```make
AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld
```

### Output locations

```make
BUILD     = build
ISO       = iso
KERNEL    = $(BUILD)/os.bin
ISOFILE   = your_os.iso
```

Object files mirror the source tree under `build/`, for example `OpenKernel/Drivers/Vga/vga.c` becomes `build/OpenKernel/Drivers/Vga/vga.o`.

### Compilation flags

```make
CFLAGS  = -m32 -ffreestanding -O2 -Wall -Wextra -I .
LDFLAGS = -m elf_i386 -T link.ld
```

- `-m32` - 32-bit code.
- `-ffreestanding` - no host libc, no implicit startup files.
- `-O2` - optimise for speed.
- `-Wall -Wextra` - full warning set.
- `-I .` - include root.

### Source lists

```make
ASM_SRC =                     # assembly sources, empty in this repository
C_SRC   = OpenKernel/Drivers/Vga/vga.c \
          OpenKernel/Drivers/Cpu/cpu.c \
          ...                 # 29 entries
```

`ASM_SRC` is empty and there are no `.asm` files anywhere in the tree. This is why no `_start` symbol exists.

`C_SRC` lists 29 of the 31 C files. The two missing entries are:

- `KernelAPI/nonuniversal_api_defs.c`
- `KernelAPI/OpenKernelUniversalAPI/universal_api_defs.c`

Two listed paths are also wrong regardless of layout:

- `OpenKernel/Drivers/Sound/sb16.c` - the real path is `OpenKernel/Drivers/Sound/sb16/sb16.c`.
- `OpenKernel/Drivers/Network/rtl8139/rtl8139.c` - the real directory is `RTL8139` (capital letters), which matters on case-sensitive filesystems.

### Build targets

| Target | Action |
|---|---|
| `make` or `make all` | Compile all sources and link `build/os.bin`. |
| `make iso` | Copy the kernel and `Boot/grub/grub.cfg` into `iso/`, run `grub-mkrescue`, create `disk.img` if missing. |
| `make run` | Build the ISO and boot it in QEMU. |
| `make clean` | Delete `build/`, `iso/`, and the ISO file. |

## 4. The linker script (`link.ld`)

```
ENTRY(_start)
```

The entry symbol is `_start`. It must be defined by your assembly boot code.

### Memory layout

The kernel is linked at physical address **1 MiB** (`0x100000`), the conventional address above the real-mode memory map. Sections are ordered and aligned as follows:

| Section | Alignment | Contents |
|---|---|---|
| `.multiboot` | 4 KiB | GRUB multiboot header (`KEEP`ed so the linker never drops it) |
| `.text` | 4 KiB | Machine code |
| `.rodata` | 4 KiB | Read-only data, between `__ro_start` and `__ro_end` |
| `.data` | 4 KiB | Initialised data |
| `.bss` | 4 KiB | Zero-initialised data, `_bss_start` to `_bss_end` |
| `.stack` | 16 bytes | 16 KiB stack, `_stack_start` to `_stack_end` |
| `.heap` | 16 bytes | 1 MiB heap, `_heap_start` to `_heap_end` |

### Exported symbols

| Symbol | Meaning |
|---|---|
| `_start` | Entry point (must be supplied by you) |
| `__ro_start`, `__ro_end` | Bounds of read-only data |
| `_bss_start`, `_bss_end` | Bounds of the BSS segment |
| `_stack_start`, `_stack_end` | The 16 KiB stack region |
| `_heap_start`, `_heap_end` | The 1 MiB heap region |
| `_end`, `_kernel_end` | End of the kernel image |

**Important:** the heap region is reserved by the linker but never wired up automatically. You must call `heap_init((uint32_t)_heap_start, ...)` yourself from your entry point. Nothing in the repository does this.

## 5. Boot flow (as intended)

The complete intended sequence:

```
GRUB (multiboot)
   │
   ▼
_start (your assembly)         sets up a stack, checks the magic value
   │
   ▼
kernel_main (your C code)      the only place where OpenKernel is initialised
   │
   ├── vga_init()              screen output becomes available
   ├── heap_init()             kmalloc becomes usable
   ├── kbd_init()              keyboard polling enabled
   ├── vfs_init(fs) + fs_init  file system ready
   └── your application code
```

There is no code in the repository for steps 1 and 2. The documentation in `notes.txt` confirms this is deliberate: OpenKernel is meant to be embedded, not booted standalone.

## 6. Release packaging

`.github/workflows/release.yml` runs only when a push to `main` changes `version.txt`.

It performs these steps:

1. Read the version string from `version.txt`.
2. `rsync` the tree into `package/OpenKernel/`, excluding `.git`, `.github`, `version.txt`, `install_pkg.sh`, `link.ld`, `Makefile`, `notes.txt`, and `package` itself.
3. Copy `Makefile`, `link.ld`, `notes.txt`, `install_pkg.sh` next to the `OpenKernel/` folder.
4. Zip everything as `OpenKernel_<version>.zip`.
5. Generate `CHANGELOG.md` from the git log between version bumps.
6. Publish a GitHub release tagged `openkernel<version>`.

**The workflow never compiles anything.** It is a packaging job, not a build job, so a broken build does not fail CI.

`.github/workflows/codeql.yml` runs CodeQL security analysis for C with `build-mode: none` on every push, pull request, and weekly. Because there is no build step, analysis is path-based rather than compile-accurate.

## 7. Conventions you will notice

- **`*_init()` naming.** Every subsystem exposes an initialisation function. Until it is called, most functions in that subsystem are no-ops or unsafe.
- **Module metadata macros.** Headers define `*_MODULE_NAME`, `*_MODULE_VER`, `*_MODULE_KRNL_VER` and similar constants. They are informational and not referenced by any logic.
- **Character mode strings.** Several functions take a `const char*` mode and dispatch on the first character, for example `vga_clear_screen("F")` for a full clear and `vga_clear_screen("C")` to keep the first row, or `sys_next_status("shutdown", seconds)`.
- **Global state.** Most subsystems keep a file-static global (`vga_enabled_driver`, `current_fs`, `ofs`, `heap_current`). There is no instance-based design.

---

[← Getting Started](02-getting-started.md) | [Documentation Index](README.md) | [Next: Kernel Services →](04-kernel-services.md)
