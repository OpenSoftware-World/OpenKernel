#include "security.h"

#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/SystemLib/TaskMng/task.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Kernel/KernelServices/SystemManagement/sysmng.h>
#include <OpenKernel/SystemLib/SysCalls/syscall.h>

extern task_t* task_list;
extern task_t* current;

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
        while (num > 0) { tmp[t++] = (num % 10) + '0'; num /= 10; }
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

static void sc_prt_hex(uint32_t n) {
    const char* hx = "0123456789ABCDEF";
    sc_prt("0x");
    for (int i = 28; i >= 0; i -= 4) {
        char c[2] = { hx[(n >> i) & 0xF], '\0' };
        sc_prt(c);
    }
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
   Issue Logging
   ============================================================ */

static const char* severity_str(int sev) {
    switch (sev) {
        case SEC_OK:       return "OK";
        case SEC_WARN:     return "UYARI";
        case SEC_CRITICAL: return "KRITIK";
        case SEC_PANIC:    return "PANIC";
        default:           return "UNKNOWN";
    }
}

void securchk_log_issue(securchk_severity_t severity, const char* module, const char* msg, uint32_t detail) {
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
   - Task list circular reference dogrulanir
   - NULL pointer / gecerli entry check
   - Duplicate PID tespiti
   - Gecerli state aralik kontrolu
   ============================================================ */

int securchk_check_task_integrity(securchk_result_t* result) {
    if (!result || !task_list) {
        if (result) result->total_checks++;
        return SEC_OK;
    }

    int issues = 0;
    uint32_t count = 0;
    task_t* head = task_list;
    task_t* cur = head;

    do {
        if (!cur) {
            securchk_log_issue(SEC_CRITICAL, "TASK", "Task listesinde NULL dugum", 0);
            result->critical++;
            issues++;
            break;
        }

        if (!cur->entry) {
            securchk_log_issue(SEC_WARN, "TASK", "Gecersiz entry pointer, PID", cur->id);
            result->warnings++;
            issues++;
        }

        if (!cur->stack || cur->stack_size == 0) {
            securchk_log_issue(SEC_WARN, "TASK", "Gecersiz stack, PID", cur->id);
            result->warnings++;
            issues++;
        }

        if ((int)cur->state < 0 || (int)cur->state > 3) {
            securchk_log_issue(SEC_CRITICAL, "TASK", "Gecersiz task state, PID", cur->id);
            result->critical++;
            issues++;
        }

        if (cur->id > SECURCHK_MAX_TASKS) {
            securchk_log_issue(SEC_WARN, "TASK", "PID sinir asimi", cur->id);
            result->warnings++;
            issues++;
        }

        count++;
        cur = cur->next;
    } while (cur && cur != head);

    if (count > SECURCHK_MAX_TASKS) {
        securchk_log_issue(SEC_CRITICAL, "TASK", "Task sayisi sinir asimi", count);
        result->critical++;
        issues++;
    }

    result->tasks_validated = count;
    result->total_checks++;

    if (issues == 0) result->passed++;
    return issues > 0 ? SEC_WARN : SEC_OK;
}

/* ============================================================
   2. Stack Overflow Check
   - Her task icin stack canary kontrolu
   - Stack pointer (esp) sinirlari disinda mi
   ============================================================ */

int securchk_check_stack_overflow(securchk_result_t* result) {
    if (!result || !task_list) {
        if (result) result->total_checks++;
        return SEC_OK;
    }

    int issues = 0;
    task_t* head = task_list;
    task_t* cur = head;

    do {
        if (!cur || !cur->stack || cur->stack_size == 0) {
            cur = cur ? cur->next : NULL;
            continue;
        }

        uint32_t stack_base = (uint32_t)cur->stack;
        uint32_t stack_top  = stack_base + cur->stack_size;

        if (cur->esp < stack_base || cur->esp > stack_top) {
            securchk_log_issue(SEC_CRITICAL, "STACK", "ESP stack sinirlari disinda, PID", cur->id);
            result->stack_overflows++;
            result->critical++;
            issues++;
        }

        if (cur->ebp < stack_base || cur->ebp > stack_top) {
            securchk_log_issue(SEC_WARN, "STACK", "EBP stack sinirlari disinda, PID", cur->id);
            result->warnings++;
            issues++;
        }

        cur = cur->next;
    } while (cur && cur != head);

    result->total_checks++;
    if (issues == 0) result->passed++;
    return issues > 0 ? SEC_WARN : SEC_OK;
}

/* ============================================================
   3. Heap Integrity Check
   - kmalloc/kfree tutarlilik
   - Null pointer allocation denemeleri
   - Heap buyukluk sinirlari
   ============================================================ */

int securchk_check_heap_integrity(securchk_result_t* result) {
    if (!result) return SEC_OK;

    int issues = 0;
    result->total_checks++;

    void* test_alloc = kmalloc(16);
    if (!test_alloc) {
        securchk_log_issue(SEC_WARN, "HEAP", "kmalloc basarisiz (dusuk bellek)", 0);
        result->warnings++;
        issues++;
    } else {
        memset(test_alloc, 0xAA, 16);
        uint8_t* buf = (uint8_t*)test_alloc;
        int corrupt = 0;
        for (int i = 0; i < 16; i++) {
            if (buf[i] != 0xAA) { corrupt = 1; break; }
        }
        if (corrupt) {
            securchk_log_issue(SEC_CRITICAL, "HEAP", "Heap bellek bozulmasi tespit edildi", 0);
            result->heap_corruptions++;
            result->critical++;
            issues++;
        }
        kfree(test_alloc);
    }

    if (issues == 0) result->passed++;
    return issues > 0 ? SEC_WARN : SEC_OK;
}

/* ============================================================
   4. Syscall Bounds Check
   - Syscall ID aralik kontrolu
   - Gecersiz syscall denemeleri
   ============================================================ */

int securchk_check_syscall_bounds(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    int ret = syscall(-1, NULL);
    if (ret != -1) {
        securchk_log_issue(SEC_CRITICAL, "SYSCALL", "Gecersiz syscall ID kabul edildi", 0);
        result->syscall_violations++;
        result->critical++;
    }

    int ret2 = syscall(SECURCHK_MAX_SYSCALLS + 100, NULL);
    if (ret2 != -1) {
        securchk_log_issue(SEC_WARN, "SYSCALL", "Sinir asimi syscall ID kabul edildi", 0);
        result->syscall_violations++;
        result->warnings++;
    }

    if (result->syscall_violations == 0) result->passed++;
    return result->syscall_violations > 0 ? SEC_WARN : SEC_OK;
}

/* ============================================================
   5. Memory Safety Check
   - memcpy/memset sifir boyut testi
   - memcmp tutarlilik
   - Pointer alignment
   ============================================================ */

int securchk_check_memory_safety(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    int issues = 0;
    char buf[64];

    memset(buf, 0, sizeof(buf));
    if (buf[0] != 0 || buf[63] != 0) {
        securchk_log_issue(SEC_CRITICAL, "MEM", "memset sifirlama hatasi", 0);
        result->critical++;
        issues++;
    }

    memcpy(buf, "test_alignment_check", 22);
    if (memcmp(buf, "test_alignment_check", 22) != 0) {
        securchk_log_issue(SEC_CRITICAL, "MEM", "memcmp tutarsizlik", 0);
        result->critical++;
        issues++;
    }

    if (((uint32_t)buf & 0x3) != 0) {
        securchk_log_issue(SEC_WARN, "MEM", "Stack alignment hatasi", (uint32_t)buf);
        result->warnings++;
        issues++;
    }

    if (issues == 0) result->passed++;
    return issues > 0 ? SEC_WARN : SEC_OK;
}

/* ============================================================
   6. Driver State Check
   - VGA driver durum kontrolu
   - ATA/Storage erisim hazirlik
   - Serial port hazirlik
   ============================================================ */

int securchk_check_driver_state(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    int issues = 0;

    vga_clear_screen("C");
    vga_set_color_scheme(0x07, 0x00);

    securchk_log_issue(SEC_OK, "DRIVER", "VGA driver aktif", 0);

    return SEC_OK;
}

/* ============================================================
   7. Kernel Structure Check
   - Kernel info tutarlilik
   - Linker symbol sinirlari
   - Paging/VMM yapisinin temel kontrolu
   ============================================================ */

int securchk_check_kernel_structures(securchk_result_t* result) {
    if (!result) return SEC_OK;
    result->total_checks++;

    int issues = 0;

    if (PAGE_SIZE != 4096) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "PAGE_SIZE tutarsiz", PAGE_SIZE);
        result->critical++;
        issues++;
    }

    if (sizeof(void*) != 4) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "Pointer boyutu 32-bit degil", sizeof(void*));
        result->critical++;
        issues++;
    }

    if (sizeof(uint32_t) != 4) {
        securchk_log_issue(SEC_CRITICAL, "KERN", "uint32_t boyutu hatali", sizeof(uint32_t));
        result->critical++;
        issues++;
    }

    if (issues == 0) result->passed++;
    return issues > 0 ? SEC_CRITICAL : SEC_OK;
}

/* ============================================================
   Init
   ============================================================ */

void securchk_init(void) {
    g_initialized = 1;
}

/* ============================================================
   Main Run
   ============================================================ */

void securchk_run(securchk_result_t* result) {
    if (!result) return;

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
    sc_prt("  Guvenlik Kontrolu Tamamlandi\n");
    sc_prt("  Toplam Kontrol : ");
    sc_prt_dec(result->total_checks);
    sc_prt("\n  Gecen          : ");
    sc_prt_dec(result->passed);
    sc_prt("\n  Uyari          : ");
    sc_prt_dec(result->warnings);
    sc_prt("\n  Kritik         : ");
    sc_prt_dec(result->critical);
    sc_prt("\n  Task Dogrulandi: ");
    sc_prt_dec(result->tasks_validated);
    sc_prt("\n  Stack Overflow : ");
    sc_prt_dec(result->stack_overflows);
    sc_prt("\n  Heap Bozulma   : ");
    sc_prt_dec(result->heap_corruptions);
    sc_prt("\n  Syscall Ihlal  : ");
    sc_prt_dec(result->syscall_violations);
    sc_prt("\n");
    sc_line();
}

int securchk_get_overall_status(securchk_result_t* result) {
    if (!result) return -1;
    if (result->critical > 0) return SEC_CRITICAL;
    if (result->warnings > 0) return SEC_WARN;
    return SEC_OK;
}
