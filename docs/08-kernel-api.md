# 08 - Kernel API

[← File Systems](07-file-systems.md) | [Documentation Index](README.md) | [Next: Project Status →](09-project-status.md)

---

`KernelAPI/` is the layer you are meant to call from your own operating system code. It wraps raw syscall numbers behind named C functions so that OpenKernel internals can change without breaking your project.

## 1. Design intent

Instead of writing this:

```c
syscall(4, &color);                    /* what does 4 mean again? */
```

you write this:

```c
openkernel_universal_api_vga_set_bg_color(0x02);
```

The API layer gives you:

- **Named, self-documenting functions** instead of magic numbers.
- **A stability boundary.** The header comment states the API is designed to be compatible with OpenKernel 3.0 and later, so you can upgrade the kernel without rewriting your calls.
- **A split between guaranteed and optional features**, described next.

## 2. The universal / non-universal split

### Universal API - `KernelAPI/OpenKernelUniversalAPI/`

Functions that every OpenKernel-based operating system can rely on. They only need the VGA driver and basic CPU support, which are always present.

```c
void openkernel_universal_api_vga_set_bg_color(uint8_t color);
void openkernel_universal_api_vga_set_text_color(uint8_t color);
void openkernel_universal_api_vga_newline(void);
void openkernel_universal_api_vga_set_cursor(uint8_t x, uint8_t y);
void openkernel_universal_api_ptchar(char c);
void openkernel_universal_api_vga_print_scr_nw(const char *str);
void openkernel_universal_api_vga_set_color_scheme(uint8_t bg_color, uint8_t text_color);
void openkernel_universal_api_get_char(void);
void openkernel_universal_api_sleep(uint32_t seconds);
```

Nine functions, syscall numbers 4 to 12.

### Non-universal API - `KernelAPI/nonuniversal_api_defs.h`

Functions that depend on which subsystems you compiled in: the file systems, the sound driver, the CPU query, logging, power control, and the memory and string helpers. If you do not link the FAT drivers, for example, the FAT calls are unavailable.

```c
/* console extras */
void openkernel_nonuniversal_api_vga_clear_screen(void);
void openkernel_nonuniversal_api_vga_print_hex(uint32_t n);
void openkernel_nonuniversal_api_vga_print_dec(int num);
void openkernel_nonuniversal_api_vga_print_bin(unsigned int num);

/* audio, cpu, logging, power */
void openkernel_nonuniversal_api_sb16_gen_flat_sound(uint8_t level);
void openkernel_nonuniversal_api_sb16_test(void);
void openkernel_nonuniversal_api_get_sys_cpuinfo(const char *mode);
void openkernel_nonuniversal_api_log_message(klog_level_t level, const char *message);
void openkernel_nonuniversal_api_exit_program(const char *mode, const char *message);
void openkernel_nonuniversal_api_sys_next_status(const char *mode, uint32_t seconds);

/* file systems */
int  openkernel_nonuniversal_api_fat12_read_file(const char* file_name, void* buf, uint32_t size);
void openkernel_nonuniversal_api_fat16_list_root(void);
int  openkernel_nonuniversal_api_fat16_read_file(const char* filename, void* buffer, uint32_t size);
int  openkernel_nonuniversal_api_fat32_read_file(const char* filename, void* buffer, uint32_t size);
void openkernel_nonuniversal_api_fat32_list_dir(uint32_t cluster);
void openkernel_nonuniversal_api_ofs_create_file(const char* name, uint32_t size);
void openkernel_nonuniversal_api_ofs_read_file(const char* name, uint8_t* buffer);
void openkernel_nonuniversal_api_ofs_write_file(const char* name, const uint8_t* data, uint32_t size);
void openkernel_nonuniversal_api_ofs_delete_file(const char* name);
void openkernel_nonuniversal_api_ofs_list_files(void);
void openkernel_nonuniversal_api_vfs_list(void);

/* memory and strings */
void* openkernel_nonuniversal_api_memset(void* dst, uint8_t val, uint32_t size);
void* openkernel_nonuniversal_api_memcpy(void* dst, const void* src, uint32_t size);
int   openkernel_nonuniversal_api_memcmp(const void* a, const void* b, uint32_t size);
void* openkernel_nonuniversal_api_kmalloc(uint32_t size);
void  openkernel_nonuniversal_api_kfree(void* ptr);
int   openkernel_nonuniversal_api_strcmp(const char *s1, const char *s2);
int   openkernel_nonuniversal_api_strncmp(const char *s1, const char *s2, unsigned int n);
int   openkernel_nonuniversal_api_strlen(const char *str);
void  openkernel_nonuniversal_api_strcpy(char *dst, const char *src);
void  openkernel_nonuniversal_api_strncpy(char *dst, const char *src, unsigned int n);

/* time */
void openkernel_nonuniversal_api_print_time(void);
```

## 3. Supporting headers

### `KernelAPI/OpenKernelEnvironment/SysCalls/syscalls.h`

The minimal contract between your code and the kernel:

```c
extern int syscall(int id, void* arg);
extern int syscall_handler(int id, void* arg);
```

### `KernelAPI/OpenKernelEnvironment/types.h`

A trimmed copy of the integer typedefs (`uint8_t`, `uint32_t`, `bool_t`, and so on) so API consumers do not have to pull in the whole of `SystemLib`.

> **Warning:** this file uses the same include guard `TYPES_H` as `SystemLib/Std/types.h`. Including both in one translation unit silently drops whichever comes second. It also defines `NULL` as `0` while `SystemLib/Std/types.h` defines it as `((void*)0)`.

### `KernelAPI/Information/openkernel_api_info.h`

```c
#define KERNEL_API_NAME "OpenKernel API"
#define KERNEL_API_VER  "3.0-16593b9"
#define KERNEL_API_ARCH "x86 (32-bit)"
#define KERNEL_API_AUTHOR "OpenSoftware-World"
```

Informational only.

## 4. The full syscall table

Numbers are defined in `SystemLib/SysCalls/syscall.h`.

| # | Constant | # | Constant | # | Constant |
|---:|---|---:|---|---:|---|
| 0 | `SYSCALL_PRINT` | 15 | `SYSCALL_PRINT_BIN` | 30 | `SYSCALL_OFS_LF` |
| 1 | `SYSCALL_CLEAR_SCREEN` | 16 | `SYSCALL_SB16_FLAT` | 31 | `SYSCALL_VFS_L` |
| 2 | `SYSCALL_SHUTDOWN` | 17 | `SYSCALL_SB16_TEST` | 32 | `SYSCALL_MEMSET` |
| 3 | `SYSCALL_REBOOT` | 18 | `SYSCALL_GET_CPU` | 33 | `SYSCALL_MEMCPY` |
| 4 | `SYSCALL_SET_BG_COLOR` | 19 | `SYSCALL_FAT12_RF` | 34 | `SYSCALL_MEMCMP` |
| 5 | `SYSCALL_SET_TXT_COLOR` | 20 | `SYSCALL_FAT16_LIST_R` | 35 | `SYSCALL_KMALLOC` |
| 6 | `SYSCALL_NEWLINE` | 21 | `SYSCALL_FAT16_RF` | 36 | `SYSCALL_KFREE` |
| 7 | `SYSCALL_SET_CURSOR` | 22 | `SYSCALL_FAT32_RF` | 37 | `SYSCALL_STRCMP` |
| 8 | `SYSCALL_PTCHAR` | 23 | `SYSCALL_FAT32_LIST_R` | 38 | `SYSCALL_STRNCMP` |
| 9 | `SYSCALL_PRINT_NW` | 24 | `SYSCALL_LOG_MSG` | 39 | `SYSCALL_STRLEN` |
| 10 | `SYSCALL_SET_COLOR_SCH` | 25 | `SYSCALL_EXIT_PROG` | 40 | `SYSCALL_STRCPY` |
| 11 | `SYSCALL_GET_CHR` | 26 | `SYSCALL_OFS_CREATE_F` | 41 | `SYSCALL_STRNCPY` |
| 12 | `SYSCALL_SLEEP` | 27 | `SYSCALL_OFS_RF` | 42 | `SYSCALL_PRINT_TIME` |
| 13 | `SYSCALL_PRINT_HEX` | 28 | `SYSCALL_OFS_WF` | | |
| 14 | `SYSCALL_PRINT_DEC` | 29 | `SYSCALL_OFS_DF` | | |

Any id outside 0 to 42 returns `-1`.

### Argument passing

Multi-value calls use wrapper structs from `SystemLib/SysCalls/Structs/structs.h`:

```c
typedef struct { uint8_t x, y; }              vga_set_cursor_t;
typedef struct { uint8_t bg, text; }          vga_set_color_scheme_t;
typedef struct { const char* name; uint32_t size; } ofs_create_file_t;
typedef struct { const char* name; uint8_t* buffer; }  ofs_read_file_t;
typedef struct { const char* s1, *s2; }       strcmp_t;
/* ... plus fat12/16/32 read, log_message, exit_program, memset, memcpy, memcmp, ... */
```

Pass a pointer to the struct as `arg`. These are handled correctly by the dispatcher.

Single-value calls take the value directly, but see the warning below.

## 5. Known problems with this layer

These are real defects in the current code, not hypothetical risks.

### 5.1 The API sources are not built

Neither `KernelAPI/nonuniversal_api_defs.c` nor `KernelAPI/OpenKernelUniversalAPI/universal_api_defs.c` appears in the `C_SRC` list in the `Makefile`. Nothing else in the repository includes the API headers either. As it stands the layer is **neither compiled nor used**.

To use it, add both files to `C_SRC`.

### 5.2 Scalar arguments are passed as addresses, then cast as values

For example:

```c
/* universal_api_defs.c */
void openkernel_universal_api_vga_set_bg_color(uint8_t color) {
    syscall(4, &color);            /* passes the ADDRESS */
}

/* syscall.c */
case SYSCALL_SET_BG_COLOR:
    if (!arg) return -1;
    vga_set_bg_color((uint8_t)arg); /* treats it as the VALUE */
    return 0;
```

The two sides disagree. The same mismatch affects syscall numbers 4, 5, 8, 11, 12, 13, 14, 15, 16, 18, 23, 29, 35, 37 to 41. Either the wrappers should pass values or the handler should dereference; they cannot both be correct as written.

The struct-based calls (7, 10, 19 to 22, 24 to 28, 32 to 34, 36, 42) do not have this problem.

### 5.3 Return values are discarded

`syscall_handler()` ends almost every case with `return 0;`. Consequently:

- `openkernel_nonuniversal_api_kmalloc()` can only ever return `0` or `-1`, never a usable pointer.
- `openkernel_nonuniversal_api_strlen()`, `_strcmp()`, `_strncmp()`, `_memcmp()` always report `0`.
- `openkernel_nonuniversal_api_fat12_read_file()` and the FAT16/FAT32 equivalents cannot report success or failure.
- `SYSCALL_GET_CHR` calls `get_char()` and throws the character away.

### 5.4 One function is declared but never defined

`openkernel_nonuniversal_api_sys_next_status()` appears in `nonuniversal_api_defs.h` but has no implementation in the `.c` file. Any consumer that calls it gets an undefined reference at link time.

### 5.5 The API inherits the `klog_level_t` breakage

`openkernel_nonuniversal_api_log_message()` takes a `klog_level_t` parameter. Because `KernelAPI/nonuniversal_api_defs.h` includes `SystemLib/SysCalls/Structs/structs.h`, it gets the correct typedef there. But the moment it is compiled together with `Kernel/KernelServices/LogService/log.h` (which declares `klog_level_t` as a variable), the two definitions collide.

## 6. Recommended usage pattern

Until the issues above are fixed, the safest approach for a downstream operating system is:

1. Call OpenKernel's real functions directly (`vga_print_scr_nw()`, `fat16_read_file()`, `ofs_read_file()`) rather than going through `KernelAPI`.
2. Add `KernelAPI/*.c` to `C_SRC` only after you have verified the argument conventions.
3. Keep your own thin wrapper layer so you can switch to `KernelAPI` later without touching application code.

```c
/* Your own wrapper - stable for your codebase */
void os_print(const char *s) {
    vga_print_scr_nw(s);
}
```

---

[← File Systems](07-file-systems.md) | [Documentation Index](README.md) | [Next: Project Status →](09-project-status.md)
