#ifndef SECURITY_H
#define SECURITY_H

#include <OpenKernel/SystemLib/Std/types.h>

#define SECURCHK_MAX_TASKS       256
#define SECURCHK_MAX_SYSCALLS    32
#define SECURCHK_STACK_CANARY    0xDEADBEEF
#define SECURCHK_HEAP_MAGIC      0xCAFEBABE

typedef enum {
    SEC_OK = 0,
    SEC_WARN,
    SEC_CRITICAL,
    SEC_PANIC
} securchk_severity_t;

typedef struct {
    int severity;
    const char* module;
    const char* message;
    uint32_t detail;
} securchk_issue_t;

typedef struct {
    uint32_t total_checks;
    uint32_t passed;
    uint32_t warnings;
    uint32_t critical;
    uint32_t tasks_validated;
    uint32_t stack_overflows;
    uint32_t heap_corruptions;
    uint32_t syscall_violations;
    uint32_t memory_leaks;
} securchk_result_t;

void securchk_init(void);
void securchk_run(securchk_result_t* result);

int  securchk_check_task_integrity(securchk_result_t* result);
int  securchk_check_stack_overflow(securchk_result_t* result);
int  securchk_check_heap_integrity(securchk_result_t* result);
int  securchk_check_syscall_bounds(securchk_result_t* result);
int  securchk_check_memory_safety(securchk_result_t* result);
int  securchk_check_driver_state(securchk_result_t* result);
int  securchk_check_kernel_structures(securchk_result_t* result);

void securchk_print_result(securchk_result_t* result);
void securchk_log_issue(securchk_severity_t severity, const char* module, const char* msg, uint32_t detail);
int  securchk_get_overall_status(securchk_result_t* result);

void securchk_stack_canary_set(uint32_t* stack, uint32_t stack_size);
int  securchk_stack_canary_check(uint32_t* stack, uint32_t stack_size);

#endif