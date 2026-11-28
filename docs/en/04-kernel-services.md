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
- **Compiles cleanly.** The old bug - `log.h` ending the enum with `} klog_level_t;`, which declared a global *variable* instead of a type alias - is fixed: `log.h` now owns `typedef enum kernel_log_level klog_level_t;`, `structs.h` and `syscall_extern.h` include `log.h` instead of redeclaring the enum, and `log.c`, `syscall.c`, and `security.c` all compile again. See [09 - Project Status](09-project-status.md).
- `SystemLib/SystemIO/qemu_io.h` resolves the type through the same `log.h` include.

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
- `seconds` is passed to `sleep()`, which delegates to `pit_sleep_ms()` (see [05 - System Library](05-system-library.md)). It waits on the IRQ0 millisecond counter, so it is accurate to one tick, but it busy-waits rather than suspending anything - and it returns immediately if the PIT has not been initialised or interrupts are off.

### `exit_program`

```c
void exit_program(const char *mode, const char *message);
```

Two modes:

- `"D"` - prints nothing extra, then halts.
- `"C"` - prints `message`, then halts.

Both end in an infinite `hlt` loop and never return.

## 4. Virtual memory - `Kernel/KernelServices/VirtualMem/vmem.c`

A subsystem that makes the whole 32-bit address space (4 GiB) usable and provides demand paging plus a pluggable swap backend. You can allocate large virtual buffers without needing more physical RAM: each page is bound to RAM only on first touch, and when RAM fills up, second-chance replacement moves pages to swap space.

### Public API

```c
int  vmem_init(const vmem_config_t *config);
void vmem_install_fault_handler(void);

int  vmem_swap_set_backend(const vmem_swap_backend_t *backend, uint32_t slots);

void *vmem_alloc(uint32_t size);                       /* WRITE | SWAPPABLE */
void *vmem_alloc_ex(uint32_t size, uint32_t flags);
int  vmem_free(void *ptr);

int  vmem_region_add(uint32_t vaddr, uint32_t size, uint32_t flags);
int  vmem_region_remove(uint32_t vaddr);
int  vmem_region_find(uint32_t vaddr, uint32_t *start, uint32_t *end,
                      uint32_t *flags);

int  vmem_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);
int  vmem_unmap_page(uint32_t vaddr);
uint32_t vmem_get_physical(uint32_t vaddr);

void *vmem_map_mmio(uint32_t paddr, uint32_t size, uint32_t flags);
int  vmem_unmap_mmio(void *vaddr, uint32_t size);

void vmem_invlpg(uint32_t vaddr);
void vmem_flush_tlb(void);
void vmem_get_stats(vmem_stats_t *out);
```

Flags defined in `vmem.h` (Intel SDM volume 3):

```c
#define PAGE_PRESENT  0x001u   #define PAGE_PSE     0x080u
#define PAGE_RW       0x002u   #define PAGE_GLOBAL  0x100u
#define PAGE_USER     0x004u   #define PAGE_ACCESSED 0x020u
#define PAGE_PWT      0x008u   #define PAGE_DIRTY   0x040u
#define PAGE_PCD      0x010u
```

Plus the region flags:

```c
#define VMEM_REGION_WRITE      0x001u
#define VMEM_REGION_USER       0x002u
#define VMEM_REGION_SWAPPABLE  0x004u
```

### What it does

`vmem_init()` registers the physical RAM range given in `vmem_config_t` (`phys_start`-`phys_end`) with the frame allocator and **identity-maps all of it**:

- The first 4 MiB use 4 KiB page tables (kernel, stack, VGA buffer live here).
- Above 4 MiB, 4 MiB PSE pages are used, so frame and shared buffers stay reachable at their physical addresses.
- A virtual range (`arena_start`-`arena_end`) is reserved for allocations. If both are `0`, the arena is placed automatically just above the identity-mapped RAM (up to the default `0xFFFFF000`).

Then paging is enabled and, if `install_fault_handler` is set, a handler is attached to vector 14 (page fault). The handler:

1. Reads the faulting address from CR2 and locates the region.
2. If the entry carries `VMEM_PTE_SWAP`, reads the page back from the swap backend (swap-in).
3. If the entry is empty, binds a freshly zeroed frame (demand-zero).
4. If no frame is free, uses second-chance: it skips accessed pages, writes unaccessed swappable pages to swap (swap-out) and reuses the frame.

Page tables are created on demand and torn down when their last user is removed; the TLB is managed with `vmem_invlpg()` / `vmem_flush_tlb()`.

### Swap backend contract

The VM layer is not tied to a particular disk driver. Swapping goes through this callback pair:

```c
typedef int (*vmem_swap_read_t)(void *context, uint32_t slot, void *page);
typedef int (*vmem_swap_write_t)(void *context, uint32_t slot, const void *page);

typedef struct {
    vmem_swap_read_t  read;
    vmem_swap_write_t write;
    void *context;
} vmem_swap_backend_t;
```

Rules:

- `read`/`write` move exactly one 4 KiB page; returning `0` means success.
- **They are called with interrupts disabled** (the page fault handler runs through an interrupt gate) and must not fault themselves. Keep their buffers and driver state in RAM, ideally in non-swappable memory.
- Call `vmem_swap_set_backend()` before the first eviction. `vmem_init()` resets the backend, so register it **after** init. The slot count is clamped to a 1 GiB upper limit.

### Integration example

```c
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/KernelServices/VirtualMem/vmem.h>

static int disk_swap_read(void *ctx, uint32_t slot, void *page);
static int disk_swap_write(void *ctx, uint32_t slot, const void *page);

void kernel_main(void)
{
    vga_init();
    heap_init(_heap_start, 0x100000);
    interrupt_init();                       /* IDT must exist first */

    vmem_config_t cfg = {
        .phys_start = PAGE_ALIGN((uint32_t)(uintptr_t)_kernel_end),
        .phys_end   = 64u * 1024u * 1024u,  /* from your bootloader */
        .arena_start = 0,                    /* automatic */
        .arena_end   = 0,
        .install_fault_handler = true,
    };
    if (vmem_init(&cfg) != 0) panic("vmem_init");

    vmem_swap_backend_t swap = {
        .read = disk_swap_read,
        .write = disk_swap_write,
        .context = NULL,
    };
    vmem_swap_set_backend(&swap, 4096);      /* 16 MiB of swap */

    /* Large buffer: only touched pages stay resident. */
    uint8_t *buf = vmem_alloc(32u * 1024u * 1024u);
    if (buf == NULL) panic("vmem_alloc");
    buf[0] = 1;                              /* binds the first page now */

    /* Memory used from interrupt context must not be swapped. */
    void *irq_buf = vmem_alloc_ex(PAGE_SIZE, VMEM_REGION_WRITE);

    vmem_stats_t st;
    vmem_get_stats(&st);                     /* st.evictions, st.free_frames ... */

    vmem_free(buf);                          /* releases frames and swap slots */
}
```

### What it does not do / limitations

- The arena is a bump allocator: `vmem_free()` returns physical frames and swap slots, but it **does not recycle the virtual address range**. A long-lived alloc/free loop can exhaust the arena.
- There is no higher-half kernel layout; the kernel stays in the identity-mapped low memory.
- `VMEM_REGION_USER` is written into PTEs, but ring-3 transitions, a TSS and separate address spaces are out of scope. There is no process isolation.
- Page replacement is second-chance only; no working-set analysis or LRU. Thrashing is possible under heavy pressure.
- Everything is single-CPU; there is no SMP lock, only interrupt-flag protection (`vmem_lock`).
- `phys_start`/`phys_end` come from the caller; the multiboot memory map is not parsed. You must pass the usable RAM range your bootloader reports.
- `paging_init()` is kept for source compatibility: it identity-maps only the first 4 MiB and enables paging. New code should use `vmem_init()`.

### Honest assessment

Scenarios verified under QEMU: demand-zero pages, physical MMIO mapping, a 160-page buffer swapped out and read back with data integrity on a 96-frame RAM budget, a second `vmem_init()` and the legacy `paging_init()` path. The test ended with no frame, page-table or swap-slot leaks. Still, this is a kernel subsystem: swapping over a real disk driver, concurrent processes and low-memory conditions have not been tested yet.

## 5. Security checker - `Kernel/KernelServices/SecurityChecker/security.c`

A self-test framework referred to internally as PSC (OpenKernel Security Checker). 602 lines, the largest file in the project.

### Types

```c
typedef enum { SEC_OK, SEC_WARN, SEC_CRITICAL, SEC_PANIC } securchk_severity_t;

typedef struct {
    int total_checks, passed, warnings, critical;
    int tasks_validated, stack_overflows, heap_corruptions;
    int syscall_violations, memory_leaks;
} securchk_result_t;

typedef struct {
    securchk_severity_t severity;
    const char* module;
    const char* message;
    uint32_t    detail;
} securchk_issue_t;          /* up to SECURCHK_MAX_ISSUES = 32 entries */
```

### Public functions

| Function | Purpose |
|---|---|
| `securchk_init(void)` | Initialises the issue log. |
| `securchk_run(securchk_result_t*)` | Auto-initialises, resets counters and the issue log, prints a banner, runs all seven checks, prints a report. |
| `securchk_check_task_integrity(...)` | Walks the task list (bounded, breaks when it returns to the head) and validates `entry`, `stack`, state range, id, and that a non-zero `esp`/`ebp` falls inside the task stack. |
| `securchk_check_stack_overflow(...)` | Same range validation for stacks, also bounded. |
| `securchk_check_heap_integrity(...)` | Verifies the guard magic of the next heap block, checks a `kmalloc`'d block for a 1 GiB address-range sanity, tests that a freed block can be re-claimed, then frees. |
| `securchk_check_syscall_bounds(...)` | Asserts `syscall(-1, NULL)` and `syscall(SYSCALL_PRINT_TIME + 1, NULL)` both return `-1` (the upper bound is kept in sync with the real syscall table, currently 43). |
| `securchk_check_memory_safety(...)` | Self-tests `memset`, `memcpy`, `memcmp` (including zero-length cases) and warns about stack alignment. |
| `securchk_check_driver_state(...)` | Probes the VGA text buffer with a cell write/readback (saving and restoring the original cell), then reports OK. |
| `securchk_check_kernel_structures(...)` | Static asserts plus linker-symbol sanity: `PAGE_SIZE == 4096`, pointer/integer widths, `__ro_start <= __bss_start <= __stack_start <= __heap_start <= __kernel_end`. |
| `securchk_print_result(...)` | Formats the counters into a report. |
| `securchk_log_issue(...)` | Records a single issue with severity, module, message, and detail in a bounded ring (drops the oldest when full). |
| `securchk_get_issue(i)` / `securchk_get_issue_count(void)` | Read access to the recorded issues. |
| `securchk_get_overall_status(...)` | Returns an overall verdict derived from the counters. |
| `securchk_stack_canary_set/check(...)` | Places or verifies the canary value `0xDEADBEEF` at the first and last word of a stack region. |

### Honest assessment

- It is a **diagnostic report generator, not a protection mechanism**. It observes state; it does not enforce anything.
- The stack canary helpers are never called anywhere in the repository.
- It now compiles cleanly (`-Wall -Wextra`) and no longer depends on the static `task_list`/`current` symbols: the task walk goes through the `task_list_head()` accessor added to `SystemLib/TaskMng/task.c`.
- The syscall upper bound is derived from `SYSCALL_PRINT_TIME`, so it cannot silently drift from the real table again.
- The heap check verifies guard magic, block ranges, and free/re-claim behaviour instead of only "did `kmalloc` return non-NULL".
- The driver-state check no longer wipes the screen; it touches one VGA cell and restores it.
- Nothing ever calls `securchk_init()` or `securchk_run()`, and the module is untested at runtime (the project has no boot code).

## 6. Interrupt service - `Kernel/Components/InterruptServices/`

The first interrupt-handling code in the project: two C modules plus the NASM entry stubs, all listed in the Makefile (`ASM_SRC` / `C_SRC`).

| File | Purpose |
|---|---|
| `interrupt.c` / `interrupt.h` | IDT construction, handler registry and the C dispatch path |
| `pic.c` / `pic.h` | 8259A initialisation (remap to vectors 32-47), masking and EOI |
| `isr.asm` | Entry stubs for the 32 exception vectors and 16 IRQ vectors (generated with `%rep`) plus the shared `isr_common` tail |

### Public functions

| Function | Purpose |
|---|---|
| `interrupt_init(void)` | Installs gates 0-47, runs `pic_init()`, resets the handler registry and loads the IDTR with `lidt`. IRQ lines stay masked afterwards. Must run before any driver registers a handler. |
| `interrupt_register_handler(vector, handler)` | Appends a C handler to an exception (0-31) or IRQ vector (32-47). Vectors keep a *list* of handlers (from a fixed 32-node pool, no allocation in interrupt context), so devices sharing a PCI IRQ line can all listen; registering the same function twice is a no-op. |
| `interrupt_dispatch(registers_t*)` | Called from `isr.asm`. Exceptions run their handler(s) or `kernel_panic()`; for IRQs every registered handler runs in registration order, then the EOI is sent either way. |
| `pic_init(void)` | ICW1-ICW4 sequence: IRQ0-15 map to vectors 32-47, then every line is masked. |
| `pic_set_mask(irq)` / `pic_clear_mask(irq)` | Mask or unmask one line (0-15). |
| `pic_send_eoi(irq)` | Acknowledge the interrupt, including the cascade line for IRQ8-15. |

### Frame contract

`registers_t` in `interrupt.h` must match the pushes in `isr.asm` exactly; two static asserts pin `sizeof(registers_t) == 64` and `eip` at offset 44. The stubs normalise the frame so exceptions that carry a CPU error code (8, 10-14, 17) have the same shape as the ones that do not.

### What it does not do

- **No GDT of its own.** Gates use selector `0x08`, which is whatever the multiboot loader left in CS; there is no GDT code in the repository.
- **Nothing registers a handler by default.** An unhandled exception panics with the vector mnemonic; an IRQ without a handler is only acknowledged (the EOI is still sent so the controller keeps running). Drivers that do register: PIT (IRQ0), Keyboard (IRQ1), Serial (IRQ4), ATA (IRQ14), Mouse (IRQ12), e1000 / RTL8139 (their PCI line).
- **All IRQ lines start masked** - a driver must call `pic_clear_mask()` for the line it wants.
- No handler *un*registration: once appended to a vector's list a handler stays there (masking the line is how a driver stops it, which is what `pit_stop()`, `kbd_close()` and `mouse_close()` do).
- No APIC support, no per-vector priority, no DPL3 (user) gates and no `int 0x80` syscall gate.
- **Nothing in the repository calls `interrupt_init()` yet** - the project still has no boot code.

### Testing

Exercised under QEMU (`-kernel`, multiboot): IDT limit/base via `sidt`, software `int $0x2F` and `int3`, a real `#GP` with its error code, a real timer IRQ0 on vector 32 after `pic_clear_mask(0)` + `sti()`, and an unhandled exception reaching `kernel_panic()`. The harness lives outside the repository; there is no in-repo test suite.

## 7. Kernel information - `Kernel/openkernel_information.h`

```c
#define KRNL_NAME        "OpenKernel"
#define KRNL_VER         "5.0"
#define KRNL_ARCH        "x86 (32-bit)"
#define KRNL_LAST_COMMIT "06c18fe"
```

Informational only. No file in the repository includes it, and `KRNL_LAST_COMMIT` is stale compared with the current HEAD.

## 8. Typical initialisation order

These services have interdependencies. A safe order from your `kernel_main()`:

```c
void kernel_main(void)
{
    vga_init();                     /* 1. screen - everything below may print */

    heap_init(_heap_start, 0x100000); /* 2. heap - needed by kmalloc users   */

    interrupt_init();               /* 3. IDT + PIC, IRQs start masked       */

    kbd_init();                     /* 4. input                              */

    /* 5. virtual memory: all RAM identity-mapped, demand paging on */
    /* vmem_config_t cfg = { ... }; vmem_init(&cfg);                 */
    /* vmem_swap_set_backend(&swap, slots);  // after vmem_init()   */

    /* 6. optional: diagnostics */
    /* securchk_result_t r; securchk_run(&r); */
}
```

Rules of thumb:

- Anything that prints requires `vga_init()` first.
- Anything that allocates requires `heap_init()` first.
- Anything that registers a handler or unmasks an IRQ requires `interrupt_init()` first; a driver then calls `pic_clear_mask()` for its own line.
- `log_message()` is usable: `log.h` now owns the `klog_level_t` typedef (the duplicate enum in `structs.h` was removed in favour of including `log.h`).

---

[← Architecture](03-architecture.md) | [Documentation Index](README.md) | [Next: System Library →](05-system-library.md)
