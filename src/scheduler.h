#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>

typedef struct thread {
    uint64_t rsp;            // saved stack pointer
    uint64_t kernel_stack;   // base of stack (for freeing later)
    int state;
    // linked list node
    struct thread *next;
} thread_t;

#define THREAD_STATE_RUNNING 1
#define THREAD_STATE_READY   2

extern thread_t *current_thread;

void scheduler_init(void);
thread_t *thread_create(void (*entry)(void), const char *name);
void schedule(void);
void switch_to(thread_t *next);
void enable_interrupts(void);

#endif