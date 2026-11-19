# 09 - Project Status

[← Kernel API](08-kernel-api.md) | [Documentation Index](README.md) | [Next: Contributing →](10-contributing.md)

---

This document is an honest assessment of the code base as it exists in the repository today. It exists so you do not discover these problems one build failure at a time.

Everything below was verified against the source tree. Where a claim is based on static reading rather than execution, it says so.

## 1. Build verification results

Test environment: `i686-elf-gcc` 14.2.0, `nasm` 2.16.01, `i686-elf-ld` (binutils) 2.42, flags `-m32 -ffreestanding -O2 -Wall -Wextra -I .`, run over all 34 `.c` files.

| Result | Count |
|---|---:|
| Pass syntax check (`-fsyntax-only`) | 34 |
| Fail syntax check | 0 |
| Pass full compile (`-c`) | 34 |
| Files that still emit warnings | 1 |

Passing: every `.c` file in the repository.

The one file that still warns:

| File | Warning | Cause |
|---|---|---|
| `SystemLib/KernelFunc/kernel_func.c` | `'kfunc_s' defined but not used` | Dead file-scope variable. Cosmetic. |

The four `cast from pointer to integer of different size` warnings that `SystemLib/SysCalls/syscall.c` used to emit are gone: the KernelAPI argument convention was fixed (see section 5, item 4), and the dispatcher now dereferences `arg` instead of casting it.

Link result: `i686-elf-ld -m elf_i386 -T link.ld <objects>` exits **0** and produces a 53 KiB image. The only diagnostic is `warning: cannot find entry symbol _start` (see blocker below).

`make` exits **0** and produces `build/os.bin`.

Note on the include root: the tests above use `-I .` because every file includes headers as `<OpenKernel/...>`. The `Makefile` already passes `-I ..`; using `-I .` from the repository root makes almost every file fail on missing includes, since the repository root *is* `OpenKernel`.

## 2. Build status

Earlier revisions of this document listed five blockers. Four of them are fixed. What remains is listed first.

### Remaining blocker - no entry point

`link.ld` declares `ENTRY(_start)`, but the repository contains no `_start`, no `kernel_main`, and no multiboot header anywhere. `isr.asm` provides the interrupt stubs only.

Consequences, in the order you hit them:

1. Link completes with `warning: cannot find entry symbol _start; defaulting to 00100000`. The image has no defined entry point, so it cannot be booted.
2. There is no `Boot/` directory, so `make iso` fails at `cp Boot/grub/grub.cfg /boot/grub/` even if the link succeeds.
3. Nothing in the repository ever calls `vga_init()`, `pit_init()`, `pci_init()`, `heap_init()`, or `ofs_init()`. Even a bootable image would execute no initialisation.

This is the single thing standing between the repository and a runnable system.

### Fixed - Makefile paths

Every `C_SRC` entry used to be prefixed `OpenKernel/` even though the `Makefile` lives inside `OpenKernel/`, and two paths were wrong in every layout (`Drivers/Sound/sb16.c` vs the real `Drivers/Sound/sb16/sb16.c`, and `rtl8139/` vs the real `RTL8139/`). All prefixes are gone, both paths are corrected, and `-I .` was changed to `-I ..`. `make` now builds end to end.

### Fixed - files that did not compile

The earlier 11-file failure list is resolved. Grouped by cause:

| Cause | Files | Fix |
|---|---|---|
| `ata_read_sector()` does not exist | `fat16.c`, `fat32.c` | Call `ata_sys_disk_read()` |
| `pci_device_t` / `pci_device_count` name mismatches | `pci.c` | Uses `pci_dev_t` / `pci_dev_count` |
| Duplicate `serial_read_char()` definition, `COMR_MCR` typo | `serial.c` | One definition kept, typo corrected |
| `return;` in non-void functions | `keyboard.c` | Returns the proper value; a ring buffer was added at the same time |
| No type header included | `vfs.h` | Includes `Std/types.h` |
| `fat12_list_root()` never declared | `fat12.h` | Declaration added |
| `extern ... vgabuffer = ...` is a definition | `mouse.c` | Reduced to a plain `extern` declaration |

`type_conv.c` was already passing when this revision was written; the older claim about its two syntax errors was stale.

### Fixed - link-time failures

1. **`multiple definition of 'vgabuffer'`** - `mouse.c` no longer defines the symbol.
2. **`undefined reference to 'ata_read_sector'`** - gone with the rename above.
3. **`undefined reference to 'vfs_list'`, `'fat32_list_dir'`, `'fat12_list_root'`, `'get_char'`, `'pci_*'`** - all resolved; the link is clean apart from `_start`.

## 3. Feature-by-feature status

Legend: **Works** = functional in isolation once initialised. **Partial** = usable with caveats. **Stub** = cannot function as written. **Absent** = not present at all.

Nothing in this section has been executed. "Works" means "compiles clean and the logic reads as correct", not "verified at runtime".

### Works

| Feature | Evidence |
|---|---|
| VGA text output | `vga.c` compiles clean, full function set |
| CPU control (`cli`, `sti`, `hlt`, CPUID) | `cpu.c` compiles clean; `cpuinfo_data()` is now `void` |
| ATA PIO single-sector read/write | `ata.c` compiles clean; LBA28, bounded waits, ERR/DF handling |
| String functions | `std.c` compiles, standard semantics |
| Port I/O helpers | `io.h`, header-only, correct inline assembly |
| Bump allocator | `mem.c` compiles; `kmalloc()` now rejects sizes that would overflow or wrap. Still needs `heap_init()` |
| RTC time reading | `time.c` compiles, CMOS access correct |
| Function registry with lockdown | `kernel_func.c` compiles (one unused-variable warning) |
| Panic handler | `panic.c` compiles |
| Power off / reboot | `sysmng.c` compiles; the syscall wrappers now pass `"shutdown"` / `"reboot"` so the branch is reachable |
| Identity-mapped paging for 4 MiB | `vmem.c` compiles, never called |
| OFS create/read/write/delete/list | `ofs.c` compiles; pointer arithmetic, superblock I/O and table persistence fixed |
| Interrupt stubs and dispatch | `isr.asm` assembles with nasm, `interrupt.c` / `pic.c` compile |
| SB16 direct DAC playback | `sb16.c` compiles, no DMA or IRQ |
| KernelAPI wrappers | Both `.c` files compile, but are not in the build |

### Partial

| Feature | What is missing or wrong |
|---|---|
| Keyboard driver | Ring buffer now works and Caps Lock is handled, but it is polling only, has no IRQ1, no Shift/Alt/Ctrl, and `kbd_init()` is never called |
| Mouse driver | Works when polled; the duplicate `vgabuffer` that broke the link is gone. No click handling, no IRQ |
| AHCI driver | Compiles clean with structure asserts and bounded timeouts; untested on hardware. Nothing calls `ahci_init()` |
| Serial driver | Compiles clean now. 16550 logic is correct and `make run` already passes `-serial stdio`, but it has never been run |
| Security checker | Compiles clean, all seven checks real; never called, untested at runtime |
| VFS | Three functions, one global file system, no paths or handles; compiles but `boot_lba` is hard-coded to `0` |
| FAT12 / FAT16 / FAT32 | Read-only by design, 8.3 names only; all three compile, geometry is validated, reads are bounded. Never executed |
| Task manager | Data structures only; no context switch, no preemption. List walks are now bounded and head removal is correct |
| KernelAPI | Argument convention, return values and the missing `sys_next_status` wrapper are fixed (see [08 - Kernel API](08-kernel-api.md) section 5); still not in the default build |
| Memory management | Bump allocator with a no-op `kfree`; `heap_init()` is never called, so `kmalloc()` returns NULL today |
| PCI bus | Compiles, scans, decodes BARs and enables the command bits; never executed, so no BAR value has been observed |
| e1000 / RTL8139 | BAR discovery, device-ID checks and NULL guards are in place; no MAC read, no link setup, no interrupt, never executed |
| Time library | `sleep()` now delegates to `pit_sleep_ms()`; returns immediately if the PIT is not running or IF is clear |

### Stub or placeholder

| Item | Evidence |
|---|---|
| Ethernet layer | 23 lines of logic, source MAC hard-coded to `0xAA` repeated six times, `ethernet_init()` declared but never defined, transmit always goes to the head of the device list |
| `kfree()` | Entire body is `(void)ptr;` |
| `ata_sys_disk_init()` | Empty function body |
| `schedule()` / `yield()` | No context-switch assembly exists; `yield()` restarts the task function instead of resuming it |
| `qemu_io.h` | Never included by any file |
| `openkernel_drivers_desc.h`, `openkernel_systemlib_desc.h` | Invalid C at file scope (assignments outside functions), never included, contain typos |
| `heap_init()` | Defined but never called; `link.ld` already reserves a 1 MiB `.heap` section with `_heap_start` / `_heap_end` |

### Absent entirely

- Bootloader, `_start`, multiboot header, `kernel_main`, `Boot/` directory
- GDT or APIC programming (the IDT, 8259A PIC remap and exception/IRQ dispatch live in `Kernel/Components/InterruptServices/`)
- A per-CPU timer; IRQ0 is driven by the PIT, there is no APIC
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
| Kernel service infrastructure | Partial - the code compiles but nothing calls the services |
| Memory management | Overstated - a bump allocator with a no-op `kfree`, and `heap_init()` is never called |
| Interrupt handling (IDT) | Accurate - `Kernel/Components/InterruptServices/` provides the IDT, PIC remap and dispatch; no driver registers an IRQ handler yet |
| Keyboard driver | Partial - polling and a ring buffer work in principle, but there are no modifier keys and `kbd_init()` is never called |
| File system (OFS) | Partial - OFS compiles and now persists its table; it still has no directories and no buffer-length parameter on read |
| Project structure lists a `Boot/` folder | Inaccurate - there is no `Boot/` directory in the repository |
| `make` then `make run` | Partial - `make` works; `make iso` needs a `Boot/grub/grub.cfg` that does not exist, and `make run` needs that image first |

## 5. Known bugs worth fixing first

Ordered roughly by how much they block other work.

1. **No entry point** - write `Boot/boot.asm` with a multiboot header and `_start`, plus `Boot/grub/grub.cfg`. Nothing else can be validated before this exists.
2. **`heap_init()` is never called** - `link.ld` already reserves `_heap_start` / `_heap_end`. Until they are wired up, `kmalloc()` returns NULL for every request, which is now safe (the e1000 driver bails out instead of dereferencing NULL) but means no dynamic allocation works.
3. **`kfree()` no-op** - without it the heap is a consumable resource with a hard ceiling.
4. **KernelAPI scalar argument convention** - fixed. `syscall_handler()` now dereferences `arg` instead of casting it, matching the wrappers' uniform `syscall(id, &arg)` convention, and value-returning calls propagate their result. This also removed the four remaining compiler warnings in `syscall.c`. The dispatch path was executed under QEMU (see section 7).
5. **OFS `ofs_read_file()` takes no buffer length** - the sector count comes from `size_bytes` alone, so the caller must guarantee the buffer is large enough. Needs a `size` parameter to be safe.
6. **OFS on-disk layout changed** - the reserved file-table area went from 8 sectors (4,096 bytes, too small for 128 x 44-byte entries) to the derived 11 sectors (5,632 bytes). `ofs_init()` validates `data_start >= file_table_start + 11` and reformats an image that does not match, so a disk written by the previous code loses its contents on first mount. That is intentional; it is better than silently overlapping the table with the data area.
7. **`e1000_send_packet()` writes `TDT = tx_index`** - the tail descriptor register points *past* the last descriptor the driver filled in, so the NIC never transmits the packet that was just queued. Not changed in this pass; it needs a hardware test to confirm.
8. **RTL8139 receive buffer alignment** - the NIC wants the 8 KiB RX buffer on a 256-byte boundary. `rx_buffer` is a plain static array with no alignment attribute.
9. **`ethernet_init()`** - declared, never defined. Harmless today because nothing calls it, but it is a link error waiting to happen. (`openkernel_nonuniversal_api_sys_next_status()` had the same problem and is now implemented on `SYSCALL_SYS_NEXT_STATUS`, 43.)
10. **No boot path, no tests, no CI** - nothing in this repository has ever been executed.

## 6. Recommended starting point

If your goal is to get a bootable image as quickly as possible:

1. Write `Boot/boot.asm` with a multiboot header and `_start`, plus `Boot/grub/grub.cfg`. This is the only remaining link-time blocker.
2. Write a `kernel_main()` that calls `vga_init()`, `pit_init(1000)`, `sti()`, and prints something.
3. Wire `heap_init((uint32_t)_heap_start, _heap_end - _heap_start)` in `kernel_main()` before anything that allocates.
4. Call `pci_init()` then `pci_dev_init()` to exercise the new BAR discovery path.
5. Build with `make`, then `make iso` and `make run`.
6. Add subsystems incrementally, checking the build after each one.

The whole tree now compiles and links in one pass, so you no longer need to build a subset to make progress.

## 7. Confidence notes

- **Verified by running the compiler:** the 34/0 syntax split, the 34/0 object compile, the clean link, the `make` success, the one remaining warning file, and the absence of `_start` / `kernel_main` / `Boot/`.
- **Verified by execution:** the KernelAPI dispatch path was run under `qemu-system-i386` using a throwaway multiboot test image with stubbed kernel functions. All 38 checks passed: every wrapper delivered its arguments to the matching stub unchanged, return values (`kmalloc`, `memset`, `memcpy`, `memcmp`, `strcmp`, `strncmp`, `strlen`, FAT reads, `get_char`) propagated, and invalid syscall ids still return `-1`. This is the first piece of this repository that has ever been executed.
- **Verified by reading the source:** the OFS pointer arithmetic and table-persistence defects, the FAT boot-sector overruns, the `0xDEADBEEF` and `0xF0000000` constants (now replaced by BAR discovery), the `0x0E` drive-select word, the `task_sleep()` unit mismatch, the `task_remove()` head-removal defect, and the syscall argument mismatch.
- **Not verified:** runtime behaviour on real hardware or in QEMU. Nothing in this repository has been executed. Every "Works" entry above is compile-verified and logic-reviewed only. The PCI, network, ATA, FAT and OFS changes have never touched hardware, so their BAR values, timeouts and on-disk layouts are unconfirmed.

---

[← Kernel API](08-kernel-api.md) | [Documentation Index](README.md) | [Next: Contributing →](10-contributing.md)
