# OpenKernel Documentation

OpenKernel is a 32-bit x86 operating system kernel project written in C and intended to be reused as a base layer by your own operating system. This folder contains the full documentation set for OpenKernel version 3.0.

Read the documents in the numbered order below. Every document has a "previous" and "next" link at the top and bottom so you can move through the set like pages in a book.

> **Before you start:** OpenKernel does not currently compile or boot out of the box. It is a kernel *library* that expects you to supply your own bootloader and entry point, and several source files have known defects. Read [09 - Project Status](09-project-status.md) early if your goal is to get something running.

---

## Reading order

| # | Document | What it covers |
|---|---|---|
| 0 | [This file](README.md) | Index of the whole documentation set. |
| 1 | [01 - Introduction](01-introduction.md) | What OpenKernel is, what it is not, its goals, and the technology it uses. |
| 2 | [02 - Getting Started](02-getting-started.md) | Toolchain requirements, environment setup, bootstrapping your own OS, and how to build and run. |
| 3 | [03 - Architecture](03-architecture.md) | Directory layout, the build pipeline, the linker script, and how the release package is produced. |
| 4 | [04 - Kernel Services](04-kernel-services.md) | Panic handling, logging, system power control, paging, and the security checker. |
| 5 | [05 - System Library](05-system-library.md) | Memory allocation, string functions, port I/O, time, task management, type conversion, and the internal syscall dispatcher. |
| 6 | [06 - Drivers](06-drivers.md) | VGA, CPU, keyboard, mouse, serial, sound, ATA, AHCI, PCI, and network drivers. |
| 7 | [07 - File Systems](07-file-systems.md) | OFS, FAT12, FAT16, FAT32, and the VFS dispatch layer. |
| 8 | [08 - Kernel API](08-kernel-api.md) | The stable API façade, the universal/non-universal split, and the full syscall number table. |
| 9 | [09 - Project Status](09-project-status.md) | Honest report of what works, what is partial, what is a stub, and the current build blockers. |
| 10 | [10 - Contributing](10-contributing.md) | How to contribute, coding conventions, the release process, and the license. |

---

## How the documentation is organised

The documentation is split by responsibility so you can look up only what you need:

- **Orientation** - documents 01 to 03 explain the project, the setup, and the structure. Start here if you are new.
- **Reference** - documents 04 to 08 describe each subsystem and its public functions. Use these while writing code.
- **Assessment and process** - documents 09 and 10 describe the real state of the codebase and how to work on it.

No document describes a feature that does not exist in the source tree. Where a feature is incomplete or broken, the document says so explicitly.
