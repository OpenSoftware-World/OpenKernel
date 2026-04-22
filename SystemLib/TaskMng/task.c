#include "task.h"
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/TimeMng/time.h>

static task_t* task_list = 0;
static task_t* current = 0;
static uint32_t next_id = 1;

void taskmng_init() {
    task_list = 0;
    current = 0;
    next_id = 1;
}

task_t* task_create(void (*entry)(void), uint32_t stack_size) {
    task_t* task = (task_t*)kmalloc(sizeof(task_t));
    if (!task) return 0;

    task->id = next_id++;
    task->entry = entry;

    task->stack_size = stack_size;
    task->stack = kmalloc(stack_size);

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
    if (!task) return;

    if (!task_list) {
        task_list = task;
        task->next = task;
        current = task;
        return;
    }

    task_t* tmp = task_list;

    while (tmp->next != task_list)
        tmp = tmp->next;

    tmp->next = task;
    task->next = task_list;
}

void task_remove(task_t* task) {
    if (!task_list || !task) return;

    task_t* prev = task_list;
    task_t* cur = task_list;

    do {
        if (cur == task) {

            if (cur == prev) {
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

void task_sleep(uint32_t ms) {
    sleep(ms);
}