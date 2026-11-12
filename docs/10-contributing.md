# 10 - Contributing

[← Project Status](09-project-status.md) | [Documentation Index](README.md)

---

This document covers how to work on OpenKernel itself: the contribution workflow, coding conventions, testing expectations, the release process, and the license.

## 1. Before you contribute

Read [09 - Project Status](09-project-status.md) first. The repository does not currently build, so a contribution that "adds a feature" without a working build is impossible to verify. Fixing the build blockers is the highest-value work available right now.

Suggested order of impact:

1. Fix the `log.h` enum declaration (unblocks three files at once).
2. Fix the Makefile paths.
3. Fix the duplicate `vgabuffer` definition.
4. Fix the `ata_read_sector` naming mismatch.
5. Add a minimal boot path (`boot.asm` plus `grub.cfg`) so the project can actually be run and tested.

## 2. Contribution workflow

1. Fork the repository.
2. Create a feature branch from `main`.
3. Make your changes.
4. Verify them (see section 4).
5. Open a pull request describing what changed and why.

Bug reports and feature requests use the templates in `.github/ISSUE_TEMPLATE/`:

- `bug-report.md` - describe the bug, reproduction steps, expected behaviour, and your environment.
- `feature_request.md` - describe the problem and the proposed solution.

When reporting a build failure, include your compiler version (`i686-elf-gcc --version`) and the exact command you ran. The same code behaves differently on GCC 13 and GCC 14, as described in [09 - Project Status](09-project-status.md).

## 3. Coding conventions

Follow the conventions already present in the tree so the code base stays consistent.

### Language and build mode

- C compiled with `-m32 -ffreestanding -O2 -Wall -Wextra`.
- No standard library. There is no `libc`, no `printf`, no `malloc` from a library. Every function you need must be defined in the tree.
- Target 32-bit x86 only. Do not introduce 64-bit assumptions.

### Naming

| Kind | Pattern | Example |
|---|---|---|
| Public function | `subsystem_action` or `subsystem_verb_noun` | `vga_print_scr_nw`, `ata_sys_disk_read` |
| Initialiser | `*_init()` | `vga_init`, `heap_init`, `ofs_init` |
| Type | `*_t` suffix | `task_t`, `ofs_file_t`, `pci_dev_t` |
| Macro constant | `UPPER_SNAKE_CASE` | `OFS_MAX_FILES`, `SB16_BASE` |
| Module metadata | `<MODULE>_MODULE_*` | `MEM_MODULE_VER`, `KRNL_VER` |

### Header style

Every header needs an include guard, and should follow this shape:

```c
#ifndef SOME_MODULE_H
#define SOME_MODULE_H

#include <OpenKernel/SystemLib/Std/types.h>

/* public declarations */

#define SOME_MODULE_NAME    "..."
#define SOME_MODULE_VER     "1.0"
#define SOME_MODULE_KRNL_VER "3.0"

#endif
```

### Comments

- Write comments in **English**.
- Place an explanatory comment at the top of any non-obvious function.
- Comment the *why*, not the *what*. The code should already say what it does.

```c
/* Reads one 512-byte sector using PIO mode; blocks until DRQ is set. */
static void ata_wait_drq(void) { ... }
```

### Design rules

- Keep one responsibility per file. Do not add a new file for a trivial helper that belongs in an existing module.
- Prefer explicit initialisation: expose an `*_init()` function rather than initialising lazily inside other calls.
- Reuse existing helpers. If you need port I/O, use `SystemIO/io.h`. If you need a string function, extend `SystemLib/Std/std.c` instead of writing a local copy. Several files currently duplicate `itoa`-style helpers; consolidating them would be a welcome change.
- Avoid unnecessary abstraction layers. This project values readability over generality.
- Keep the module metadata macros up to date when you change a module's version.

### Do not

- Do not add a standard library dependency.
- Do not add a new build system. `make` and `link.ld` stay.
- Do not commit build output. `build/`, `iso/`, `*.o`, and `*.iso` are already covered by `.gitignore`.
- Do not commit secrets, keys, or credentials. `.gitignore` already excludes `secrets/`, `credentials/`, and `keys/`.
- Do not change `version.txt` except as part of a release (see section 6).

## 4. Testing

**There is no automated test infrastructure in this repository.** There is no test directory, no test runner, and no CI job that compiles the code. Saying "all tests pass" is not possible today.

What you should do instead:

### Compile check

Run every source file through a syntax check and report the result:

```bash
for f in $(find . -name '*.c' -not -path './.git/*'); do
    if i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -I .. -fsyntax-only "$f"; then
        echo "PASS $f"
    else
        echo "FAIL $f"
    fi
done
```

Use `-I ..` when running from inside the repository, because headers are included as `<OpenKernel/...>`. Use `-I .` when running from the package layout.

Treat new warnings as failures. The project already carries `-Wall -Wextra`.

### Link check

Once the Makefile paths are corrected, a full `make` is the real gate. Report the first error you hit rather than fixing ten things at once.

### Runtime check

If you have a bootable image:

```bash
make run
```

QEMU is started with 512 MiB RAM, 4 virtual CPUs, an IDE disk at `disk.img`, an SB16 device, and serial output on stdio. Capture the serial output when reporting a runtime problem.

### What a good report looks like

- Compiler version and host distribution.
- The exact command and its full output.
- Which files you changed.
- Whether the change was verified by compile, by link, by boot, or not at all.

Being explicit about the last point matters more than anything else in a project with no test suite.

## 5. Continuous integration

Two workflows run on GitHub:

| Workflow | Trigger | What it does |
|---|---|---|
| `release.yml` | Push to `main` that changes `version.txt` | Packages the release ZIP and publishes it |
| `codeql.yml` | Every push and pull request, plus weekly | CodeQL security analysis for C with `build-mode: none` |

Important: **neither workflow compiles the code.** CodeQL runs without a build, so it cannot catch compile errors. The release workflow only packages files.

A valuable contribution would be a third workflow that runs the compile check from section 4 on every pull request. That would have caught all 16 failing files automatically.

## 6. Release process

Releases are driven entirely by `version.txt`.

1. Update `version.txt` with the new version string (currently `3.0`).
2. Push to `main`.

The workflow then:

1. Reads the version.
2. Copies the tree into `package/OpenKernel/`, excluding `.git`, `.github`, `version.txt`, `install_pkg.sh`, `link.ld`, `Makefile`, `notes.txt`, and `package`.
3. Places `Makefile`, `link.ld`, `notes.txt`, and `install_pkg.sh` alongside the `OpenKernel/` folder.
4. Produces `OpenKernel_<version>.zip`.
5. Generates `CHANGELOG.md` from the git log since the previous version bump.
6. Creates a release tagged `openkernel<version>`.

Because the packaging step excludes `Makefile` and `link.ld` from the inner folder and re-parents them, the resulting ZIP has the layout that `C_SRC` expects. See [02 - Getting Started](02-getting-started.md) section 3.

Two related maintenance tasks are worth doing at release time:

- Update `KRNL_LAST_COMMIT` in `Kernel/openkernel_information.h` and `KERNEL_API_VER` in `KernelAPI/Information/openkernel_api_info.h`. Both are currently stale.
- Update `KRNL_VER` in `Kernel/openkernel_information.h` if the version changed.

## 7. Documentation

If you change user-visible behaviour, update the relevant file in `docs/`:

| Change | Update |
|---|---|
| New subsystem or public function | The relevant reference document (04 to 08) |
| Build process or requirements | `02-getting-started.md` |
| Directory layout or linker script | `03-architecture.md` |
| Status of a feature | `09-project-status.md` |
| New documentation file | The reading-order table in `docs/README.md`, plus prev/next links in the adjacent documents |

Documentation rules used in this set:

- One topic per file, in `docs/`.
- A main index at `docs/README.md` explaining what each file covers.
- Previous and next links at the top and bottom of every document.
- English, plain language, minimal emoji.
- Never document a feature that does not exist. If something is broken, say so.

## 8. License

OpenKernel is licensed under the **GNU General Public License, Version 2**. See the `LICENSE` file in the repository root.

In practical terms:

- You may use, modify, and distribute OpenKernel.
- Derivative works must also be distributed under GPL-2.0.
- There is no warranty. The project is intended for educational and development use.

If you contribute, your contribution is made under the same license.

## 9. Disclaimer

This project is intended for educational and development purposes. The user assumes all responsibility for any potential issues or damages caused by using this software.

Do not run experimental kernels on a machine containing data you care about without a backup. Kernel bugs can corrupt disks, and several subsystems in the current code base (notably OFS metadata handling) write to unintended memory locations.

---

[← Project Status](09-project-status.md) | [Documentation Index](README.md)
