#include <stdint.h>
#include "bus.h"
#include "ipc.h"
#include "serial.h"

void vfs_service(void) {
    int port = bus_register("vfs");
    if (port < 0) return;

    while (1) {
        struct ipc_message req;
        ipc_recv(port, &req);

        if (req.length < 2) continue;
        uint8_t opcode = req.data[0];
        int fd = req.data[1];

        switch (opcode) {
        case 1: { /* write */
            const char *buf = &req.data[2];
            uint64_t len = req.length - 2;
            if (fd == 1 || fd == 2) {
                for (uint64_t i=0; i<len; i++) serial_putc(buf[i]);
            }
            break;
        }
        case 2: { /* read – not fully implemented */ break; }
        }
    }
}