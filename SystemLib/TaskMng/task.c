#include "task.h"
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/TimeMng/time.h>
#include <OpenKernel/Drivers/PIT/pit.h>

static task_t* task_list = 0;
static task_t* current = 0;
static uint32_t next_id = 1;

/*
 * Upper bound on a walk through the circular task list. The list is only ever
 * built by task_add(), so no honest run comes close to this; it exists so a
 * corrupted or non-circular link cannot spin the kernel forever.
 */
#define TASK_WALK_LIMIT 4096u

void taskmng_init() {
    task_list = 0;
    current = 0;
    next_id = 1;
}

task_t* task_create(void (*entry)(void), uint32_t stack_size) {
    task_t* task;

    if (!entry || stack_size == 0) return 0;

    task = (task_t*)kmalloc(sizeof(task_t));
    if (!task) return 0;

    task->id = next_id++;
    task->entry = entry;

    task->stack_size = stack_size;
    task->stack = kmalloc(stack_size);

    /* Without a stack the task is unusable: bail out instead of handing back
       a structure whose esp would point at nothing. */
    if (!task->stack) {
        kfree(task);
        return 0;
    }

    task->esp = 0;
    task->ebp = 0;
    task->eip = (uint32_t)entry;

    task->state = TASK_READY;
    task->next = 0;

    return task;
}

void task_destroy(task_t* task) {
    if (!task) return;

    if (task->stack)
        kfree(task->stack);

    kfree(task);
}

void task_add(task_t* task) {
    task_t* tmp;
    uint32_t guard = 0;

    if (!task) return;

    if (!task_list) {
        task_list = task;
        task->next = task;
        current = task;
        return;
    }

    tmp = task_list;

    for (;;) {
        /* Inserting a task that is already linked would corrupt the circle. */
        if (tmp == task) return;
        /* tmp is the tail once it points back at the head. */
        if (tmp->next == task_list) break;
        /* A non-circular chain would otherwise loop here forever. */
        if (++guard > TASK_WALK_LIMIT) return;

        tmp = tmp->next;
    }

    tmp->next = task;
    task->next = task_list;
}

void task_remove(task_t* task) {
    if (!task_list || !task) return;

    task_t* prev = task_list;
    task_t* cur = task_list;
    uint32_t guard = 0;

    do {
        if (cur == task) {

            /* Only a single-element list is its own predecessor. The old test
               compared prev with cur on the first pass, where they are always
               equal, so removing the head wiped the whole list. */
            if (task_list->next == task_list) {
                task_list = 0;
                current = 0;
            } else {
                prev->next = cur->next;

                if (task_list == cur)
                    task_list = cur->next;

                if (current == cur)
                    current = cur->next;
            }

            break;
        }

        prev = cur;
        cur = cur->next;

        if (++guard > TASK_WALK_LIMIT) break;
    } while (cur != task_list);
}

void schedule() {
    if (!current)
        return;

    current = current->next;
}

void yield() {
    if (!current)
        return;

    schedule();

    if (current && current->entry) {
        current->state = TASK_RUNNING;
        current->entry();
    }
}

task_t* task_current() {
    return current;
}

/* Read-only access to the circular task list head for external checks */
task_t* task_list_head() {
    return task_list;
}

void task_block() {
    if (!current)
        return;

    current->state = TASK_BLOCKED;
    schedule();
}

void task_unblock(task_t* task) {
    if (!task)
        return;

    task->state = TASK_READY;
}

/* The argument is milliseconds, but sleep() takes seconds. Going through
       the PIT keeps the unit honest instead of rounding every sub-second
       sleep down to zero. */
void task_sleep(uint32_t ms) {
    pit_sleep_ms(ms);
}