#include "scheduler.h"
#include "serial.h"
#include "pmm.h"
#include "vmm.h"
#include "sync.h"
#include "tss.h"
#include "proc.h"
#include <stddef.h>

#define STACK_SIZE 4096

extern uint64_t kernel_cr3;
extern process_t *current_process;

static spinlock_t sched_lock = 0;
thread_t *current_thread = NULL;
static thread_t *ready_queue = NULL;
static int thread_count = 0;

extern void __switch_to(thread_t *old, thread_t *new, uint64_t new_cr3);
extern void userspace_entry(void);

static void idle_thread(void) {
    for (;;) { __asm__ volatile ("sti; hlt"); }
}

void scheduler_init(void) {
    thread_t *idle = thread_create(idle_thread, "idle", kernel_cr3, NULL);
    if (!idle) {
        serial_write("[SCHED] Failed to create idle thread\n");
        return;
    }
    idle->state = THREAD_STATE_READY;
    current_thread = idle;
    serial_printf("[SCHED] Idle thread created, total threads: %d\n", thread_count);
}

thread_t *thread_create(void (*entry)(void), const char *name, uint64_t cr3,
                        struct process *proc) {
    thread_t *t = kmalloc(sizeof(thread_t));
    if (!t) {
        serial_write("[SCHED] kmalloc failed for thread\n");
        return NULL;
    }

    void *stack = kmalloc(STACK_SIZE);
    if (!stack) {
        serial_write("[SCHED] stack alloc failed for thread\n");
        kfree(t);
        return NULL;
    }

    t->kernel_stack = (uint64_t)stack + STACK_SIZE;
    t->cr3   = cr3;
    t->iopl  = 0;
    t->process = proc;

    uint64_t *stk = (uint64_t *)t->kernel_stack - 7;
    stk[0] = 0;
    stk[1] = 0;
    stk[2] = 0;
    stk[3] = 0;
    stk[4] = 0;
    stk[5] = 0;
    stk[6] = (uint64_t)entry;

    t->rsp   = (uint64_t)stk;
    t->state = THREAD_STATE_READY;

    scheduler_add_thread(t);
    serial_printf("[SCHED] Created thread '%s', total: %d\n", name, thread_count);
    return t;
}

thread_t *thread_create_user(uint64_t user_entry, uint64_t user_stack,
                             uint64_t pml4_phys, struct process *proc) {
    thread_t *t = kmalloc(sizeof(thread_t));
    if (!t) return NULL;

    void *kstack = kmalloc(STACK_SIZE);
    if (!kstack) { kfree(t); return NULL; }

    t->kernel_stack = (uint64_t)kstack + STACK_SIZE;
    t->cr3   = pml4_phys;
    t->iopl  = 0;
    t->process = proc;

    uint64_t *stk = (uint64_t *)t->kernel_stack;

    stk -= 5;
    stk[0] = 0x23;
    stk[1] = user_stack;
    stk[2] = 0x202;
    stk[3] = 0x1B;
    stk[4] = user_entry;

    stk--;
    *stk = (uint64_t)userspace_entry;

    stk -= 6;
    for (int i = 0; i < 6; i++) stk[i] = 0;

    t->rsp   = (uint64_t)stk;
    t->state = THREAD_STATE_READY;

    scheduler_add_thread(t);
    serial_printf("[SCHED] Created user thread (entry=0x%x), total: %d\n",
                  user_entry, thread_count);
    return t;
}

void thread_exit(void) {
    spin_lock(&sched_lock);
    if (current_thread) {
        current_thread->state = 0;
        current_thread->next = NULL;
    }
    spin_unlock(&sched_lock);
    schedule();
    __builtin_unreachable();
}

void thread_block(void) {
    spin_lock(&sched_lock);
    current_thread->state = THREAD_STATE_BLOCKED;
    spin_unlock(&sched_lock);
    schedule();
}

void thread_unblock(thread_t *t) {
    spin_lock(&sched_lock);
    if (t->state == THREAD_STATE_BLOCKED) {
        t->state = THREAD_STATE_READY;
        t->next = NULL;
        if (ready_queue) {
            thread_t *tail = ready_queue;
            while (tail->next) tail = tail->next;
            tail->next = t;
        } else {
            ready_queue = t;
        }
    }
    spin_unlock(&sched_lock);
}

void schedule(void) {
    static int call_count = 0;
    spin_lock(&sched_lock);

    if (!current_thread) {
        spin_unlock(&sched_lock);
        return;
    }

    thread_t *next = ready_queue;
    if (!next) {
        spin_unlock(&sched_lock);
        return;
    }

    if (call_count < 3) {
        serial_printf("[SCHED] schedule #%d: next=0x%x\n", call_count, (uint64_t)next);
        call_count++;
    }

    ready_queue = next->next;

    if (current_thread->state == THREAD_STATE_RUNNING) {
        current_thread->state = THREAD_STATE_READY;
        current_thread->next = NULL;
        if (ready_queue) {
            thread_t *tail = ready_queue;
            while (tail->next) tail = tail->next;
            tail->next = current_thread;
        } else {
            ready_queue = current_thread;
        }
    }

    next->state = THREAD_STATE_RUNNING;

    if (next->cr3 != kernel_cr3) {
        tss_set_rsp0(next->kernel_stack);
    }

    thread_t *prev = current_thread;
    uint64_t new_cr3 = next->cr3;
    current_thread = next;

    if (next->process)
        current_process = next->process;

    spin_unlock(&sched_lock);
    __switch_to(prev, next, new_cr3);
}

void enable_interrupts(void) { __asm__ volatile ("sti"); }

void scheduler_add_thread(thread_t *t) {
    spin_lock(&sched_lock);
    t->next = ready_queue;
    ready_queue = t;
    thread_count++;
    spin_unlock(&sched_lock);
}