#include "kbd_buf.h"
#include "sync.h"
#include "scheduler.h"
#include <stddef.h>

#define BUF_SIZE 256

static char buf[BUF_SIZE];
static unsigned head = 0, tail = 0;
static spinlock_t kbd_lock = 0;

void kbd_buf_init(void) {
    head = tail = 0;
}

void kbd_buf_put(char c) {
    spin_lock(&kbd_lock);
    unsigned next = (head + 1) % BUF_SIZE;
    if (next != tail) {
        buf[head] = c;
        head = next;
    }
    spin_unlock(&kbd_lock);
    /* wake any blocked reader – we need a mechanism for that.
       For now, kbd_buf_get spins, which is okay for a simple demo. */
}

char kbd_buf_get(void) {
    while (1) {
        spin_lock(&kbd_lock);
        if (tail != head) {
            char c = buf[tail];
            tail = (tail + 1) % BUF_SIZE;
            spin_unlock(&kbd_lock);
            return c;
        }
        spin_unlock(&kbd_lock);
        /* yield to other threads while waiting */
        __asm__ volatile ("mov $158, %%rax; syscall" ::: "rax");  /* sys_yield */
    }
}