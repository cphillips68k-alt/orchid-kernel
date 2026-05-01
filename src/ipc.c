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
            struct ipc_message pending_msg;  // for sender
        } snd;
        struct {
            struct ipc_message *buffer;     // for receiver: ptr to user's msg
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

void ipc_send(uint64_t port, const struct ipc_message *msg) {
    if (!ipc_initialized) ipc_init();
    if (port >= MAX_PORTS) return;

    port_t *p = &ports[port];
    spin_lock(&p->lock);

    if (p->recvers) {
        /* Deliver immediately */
        wait_node_t *node = p->recvers;
        p->recvers = node->next;

        /* Copy message into receiver's buffer */
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

    /* No receiver – block sender */
    wait_node_t *node = kmalloc(sizeof(wait_node_t));
    if (!node) {
        spin_unlock(&p->lock);
        return; /* out of memory, message lost */
    }
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
    /* When we return here, the message was delivered (we've been unblocked) */
}

void ipc_recv(uint64_t port, struct ipc_message *msg) {
    if (!ipc_initialized) ipc_init();
    if (port >= MAX_PORTS) return;

    port_t *p = &ports[port];
    spin_lock(&p->lock);

    if (p->senders) {
        /* A sender is already waiting */
        wait_node_t *node = p->senders;
        p->senders = node->next;

        /* Copy the pending message into msg */
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

    /* No sender – block receiver, save buffer pointer */
    wait_node_t *node = kmalloc(sizeof(wait_node_t));
    if (!node) {
        spin_unlock(&p->lock);
        return;
    }
    node->thread = current_thread;
    node->rcv.buffer = msg;
    node->next = p->recvers;
    p->recvers = node;
    spin_unlock(&p->lock);

    thread_block();
    /* When we return, msg has been filled by ipc_send */
}