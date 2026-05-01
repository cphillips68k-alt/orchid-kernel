#include "scheduler.h"
#include "serial.h"

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

// Assembly magic to do the actual context switch
extern void __switch_to(thread_t *old, thread_t *new);

void scheduler_init(void) {
    // Create idle thread
    thread_t *idle = thread_create(idle_thread, "idle");
    idle->state = THREAD_STATE_READY;
    current_thread = idle;
}

thread_t *thread_create(void (*entry)(void), const char *name) {
    if (next_tcb >= 16) return NULL;

    thread_t *t = &tcb[next_tcb];
    t->kernel_stack = (uint64_t)&stacks[next_tcb][STACK_SIZE];
    uint64_t *stack = (uint64_t *)t->kernel_stack;

    // Set up initial stack frame as if we were interrupted
    // We'll simulate the state after `isr_common` pushes.
    // The CPU already pushed: SS, RSP, RFLAGS, CS, RIP (for a ring0 interrupt)
    // The stub pushes: int_no, err_code, then 15 regs.
    // So when we iret from schedule, we just pop regs and iret.
    // We want the thread to start executing `entry`.
    // So we prepare the stack so that `iretq` will jump to entry.

    // Order of pushed items (from top of stack to bottom):
    //   SS, RSP, RFLAGS, CS, RIP  <-- iret pops these
    // We are in kernel mode, so SS = 0x10, RSP = whatever (top of new stack? we already set it)
    // Let's fill:
    stack[-1] = 0x10;              // SS
    stack[-2] = (uint64_t)entry;   // RSP (new task's stack pointer = top of stack? Actually, before we push these, we set t->rsp = address of stack[-?])
    // We'll fill later.

    // Actually, the correct interrupt frame for a kernel->kernel switch is:
    //   [RIP] [CS] [RFLAGS] [RSP] [SS]   (CPU pushes these)
    // Since we're not coming from user mode, RSP and SS are just the stack we want after iret.
    // We can set SS=0x10, RSP=anything (we'll set it to the top of the new stack just below the frame).
    // But typical approach: we set up the frame so that iret loads RIP=entry, CS=0x08, RFLAGS=0x202 (IF enabled), RSP=new_stack_top, SS=0x10. Then the "pop regs" part restores all general regs (which we set to 0).
    // After iret, execution starts at entry with the new stack.

    // Stack layout (growing downward) after isr_common saved regs:
    // r15, r14, ..., rdi, int_no, err_code, rip, cs, rflags, rsp, ss
    // We'll fill all of these:
    uint64_t *frame = stack - 22; // 15 regs + int_no + err_code + 5 iret = 22 items
    // Fill general purpose registers (just zero them)
    for (int i = 0; i < 15; i++) frame[i] = 0;
    frame[15] = 0;   // int_no (unused)
    frame[16] = 0;   // err_code (unused)

    // iret frame
    frame[17] = (uint64_t)entry;   // RIP
    frame[18] = 0x08;              // CS (kernel code selector)
    frame[19] = 0x202;             // RFLAGS (IF = 1)
    frame[20] = (uint64_t)(stack - 22); // RSP (we'll set this as the task's stack pointer after iret)
    frame[21] = 0x10;              // SS

    t->rsp = (uint64_t)frame;
    t->state = THREAD_STATE_READY;

    // Insert into ready queue
    t->next = ready_queue;
    ready_queue = t;

    next_tcb++;
    return t;
}

void schedule(void) {
    if (!current_thread) return;

    // Simple round-robin: put current back at end of ready queue, get first
    thread_t *next = ready_queue;
    if (!next) return; // only idle

    // Remove head from queue
    ready_queue = next->next;

    // Add current to tail if it's still running (and not the same)
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