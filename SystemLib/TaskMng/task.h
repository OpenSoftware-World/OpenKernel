#ifndef TASK_H
#define TASK_H

#include <OpenKernel/SystemLib/Std/types.h>

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

typedef struct task {
    uint32_t id;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
    void (*entry)(void);
    void* stack;
    uint32_t stack_size;
    task_state_t state;
    struct task* next;
} task_t;

void taskmng_init();
task_t* task_create(void (*entry)(void), uint32_t stack_size);
void task_destroy(task_t* task);
void task_add(task_t* task);
void task_remove(task_t* task);
void schedule();
void yield();
task_t* task_current();
void task_sleep(uint32_t ms);
void task_block();
void task_unblock(task_t* task);

#endif