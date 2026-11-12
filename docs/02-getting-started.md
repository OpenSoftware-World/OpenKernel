# 02 - Getting Started

[← Introduction](01-introduction.md) | [Documentation Index](README.md) | [Next: Architecture →](03-architecture.md)

---

This document explains what you need installed, how to set up a Linux host, and the exact steps to turn OpenKernel into a bootable image of *your own* operating system.

## 1. Requirements

### Required tools

| Tool | Why it is needed |
|---|---|
| `i686-elf-gcc` | Cross compiler for 32-bit freestanding x86. Produces the kernel objects. |
| `i686-elf-ld` | Linker that applies `link.ld`. Usually installed with the cross compiler. |
| `nasm` | Assembles your boot code into `elf32` objects. |
| `make` | Drives the build. |
| `qemu-system-i386` | Emulator used by `make run`. |
| `grub-mkrescue` | Builds the bootable ISO. |
| `xorriso` | Used by `grub-mkrescue` under the hood. |
| `grub-pc-bin` | Provides the 32-bit GRUB modules for `grub-mkrescue`. |

### Optional tools

| Tool | Why |
|---|---|
| `git` | Version control and the release changelog. |
| PulseAudio (`pulseaudio`) | The `make run` command attaches a Sound Blaster 16 device through `-audiodev pa`. Without a working audio backend QEMU may warn or fail on that device. |

### Platform

`install_pkg.sh` supports **Linux only** (apt, dnf, pacman). On Windows or macOS you must install the tools manually.

## 2. Setting up the host environment

```bash
chmod +x install_pkg.sh
sudo ./install_pkg.sh
```

The script does the following:

1. Detects the package manager (`apt`, `dnf`, or `pacman`).
2. Updates the system.
3. Installs the development packages: `nasm`, `gcc`, `make`, `qemu-system-x86`, `grub`, `xorriso`, `binutils`, `git`.
4. Installs `pulseaudio`.

**Important limitation:** the script installs the *host* GCC, not the cross compiler. You still have to build or obtain `i686-elf-gcc` yourself, as stated in `notes.txt`. Without it, `make` fails immediately with `i686-elf-gcc: command not found`.

## 3. Understanding the two directory layouts

This is the single most confusing part of the project, so read it carefully.

### Layout A - the git repository (what you cloned)

```
OpenKernel/          <- repository root, Makefile lives here
├── Makefile
├── link.ld
├── notes.txt
├── install_pkg.sh
├── version.txt
├── Kernel/
├── Drivers/
├── SystemLib/
├── ...
```

Inside this layout, every path in the `C_SRC` variable of the `Makefile` starts with `OpenKernel/`, for example `OpenKernel/Drivers/Vga/vga.c`. Since the Makefile is already *inside* the `OpenKernel` folder, that path does not resolve. Running `make` here fails with:

```
make: *** No rule to make target 'build/OpenKernel/Drivers/Vga/vga.o', needed by 'build/os.bin'.  Stop.
```

### Layout B - the release package (what the project expects)

```
project/                     <- you run make here
├── Makefile
├── link.ld
├── notes.txt
├── install_pkg.sh
└── OpenKernel/              <- the kernel source tree
    ├── Kernel/
    ├── Drivers/
    ├── SystemLib/
    └── ...
```

In this layout `OpenKernel/Drivers/Vga/vga.c` resolves correctly, and `-I .` makes the `#include <OpenKernel/...>` style work.

**How to get Layout B:** download a release ZIP from the GitHub Releases page, or build it yourself the way `.github/workflows/release.yml` does:

```bash
mkdir -p package/OpenKernel
rsync -av --exclude='.git' --exclude='.github' --exclude='version.txt' \
     --exclude='package' --exclude='install_pkg.sh' --exclude='link.ld' \
     --exclude='Makefile' --exclude='notes.txt' \
     ./ package/OpenKernel/
cp Makefile link.ld notes.txt install_pkg.sh package/
```

Then run `make` from `package/`.

> Even in Layout B the build does not fully succeed today. Two `C_SRC` paths are wrong regardless of layout, and 11 of 31 C files have compile errors. See [09 - Project Status](09-project-status.md).

## 4. Bootstrapping your own operating system

`notes.txt` describes the intended workflow. The essential idea: **OpenKernel provides no boot code; you write it.**

### Step 1 - Write a bootloader or a multiboot header

Two options:

- Write a raw bootloader in assembly, following the `ENTRY(_start)` symbol declared in `link.ld`.
- Write a GRUB multiboot header so GRUB loads the kernel directly.

A minimal multiboot entry point looks like this (this is a pattern, not code taken from this repository - the repository contains no assembly):

```nasm
; boot.asm
MBALIGN  equ 1 << 0
MEMINFO  equ 1 << 1
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom: resb 16384
stack_top:

section .text
global _start
extern kernel_main
_start:
    mov esp, stack_top
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
```

### Step 2 - Register the assembly file

Add your file to the `ASM_SRC` variable in the `Makefile`:

```make
ASM_SRC = Boot/boot.asm
```

The Makefile assembles it with `nasm -f elf32` into `build/Boot/boot.o`.

### Step 3 - Write your entry point

Implement `kernel_main()` in C. This is where you initialise subsystems, because **nothing inside OpenKernel initialises itself**:

```c
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Keyboard/keyboard.h>
#include <OpenKernel/VFS/vfs.h>
#include <OpenKernel/OFS/ofs.h>

void kernel_main(void)
{
    vga_init();                                  /* required before any print */
    heap_init(0x100000, 1024 * 1024);            /* 1 MiB heap */
    kbd_init();
    vfs_init(OFS);
    ofs_init();

    vga_set_color_scheme(0x00, 0x0A);
    vga_print_scr_nw("My OS booted on OpenKernel 4.0");
}
```

Calling order matters. For example, every VGA print function returns immediately if `vga_init()` has not been called, and `kmalloc()` hands out invalid addresses if `heap_init()` has not been called.

### Step 4 - Add your own sources

Append your `.c` files to `C_SRC` in the `Makefile`. Keep OpenKernel's own entries as they are.

`notes.txt` asks you not to modify the OpenKernel folder, so that you can drop in a newer release without merging conflicts.

### Step 5 - Provide a GRUB configuration

The `iso` target copies `Boot/grub/grub.cfg` into the ISO. That file is not in the repository, so you must create it:

```
set timeout=0
set default=0

menuentry "My OS" {
    multiboot /boot/os.bin
    boot
}
```

### Step 6 - Build and run

```bash
make        # compile and link -> build/os.bin
make iso    # build ISO -> your_os.iso, create disk.img if missing
make run    # boot it in QEMU
```

`make run` executes:

```bash
qemu-system-i386 \
    -m 512M \
    -smp 4 \
    -cdrom your_os.iso \
    -drive file=disk.img,format=raw,if=ide \
    -audiodev pa,id=snd0 \
    -device sb16,audiodev=snd0 \
    -boot d \
    -serial stdio
```

What each flag gives you:

| Flag | Effect |
|---|---|
| `-m 512M` | 512 MiB of RAM. |
| `-smp 4` | 4 virtual CPUs. OpenKernel has no SMP support, so this only affects timing-sensitive busy loops. |
| `-cdrom your_os.iso` | Boots your ISO. |
| `-drive file=disk.img,...,if=ide` | Attaches a 25 MiB raw IDE disk. ATA and OFS operate on this. |
| `-audiodev pa` / `-device sb16` | Sound Blaster 16 on port `0x220`. |
| `-boot d` | Boot from CD first. |
| `-serial stdio` | Routes COM1 output to your terminal. Useful once the serial driver compiles. |

### Cleaning up

```bash
make clean
```

Removes `build/`, `iso/`, `your_os.iso`, and leaves `disk.img` alone.

## 5. Working with the disk image

`make iso` creates `disk.img` (25 MiB) if it does not exist. To inspect or populate it from the host:

```bash
# list partitions
fdisk -l disk.img

# read the first 64 sectors
xxd -l 32768 disk.img
```

There is no FAT formatter in this project. If you want to test the FAT readers you must create a FAT filesystem on the image with host tools such as `mkfs.fat` and mount it with `mtools` or a loop device.

---

[← Introduction](01-introduction.md) | [Documentation Index](README.md) | [Next: Architecture →](03-architecture.md)
