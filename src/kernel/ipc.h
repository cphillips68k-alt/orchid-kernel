#ifndef IPC_H
#define IPC_H
#include <stdint.h>
#include <stddef.h>

#define IPC_MSG_MAX 256

struct ipc_message {
    uint64_t length;
    char data[IPC_MSG_MAX];
};

void ipc_send(uint64_t port, const struct ipc_message *msg);
void ipc_recv(uint64_t port, struct ipc_message *msg);
void ipc_kernel_send(uint64_t port, const struct ipc_message *msg);

#endif