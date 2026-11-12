# 04 - Kernel Services

[← Architecture](03-architecture.md) | [Documentation Index](README.md) | [Next: System Library →](05-system-library.md)

---

`Kernel/` holds the core services: error handling, logging, power control, paging, and runtime self-checks. This document covers each module, its public functions, and what you must do before calling them.

## 1. Error handling - `Kernel/ErrorHandler/panic.c`

```c
void kernel_panic(const char *msg, const char *error_code);
```

Halts the system after printing an error banner.

What it does, in order:

1. Clears the screen with `vga_clear_screen("F")`.
2. Hides the text cursor with `vga_cursor_mode("D", 0, 0)`.
3. Sets red text on a white background.
4. Prints `== OpenKernel PANIC ==`, then your `msg`, then `Error Details:` and `Error code:` with `error_code`.
5. Enters an infinite `hlt` loop.

```c
if (something_failed) {
    kernel_panic("Page table allocation failed", "E_MEM_01");
}
```

Requirements and limitations:

- `vga_init()` must have been called, otherwise the message is never displayed.
- There is no register dump, no stack trace, and no caller address. You get exactly two strings.
- It never returns. Do not put cleanup code after it.

## 2. Logging - `Kernel/KernelServices/LogService/log.c`

```c
void log_message(klog_level_t level, const char *message);
```

Prints a prefixed, colour-coded message to the VGA console.

| Level | Prefix | Colour |
|---|---|---|
| `LOG_SUCCESS` | `[SUCCESS]: ` | Green |
| `LOG_ERROR` | `[ERROR]: ` | Red |
| `LOG_WARNING` | `[WARNING]: ` | Light brown |
| `LOG_INFO` | `[INFO]: ` | Light blue |
| `LOG_UNKNOWN` | `[UNKNOWN]: ` | Light grey |

```c
log_message(LOG_INFO, "Keyboard driver initialised");
log_message(LOG_ERROR, "Disk read failed");
```

Requirements and limitations:

- `vga_init()` must have been called.
- **This module does not compile as written.** In `log.h` the enum declaration ends with `} klog_level_t;`, which declares a global *variable* named `klog_level_t` instead of creating a type alias. The correct form exists in `SystemLib/SysCalls/Structs/structs.h` (`typedef enum kernel_log_level klog_level_t;`). Until the header is fixed, `log.c`, `syscall.c`, and `security.c` all fail to compile. See [09 - Project Status](09-project-status.md).
- The same symbol breaks `SystemLib/SystemIO/qemu_io.h`.

## 3. System management - `Kernel/KernelServices/SystemManagement/sysmng.c`

```c
void sys_next_status(const char *mode, uint32_t seconds);
void exit_program(const char *mode, const char *message);
```

### `sys_next_status`

Power and reset control. The mode is matched on the whole string, not just the first character.

| Mode | Behaviour |
|---|---|
| `"shutdown"` | Sleeps `seconds`, then writes `0x00` to I/O port `0x604` (QEMU/ICH9 ACPI power button), waits via port `0x80`, then writes `0x20` to port `0x605`. |
| `"reboot"` | Disables interrupts, waits for the 8042 keyboard controller input buffer to clear (port `0x64` bit 1), then writes `0xFE` to port `0x64` to trigger a CPU reset. |

```c
sys_next_status("shutdown", 3);   /* power off after 3 seconds */
sys_next_status("reboot", 0);     /* restart immediately */
```

Notes:

- The shutdown path targets QEMU and similar PC firmware. On real hardware the ACPI ports differ and this may do nothing.
- `seconds` is passed to `sleep()`, which is a busy loop (see [05 - System Library](05-system-library.md)). The delay is approximate and CPU-burning.

### `exit_program`

```c
void exit_program(const char *mode, const char *message);
```

Two modes:

- `"D"` - prints nothing extra, then halts.
- `"C"` - prints `message`, then halts.

Both end in an infinite `hlt` loop and never return.

## 4. Virtual memory - `Kernel/KernelServices/VirtualMem/vmem.c`

```c
void paging_init(void);
void load_page_dir(uint32_t *pd);
void enable_paging(void);
```

Flags defined in `vmem.h`:

```c
#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2
#define PAGE_USER    0x4
```

### What it does

`paging_init()` builds a single page directory and one page table, both declared `__attribute__((aligned(4096)))` as static arrays. It **identity-maps the first 4 MiB** - virtual address N maps to physical address N - then:

1. Points page directory entry 0 at the first page table.
2. Loads the directory into CR3 via `load_page_dir()`.
3. Sets CR0 bit 31 (paging enable) via `enable_paging()`.

### What it does not do

- It maps only 1 of the 1024 possible page directory entries, so only 4 MiB of the address space is valid. Access beyond that faults.
- `PAGE_USER` is never applied to any entry; there is no user/supervisor distinction.
- There is no page fault handler, because there is no IDT to attach one to.
- There is no map/unmap function for dynamic allocation, no higher-half layout, and no TLB management.
- **Nothing in the repository calls `paging_init()`.** Enabling paging is entirely up to your entry point, and you should only do so once you have a fault handler or you are certain all your accesses fall in the mapped range.

## 5. Security checker - `Kernel/KernelServices/SecurityChecker/security.c`

A self-test framework referred to internally as PSC (OpenKernel Security Checker). About 520 lines, the second largest file in the project.

### Types

```c
typedef enum { SEC_OK, SEC_WARN, SEC_CRITICAL, SEC_PANIC } securchk_severity_t;

typedef struct {
    int total_checks, passed, warnings, critical;
    int tasks_validated, stack_overflows, heap_corruptions;
    int syscall_violations, memory_leaks;
} securchk_result_t;
```

### Public functions

| Function | Purpose |
|---|---|
| `securchk_init(void)` | Sets an internal initialisation flag. |
| `securchk_run(securchk_result_t*)` | Resets counters, prints a banner, runs all seven checks, prints a report. |
| `securchk_check_task_integrity(...)` | Walks the task list and validates `entry`, `stack`, state range, id, and that `esp`/`ebp` fall inside the task stack. |
| `securchk_check_stack_overflow(...)` | Same range validation for stacks. |
| `securchk_check_heap_integrity(...)` | Allocates 16 bytes with `kmalloc`, fills with `0xAA`, verifies the contents, frees. |
| `securchk_check_syscall_bounds(...)` | Asserts `syscall(-1, NULL)` and `syscall(132, NULL)` both return `-1`. |
| `securchk_check_memory_safety(...)` | Self-tests `memset`, `memcpy`, `memcmp` and warns about stack alignment. |
| `securchk_check_driver_state(...)` | Clears the screen and resets the colour scheme, then reports OK. |
| `securchk_check_kernel_structures(...)` | Compile-time style assertions: `PAGE_SIZE == 4096`, `sizeof(void*) == 4`, `sizeof(uint32_t) == 4`. |
| `securchk_print_result(...)` | Formats the counters into a report. |
| `securchk_log_issue(...)` | Records a single issue with severity, module, message, and detail. |
| `securchk_get_overall_status(...)` | Returns an overall verdict derived from the counters. |
| `securchk_stack_canary_set/check(...)` | Places or verifies the canary value `0xDEADBEEF` at the first and last word of a stack region. |

### Honest assessment

- It is a **diagnostic report generator, not a protection mechanism**. It observes state; it does not enforce anything.
- The stack canary helpers are never called anywhere in the repository.
- `securchk_check_driver_state()` clears the user's screen as its "check", which is almost certainly not what you want in production.
- `SECURCHK_MAX_SYSCALLS` is defined as 32, but there are 43 syscall numbers (0 to 42), so the bound is stale.
- `SECURCHK_HEAP_MAGIC`, `securchk_issue_t`, and the `memory_leaks` counter are declared but never used.
- **The module does not compile** because it includes `syscall.h`, which pulls in the broken `klog_level_t` declaration.
- Even if it compiled, it would fail to link: it declares `extern task_t* task_list;` and `extern task_t* current;`, while `SystemLib/TaskMng/task.c` defines both as `static`.
- Nothing ever calls `securchk_init()` or `securchk_run()`.

## 6. Kernel information - `Kernel/openkernel_information.h`

```c
#define KRNL_NAME        "OpenKernel"
#define KRNL_VER         "3.0"
#define KRNL_ARCH        "x86 (32-bit)"
#define KRNL_LAST_COMMIT "16593b9"
```

Informational only. No file in the repository includes it, and `KRNL_LAST_COMMIT` is stale compared with the current HEAD.

## 7. Typical initialisation order

These services have interdependencies. A safe order from your `kernel_main()`:

```c
void kernel_main(void)
{
    vga_init();                     /* 1. screen - everything below may print */

    heap_init(_heap_start, 0x100000); /* 2. heap - needed by kmalloc users   */

    kbd_init();                     /* 3. input                              */

    /* 4. optional: paging only once you have a fault handler */
    /* paging_init();               */

    /* 5. optional: diagnostics */
    /* securchk_result_t r; securchk_run(&r); */
}
```

Rules of thumb:

- Anything that prints requires `vga_init()` first.
- Anything that allocates requires `heap_init()` first.
- `log_message()` cannot be used at all until the `klog_level_t` declaration in `log.h` is corrected.

---

[← Architecture](03-architecture.md) | [Documentation Index](README.md) | [Next: System Library →](05-system-library.md)
