# 09 - Project Status

[← Kernel API](08-kernel-api.md) | [Documentation Index](README.md) | [Next: Contributing →](10-contributing.md)

---

This document is an honest assessment of the code base as it exists in the repository today. It exists so you do not discover these problems one build failure at a time.

Everything below was verified against the source tree. Where a claim is based on static reading rather than execution, it says so.

## 1. Build verification results

Test environment: `i686-elf-gcc` 14.2.0, flags `-m32 -ffreestanding -O2 -Wall -Wextra -I .. -fsyntax-only`, run over all 31 `.c` files.

| Result | Count |
|---|---:|
| Pass syntax check | 15 |
| Fail syntax check | 16 |

Passing: `kernel_func.c`, `mem.c`, `task.c`, `time.c`, `std.c`, `ofs.c`, `sb16.c`, `vga.c`, `cpu.c`, `ata.c`, `universal_api_defs.c`, `nonuniversal_api_defs.c`, `panic.c`, `vmem.c`, `sysmng.c`.

Failing: `syscall.c`, `type_conv.c`, `vfs.c`, `fat12.c`, `fat16.c`, `fat32.c`, `ethernet.c`, `e1000.c`, `rtl8139.c`, `ahci.c`, `keyboard.c`, `mouse.c`, `pci.c`, `serial.c`, `log.c`, `security.c`.

Note on the include root: the tests above use `-I ..` because every file includes headers as `<OpenKernel/...>`. Using `-I .` from the repository root makes almost every file fail on missing includes, since the repository root *is* `OpenKernel`.

## 2. Build blockers

These prevent any successful build today, in the order you would hit them.

### Blocker 1 - Makefile paths do not resolve inside the repository

Every `C_SRC` entry is prefixed `OpenKernel/`, but the `Makefile` already lives inside `OpenKernel/`. Running `make` produces:

```
make: *** No rule to make target 'build/OpenKernel/Drivers/Vga/vga.o', needed by 'build/os.bin'.  Stop.
```

This is correct only in the release package layout, where `Makefile` sits *next to* an `OpenKernel/` folder. See [02 - Getting Started](02-getting-started.md) section 3.

### Blocker 2 - Two `C_SRC` paths are wrong in every layout

| Listed in Makefile | Actual location |
|---|---|
| `OpenKernel/Drivers/Sound/sb16.c` | `OpenKernel/Drivers/Sound/sb16/sb16.c` |
| `OpenKernel/Drivers/Network/rtl8139/rtl8139.c` | `OpenKernel/Drivers/Network/RTL8139/rtl8139.c` |

The second one is a case difference, which fails on Linux and macOS but may pass on case-insensitive filesystems such as default Windows or macOS volumes.

### Blocker 3 - No entry point

`link.ld` declares `ENTRY(_start)`, `ASM_SRC` is empty, and the repository contains **no assembly files at all**. There is no `_start`, no `kernel_main`, no multiboot header. Even a perfect compile would fail at link with `cannot find entry symbol _start`, and GRUB would find no multiboot header to boot.

### Blocker 4 - 16 files do not compile

Grouped by cause:

**The `klog_level_t` declaration (3 files)**

In `Kernel/KernelServices/LogService/log.h`:

```c
enum kernel_log_level {
    LOG_SUCCESS, LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_UNKNOWN
} klog_level_t;          /* declares a VARIABLE, not a type */
```

This should be `typedef enum kernel_log_level klog_level_t;`, which is exactly how `SystemLib/SysCalls/Structs/structs.h` does it. The broken form breaks `log.c`, and propagates through `SystemLib/SysCalls/syscall_extern.h` to break `syscall.c` and `security.c`.

**Missing symbols and name mismatches (7 files)**

| File | Problem |
|---|---|
| `fat16.c`, `fat32.c` | Call `ata_read_sector()`; the real function is `ata_sys_disk_read()` |
| `fat12.c` | Uses `entry[j].filename`; the struct member is `file_name` |
| `pci.c` | Uses `pci_device_t`; the type is `pci_dev_t`. Also references `pci_device_count` instead of `pci_dev_count` |
| `serial.c` | Defines `serial_read_char()` twice; references `COMR_MCR` instead of `COM1_MCR` |
| `rtl8139.c` | Uses `RTL_RX_BUFFER_SIZE` and `RTL_REG_COMMAND`; the header defines `RTL_RX_BUF_SIZE` and `RTL_REG_CMD` |
| `e1000.c`, `ethernet.c` | Include `SystemLib/Mem/mem.h`; the directory is `SystemLib/Memory/` |

**Syntax errors (1 file)**

`type_conv.c` has an undeclared identifier `str1` in `itoa()` and a missing semicolon in `htoan()`.

**GCC 14 strictness (4 files)**

These are warnings on GCC 13 and earlier but errors on GCC 14:

| File | Problem |
|---|---|
| `keyboard.c` | `return;` in `to_upper()`, `kb_check()`, and `get_char()`, all of which return values |
| `mouse.c` | Implicit declaration of `sleep()` (missing include) |
| `ahci.c` | Assigns `uint8_t*` to `ahci_prdt_entry_t*` without a cast (line 148) |
| `vfs.c` / `vfs.h` | `uint8_t` and `uint32_t` unknown (no type header included); `fat32_list_dir()` called with no arguments; `fat12_list_root()` used without a declaration |

**Type definition (1 file)**

`security.c` fails on `klog_level_t` via `syscall.h`.

### Blocker 5 - Problems that appear at link time

Even after fixing compilation:

1. **`multiple definition of 'vgabuffer'`** - `Drivers/Vga/vga.c` defines it, and `Drivers/Mouse/mouse.c` line 13 has `extern uint16_t *vgabuffer = (uint16_t *)0xB8000;`, which is a definition with an initializer, not just a declaration.
2. **`undefined reference to 'task_list'` and `'current'`** - `security.c` declares them `extern`; `task.c` defines them `static`. There is no non-static definition anywhere.
3. **`undefined reference to 'ata_read_sector'`** - from `fat16.c` and `fat32.c`.
4. **`cannot find entry symbol _start`** - no boot code exists.

## 3. Feature-by-feature status

Legend: **Works** = functional in isolation once initialised. **Partial** = usable with caveats. **Stub** = cannot function as written. **Absent** = not present at all.

### Works

| Feature | Evidence |
|---|---|
| VGA text output | `vga.c`, 291 lines, compiles, full function set |
| CPU control (`cli`, `sti`, `hlt`, CPUID) | `cpu.c` compiles and is correct |
| ATA PIO single-sector read/write | `ata.c` compiles, LBA28 logic correct |
| String functions | `std.c` compiles, standard semantics |
| Port I/O helpers | `io.h`, header-only, correct inline assembly |
| Bump allocator | `mem.c` compiles; works if `heap_init()` is called first |
| RTC time reading | `time.c` compiles, CMOS access correct |
| Function registry with lockdown | `kernel_func.c` compiles, logic correct |
| Panic handler | `panic.c` compiles, works |
| Power off / reboot | `sysmng.c` compiles, QEMU-compatible ports |
| Identity-mapped paging for 4 MiB | `vmem.c` compiles, never called |
| OFS create/read/write/delete/list | `ofs.c` compiles, has the defects listed below |
| SB16 direct DAC playback | `sb16.c` compiles, no DMA or IRQ |
| KernelAPI wrappers | Both `.c` files compile, but are not in the build |

### Partial

| Feature | What is missing or wrong |
|---|---|
| Keyboard driver | Polling only, no ring buffer (declared but unused), no Shift/Alt/Ctrl, GCC 14 errors, `kbd_init()` never called |
| Mouse driver | Works when polled, but breaks the link with a duplicate `vgabuffer` symbol; no click handling, no IRQ |
| AHCI driver | Nearly complete; missing cast on line 148, IDENTIFY issued before port configuration, DMA buffers only 4-byte aligned |
| Serial driver | Logic is correct for a 16550, but does not compile (redefinition and typo) |
| Security checker | Well structured but does not compile, would not link, never called, and one check clears the screen |
| VFS | Three functions, one global file system, no paths or handles, does not compile |
| FAT12 / FAT16 / FAT32 | Read-only by design, 8.3 names only, none compile |
| Task manager | Data structures only; no context switch, no timer, no preemption |
| KernelAPI | Not in the build, scalar argument mismatch, return values discarded |
| Memory management | Bump allocator with a no-op `kfree`; `heap_init()` never called |

### Stub or placeholder

| Item | Evidence |
|---|---|
| RTL8139 driver | `nic = (rtl8139_t*)0xDEADBEEF;` then dereferences it |
| e1000 driver | MMIO base hard-coded to `0xF0000000`, no BAR discovery, no MAC or link setup |
| Ethernet layer | 23 lines of logic, source MAC hard-coded to `0xAA` repeated six times, `ethernet_init()` declared but never defined |
| `kfree()` | Entire body is `(void)ptr;` |
| `ata_sys_disk_init()` | Empty function body |
| `schedule()` / `yield()` | No assembly context switch exists anywhere in the repository |
| `qemu_io.h` | Never included by any file |
| `openkernel_drivers_desc.h`, `openkernel_systemlib_desc.h` | Invalid C at file scope (assignments outside functions), never included, contain typos |
| `openkernel_nonuniversal_api_sys_next_status()` | Declared, never defined |

### Absent entirely

- Bootloader, `_start`, multiboot header, `kernel_main`
- IDT, GDT, PIC or APIC programming, any interrupt or exception handling
- Timer interrupts, PIT, APIC timer
- Real preemptive multitasking or a context switch
- A userspace syscall gate (`int 0x80`, `sysenter`, ring transitions)
- `printf` or any varargs formatting
- Free-list memory allocation, memory reclamation, virtual-to-physical mapping API
- Page fault handling
- FAT writing or formatting
- Directory support in OFS, subdirectory reads in FAT12/FAT16
- A mount table or real VFS abstraction
- Any network transmit or receive path that reaches real hardware
- Any automated test suite or CI build job

## 4. README claims versus reality

The project README lists a feature checklist. Here is how it compares with the code:

| README claim | Verdict |
|---|---|
| VGA text mode output system | Accurate |
| Basic CPU control (CLI, HLT, etc.) | Accurate |
| Kernel service infrastructure | Partial - the code exists but `log.c` does not compile and nothing calls the services |
| Memory management | Overstated - a bump allocator with a no-op `kfree`, and `heap_init()` is never called |
| Interrupt handling (IDT) | Correctly marked as not done - no interrupt code exists |
| Keyboard driver | Overstated - polling works in principle, but there is no ring buffer, no modifier keys, and it does not compile on GCC 14 |
| File system (OFS) | Overstated - OFS exists and compiles, but has an out-of-bounds table bug and never persists metadata |
| Project structure lists a `Boot/` folder | Inaccurate - there is no `Boot/` directory in the repository |
| `make` then `make run` | Inaccurate as written - `make` fails on paths, and `make iso` needs a `Boot/grub/grub.cfg` that does not exist |

## 5. Known bugs worth fixing first

Ordered roughly by how much they block other work.

1. **`log.h` enum declaration** - one-line fix, unblocks `log.c`, `syscall.c`, and `security.c`.
2. **Makefile paths** - either strip the `OpenKernel/` prefix or document that builds must run from the package layout. Fix the `sb16` and `RTL8139` paths.
3. **Duplicate `vgabuffer`** - remove the initializer from `mouse.c` line 13.
4. **`ata_read_sector` naming** - either rename the ATA function or fix the two FAT callers.
5. **`task_list` / `current` linkage** - make them non-static in `task.c` or drop the `extern` usage in `security.c`.
6. **OFS table pointer arithmetic** - `ofs.file_table + (i * SECTOR_SIZE)` must become `(uint8_t*)ofs.file_table + (i * SECTOR_SIZE)`. This one actively corrupts memory.
7. **OFS table persistence** - write the table back after every mutation, and either enlarge the reserved area or reduce `OFS_MAX_FILES`.
8. **`kfree()` no-op** - without it the heap is a consumable resource with a hard ceiling.
9. **KernelAPI scalar argument convention** - pick one side (dereference in the handler, or pass values from the wrapper) and make it consistent.
10. **`type_conv.c` syntax errors** - two characters-level fixes.
11. **`serial.c` redefinition and typo** - unblocks a driver that is already wired into `make run` via `-serial stdio`.
12. **PCI type name mismatch** - unblocks PCI, which in turn unblocks AHCI and the network drivers.

## 6. Recommended starting point

If your goal is to get a bootable image as quickly as possible:

1. Create the package layout described in [02 - Getting Started](02-getting-started.md) section 3.
2. Write `Boot/boot.asm` with a multiboot header and `_start`, plus `Boot/grub/grub.cfg`.
3. Fix blockers 1 and 2 in the Makefile, then fix the `log.h` declaration.
4. Write a `kernel_main()` that calls `vga_init()` and prints something.
5. Build with a subset of `C_SRC` - only the files you actually need (`vga.c`, `cpu.c`, `std.c`, `mem.c`, `io.h`). Do not try to compile everything at once.
6. Add subsystems incrementally, checking the build after each one.

Trying to fix all 16 failing files before booting anything makes it hard to tell which fix caused which regression.

## 7. Confidence notes

- **Verified by running the compiler:** the 15/16 pass-fail split, the Makefile path failure, the absence of assembly files, and the absence of `_start` and `kernel_main`.
- **Verified by reading the source:** the `klog_level_t` declaration, the duplicate `vgabuffer` definition, the `static` versus `extern` task globals, the missing `ata_read_sector`, the `kfree` no-op, the `0xDEADBEEF` pointer, the OFS pointer arithmetic, and the syscall argument mismatch.
- **Not verified:** runtime behaviour on real hardware or in QEMU. Nothing in this repository was executed, because nothing in this repository can currently be built or booted.

---

[← Kernel API](08-kernel-api.md) | [Documentation Index](README.md) | [Next: Contributing →](10-contributing.md)
