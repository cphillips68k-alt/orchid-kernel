#include "timer.h"
#include "scheduler.h"
#include "vmm.h"
#include "sync.h"
#include "serial.h"
#include <stddef.h>

static uint64_t ticks = 0;           /* monotonic counter, incremented every timer IRQ */
static spinlock_t timer_lock = 0;

typedef struct timer_entry {
    thread_t *thread;
    uint64_t deadline;               /* tick value when the thread should wake up */
    struct timer_entry *next;
} timer_entry_t;

static timer_entry_t *timer_queue = NULL;

void timer_init(void) {
    /* nothing to do; the PIT is already running */
}

void timer_tick(void) {
    spin_lock(&timer_lock);
    ticks++;

    /* Wake any threads whose deadline has passed */
    timer_entry_t **prev = &timer_queue;
    while (*prev) {
        timer_entry_t *entry = *prev;
        if (ticks >= entry->deadline) {
            *prev = entry->next;
            thread_unblock(entry->thread);
            kfree(entry);
        } else {
            prev = &entry->next;
        }
    }
    spin_unlock(&timer_lock);
}

void sleep_until(uint64_t tick_deadline) {
    spin_lock(&timer_lock);
    if (ticks >= tick_deadline) {
        spin_unlock(&timer_lock);
        return;
    }

    timer_entry_t *entry = kmalloc(sizeof(timer_entry_t));
    if (!entry) {
        spin_unlock(&timer_lock);
        return;
    }
    entry->thread = current_thread;
    entry->deadline = tick_deadline;

    /* Insert sorted by deadline (simple insertion sort) */
    timer_entry_t **prev = &timer_queue;
    while (*prev && (*prev)->deadline < entry->deadline)
        prev = &(*prev)->next;
    entry->next = *prev;
    *prev = entry;

    spin_unlock(&timer_lock);
    thread_block();   /* will be woken by timer_tick */
}

uint64_t timer_get_ticks(void) {
    return ticks;
}