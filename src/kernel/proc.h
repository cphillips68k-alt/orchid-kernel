#ifndef PROC_H
#define PROC_H
#include <stdint.h>
#include "scheduler.h"

typedef struct process {
    uint64_t pid;
    uint64_t pml4_phys;
    thread_t *main_thread;
    struct process *next;
} process_t;

extern process_t *current_process;

void  proc_init(void);
void  proc_set_current(process_t *p);
uint64_t sys_fork(void);
void  proc_exit(void);
uint64_t proc_alloc_pid(void);

#endif