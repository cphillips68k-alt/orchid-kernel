#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>

struct process;   /* forward declaration */

typedef struct thread {
    uint64_t rsp;
    uint64_t kernel_stack;
    int state;
    uint64_t cr3;
    int iopl;
    struct process *process;   /* owning process, NULL for kernel threads */
    struct thread *next;
} thread_t;

#define THREAD_STATE_RUNNING 1
#define THREAD_STATE_READY   2
#define THREAD_STATE_BLOCKED 3

extern thread_t *current_thread;

void scheduler_init(void);
thread_t *thread_create(void (*entry)(void), const char *name, uint64_t cr3, struct process *proc);
void schedule(void);
void enable_interrupts(void);
void thread_exit(void);
void thread_block(void);
void thread_unblock(thread_t *t);
void scheduler_add_thread(thread_t *t);

#endif