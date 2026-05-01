#ifndef IPC_H
#define IPC_H
#include <stdint.h>
#include <stddef.h>

#define IPC_MSG_MAX 256

struct ipc_message {
    uint64_t length;              /* bytes in data */
    char data[IPC_MSG_MAX];
};

/* Send a message to a port. Blocks until delivered (i.e., a receiver is waiting). */
void ipc_send(uint64_t port, const struct ipc_message *msg);

/* Receive a message from a port. Blocks until a message arrives. */
void ipc_recv(uint64_t port, struct ipc_message *msg);

#endif