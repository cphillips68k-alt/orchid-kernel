#include "scheduler.h"
#include "serial.h"
#include <stddef.h>

#define STACK_SIZE 4096

// Static stack and TCB pools
static uint8_t stacks[16][STACK_SIZE] __attribute__((aligned(16)));
static thread_t tcb[16];
static int next_tcb = 0;

static thread_t *ready_queue = NULL;
thread_t *current_thread = NULL;

// Idle thread
static void idle_thread(void) {
    for (;;) {
        __asm__ volatile ("sti; hlt");
    }
}

extern void __switch_to(thread_t *old, thread_t *new);

void scheduler_init(void) {
    // Create idle thread
    thread_t *idle = thread_create(idle_thread, "idle");
    idle->state = THREAD_STATE_READY;
    current_thread = idle;
}

thread_t *thread_create(void (*entry)(void), const char *name) {
    (void)name; // Name is for future use, suppress warning

    if (next_tcb >= 16) return NULL;

    thread_t *t = &tcb[next_tcb];
    t->kernel_stack = (uint64_t)&stacks[next_tcb][STACK_SIZE];
    uint64_t *stack = (uint64_t *)t->kernel_stack;

    // Set up initial stack frame as if after an interrupt
    uint64_t *frame = stack - 22; // 15 regs + int_no + err_code + 5 iret = 22 items
    // Zero general purpose registers
    for (int i = 0; i < 15; i++) frame[i] = 0;
    frame[15] = 0;   // int_no
    frame[16] = 0;   // err_code

    // iret frame
    frame[17] = (uint64_t)entry;   // RIP
    frame[18] = 0x08;              // CS (kernel code selector)
    frame[19] = 0x202;             // RFLAGS (IF=1)
    frame[20] = (uint64_t)(stack - 22); // RSP (top of this frame)
    frame[21] = 0x10;              // SS (kernel data selector)

    t->rsp = (uint64_t)frame;
    t->state = THREAD_STATE_READY;

    // Insert at head of ready queue (order doesn't matter for now)
    t->next = ready_queue;
    ready_queue = t;

    next_tcb++;
    return t;
}

void schedule(void) {
    if (!current_thread) return;

    thread_t *next = ready_queue;
    if (!next) return; // nothing to run

    // Remove head
    ready_queue = next->next;

    // Add current back to tail if it was running
    if (current_thread->state == THREAD_STATE_RUNNING) {
        current_thread->state = THREAD_STATE_READY;
        current_thread->next = NULL;
        // Find tail
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

    __switch_to(prev, next);
}

void enable_interrupts(void) {
    __asm__ volatile ("sti");
}