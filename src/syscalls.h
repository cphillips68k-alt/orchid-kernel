#ifndef SYSCALLS_H
#define SYSCALLS_H
#include <stdint.h>

/* Syscall numbers (subset of Linux x86_64) */
#define SYS_read    0
#define SYS_write   1
#define SYS_open    2
#define SYS_close   3
#define SYS_gettid  4
#define SYS_yield   158
#define SYS_exit    60

extern uint64_t syscall_retval;   /* defined in main.c */

void syscall_handler(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3,
                     uint64_t a4, uint64_t a5, uint64_t a6);
#endif