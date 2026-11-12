# 05 - System Library

[← Kernel Services](04-kernel-services.md) | [Documentation Index](README.md) | [Next: Drivers →](06-drivers.md)

---

`SystemLib/` contains the routines every part of the kernel depends on: memory, strings, port I/O, time, tasks, conversion, and the internal function and syscall registries.

## 1. Memory - `SystemLib/Memory/mem.c`

### Functions

```c
void* memset(void* dst, uint8_t val, uint32_t size);
void* memcpy(void* dst, const void* src, uint32_t size);
int   memcmp(const void* a, const void* b, uint32_t size);

void  heap_init(uint32_t start, uint32_t size);
void* kmalloc(uint32_t size);
void  kfree(void* ptr);
```

`memset`, `memcpy`, and `memcmp` behave like their standard C counterparts and are used throughout the tree.

### Heap allocation

`heap_init()` records three file-static values: `heap_start`, `heap_end`, and `heap_current = start`.

`kmalloc()` is a **bump allocator**:

1. Rounds the requested size up to a multiple of 4.
2. Returns the current pointer and advances `heap_current` by the rounded size.
3. Returns `NULL` if the request would reach or pass `heap_end`.

```c
heap_init(0x100000, 1024 * 1024);   /* 1 MiB heap */

int* a = (int*)kmalloc(4);
int* b = (int*)kmalloc(16);
```

### Critical limitations

- **`kfree()` does nothing.** Its entire body is `(void)ptr;`. Memory allocated with `kmalloc` is never reclaimed. Every allocation permanently consumes heap space.
- There is no free list, no block headers, no coalescing, and no reuse.
- Alignment is 4 bytes only. DMA-capable buffers (for example those the AHCI driver needs) are not guaranteed any stronger alignment.
- **`heap_init()` is never called anywhere in the repository.** If you call `kmalloc()` first, `heap_current` is still 0 and the allocator will hand out addresses starting at `0x0`. Always call `heap_init()` before the first allocation.
- The linker script reserves a 1 MiB `.heap` region with `_heap_start` and `_heap_end` symbols, but nothing connects them to `heap_init()` automatically.

## 2. String functions - `SystemLib/Std/std.c`

```c
int  strcmp(const char *s1, const char *s2);
int  strncmp(const char *s1, const char *s2, unsigned int n);
int  strlen(const char *str);
void strcpy(char *dst, const char *src);
void strncpy(char *dst, const char *src, unsigned int n);
```

Standard semantics: `strcmp`/`strncmp` return negative, zero, or positive; `strlen` returns the length excluding the terminator.

**There is no `printf`, `sprintf`, or `snprintf` anywhere in this project.** There is no varargs support at all. To print formatted output you must format into a buffer yourself and then call the VGA functions.

Formatting helpers exist in the VGA driver instead:

```c
vga_print_hex(0xB8000);   /* prints hex */
vga_print_dec(-42);       /* prints decimal */
vga_print_bin(0x0F);      /* prints binary */
```

The subsystems that need arbitrary formatting (`security.c`, `ahci.c`, `time.c`) each carry their own local `itoa`-style helper, which is duplicated code you may want to consolidate.

## 3. Port I/O - `SystemLib/SystemIO/io.h`

Header-only, all `static inline`, so including it costs nothing extra.

```c
void outb(uint16_t port, uint8_t val);   uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t val);  uint16_t inw(uint16_t port);
void outl(uint16_t port, uint32_t val);  uint32_t inl(uint16_t port);

void insb(uint16_t port, void *addr, uint32_t count);
void outsb(uint16_t port, void *addr, uint32_t count);
void insw(uint16_t port, void *addr, uint32_t count);
void outsw(uint16_t port, void *addr, uint32_t count);

void sys_wait(void);      /* writes to port 0x80, a short ISA delay */
void sys_barrier(void);
void mem_fence(void);     /* mfence */
uint32_t eflags_r(void);  /* pushf / pop, reads RFLAGS */
```

This is the lowest-level interface in the project. Drivers build on it.

### `SystemIO/qemu_io.h`

Provides `qemu_print_dbg(char)`, `qemu_str_print_dbg(const char*)`, and `qemu_log_message(...)` using the QEMU debug console at port `0xE9`. Nothing in the repository includes it. It now resolves `klog_level_t` correctly because it includes `log.h`, which owns the typedef.

## 4. Time - `SystemLib/TimeMng/time.c`

```c
systime get_time(void);
void    uptime(void);
void    print_time(void);
void    sleep(uint32_t seconds);
```

### Reading the clock

`get_time()` reads the RTC through CMOS ports `0x70` and `0x71`:

| Register | Field |
|---|---|
| `0x00` | seconds |
| `0x02` | minutes |
| `0x04` | hours |
| `0x07` | day |
| `0x08` | month |
| `0x09` | year |

Values arrive in BCD and are converted with `bcd_to_bin()`. The result is stored in the global `sys_time` and the year is interpreted as a two-digit year plus 2000, so dates are only correct for years 2000-2099.

`print_time()` formats `DD/MM/YYYY HH:MM:SS` into a 20-byte buffer and prints it.

### Sleeping

`sleep(seconds)` is a **pure busy loop**:

```c
for (i = 0; i < seconds * 50000000ULL; i++) nop();
```

Consequences you must know:

- The CPU runs at 100% for the whole duration. Nothing is suspended.
- There is no timer interrupt and no PIT or APIC timer driver in the project, so this is the only delay primitive available.
- The `50000000` constant is an estimate. Actual wall-clock duration varies with QEMU flags, `-smp`, and host load. Under `-smp 4` it will not be accurate.
- `uptime()` relies on the same class of counting and is not a reliable uptime counter.

## 5. Task management - `SystemLib/TaskMng/task.c`

```c
typedef struct task {
    uint32_t id, esp, ebp, eip;
    void (*entry)(void);
    void *stack;
    uint32_t stack_size;
    task_state_t state;     /* TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_TERMINATED */
    struct task *next;
} task_t;

void taskmng_init(void);
task_t* task_create(void (*entry)(void), uint32_t stack_size);
void    task_destroy(task_t *task);
void    task_add(task_t *task);
void    task_remove(task_t *task);
void    schedule(void);
void    yield(void);
task_t* task_current(void);
void    task_sleep(uint32_t ms);
void    task_block(void);
void    task_unblock(task_t *task);
```

### Honest assessment: this is not a scheduler

The data structures exist; the mechanism does not.

- **There is no context switch.** The `esp`, `ebp`, and `eip` fields are never saved or restored. The repository contains no assembly code at all, and a context switch on x86 requires it.
- `task_create()` sets `eip = (uint32_t)entry` and `esp = 0`. The `stack` buffer it allocates with `kmalloc()` is never used as a stack.
- `schedule()` simply does `current = current->next;` on a circular list. It does not check `state`, so blocked and terminated tasks are still selected.
- `yield()` calls `schedule()` and then calls `current->entry()` directly from the top. That *restarts* the task function rather than resuming it, producing nested and recursive execution instead of multitasking.
- `task_block()` advances `current` but never runs the next task.
- `task_sleep(uint32_t ms)` calls `sleep(ms)`, but `sleep()` takes **seconds**, so the unit is wrong by a factor of 1000.
- `task_destroy()` calls `kfree()`, which is a no-op, so stacks leak.
- `task_list` and `current` are declared `static`; `SecurityChecker` reaches the list through the `task_list_head()` accessor added to `task.c`, so the globals stay private.
- Nothing in the repository calls `taskmng_init()`, `task_create()`, `yield()`, or `schedule()`.

Treat this module as a **skeleton you can build on**, not as working multitasking. Real preemption would require an IDT, a timer IRQ, and assembly context-switch code, none of which exist here.

## 6. Type conversion - `SystemLib/TypeConversion/type_conv.c`

```c
char*    itoa(int val, char* str);                 /* decimal */
char*    htoa(uint32_t val, char* str);            /* hexadecimal */
char*    htoan(uint32_t val, char* str, int width);/* hexadecimal, zero padded to width */
int      atoi(const char* str);                    /* decimal */
uint8_t  bcd_to_bin(uint8_t val);                  /* RTC BCD decoding */
```

`htoan()` is useful for fixed-width register dumps:

```c
char buf[9];
htoan(0x000A8000, buf, 8);   /* "000a8000" */
```

`bcd_to_bin()` computes `((v / 16) * 10) + (v & 0x0F)` and is correct; `time.c` depends on it.

**This module does not compile.** There are two syntax errors: a stray `str1[1] = '\0';` that references an undeclared identifier in `itoa()`, and a missing semicolon in `htoan()`.

## 7. Kernel function registry - `SystemLib/KernelFunc/kernel_func.c`

A named function table, similar in spirit to a shell command registry.

```c
typedef void (*kfunc_t)(void);

void register_func(const char *name, kfunc_t func);
void exec_func(const char *func_name);
void exec_command(const char *command_name);   /* alias for exec_func */
```

The table holds up to 256 entries (`MAX_FUNC 256`), looked up linearly with `strcmp`.

### Lockdown by default

A global flag `kernel_func_enabled` starts as `false`. Both `register_func()` and `exec_func()` return immediately unless it has been set:

```c
KernelFunc_O();                       /* enable the registry */
register_func("clear", vga_clear_screen_wrapper);
exec_command("clear");               /* runs it */

KernelFunc_C();                       /* disable again */
```

`KernelFunc_O()` opens the registry, `KernelFunc_C()` closes it, and `kernel_func_init()` clears the table.

Caveats:

- `kernel_func_init()` does **not** reset `kernel_func_enabled`, so the open/closed state survives a re-init.
- The registered functions take `void`. There is no way to pass arguments, and no way to get a return value back.
- Nothing in the repository registers or executes any function, so the registry is unused by default.

## 8. Internal syscall dispatcher - `SystemLib/SysCalls/`

```c
int syscall(int id, void* arg);
int syscall_handler(int id, void* arg);
```

### What it actually is

A `switch` statement over 43 syscall numbers (0 to 42), mapping each number to a direct C function call. It is **an internal dispatch table, not a real syscall mechanism**:

- No `int 0x80`, no `sysenter`, no IDT gate.
- No privilege level transition; everything runs in kernel mode.
- `syscall()` is a plain function call to `syscall_handler()`.

It still has value: it gives `KernelAPI/` a single indirection point, so syscall numbers can be renumbered without changing the public API.

### Argument passing

Two conventions coexist:

1. **Struct arguments** - multi-value calls pass a pointer to a wrapper struct defined in `SystemLib/SysCalls/Structs/structs.h`, for example `vga_set_cursor_t`, `fat16_read_file_t`, `ofs_read_file_t`, `memset_t`, `strcmp_t`. These are handled correctly.
2. **Scalar arguments** - the handler casts the pointer itself, as in `vga_set_bg_color((uint8_t)arg)` and `kmalloc((uint32_t)arg)`, while the `KernelAPI` wrappers pass the *address* of the value (`syscall(4, &color)`). Both sides cannot be right; as written, these calls operate on truncated addresses.

### Return values

Almost every case ends with `return 0;` regardless of what the underlying function returned. As a result:

- `SYSCALL_KMALLOC` calls `kmalloc()` and discards the pointer, returning 0.
- `SYSCALL_STRLEN`, `SYSCALL_STRCMP`, `SYSCALL_MEMCMP`, and `SYSCALL_GET_CHR` all discard their results, so the caller can never learn the answer.

The struct-based calls are the reliable half of this interface.

### Compile status

`syscall.c` compiles cleanly. It previously failed because `syscall_extern.h` used `klog_level_t` without including the header that typedefs it; `syscall_extern.h` now includes `log.h`, which owns the typedef.

## 9. Module metadata pattern

Most headers end with a block like this:

```c
#define MEM_MODULE_NAME     "Memory Library"
#define MEM_MODULE_DESC     "Memory Library for OpenKernel"
#define MEM_MODULE_VER      "1.0"
#define MEM_MODULE_AUTHOR   "OpenSoftware-World"
#define MEM_MODULE_FILE_NAME "mem.lib"
#define MEM_MODULE_KRNL_VER "2.0"
```

These macros are documentation embedded in code. No logic reads them. Note that some report a kernel version older than the current 4.0, which is informational drift rather than a functional problem.

---

[← Kernel Services](04-kernel-services.md) | [Documentation Index](README.md) | [Next: Drivers →](06-drivers.md)
