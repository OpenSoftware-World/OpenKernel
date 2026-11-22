# 09 - Project Status

[← Kernel API](08-kernel-api.md) | [Documentation Index](README.md) | [Next: Contributing →](10-contributing.md)

---

This document is an honest assessment of the code base as it exists in the repository today. It exists so you do not discover these problems one build failure at a time.

Everything below was verified against the source tree. Where a claim is based on static reading rather than execution, it says so.

## 1. Build verification results

Test environment: `i686-elf-gcc` 14.2.0, `nasm` 2.16.01, `i686-elf-ld` (binutils) 2.42, flags `-m32 -ffreestanding -O2 -Wall -Wextra -I ..`, run over all 37 `.c` files.

| Result | Count |
|---|---:|
| Pass syntax check (`-fsyntax-only`) | 37 |
| Fail syntax check | 0 |
| Pass full compile (`-c`) | 37 |
| Files that still emit warnings | 0 |

Passing: every `.c` file in the repository.

No file emits a warning any more. The earlier row claiming `SystemLib/KernelFunc/kernel_func.c` warns with `'kfunc_s' defined but not used` did not reproduce when re-run over this tree, so the claim is withdrawn rather than repeated.

The four `cast from pointer to integer of different size` warnings that `SystemLib/SysCalls/syscall.c` used to emit are gone: the KernelAPI argument convention was fixed (see section 5, item 5), and the dispatcher now dereferences `arg` instead of casting it.

Link result: `i686-elf-ld -m elf_i386 -T link.ld <objects>` exits **0** and produces a 66 KiB image (66,936 bytes). The only diagnostic is `warning: cannot find entry symbol _start` (see blocker below).

`make` exits **0** and produces `build/os.bin`.

Note on the include root: every file includes headers as `<OpenKernel/...>`, and the repository root *is* the `OpenKernel` directory of its parent, so the include root has to be `-I ..`. Both the `Makefile` and the verification runs above use it; `-I .` from the repository root makes almost every file fail on missing includes.

## 2. Build status

Earlier revisions of this document listed five blockers. Four of them are fixed. What remains is listed first.

### Remaining blocker - no entry point

`link.ld` declares `ENTRY(_start)`, but the repository contains no `_start`, no `kernel_main`, and no multiboot header anywhere. `isr.asm` provides the interrupt stubs only.

Consequences, in the order you hit them:

1. Link completes with `warning: cannot find entry symbol _start; defaulting to 00100000`. The image has no defined entry point, so it cannot be booted.
2. There is no `Boot/` directory, so `make iso` fails at `cp Boot/grub/grub.cfg /boot/grub/` even if the link succeeds.
3. Nothing in the repository ever calls `vga_init()`, `pit_init()`, `pci_init()`, `heap_init()`, or `ofs_init()`. Even a bootable image would execute no initialisation.

This is the single thing standing between the repository and a runnable system.

### Fixed (this revision) - Makefile paths

Two separate defects made `make` fail in every layout:

1. Every `C_SRC` entry was prefixed `OpenKernel/` while the sources actually sit at the repository root (`OpenKernel/Drivers/...` does not exist; `OpenKernel/` *is* the repository). All prefixes are now gone.
2. `CFLAGS` passed `-I .`, but headers are included as `<OpenKernel/...>`. Since the repository root is the `OpenKernel` directory of its parent, the include root has to be `-I ..`. It now is.

`make` now builds end to end from the repository root, exits 0, and produces `build/os.bin` (66 KiB). The only diagnostic left is `warning: cannot find entry symbol _start`, which is the missing entry point below.

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

Most of this section is compile-verified and logic-reviewed only. Where a row says **verified under QEMU**, it was executed in a throwaway multiboot test image built against this tree (see section 7); everything else has not been run.

### Works

| Feature | Evidence |
|---|---|
| VGA text output | `vga.c` compiles clean, full function set |
| CPU control (`cli`, `sti`, `hlt`, CPUID) | `cpu.c` compiles clean; `cpuinfo_data()` returns `uint32_t` |
| ATA PIO single-sector read/write | `ata.c` compiles clean; LBA28, bounded waits, ERR/DF handling |
| String functions | `std.c` compiles, standard semantics |
| Port I/O helpers | `io.h`, header-only, correct inline assembly |
| Heap allocator | `mem.c` compiles; first-fit, address-ordered free list with coalescing `kfree()`. **Verified under QEMU**: block reuse after free, neighbour merge, double-free and `NULL` ignored. Still needs `heap_init()` called by a boot path |
| RTC time reading | `time.c` compiles, CMOS access correct |
| Function registry with lockdown | `kernel_func.c` compiles clean |
| Power off / reboot | `sysmng.c` compiles; the syscall wrappers now pass `"shutdown"` / `"reboot"` so the branch is reachable |
| Identity-mapped paging for 4 MiB | `vmem.c` compiles, never called |
| OFS create/read/write/delete/list | `ofs.c` compiles; pointer arithmetic, superblock I/O and table persistence fixed |
| Interrupt stubs and dispatch | `isr.asm` assembles with nasm, `interrupt.c` / `pic.c` compile. **Verified under QEMU**: IDT load, PIC remap, `#DE` and real `#GP` frames, PIT tick/EOI, and software-int dispatch all correct |
| Panic handler | `panic.c` compiles. **Verified under QEMU**: `cli()` added this revision, a panic from task context now really freezes the system (no further timer-driven activity) |
| SB16 direct DAC playback | `sb16.c` compiles, no DMA or IRQ |
| KernelAPI wrappers | Both `.c` files compile, but are not in the build |

### Partial

| Feature | What is missing or wrong |
|---|---|
| Keyboard driver | IRQ1 driven now (ring buffer written by the handler, `get_char()` sleeps in `hlt()`, and returns 0 instead of hanging when IF is clear - **verified under QEMU**), but still no Shift/Alt/Ctrl and `kbd_init()` is never called in the repository |
| Mouse driver | IRQ12 driven now; the duplicate `vgabuffer` that broke the link is gone. No click handling |
| AHCI driver | Compiles clean with structure asserts and bounded timeouts; untested on hardware. Nothing calls `ahci_init()`, and it stays polled on purpose (see [06 - Drivers](06-drivers.md) section 8) |
| Serial driver | Compiles clean, IRQ4 driven with TX/RX rings and a polled fallback. 16550 logic is correct and `make run` already passes `-serial stdio`, but it has never been run |
| Security checker | Compiles clean, all seven checks real; never called, untested at runtime |
| VFS | Five functions (`init`, `list`, `read`, `create`/`write`/`delete`), one global file system, no paths or handles; compiles but `boot_lba` is hard-coded to `0` |
| FAT12 / FAT16 / FAT32 | Read **and** write of root-directory files through the shared `FATLib/fatlib.c` core, 8.3 names only; all four files compile, geometry is validated, transfers are bounded. Never executed |
| Task manager | Round-robin preemptive scheduler with a real `context_switch()`, `yield()`, `sleep()`, `block()`/`unblock()` and `task_exit()`; verified under QEMU (see section 7). Head removal was defective and is now fixed. |
| KernelAPI | Argument convention, return values and the missing `sys_next_status` wrapper are fixed (see [08 - Kernel API](08-kernel-api.md) section 5); still not in the default build |
| Memory management | Free-list allocator (first-fit, address-ordered, coalescing) with a real `kfree()` - **verified under QEMU** (section 5). `heap_init()` is still never called in the repository, so `kmalloc()` returns NULL until a boot path wires it up |
| PCI bus | Compiles, scans, decodes BARs and enables the command bits; never executed, so no BAR value has been observed |
| e1000 / RTL8139 | BAR discovery, device-ID checks, NULL guards and an armed PCI interrupt (IMS/IMR + handler) are in place; no MAC read, no link setup, never executed |
| Time library | `sleep()` now delegates to `pit_sleep_ms()`; returns immediately if the PIT is not running or IF is clear |

### Stub or placeholder

| Item | Evidence |
|---|---|
| Ethernet layer | 23 lines of logic, source MAC hard-coded to `0xAA` repeated six times, `ethernet_init()` declared but never defined, transmit always goes to the head of the device list |
| `ata_sys_disk_init()` | Was an empty body; now hooks IRQ14 and clears nIEN, but nothing in the repository calls it yet |
| `schedule()` / `yield()` | `context_switch()` assembly exists and is exercised at runtime; `task_create()` builds the initial frame and `task_start()` installs the boot context as the idle task. |
| `qemu_io.h` | Never included by any file |
| `openkernel_drivers_desc.h`, `openkernel_systemlib_desc.h` | Invalid C at file scope (assignments outside functions), never included, contain typos |
| `heap_init()` | Defined but never called; `link.ld` already reserves a 1 MiB `.heap` section with `_heap_start` / `_heap_end` |

### Absent entirely

- Bootloader, `_start`, multiboot header, `kernel_main`, `Boot/` directory
- APIC programming (the flat GDT, IDT, 8259A PIC remap and exception/IRQ dispatch all live in `Kernel/Components/InterruptServices/`)
- A per-CPU timer; IRQ0 is driven by the PIT, there is no APIC
- A userspace syscall gate (`int 0x80`, `sysenter`, ring transitions)
- `printf` or any varargs formatting
- Virtual-to-physical mapping API (free-list allocation itself now exists - see section 5)
- Page fault handling
- Directory support in OFS, subdirectory reads in FAT12/FAT16, and creating subdirectories through the FAT drivers
- FAT formatting - the FAT drivers mount and write an existing volume, but nothing in the project writes a boot sector
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
| Memory management | Partial - a free-list allocator with a working `kfree()` now (verified under QEMU), but `heap_init()` is never called in the repository |
| Interrupt handling (IDT) | Accurate - `Kernel/Components/InterruptServices/` provides the IDT, PIC remap and dispatch; the handler registry is chained, and PIT, Keyboard, Serial, ATA, Mouse and both network drivers register a handler |
| Keyboard driver | Partial - IRQ1 driven with a ring buffer, but there are no modifier keys and `kbd_init()` is never called |
| File system (OFS) | Partial - OFS compiles and now persists its table; it still has no directories and no buffer-length parameter on read |
| Project structure lists a `Boot/` folder | Inaccurate - there is no `Boot/` directory in the repository |
| `make` then `make run` | Partial - `make` works; `make iso` needs a `Boot/grub/grub.cfg` that does not exist, and `make run` needs that image first |

## 5. Known bugs worth fixing first

Ordered roughly by how much they block other work.

1. **Software `int` to an error-code vector misaligns the frame** - fixed. `isr.asm` assumes the CPU pushed an error code for vectors 8, 10-14, 17, 21, 29 and 30, but a software `INT n` never pushes one, so the frame sat one dword low: `add esp, 8` in `isr_common` dropped a live slot, `iret` loaded a garbage CS and the CPU raised `#GP` at the `iret` itself, forever (**verified**: 83,439 log lines of identical dumps in 8 seconds under QEMU, all with `eip` on the `iret`). `interrupt_dispatch()` now detects the short frame before anyone reads it - a genuine CS is always 0x08 or 0x18, while that slot holds EFLAGS, which can never be either (EFLAGS bit 1 is reserved and always reads 1) - and shifts the slots up one dword, which repairs the frame for the handler *and* the `iret` path. **Verified under QEMU after the fix**: `int $0x0D` delivers exactly one repaired frame (`error_code=0`, `cs=8`, `eflags=0x246`) and returns to the next instruction - the log's last test line is 42 instead of 83,439 (114 lines total including the idle heartbeats). Real exceptions still arrive with their genuine error code (`#DE` and the real `#GP` test both keep passing).
2. **No entry point** - write `Boot/boot.asm` with a multiboot header and `_start`, plus `Boot/grub/grub.cfg`. Nothing else can be validated before this exists.
3. **`heap_init()` is never called** - `link.ld` already reserves `_heap_start` / `_heap_end`. Until they are wired up, `kmalloc()` returns NULL for every request, which is now safe (the e1000 driver bails out instead of dereferencing NULL) but means no dynamic allocation works.
4. **`kfree()` no-op** - fixed: the bump pointer was replaced by a first-fit, address-ordered free list; `kfree()` merges a block with both neighbours, and double-free, `NULL` and out-of-range pointers are ignored instead of corrupting the list. **Verified under QEMU** (evidence in the table below).
5. **KernelAPI scalar argument convention** - fixed. `syscall_handler()` now dereferences `arg` instead of casting it, matching the wrappers' uniform `syscall(id, &arg)` convention, and value-returning calls propagate their result. This also removed the four remaining compiler warnings in `syscall.c`. The dispatch path was executed under QEMU (see section 7).
6. **OFS `ofs_read_file()` takes no buffer length** - the sector count comes from `size_bytes` alone, so the caller must guarantee the buffer is large enough. Needs a `size` parameter to be safe.
7. **OFS on-disk layout changed** - the reserved file-table area went from 8 sectors (4,096 bytes, too small for 128 x 44-byte entries) to the derived 11 sectors (5,632 bytes). `ofs_init()` validates `data_start >= file_table_start + 11` and reformats an image that does not match, so a disk written by the previous code loses its contents on first mount. That is intentional; it is better than silently overlapping the table with the data area.
8. **`e1000_send_packet()` writes `TDT = tx_index`** - the tail descriptor register points *past* the last descriptor the driver filled in, so the NIC never transmits the packet that was just queued. Not changed in this pass; it needs a hardware test to confirm.
9. **RTL8139 receive buffer alignment** - the NIC wants the 8 KiB RX buffer on a 256-byte boundary. `rx_buffer` is a plain static array with no alignment attribute.
10. **`ethernet_init()`** - declared, never defined. Harmless today because nothing calls it, but it is a link error waiting to happen. (`openkernel_nonuniversal_api_sys_next_status()` had the same problem and is now implemented on `SYSCALL_SYS_NEXT_STATUS`, 43.)
11. **No GDT** - fixed: `Kernel/Components/InterruptServices/gdt.c` installs a flat five-descriptor GDT (null, ring-0 code/data, ring-3 code/data) and `interrupt_init()` loads it *before* the IDT, so the hard-coded `0x08`/`0x10` selectors now describe this kernel's own table instead of whatever the bootloader left behind. **Verified under QEMU**: `sgdt` reports the kernel's table (limit 39) before *and* after a real `#GP`, with exceptions and IRQs running on top of it.
12. **No boot path, no in-repo tests, no CI** - there is still no `_start` and no test suite in the repository (see section 7 for the external harness used to verify this revision).

### Fixed in this revision

All eight were verified under QEMU after the change:

| Bug | Evidence it is gone |
|---|---|
| `task_remove()` left a removed **head** node reachable through the circle, so the scheduler kept running a destroyed task | `removed task reachable` went from `1` to `0`, and the removed task's run counter went from `1` to `0`; removing a non-head node still works (control case) |
| `kernel_panic()` did not `cli()`, so the system kept serving IRQs and switching tasks after a panic | timer-driven output after the panic call went from 112 lines to **0** |
| `interrupt_init()` wiped every handler registered before it, silently leaving an unmasked IRQ with nobody listening | the pre-init registration now runs instead of being dropped |
| `TASK_MIN_STACK_SIZE` was 64 bytes, below the ~64-byte interrupt frame alone | raised to 1024 |
| A software `int` to an error-code vector looped forever in `#GP` at the `iret` | the log went from **83,439** lines of repeated dumps to a run whose last test line is **42** (114 with idle heartbeats), ending on a correctly repaired frame (`error_code=0`, `cs=8`) with control returned to the instruction after the `int` |
| `kfree()` did nothing, so the heap was a consumable resource | freed block is handed out again by the next `kmalloc()`; two freed neighbours satisfy one 512-byte request neither fits alone; double-free and `NULL` are ignored and the next allocation still succeeds |
| No GDT of our own - the IDT's hard-coded selectors only worked while the bootloader's table described the same segments | `sgdt` now reports the kernel's own table (limit 39, base inside `gdt.c`'s static array) with exceptions and IRQs running on top of it |
| `get_char()` spun forever when called with interrupts off and an empty buffer | returns **0** immediately instead of hanging the caller |

Also fixed: the Makefile path and `-I` defects (section 2), the missing error-code stubs for vectors 21, 29 and 30 (`isr21` now pushes only the vector, verified by disassembly), the stale comment in `pit_sleep_ms()`, and the stale rows in sections 1, 3 and 4 of this document.

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

- **Verified by running the compiler:** the 37/0 syntax split, the 37/0 object compile, zero files emitting warnings, the clean link, the `make` success, and the absence of `_start` / `kernel_main` / `Boot/`.
- **Verified by execution (QEMU, this revision):** a throwaway multiboot image linked against this tree (its own `_start`, reporting through port 0xE9 so the reporter never depends on the code under test) exercised the interrupt and scheduler components directly. Passing: IDT load (`sidt`), the kernel's own GDT (`sgdt` limit 39), software-int dispatch before *and* after `interrupt_init()`, PIT tick and EOI continuity (100 ms wait → 100 ms measured; 300 ms sleep → exactly 300 ms), `#DE` and real `#GP` frames (faulting `eip` and `error_code` both correct), the heap allocator (block reuse, neighbour coalescing, double-free and `NULL` ignored), `get_char()` with interrupts off, `yield()`/`task_sleep()` round-robin, and serial TX. Failing before the fixes and passing after: `task_remove()` of the head node, `kernel_panic()` freezing the system, the pre-init handler registration, `int` to an error-code vector (83,439 dump lines → last test line 42, frame repaired and control returned), and the `get_char()` hang - each with its evidence in section 5.
- **Verified by execution (earlier):** the KernelAPI dispatch path was run under `qemu-system-i386` using a throwaway multiboot test image with stubbed kernel functions. All 38 checks passed: every wrapper delivered its arguments to the matching stub unchanged, return values (`kmalloc`, `memset`, `memcpy`, `memcmp`, `strcmp`, `strncmp`, `strlen`, FAT reads, `get_char`) propagated, and invalid syscall ids still return `-1`.
- **Verified by reading the source:** the OFS pointer arithmetic and table-persistence defects, the FAT boot-sector overruns, the `0xDEADBEEF` and `0xF0000000` constants (now replaced by BAR discovery), the `0x0E` drive-select word, the `task_sleep()` unit mismatch, and the syscall argument mismatch.
- **Not verified:** runtime behaviour on real hardware. The PCI, network, ATA, FAT and OFS changes have never touched hardware, so their BAR values, timeouts and on-disk layouts are unconfirmed. The QEMU harness lives outside this repository, so nothing here is repeatable by `make test` yet.

---

[← Kernel API](08-kernel-api.md) | [Documentation Index](README.md) | [Next: Contributing →](10-contributing.md)
