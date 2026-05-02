#include <stdint.h>
#include "bus.h"
#include "ipc.h"
#include "syscalls.h"

static int vfs_port = -1, kbd_port = -1;

static uint8_t scancode_to_ascii(uint8_t sc) {
    static const char tbl[128] = {
        0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
        'q','w','e','r','t','y','u','i','o','p','[',']',0,0,'a','s',
        'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c',
        'v','b','n','m',',','.','/',0,0,0,' ',0
    };
    if (sc < 128) return tbl[sc];
    return 0;
}

void kbd_service(void) {
    kbd_port = bus_register("kbd");
    if (kbd_port < 0) return;
    vfs_port = bus_lookup("vfs");
    if (vfs_port < 0) return;

    /* Request I/O privilege (iopl=3) */
    __asm__ volatile ("mov $8, %%rax; mov $3, %%rdi; syscall" ::: "rax","rdi");
    /* Register IRQ1 for our port */
    __asm__ volatile ("mov $9, %%rax; mov $1, %%rdi; mov %0, %%rsi; syscall"
                      :: "r"((uint64_t)kbd_port) : "rax","rdi","rsi");

    while (1) {
        struct ipc_message msg;
        ipc_recv(kbd_port, &msg);   /* wait for IRQ */

        uint8_t st;
        do {
            __asm__ volatile ("inb $0x64, %0" : "=a"(st));
        } while (!(st & 1));
        uint8_t sc;
        __asm__ volatile ("inb $0x60, %0" : "=a"(sc));
        if (sc & 0x80) continue;
        char ch = scancode_to_ascii(sc);
        if (!ch) continue;

        msg.length = 1;
        msg.data[0] = ch;
        ipc_send(vfs_port, &msg);
    }
}