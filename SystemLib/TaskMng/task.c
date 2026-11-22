#include "task.h"
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/TimeMng/time.h>
#include <OpenKernel/Drivers/PIT/pit.h>
#include <OpenKernel/Drivers/Cpu/cpu.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/Components/InterruptServices/pic.h>

static task_t* task_list = 0;
static task_t* current = 0;
static uint32_t next_id = 1;

/*
 * The boot context wrapped as a task. Static, not heap allocated: it must
 * outlive every other task and it runs on the linker-provided boot stack,
 * not on a kmalloc()ed one.
 */
static task_t idle_task;

/*
 * Bounds of the single boot stack (defined by link.ld). The idle task
 * keeps executing on it, so these are the window its esp/ebp are checked
 * against by the SecurityChecker.
 */
extern uint8_t _stack_start[], _stack_end[];

/*
 * Upper bound on a walk through the circular task list. The list is only ever
 * built by task_add(), so no honest run comes close to this; it exists so a
 * corrupted or non-circular link cannot spin the kernel forever.
 */
#define TASK_WALK_LIMIT 4096u

/*
 * Smallest stack a task may be created with. One interrupt frame is already
 * 64 bytes, and the handler plus its call depth needs room on top of that,
 * so anything below this would overflow on the very first tick.
 */
#define TASK_MIN_STACK_SIZE 1024u

/*
 * Swap the register context of two tasks. This is the only place the CPU
 * ever changes stacks outside of an interrupt frame.
 *
 * cdecl bookkeeping makes this safe: the caller sees a normal call, so it
 * already assumes eax/ecx/edx are clobbered and that ebx/esi/edi/ebp come
 * back unchanged - which they do, because each context saves and restores
 * its own copy. pushf/cli runs before the swap so the actual exchange is
 * never interleaved with a tick; the flags captured here are restored on
 * resume, so a task that switched from interrupt context comes back with
 * IF=0 (its dispatch still has to finish) and one that yielded comes back
 * with whatever it had.
 *
 * naked: no compiler prologue/epilogue - the saved stack pointer must
 * point exactly at our pushes, and `ret` must pop the return address the
 * *resumed* context made when it called us.
 */
__attribute__((naked, noinline)) static void context_switch(uint32_t*, uint32_t) {
    __asm__ __volatile__(
        "pushfl\n\t"
        "cli\n\t"
        "pushl %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl 24(%esp), %eax\n\t"   /* save_esp: ret + 4 args pushed, so +4+20 */
        "movl 28(%esp), %edx\n\t"   /* load_esp */
        "movl %esp, (%eax)\n\t"     /* suspend the outgoing context */
        "movl %edx, %esp\n\t"       /* resume the incoming one */
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "popl %ebp\n\t"
        "popfl\n\t"
        "ret\n\t"
    );
}

/*
 * First code a new task executes: run the entry point and end the task if
 * the entry function ever returns. Entered via the return address baked
 * into the initial stack frame by task_create().
 */
static void task_entry_trampoline(void) {
    current->entry();
    task_exit();
}

/* Idle task body: also where task_start() parks the boot context. */
static void task_idle_entry(void) {
    for (;;)
        hlt();
}

/* Hand out task ids with interrupts off: next_id++ is a read-modify-write
   a tick could otherwise interleave between two creators. */
static uint32_t task_alloc_id(void) {
    uint32_t id;
    uint32_t flags = eflags_r();

    cli();
    id = next_id++;
    if (flags & EFLAGS_IF) sti();

    return id;
}

/*
 * Round-robin: the READY task after the running one. Returns 0 when no
 * other task is runnable, in which case the caller keeps the current one.
 * Bounded walk so a corrupted circle cannot hang the scheduler; a NULL
 * link is treated the same way.
 */
static task_t* task_pick_next(void) {
    task_t* start;
    task_t* t;
    uint32_t guard = 0;

    if (!current || !current->next) return 0;

    start = current->next;
    t = start;

    do {
        if (t->state == TASK_READY) return t;

        t = t->next;
        if (!t) return 0;
        if (++guard > TASK_WALK_LIMIT) return 0;
    } while (t != start);

    return 0;
}

/*
 * Hand the CPU to `next`. Interrupts must already be off (interrupt gate
 * or an explicit cli): updating `current` and exchanging the stacks is
 * one atomic step from the scheduler's point of view - a tick landing in
 * between would save the outgoing stack under the wrong task. Returns
 * only after this task is picked again.
 */
static void task_switch_off(task_t* next) {
    task_t* prev = current;

    if (!next || next == prev) return;

    /* A task that blocked or exited keeps that state; only a running one
       goes back to the run queue. */
    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    current = next;

    context_switch(&prev->esp, next->esp);
}

/* Wake sleepers whose deadline has passed. Runs from the timer handler
   with interrupts off; the comparison is wrap-safe across the ~49 day
   rollover of the PIT millisecond counter. */
static void task_wake_sleepers(void) {
    task_t* t = task_list;
    uint32_t now;
    uint32_t guard = 0;

    if (!t) return;
    now = pit_get_uptime_ms();

    do {
        if (t->state == TASK_BLOCKED && t->wake_ms != 0 &&
            (int32_t)(now - t->wake_ms) >= 0)
            t->state = TASK_READY;

        t = t->next;
        if (!t) return;
        if (++guard > TASK_WALK_LIMIT) return;
    } while (t != task_list);
}

/*
 * Cooperative half of the scheduler: pick another runnable task and switch
 * to it, returning only when this task runs again. Used by yield, sleep,
 * block and exit. The timer handler takes the same road but with
 * interrupts already off, so it does not need this wrapper.
 */
static void task_reschedule(void) {
    uint32_t flags;
    task_t* next;

    if (!current) return;

    flags = eflags_r();
    cli();

    next = task_pick_next();
    if (next) task_switch_off(next);

    /* Restored here, on this task's own stack, after it is scheduled
       again: whatever IF the switch started with is put back. */
    if (flags & EFLAGS_IF) sti();
}

/*
 * IRQ0 tick: wake expired sleeps and preempt the running task.
 *
 * interrupt_dispatch() sends the EOI only after every handler on the line
 * has returned, so a switch that leaves the handler list mid-way would
 * never let it get there - and a PIC without EOI stops delivering ticks,
 * which means the preempted task is never resumed either. Acknowledge
 * IRQ0 here before switching. The EOI dispatch() then sends when this
 * task resumes finds an empty in-service register, which the 8259A
 * ignores; it cannot hit another device's bit because any other dispatch
 * would have to be running on this same CPU, and it never is (IF=0 for
 * the whole handler list).
 */
static void scheduler_tick(registers_t* regs) {
    task_t* next;

    (void)regs;
    if (!current) return;

    task_wake_sleepers();

    next = task_pick_next();
    if (!next) return;      /* nobody else to run: dispatch() owns the EOI */

    pic_send_eoi(PIT_IRQ_LINE);
    task_switch_off(next);  /* comes back only when this task is resumed */
}

void taskmng_init() {
    task_list = 0;
    current = 0;
    next_id = 1;
}

task_t* task_create(void (*entry)(void), uint32_t stack_size) {
    task_t* task;
    uint32_t* sp;
    uint32_t top;

    if (!entry || stack_size < TASK_MIN_STACK_SIZE) return 0;

    task = (task_t*)kmalloc(sizeof(task_t));
    if (!task) return 0;

    task->id = task_alloc_id();
    task->entry = entry;

    task->stack_size = stack_size;
    task->stack = kmalloc(stack_size);

    /* Without a stack the task is unusable: bail out instead of handing back
       a structure whose esp would point at nothing. */
    if (!task->stack) {
        kfree(task);
        return 0;
    }

    /*
     * Build the frame context_switch() expects the first time this task
     * runs: the four callee-saved registers it pops, EFLAGS with IF set
     * (a fresh task must be preemptible too) and a return address into
     * the entry trampoline. The stack top is rounded down to 16 bytes and
     * the frame sits 28 bytes below it, so the trampoline observes the
     * ABI's entry alignment (esp = top - 4 = 12 mod 16).
     */
    top = ((uint32_t)task->stack + stack_size) & ~15u;
    sp = (uint32_t*)(top - 28);
    sp[0] = 0;              /* edi */
    sp[1] = 0;              /* esi */
    sp[2] = 0;              /* ebx */
    sp[3] = 0;              /* ebp */
    sp[4] = 0x202;          /* EFLAGS: IF | the always-set bit 1 */
    sp[5] = (uint32_t)task_entry_trampoline;

    task->esp = (uint32_t)sp;
    task->ebp = 0;
    task->eip = (uint32_t)entry;

    task->state = TASK_READY;
    task->wake_ms = 0;
    task->next = 0;

    return task;
}

void task_destroy(task_t* task) {
    if (!task) return;

    /* The running task and the idle task cannot be destroyed: the first
       would leave `current` dangling, the second is not heap allocated. */
    if (task == current || task == &idle_task) return;

    task_remove(task);

    if (task->stack)
        kfree(task->stack);

    kfree(task);
}

void task_add(task_t* task) {
    task_t* tmp;
    uint32_t guard = 0;
    uint32_t flags;

    if (!task) return;

    /*
     * The timer handler walks this list with interrupts off, so build it
     * the same way: a tick must never see a half-linked node. `current`
     * is deliberately not touched - it has to name the task that is
     * actually running, and only task_start() knows which one that is.
     */
    flags = eflags_r();
    cli();

    if (!task_list) {
        task_list = task;
        task->next = task;
    } else {
        tmp = task_list;

        for (;;) {
            /* Inserting a task that is already linked would corrupt the circle. */
            if (tmp == task) break;
            /* tmp is the tail once it points back at the head. */
            if (tmp->next == task_list) {
                tmp->next = task;
                task->next = task_list;
                break;
            }
            /* A non-circular chain would otherwise loop here forever. */
            if (++guard > TASK_WALK_LIMIT) break;

            tmp = tmp->next;
        }
    }

    if (flags & EFLAGS_IF) sti();
}

void task_remove(task_t* task) {
    task_t* prev;
    task_t* cur;
    uint32_t guard = 0;
    uint32_t flags;

    if (!task_list || !task) return;

    /* Removing the running task would strand its saved context outside
       the list, where the scheduler could never pick it up again; the
       idle task is the fallback the scheduler falls back to when
       everything else blocks, so it must stay too. */
    if (task == current || task == &idle_task) return;

    flags = eflags_r();
    cli();

    prev = 0;
    cur = task_list;

    do {
        if (cur == task) {
            if (cur->next == cur) {
                /* Only a single-element list is its own predecessor. */
                task_list = 0;
            } else if (cur == task_list) {
                /*
                 * Head removal: the tail still links back to it. Relinking
                 * only task_list (or only prev, which equals the head on the
                 * first pass and makes prev->next a no-op) leaves the removed
                 * node reachable through the circle, so the scheduler keeps
                 * running a task that was supposedly destroyed.
                 */
                task_t* tail = task_list;
                uint32_t tail_guard = 0;

                while (tail->next != task_list) {
                    tail = tail->next;
                    if (++tail_guard > TASK_WALK_LIMIT) break;
                }

                tail->next = cur->next;
                task_list = cur->next;
            } else {
                prev->next = cur->next;
            }

            break;
        }

        prev = cur;
        cur = cur->next;

        if (++guard > TASK_WALK_LIMIT) break;
    } while (cur != task_list);

    if (flags & EFLAGS_IF) sti();
}

/* Switch to the next runnable task, if any. Returns only after this task
   is scheduled again. */
void schedule() {
    task_reschedule();
}

/* Voluntarily give the CPU up; same path as schedule(). */
void yield() {
    task_reschedule();
}

task_t* task_current() {
    return current;
}

/* Read-only access to the circular task list head for external checks */
task_t* task_list_head() {
    return task_list;
}

void task_block() {
    if (!current) return;

    current->wake_ms = 0;   /* an explicit block has no timer wake-up */
    current->state = TASK_BLOCKED;
    task_reschedule();
}

void task_unblock(task_t* task) {
    if (!task) return;

    /* Only a blocked task may be revived: handing the scheduler a
       terminated context again would resume a stack that gave up. */
    if (task->state == TASK_BLOCKED)
        task->state = TASK_READY;
}

/*
 * Block the calling task for at least ms milliseconds; the IRQ0 handler
 * wakes it, so the CPU runs other tasks meanwhile. Must not be called
 * from an interrupt handler (the EOI problem above applies to every
 * switch, not just the timer's). Without a running scheduler this falls
 * back to the PIT busy-wait, keeping the pre-task_start behaviour.
 */
void task_sleep(uint32_t ms) {
    uint32_t wake;

    if (!current) {
        pit_sleep_ms(ms);
        return;
    }

    if (ms == 0) return;

    wake = pit_get_uptime_ms() + ms;
    /* 0 marks "no deadline" for task_block(); nudge the pathological
       wrap-around deadline off it - one millisecond late, once per ~49
       days, beats never waking up. */
    if (wake == 0) wake = 1;

    current->wake_ms = wake;
    current->state = TASK_BLOCKED;
    task_reschedule();
}

void task_exit() {
    if (!current) return;

    current->state = TASK_TERMINATED;
    task_reschedule();

    /* Normally unreachable: a TERMINATED task is never picked again, so
       task_reschedule() does not come back here. The loop is the
       backstop for a list that somehow holds nothing else - the next
       tick finds this task non-runnable and moves on to another. */
    for (;;)
        hlt();
}

/*
 * Turn the caller into the idle task and start preemption. The boot
 * context keeps running this function's hlt loop whenever nothing else
 * is runnable, which is exactly what an idle task should do - it is also
 * the only task that never blocks, so the scheduler always has a valid
 * fallback.
 */
void task_start() {
    if (current) return;    /* already started */

    idle_task.id = task_alloc_id();
    idle_task.entry = task_idle_entry;
    idle_task.stack = (void*)_stack_start;
    idle_task.stack_size = (uint32_t)(_stack_end - _stack_start);
    idle_task.esp = 0;
    idle_task.ebp = 0;
    idle_task.eip = 0;
    idle_task.state = TASK_RUNNING;
    idle_task.wake_ms = 0;

    task_add(&idle_task);
    current = &idle_task;

    /* IRQ0, registered after pit_init()'s own handler so the tick counter
       is updated before the scheduler looks at the clock. */
    interrupt_register_handler((uint8_t)(PIC_IRQ_BASE + PIT_IRQ_LINE), scheduler_tick);

    sti();
    task_idle_entry();      /* never returns */
}
