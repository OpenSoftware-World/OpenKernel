# 01 - Introduction

[← Documentation Index](README.md) | [Next: Getting Started →](02-getting-started.md)

---

## What OpenKernel is

OpenKernel is a kernel-level code base for 32-bit x86 machines. It provides the low-level pieces an operating system needs: text-mode screen output, port I/O helpers, memory and string routines, time reading, hardware drivers, file system readers, and a C function registry.

It is designed to be **reused**. You do not flash OpenKernel to a disk and boot it as a finished product. Instead, you place it inside your own project, write your own bootloader and entry point, and call its functions from your own kernel code.

Current version: **4.0** (see `version.txt`).

## What OpenKernel is not

Being precise here saves you a lot of time:

- **It is not a bootable operating system.** The repository contains no assembly files, no multiboot header, no `_start` symbol, and no `kernel_main`. The `link.ld` script declares `ENTRY(_start)`, but nothing defines it. You must supply the entry point.
- **It is not a complete kernel.** There is no interrupt controller setup, no IDT, no GDT, no timer interrupt, and no privilege levels. Keyboard and mouse input are polled, not interrupt driven.
- **It is not a userspace syscall interface.** The `syscall()` function is an ordinary C function call inside the kernel. There is no `int 0x80`, no `sysenter`, and no ring transition.
- **It is not a general-purpose library for user programs.** Everything runs in kernel context.

## Goals

This project aims to create a system where people can easily write their own programs and where the code can be easily read and modified:

- Understand direct hardware interaction (CPU, memory, I/O ports).
- Keep the architecture small enough to read in one sitting.
- Keep the structure modular so individual subsystems can be replaced.
- Provide a stable C API so downstream projects survive OpenKernel refactoring.

## Technology

| Area | Choice |
|---|---|
| Languages | C, (assembly expected from you for boot code) |
| Compiler | `i686-elf-gcc` freestanding, 32-bit |
| Linker | `i686-elf-ld` with a custom `link.ld` |
| Assembler | NASM (`nasm -f elf32`) |
| Emulator | QEMU (`qemu-system-i386`), VMware |
| Bootloader | GRUB multiboot (expected, not included) |

The kernel is compiled with `-m32 -ffreestanding -O2`. Freestanding means no standard C library is available: there is no `libc`, no `printf`, no `malloc` from a library. Everything you call must be defined inside the project.

## Design principles

- **No dependencies outside the tree.** Every function the kernel uses is in the repository.
- **Module metadata headers.** Most subsystem headers carry `*_MODULE_NAME`, `*_MODULE_VER`, `*_MODULE_KRNL_VER` macros that describe the module and the minimum kernel version it targets.
- **Explicit initialisation.** Subsystems expose an `*_init()` function. Most of them silently do nothing until you call it.
- **A stable API layer.** `KernelAPI/` wraps raw syscall numbers behind named functions so your code does not break when internals change.

## Project layout at a glance

```
OpenKernel/
├── Kernel/          Kernel core services (panic, log, paging, power, security)
├── SystemLib/       Memory, strings, I/O, time, tasks, syscalls, conversion
├── Drivers/         Hardware drivers (VGA, CPU, keyboard, mouse, serial, ...)
├── KernelAPI/       Stable C API façade for downstream operating systems
├── OFS/             Open File System (simple flat file system)
├── FAT12/ FAT16/ FAT32/   FAT readers (read-only)
├── VFS/             Thin dispatch layer over the file systems
├── Makefile         Build rules
├── link.ld          Linker script
├── install_pkg.sh   Host environment setup script (Linux only)
├── notes.txt        Short bootstrapping instructions
└── version.txt      Release version
```

Details are in [03 - Architecture](03-architecture.md).

## Honest summary of the current state

OpenKernel 4.0 contains roughly 5,300 lines of C across 73 source and header files. Several subsystems work well in isolation (VGA output, CPU control, ATA PIO reads, string and port I/O helpers). Others are partial, and some are placeholders that cannot work as written.

As of the current repository state:

- The build does not succeed. See [09 - Project Status](09-project-status.md) for the exact blockers.
- 15 of 31 C files pass a compiler syntax check; 16 do not.
- There is no entry point, so nothing can be executed even if it compiled.

Read document 09 before planning any work on this code base.

---

[← Documentation Index](README.md) | [Next: Getting Started →](02-getting-started.md)
