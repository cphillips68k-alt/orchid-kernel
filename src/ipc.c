#include "ipc.h"
#include "scheduler.h"
#include "sync.h"
#include "vmm.h"
#include <stddef.h>

#define MAX_PORTS 64

typedef struct wait_node {
    thread_t *thread;
    union {
        struct {
            struct ipc_message pending_msg;
        } snd;
        struct {
            struct ipc_message *buffer;
        } rcv;
    };
    struct wait_node *next;
} wait_node_t;

typedef struct {
    spinlock_t lock;
    wait_node_t *senders;
    wait_node_t *recvers;
} port_t;

static port_t ports[MAX_PORTS];
static int ipc_initialized = 0;

static void ipc_init(void) {
    for (int i = 0; i < MAX_PORTS; i++) {
        ports[i].lock = 0;
        ports[i].senders = NULL;
        ports[i].recvers = NULL;
    }
    ipc_initialized = 1;
}

/* Non‑blocking send from kernel (for IRQ forwarding) */
void ipc_kernel_send(uint64_t port, const struct ipc_message *msg) {
    if (!ipc_initialized) ipc_init();
    if (port >= MAX_PORTS) return;
    port_t *p = &ports[port];
    spin_lock(&p->lock);
    if (p->recvers) {
        wait_node_t *node = p->recvers;
        p->recvers = node->next;
        if (node->rcv.buffer) {
            uint64_t len = msg->length;
            if (len > IPC_MSG_MAX) len = IPC_MSG_MAX;
            node->rcv.buffer->length = len;
            for (uint64_t i = 0; i < len; i++)
                node->rcv.buffer->data[i] = msg->data[i];
        }
        spin_unlock(&p->lock);
        thread_unblock(node->thread);
        kfree(node);
    } else {
        spin_unlock(&p->lock);
    }
}

void ipc_send(uint64_t port, const struct ipc_message *msg) {
    if (!ipc_initialized) ipc_init();
    if (port >= MAX_PORTS) return;
    port_t *p = &ports[port];
    spin_lock(&p->lock);
    if (p->recvers) {
        wait_node_t *node = p->recvers;
        p->recvers = node->next;
        if (node->rcv.buffer) {
            uint64_t len = msg->length;
            if (len > IPC_MSG_MAX) len = IPC_MSG_MAX;
            node->rcv.buffer->length = len;
            for (uint64_t i = 0; i < len; i++)
                node->rcv.buffer->data[i] = msg->data[i];
        }
        spin_unlock(&p->lock);
        thread_unblock(node->thread);
        kfree(node);
        return;
    }
    wait_node_t *node = kmalloc(sizeof(wait_node_t));
    if (!node) { spin_unlock(&p->lock); return; }
    node->thread = current_thread;
    uint64_t len = msg->length;
    if (len > IPC_MSG_MAX) len = IPC_MSG_MAX;
    node->snd.pending_msg.length = len;
    for (uint64_t i = 0; i < len; i++)
        node->snd.pending_msg.data[i] = msg->data[i];
    node->next = p->senders;
    p->senders = node;
    spin_unlock(&p->lock);
    thread_block();
}

void ipc_recv(uint64_t port, struct ipc_message *msg) {
    if (!ipc_initialized) ipc_init();
    if (port >= MAX_PORTS) return;
    port_t *p = &ports[port];
    spin_lock(&p->lock);
    if (p->senders) {
        wait_node_t *node = p->senders;
        p->senders = node->next;
        uint64_t len = node->snd.pending_msg.length;
        if (len > IPC_MSG_MAX) len = IPC_MSG_MAX;
        msg->length = len;
        for (uint64_t i = 0; i < len; i++)
            msg->data[i] = node->snd.pending_msg.data[i];
        spin_unlock(&p->lock);
        thread_unblock(node->thread);
        kfree(node);
        return;
    }
    wait_node_t *node = kmalloc(sizeof(wait_node_t));
    if (!node) { spin_unlock(&p->lock); return; }
    node->thread = current_thread;
    node->rcv.buffer = msg;
    node->next = p->recvers;
    p->recvers = node;
    spin_unlock(&p->lock);
    thread_block();
}