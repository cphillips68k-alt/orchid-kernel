#include "syscalls.h"
#include "serial.h"
#include "scheduler.h"

extern uint64_t syscall_retval;   /* defined in main.c */

static uint64_t do_write(uint64_t fd, const char *buf, uint64_t count) {
    if (fd == 1 || fd == 2) {
        for (uint64_t i = 0; i < count; i++) serial_putc(buf[i]);
        return count;
    }
    return 0;
}

void syscall_handler(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3,
                     uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    switch (nr) {
        case SYS_read:   syscall_retval = 0; break;
        case SYS_write:  syscall_retval = do_write(a1, (const char *)a2, a3); break;
        case SYS_open:   syscall_retval = 10; break;   /* dummy fd */
        case SYS_close:  syscall_retval = 0; break;
        case SYS_gettid: syscall_retval = (uint64_t)current_thread; break;
        case SYS_yield:  schedule(); syscall_retval = 0; break;
        case SYS_exit:   thread_exit(); break;
        default:         syscall_retval = (uint64_t)-1; break;
    }
}