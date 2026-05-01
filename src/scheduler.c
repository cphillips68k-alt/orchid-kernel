#include "scheduler.h"
#include "serial.h"
#include "pmm.h"
#include "vmm.h"
#include "sync.h"
#include <stddef.h>

#define STACK_SIZE 4096

static thread_t *ready_queue = NULL;
thread_t *current_thread = NULL;
static spinlock_t sched_lock = 0;

/* Idle thread – runs when nothing else is ready */
static void idle_thread(void) {
    for (;;) {
        __asm__ volatile ("sti; hlt");
    }
}

extern void __switch_to(thread_t *old, thread_t *new);

void scheduler_init(void) {
    thread_t *idle = thread_create(idle_thread, "idle");
    if (!idle) {
        serial_write("[SCHED] Failed to create idle thread\n");
        return;
    }
    idle->state = THREAD_STATE_READY;
    current_thread = idle;
}

thread_t *thread_create(void (*entry)(void), const char *name) {
    (void)name; /* reserved for future use */

    /* Allocate TCB */
    thread_t *t = kmalloc(sizeof(thread_t));
    if (!t) return NULL;

    /* Allocate kernel stack */
    void *stack = kmalloc(STACK_SIZE);
    if (!stack) {
        kfree(t);
        return NULL;
    }

    t->kernel_stack = (uint64_t)stack + STACK_SIZE;

    /*
     * Set up initial stack frame as if the thread was interrupted.
     * The CPU pushes SS, RSP, RFLAGS, CS, RIP.
     * The ISR stub then pushes 15 general-purpose registers,
     * interrupt number, and an error code.
     * Total: 22 qwords (5 + 2 + 15).
     */
    uint64_t *frame = (uint64_t *)t->kernel_stack - 22;
    for (int i = 0; i < 15; i++) frame[i] = 0;  /* zero out regs */
    frame[15] = 0;   /* int_no (unused) */
    frame[16] = 0;   /* err_code (unused) */

    /* IRET frame */
    frame[17] = (uint64_t)entry;   /* RIP */
    frame[18] = 0x08;              /* CS (kernel code selector) */
    frame[19] = 0x202;             /* RFLAGS (IF=1) */
    frame[20] = (uint64_t)frame;   /* RSP after iret (point to this frame) */
    frame[21] = 0x10;              /* SS (kernel data selector) */

    t->rsp = (uint64_t)frame;
    t->state = THREAD_STATE_READY;

    /* Insert into ready queue */
    spin_lock(&sched_lock);
    t->next = ready_queue;
    ready_queue = t;
    spin_unlock(&sched_lock);

    return t;
}

void thread_exit(void) {
    spin_lock(&sched_lock);
    if (current_thread) {
        current_thread->state = 0;   /* dead */
        current_thread->next = NULL;
    }
    spin_unlock(&sched_lock);

    /* Give up CPU – never returns */
    schedule();
    __builtin_unreachable();
}

void schedule(void) {
    spin_lock(&sched_lock);

    if (!current_thread) {
        spin_unlock(&sched_lock);
        return;
    }

    thread_t *next = ready_queue;
    if (!next) {
        spin_unlock(&sched_lock);
        return;   /* nothing to run (should never happen) */
    }

    /* Remove head of ready queue */
    ready_queue = next->next;

    /* If current thread is still runnable, add it to tail */
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
    thread_t *prev = current_thread;
    current_thread = next;

    spin_unlock(&sched_lock);

    __switch_to(prev, next);
}

void enable_interrupts(void) {
    __asm__ volatile ("sti");
}