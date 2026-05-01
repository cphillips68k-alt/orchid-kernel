#include "scheduler.h"
#include "serial.h"
#include <stddef.h>

#define STACK_SIZE 4096

/* Static stack and TCB pools */
static uint8_t stacks[16][STACK_SIZE] __attribute__((aligned(16)));
static thread_t tcb[16];
static int next_tcb = 0;

static thread_t *ready_queue = NULL;
thread_t *current_thread = NULL;

/* Idle thread - runs when nothing else is ready */
static void idle_thread(void) {
    for (;;) {
        __asm__ volatile ("sti; hlt");
    }
}

extern void __switch_to(thread_t *old, thread_t *new);

void scheduler_init(void) {
    /* Create idle thread */
    thread_t *idle = thread_create(idle_thread, "idle");
    idle->state = THREAD_STATE_READY;
    current_thread = idle;
}

thread_t *thread_create(void (*entry)(void), const char *name) {
    (void)name; /* Name is reserved for future use */

    if (next_tcb >= 16) return NULL;

    thread_t *t = &tcb[next_tcb];
    t->kernel_stack = (uint64_t)&stacks[next_tcb][STACK_SIZE];
    uint64_t *stack = (uint64_t *)t->kernel_stack;

    /*
     * Set up initial stack frame as if the thread was interrupted
     * by the timer IRQ. The CPU pushed SS, RSP, RFLAGS, CS, RIP.
     * The ISR stub then pushed 15 general-purpose registers,
     * the interrupt number, and a dummy error code.
     * Total: 5 (iret frame) + 2 (int_no + err) + 15 (regs) = 22 qwords.
     */
    uint64_t *frame = stack - 22;

    /* Zero all general-purpose registers */
    for (int i = 0; i < 15; i++) {
        frame[i] = 0;
    }
    frame[15] = 0;   /* int_no (unused) */
    frame[16] = 0;   /* err_code (unused) */

    /* iret frame - this is what iretq will pop */
    frame[17] = (uint64_t)entry;        /* RIP */
    frame[18] = 0x08;                   /* CS (kernel code selector) */
    frame[19] = 0x202;                  /* RFLAGS (IF=1, interrupts enabled) */
    frame[20] = (uint64_t)(stack - 22); /* RSP (point to this frame) */
    frame[21] = 0x10;                   /* SS (kernel data selector) */

    t->rsp = (uint64_t)frame;
    t->state = THREAD_STATE_READY;

    /* Insert into ready queue */
    t->next = ready_queue;
    ready_queue = t;

    next_tcb++;
    return t;
}

void thread_exit(void) {
    /*
     * Mark the current thread as dead so schedule() won't
     * re-add it to the ready queue.
     */
    if (current_thread) {
        current_thread->state = 0;   /* dead */
        current_thread->next = NULL;
    }

    /* Give up the CPU - never returns */
    schedule();
    __builtin_unreachable();
}

void schedule(void) {
    if (!current_thread) return;

    thread_t *next = ready_queue;
    if (!next) return; /* Nothing to run (should never happen with idle thread) */

    /* Remove next thread from the head of the ready queue */
    ready_queue = next->next;

    /*
     * If the current thread is still running, add it to the
     * tail of the ready queue so it gets scheduled again later.
     * Dead threads (state == 0) are simply dropped.
     */
    if (current_thread->state == THREAD_STATE_RUNNING) {
        current_thread->state = THREAD_STATE_READY;
        current_thread->next = NULL;

        if (ready_queue) {
            thread_t *tail = ready_queue;
            while (tail->next) {
                tail = tail->next;
            }
            tail->next = current_thread;
        } else {
            ready_queue = current_thread;
        }
    }

    next->state = THREAD_STATE_RUNNING;
    thread_t *prev = current_thread;
    current_thread = next;

    /*
     * __switch_to saves the current CPU context to prev->rsp,
     * loads next->rsp into RSP, and returns. From the perspective
     * of the next thread, it simply returns from __switch_to
     * and continues where it left off (or starts fresh if new).
     */
    __switch_to(prev, next);
}

void enable_interrupts(void) {
    __asm__ volatile ("sti");
}