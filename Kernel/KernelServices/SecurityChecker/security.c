#include "security.h"

#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/TaskMng/task.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/SystemLib/SysCalls/syscall.h>

static int g_initialized = 0;

/* ============================================================
   String / Number Helpers (freestanding - no libc)
   ============================================================ */

static void sc_itoa(uint32_t num, char* buf, int sz) {
    if (!buf || sz <= 0) return;
    int i = 0;
    if (num == 0) { buf[i++] = '0'; }
    else {
        char tmp[12]; int t = 0;
        while (num > 0) { tmp[t++] = (char)((num % 10) + '0'); num /= 10; }
        while (t > 0 && i < sz - 1) buf[i++] = tmp[--t];
    }
    buf[i] = '\0';
}

static void sc_prt(const char* s) {
    if (s) vga_print_scr_nw(s);
}

static void sc_prt_dec(uint32_t n) {
    char b[12]; sc_itoa(n, b, sizeof(b)); sc_prt(b);
}

/* vga_print_hex already renders exactly "0x" + 8 uppercase digits */
static void sc_prt_hex(uint32_t n) {
    sc_prt("0x");
    vga_print_hex(n);
}

static void sc_line(void) {
    sc_prt("-------------------------------------\n");
}

static void sc_sep(const char* title) {
    sc_prt("\n  ");
    sc_prt(title);
    sc_prt("\n");
    sc_line();
}

/* ============================================================
   Issue Log (bounded)
   The first SECURCHK_MAX_ISSUES entries are kept for later
   inspection via securchk_get_issue(); the running total counts
   everything. Stored pointers must outlive the log, so callers
   pass string literals.
   ============================================================ */

static securchk_issue_t g_issues[SECURCHK_MAX_ISSUES];
static uint32_t g_issue_count = 0;

static void issue_log_reset(void) {
    g_issue_count = 0;
}

uint32_t securchk_get_issue_count(void) {
    return g_issue_count;
}

const securchk_issue_t* securchk_get_issue(uint32_t index) {
    if (index >= g_issue_count || index >= SECURCHK_MAX_ISSUES) return NULL;
    return &g_issues[index];
}

/* ============================================================
   Issue Logging
   ============================================================ */

static const char* severity_str(int sev) {
    switch (sev) {
        case SEC_OK:       return "OK";
        case SEC_WARN:     return "WARNING";
        case SEC_CRITICAL: return "CRITICAL";
        case SEC_PANIC:    return "PANIC";
        default:           return "UNKNOWN";
    }
}

void securchk_log_issue(securchk_severity_t severity, const char* module, const char* msg, uint32_t detail) {
    if (!module) module = "-";
    if (!msg) msg = "";

    if (g_issue_count < SECURCHK_MAX_ISSUES) {
        securchk_issue_t* issue = &g_issues[g_issue_count];
        issue->severity = (int)severity;
        issue->module = module;
        issue->message = msg;
        issue->detail = detail;
    }
    g_issue_count++;

    sc_prt("[PSC] ");
    sc_prt(severity_str(severity));
    sc_prt(" [");
    sc_prt(module);
    sc_prt("] ");
    sc_prt(msg);
    if (detail != 0) {
        sc_prt(" (");
        sc_prt_hex(detail);
        sc_prt(")");
    }
    sc_prt("\n");
}

/* ============================================================
   Check Epilogue
   Every finding must bump warnings or critical, so the severity
   of a check is simply how far those counters moved while it ran.
   ============================================================ */

static securchk_severity_t securchk_finish(securchk_result_t* result,
                                           uint32_t warn0, uint32_t crit0) {
    if (result->warnings == warn0 && result->critical == crit0)
        result->passed++;

    if (result->critical > crit0) return SEC_CRITICAL;
    if (result->warnings > warn0) return SEC_WARN;
    return SEC_OK;
}

/* ============================================================
   Stack Canary
   ============================================================ */

void securchk_stack_canary_set(uint32_t* stack, uint32_t stack_size) {
    if (!stack || stack_size < 8) return;
    uint32_t words = stack_size / sizeof(uint32_t);
    stack[0] = SECURCHK_STACK_CANARY;
    stack[words - 1] = SECURCHK_STACK_CANARY;
}

int securchk_stack_canary_check(uint32_t* stack, uint32_t stack_size) {
    if (!stack || stack_size < 8) return 0;
    uint32_t words = stack_size / sizeof(uint32_t);
    if (stack[0] != SECURCHK_STACK_CANARY) return 0;
    if (stack[words - 1] != SECURCHK_STACK_CANARY) return 0;
    return 1;
}

/* ============================================================
   1. Task Integrity Check
   - The circular list is walked with a hard iteration bound, so
     a corrupted list cannot hang the checker itself
   - entry / stack / state / PID validation for every node
   - duplicate PID detection and a broken circular link
   ============================================================ */

securchk_severity_t securchk_check_task_integrity(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    uint32_t warn0 = result->warnings;
    uint32_t crit0 = result->critical;

    task_t* head = task_list_head(); /* NULL = no tasks yet: nothing to do */
    task_t* cur = head;
    uint32_t count = 0;

    while (cur && count < SECURCHK_MAX_TASKS) {
        if (!cur->entry) {
            securchk_log_issue(SEC_WARN, "TASK", "Invalid entry pointer, PID", cur->id);
            result->warnings++;
        }

        if (!cur->stack || cur->stack_size == 0) {
            securchk_log_issue(SEC_WARN, "TASK", "Invalid stack, PID", cur->id);
            result->warnings++;
        }

        if ((int)cur->state < 0 || (int)cur->state > (int)TASK_TERMINATED) {
            securchk_log_issue(SEC_CRITICAL, "TASK", "Invalid task state, PID", cur->id);
            result->critical++;
        }

        if (cur->id == 0) {
            securchk_log_issue(SEC_WARN, "TASK", "Invalid PID 0", 0);
            result->warnings++;
        }

        /* Duplicate PID: compare against the nodes visited so far */
        task_t* prev = head;
        for (uint32_t k = 0; k < count && prev; k++) {
            if (prev != cur && prev->id == cur->id) {
                securchk_log_issue(SEC_WARN, "TASK", "Duplicate PID detected, PID", cur->id);
                result->warnings++;
                break;
            }
            prev = prev->next;
        }

        count++;
        cur = cur->next;
        if (cur == head) break; /* closed the circle normally */
    }

    /* Did the walk close the circle? */
    if (head && !cur) {
        securchk_log_issue(SEC_CRITICAL, "TASK", "Task list contains NULL node (broken circular link)", count);
        result->critical++;
    } else if (head && cur != head) {
        securchk_log_issue(SEC_CRITICAL, "TASK", "Task list exceeded bounds or contains loop", count);
        result->critical++;
    }

    result->tasks_validated = count;
    return securchk_finish(result, warn0, crit0);
}

/* ============================================================
   2. Stack Overflow Check
   - esp/ebp must fall inside their task's stack window
   - esp == ebp == 0 means the context was never saved (fresh
     task), which is normal here - not an overflow
   - terminated tasks are skipped: their saved context is stale
   ============================================================ */

securchk_severity_t securchk_check_stack_overflow(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    uint32_t warn0 = result->warnings;
    uint32_t crit0 = result->critical;

    task_t* head = task_list_head();
    task_t* cur = head;
    uint32_t count = 0;

    while (cur && count < SECURCHK_MAX_TASKS) {
        if (cur->state != TASK_TERMINATED && cur->stack && cur->stack_size != 0) {
            uint32_t stack_base = (uint32_t)cur->stack;
            uint32_t stack_top = stack_base + cur->stack_size;

            if (cur->esp && (cur->esp < stack_base || cur->esp > stack_top)) {
                securchk_log_issue(SEC_CRITICAL, "STACK", "ESP stack boundaries exceeded, PID", cur->id);
                result->stack_overflows++;
                result->critical++;
            }

            if (cur->ebp && (cur->ebp < stack_base || cur->ebp > stack_top)) {
                securchk_log_issue(SEC_WARN, "STACK", "EBP stack boundaries exceeded, PID", cur->id);
                result->warnings++;
            }
        }

        count++;
        cur = cur->next;
        if (cur == head) break; /* closed the circle normally */
    }

    return securchk_finish(result, warn0, crit0);
}

/* ============================================================
   3. Heap Integrity Check
   - kmalloc must return usable memory (guard words at both ends)
   - an allocation larger than any sane heap must fail
   - kfree reclamation: the next same-size request should be able
     to reuse the freed block (counts toward memory_leaks if not)
   ============================================================ */

securchk_severity_t securchk_check_heap_integrity(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    uint32_t warn0 = result->warnings;
    uint32_t crit0 = result->critical;

    void* block = kmalloc(16);
    if (!block) {
        /* Also the expected result when heap_init() was never called */
        securchk_log_issue(SEC_WARN, "HEAP", "kmalloc failed (heap not initialized or memory low)", 0);
        result->warnings++;
    } else {
        /* 16 bytes = 4 words: guard words at both ends, pattern between */
        uint32_t* words = (uint32_t*)block;
        uint8_t* bytes = (uint8_t*)block;

        words[0] = SECURCHK_HEAP_MAGIC;
        words[3] = SECURCHK_HEAP_MAGIC;
        memset(bytes + 4, 0xAA, 8);

        if (words[0] != SECURCHK_HEAP_MAGIC || words[3] != SECURCHK_HEAP_MAGIC ||
            bytes[4] != 0xAA || bytes[11] != 0xAA) {
            securchk_log_issue(SEC_CRITICAL, "HEAP", "Heap memory corruption detected", 0);
            result->heap_corruptions++;
            result->critical++;
        }

        kfree(block);

        /* A correct allocator hands the freed block back to the next
           request of the same size; a bump allocator never does. */
        void* again = kmalloc(16);
        if (again) {
            if (again != block) {
                securchk_log_issue(SEC_WARN, "HEAP", "kfree did not reuse the block (memory reclamation could not be verified)", 0);
                result->memory_leaks++;
                result->warnings++;
            }
            kfree(again);
        }
    }

    /* Bounds: a request larger than any plausible heap must fail */
    void* huge = kmalloc(0x40000000); /* 1 GiB */
    if (huge) {
        securchk_log_issue(SEC_CRITICAL, "HEAP", "Heap bounds check failed (1 GiB allocation accepted)", 0);
        result->critical++;
        kfree(huge);
    }

    return securchk_finish(result, warn0, crit0);
}

/* ============================================================
   4. Syscall Bounds Check
   - invalid syscall ids must be rejected with -1
   - the local bound macro must stay in sync with syscall.h
   ============================================================ */

securchk_severity_t securchk_check_syscall_bounds(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    uint32_t warn0 = result->warnings;
    uint32_t crit0 = result->critical;

    /* Guard against the bound drifting away from syscall.h */
    if (SECURCHK_MAX_SYSCALLS != (SYSCALL_PRINT_TIME + 1)) {
        securchk_log_issue(SEC_CRITICAL, "SYSCALL", "SECURCHK_MAX_SYSCALLS and syscall.h are out of sync", SECURCHK_MAX_SYSCALLS);
        result->syscall_violations++;
        result->critical++;
    }

    int ret = syscall(-1, NULL);
    if (ret != -1) {
        securchk_log_issue(SEC_CRITICAL, "SYSCALL", "Invalid syscall ID accepted", 0);
        result->syscall_violations++;
        result->critical++;
    }

    int ret2 = syscall(SECURCHK_MAX_SYSCALLS + 100, NULL);
    if (ret2 != -1) {
        securchk_log_issue(SEC_WARN, "SYSCALL", "Bounds violation syscall ID accepted", 0);
        result->syscall_violations++;
        result->warnings++;
    }

    return securchk_finish(result, warn0, crit0);
}

/* ============================================================
   5. Memory Safety Check
   - memset/memcpy/memcmp must honour zero-size requests
   - a known pattern must survive copy + compare
   - stack buffer alignment
   ============================================================ */

securchk_severity_t securchk_check_memory_safety(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    uint32_t warn0 = result->warnings;
    uint32_t crit0 = result->critical;

    char buf[64];
    const char pattern[] = "test_alignment_check"; /* 20 chars + NUL */
    const uint32_t plen = (uint32_t)sizeof(pattern); /* includes the NUL */

    memset(buf, 0, sizeof(buf));
    if (buf[0] != 0 || buf[63] != 0) {
        securchk_log_issue(SEC_CRITICAL, "MEM", "memset zeroing error", 0);
        result->critical++;
    }

    /* Zero-size operations must be no-ops */
    buf[0] = 0x5A;
    memset(buf + 1, 0x00, 0);
    if (buf[0] != 0x5A) {
        securchk_log_issue(SEC_CRITICAL, "MEM", "memset(size=0) buffer corrupted", 0);
        result->critical++;
    }
    if (memcmp(buf, pattern, 0) != 0) {
        securchk_log_issue(SEC_CRITICAL, "MEM", "memcmp(size=0) result is not zero", 0);
        result->critical++;
    }

    memcpy(buf, pattern, plen);
    if (memcmp(buf, pattern, plen) != 0) {
        securchk_log_issue(SEC_CRITICAL, "MEM", "memcmp mismatch", 0);
        result->critical++;
    }

    if (((uint32_t)(uintptr_t)buf & 0x3) != 0) {
        securchk_log_issue(SEC_WARN, "MEM", "Stack alignment error", (uint32_t)(uintptr_t)buf);
        result->warnings++;
    }

    return securchk_finish(result, warn0, crit0);
}

/* ============================================================
   6. Driver State Check
   - VGA text buffer write/read-back integrity: one cell is
     written and immediately restored, so there is no visible
     side effect on the screen
   ============================================================ */

securchk_severity_t securchk_check_driver_state(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    uint32_t warn0 = result->warnings;
    uint32_t crit0 = result->critical;

    volatile uint16_t* cell = (volatile uint16_t*)(uintptr_t)VBUFFER;
    uint16_t saved = *cell;
    uint16_t probe = (uint16_t)((saved & 0xFF00) | (uint16_t)'A');

    *cell = probe;
    uint16_t read_back = *cell;
    *cell = saved; /* restore right away */

    if (read_back != probe) {
        securchk_log_issue(SEC_CRITICAL, "DRIVER", "VGA buffer write/read-back mismatch", (uint32_t)probe);
        result->critical++;
    } else {
        securchk_log_issue(SEC_OK, "DRIVER", "VGA driver is active", 0);
    }

    return securchk_finish(result, warn0, crit0);
}

/* ============================================================
   7. Kernel Structure Check
   - Linker symbols produced by link.ld must be ordered and sized
     correctly (sections, stack, heap)
   - Compile-time invariants of the 32-bit target
   ============================================================ */

/* Symbols defined by link.ld */
extern uint8_t __ro_start[], __ro_end[];
extern uint8_t _bss_start[], _bss_end[];
extern uint8_t _stack_start[], _stack_end[];
extern uint8_t _heap_start[], _heap_end[];
extern uint8_t _kernel_end[];

securchk_severity_t securchk_check_kernel_structures(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    uint32_t warn0 = result->warnings;
    uint32_t crit0 = result->critical;

    if ((uintptr_t)__ro_start >= (uintptr_t)__ro_end) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "Linker: __ro_start >= __ro_end", 0);
        result->critical++;
    }
    if ((uintptr_t)_bss_start >= (uintptr_t)_bss_end) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "Linker: _bss_start >= _bss_end", 0);
        result->critical++;
    }
    if ((uintptr_t)_stack_start >= (uintptr_t)_stack_end) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "Linker: _stack_start >= _stack_end", 0);
        result->critical++;
    }
    if ((uintptr_t)_heap_start >= (uintptr_t)_heap_end) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "Linker: _heap_start >= _heap_end", 0);
        result->critical++;
    }
    if ((uintptr_t)_stack_end > (uintptr_t)_heap_start) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "Linker: stack and heap are overlapping", 0);
        result->critical++;
    }
    if ((uintptr_t)_heap_end > (uintptr_t)_kernel_end) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "Linker: heap is exceeding kernel end", 0);
        result->critical++;
    }

    if (PAGE_SIZE != 4096) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "PAGE_SIZE is incorrect", PAGE_SIZE);
        result->critical++;
    }

    if (sizeof(void*) != 4) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "Pointer size is not 32-bit", sizeof(void*));
        result->critical++;
    }

    if (sizeof(uint32_t) != 4) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "uint32_t size is incorrect", sizeof(uint32_t));
        result->critical++;
    }

    return securchk_finish(result, warn0, crit0);
}

/* ============================================================
   Init
   ============================================================ */

void securchk_init(void) {
    issue_log_reset();
    g_initialized = 1;
}

/* ============================================================
   Main Run
   ============================================================ */

void securchk_run(securchk_result_t* result) {
    if (!result) return;

    if (!g_initialized) securchk_init();
    issue_log_reset(); /* every run starts with a fresh issue log */

    result->total_checks     = 0;
    result->passed           = 0;
    result->warnings         = 0;
    result->critical         = 0;
    result->tasks_validated  = 0;
    result->stack_overflows  = 0;
    result->heap_corruptions = 0;
    result->syscall_violations = 0;
    result->memory_leaks     = 0;

    sc_line();
    sc_prt("  OpenKernel Security Checker (PSC)\n");
    sc_line();

    sc_sep("Task Integrity");
    securchk_check_task_integrity(result);

    sc_sep("Stack Overflow");
    securchk_check_stack_overflow(result);

    sc_sep("Heap Integrity");
    securchk_check_heap_integrity(result);

    sc_sep("Syscall Bounds");
    securchk_check_syscall_bounds(result);

    sc_sep("Memory Safety");
    securchk_check_memory_safety(result);

    sc_sep("Driver State");
    securchk_check_driver_state(result);

    sc_sep("Kernel Structures");
    securchk_check_kernel_structures(result);

    securchk_print_result(result);
}

/* ============================================================
   Result Print
   ============================================================ */

void securchk_print_result(securchk_result_t* result) {
    if (!result) return;

    sc_line();
    sc_prt("  Security Check Completed\n");
    sc_prt("  Total Checks : ");
    sc_prt_dec(result->total_checks);
    sc_prt("\n  Passed         : ");
    sc_prt_dec(result->passed);
    sc_prt("\n  Warnings       : ");
    sc_prt_dec(result->warnings);
    sc_prt("\n  Critical       : ");
    sc_prt_dec(result->critical);
    sc_prt("\n  Tasks Validated: ");
    sc_prt_dec(result->tasks_validated);
    sc_prt("\n  Stack Overflow : ");
    sc_prt_dec(result->stack_overflows);
    sc_prt("\n  Heap Corruption: ");
    sc_prt_dec(result->heap_corruptions);
    sc_prt("\n  Syscall Violations: ");
    sc_prt_dec(result->syscall_violations);
    sc_prt("\n  Memory Leaks     : ");
    sc_prt_dec(result->memory_leaks);
    sc_prt("\n");
    sc_line();
}

int securchk_get_overall_status(securchk_result_t* result) {
    if (!result) return -1;
    if (result->critical > 0) return SEC_CRITICAL;
    if (result->warnings > 0) return SEC_WARN;
    return SEC_OK;
}
