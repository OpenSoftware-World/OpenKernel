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
char openkernel_universal_api_get_char(void);
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
#define KERNEL_API_VER  "4.0-960224c"
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
| 13 | `SYSCALL_PRINT_HEX` | 28 | `SYSCALL_OFS_WF` | 43 | `SYSCALL_SYS_NEXT_STATUS` |
| 14 | `SYSCALL_PRINT_DEC` | 29 | `SYSCALL_OFS_DF` | | |

Any id outside 0 to 43 returns `-1`.

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

Single-value calls follow the same "address of the argument" rule: the wrapper passes `&value` for scalars and `&pointer` for pointer arguments, and `syscall_handler()` dereferences accordingly. `SYSCALL_SYS_NEXT_STATUS` (43) also uses a struct, `sys_next_status_t`, because it carries two arguments.

## 5. Problems with this layer

### 5.1 The API sources are not built

Neither `KernelAPI/nonuniversal_api_defs.c` nor `KernelAPI/OpenKernelUniversalAPI/universal_api_defs.c` appears in the `C_SRC` list in the `Makefile`. Nothing else in the repository includes the API headers either. As it stands the layer is **neither compiled nor used**.

To use it, add both files to `C_SRC` with the same path style as the rest of the list.

### 5.2 Scalar arguments were passed as addresses, then cast as values (fixed)

The wrappers have always been uniform - `syscall(id, &arg)` - but `syscall_handler()` used to cast `arg` straight to the value type (`vga_set_bg_color((uint8_t)arg)` received the low byte of the *address*, not the colour). The mismatch affected syscall numbers 0, 4, 5, 8, 9, 12 to 16, 18, 23, 29, 35, 36, 39 and 41 (the earlier revision of this document missed 9, 36 and 39).

`syscall_handler()` now dereferences `arg`, so the whole ABI follows one rule:

- scalar argument: `arg` points to the value (`&value`)
- pointer argument: `arg` points to the pointer (`&pointer`)
- multi-field call: `arg` points to a wrapper struct
- no argument: `arg` is `NULL`

This fix also removed the four `cast from pointer to integer of different size` warnings from `syscall.c`.

### 5.3 Return values were discarded (fixed)

`syscall_handler()` used to end almost every case with `return 0;`, so `openkernel_nonuniversal_api_kmalloc()` could never return a usable pointer, the string helpers always reported `0`, and `SYSCALL_GET_CHR` threw the character away.

Value-returning calls now propagate the result through `syscall()`: `kmalloc`, `memset`, `memcpy` (pointers fit in the `int` return because this is a 32-bit target), `memcmp`, `strcmp`, `strncmp`, `strlen`, the FAT12/16/32 read calls, and `get_char()`. As part of this, `openkernel_universal_api_get_char()` changed from `void` to `char`; it is the only signature change in this fix.

### 5.4 One function was declared but never defined (fixed)

`openkernel_nonuniversal_api_sys_next_status()` appeared in `nonuniversal_api_defs.h` with no implementation. It is now implemented on top of the new `SYSCALL_SYS_NEXT_STATUS` (43) and the `sys_next_status_t` struct, and was verified to deliver both arguments (`mode` and `seconds`) correctly.

### 5.5 The API previously inherited the `klog_level_t` breakage (resolved)

`openkernel_nonuniversal_api_log_message()` takes a `klog_level_t` parameter. `KernelAPI/nonuniversal_api_defs.h` includes `SystemLib/SysCalls/Structs/structs.h`, which now includes `log.h`, the single owner of the `klog_level_t` typedef. The old collision - `structs.h` typedef-ing the enum while `log.h` declared a *variable* named `klog_level_t` - is gone; both headers agree on one definition.

## 6. Recommended usage pattern

The argument convention and return-value defects are fixed (section 5); the remaining limitation is 5.1 - the API sources are not in the default build. Until you add them to `C_SRC`, a downstream operating system should:

1. Call OpenKernel's real functions directly (`vga_print_scr_nw()`, `fat16_read_file()`, `ofs_read_file()`) rather than going through `KernelAPI`, or add the two `KernelAPI/*.c` files to `C_SRC`.
2. Keep your own thin wrapper layer so you can switch to `KernelAPI` later without touching application code.

```c
/* Your own wrapper - stable for your codebase */
void os_print(const char *s) {
    vga_print_scr_nw(s);
}
```

---

[← File Systems](07-file-systems.md) | [Documentation Index](README.md) | [Next: Project Status →](09-project-status.md)
