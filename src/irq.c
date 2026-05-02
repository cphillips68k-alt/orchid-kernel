#include "irq.h"
#include "ipc.h"
#include "sync.h"
#include <stddef.h>

static struct {
    uint64_t port;
    int in_use;
} irq_table[16];

static spinlock_t irq_lock = 0;

void irq_init(void) {
    for (int i = 0; i < 16; i++) {
        irq_table[i].in_use = 0;
    }
}

int irq_register(uint8_t irq, uint64_t port) {
    if (irq >= 16) return -1;
    spin_lock(&irq_lock);
    if (irq_table[irq].in_use) {
        spin_unlock(&irq_lock);
        return -1;
    }
    irq_table[irq].port = port;
    irq_table[irq].in_use = 1;
    spin_unlock(&irq_lock);
    return 0;
}

void irq_handler(uint8_t irq) {
    spin_lock(&irq_lock);
    if (irq < 16 && irq_table[irq].in_use) {
        uint64_t port = irq_table[irq].port;
        spin_unlock(&irq_lock);
        struct ipc_message msg;
        msg.length = 0;
        ipc_kernel_send(port, &msg);
    } else {
        spin_unlock(&irq_lock);
    }
}