#include "syscalls.h"
#include "serial.h"
#include "console.h"
#include "scheduler.h"
#include "timer.h"
#include "proc.h"
#include "irq.h"
#include "elf.h"
#include "kbd_buf.h"
#include "vmm.h"
#include "pmm.h"
#include <stddef.h>

extern uint64_t syscall_retval;
extern uint64_t hhdm_offset;
extern process_t *current_process;
uint64_t saved_user_rip = 0;

/* Embedded binaries */
extern uint8_t _binary_init_bin_start[];
extern uint8_t _binary_init_bin_end[];
extern uint8_t _binary_shell_bin_start[];
extern uint8_t _binary_shell_bin_end[];

static struct {
    const char *name;
    uint8_t *start;
    uint8_t *end;
} embedded_binaries[] = {
    {"init",  _binary_init_bin_start,  _binary_init_bin_end},
    {"shell", _binary_shell_bin_start, _binary_shell_bin_end},
    {NULL, NULL, NULL}
};

static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

/*
 * Copy data FROM kernel TO user space.
 * uaddr: user virtual address
 * kdata: kernel source buffer
 * len:   bytes to copy
 * Returns 0 on success, -1 if any page is not mapped.
 */
static int copy_to_user(uint64_t uaddr, const void *kdata, size_t len) {
    if (!current_process) return -1;
    while (len) {
        uint64_t page = uaddr & ~((uint64_t)PAGE_SIZE - 1);
        uint64_t off  = uaddr & (PAGE_SIZE - 1);
        size_t chunk = PAGE_SIZE - off;
        if (chunk > len) chunk = len;

        uint64_t phys = user_virt_to_phys(current_process->pml4_phys, page);
        if (!phys) return -1;
        void *uvirt = (void *)(phys + hhdm_offset + off);
        for (size_t i = 0; i < chunk; i++)
            ((char *)uvirt)[i] = ((const char *)kdata)[i];

        uaddr += chunk;
        kdata = (const char *)kdata + chunk;
        len -= chunk;
    }
    return 0;
}

/*
 * Copy data FROM user space TO kernel.
 * kdest: kernel destination buffer
 * uaddr: user virtual address
 * len:   bytes to copy
 * Returns 0 on success, -1 if any page is not mapped.
 */
static int copy_from_user(void *kdest, uint64_t uaddr, size_t len) {
    if (!current_process) return -1;
    while (len) {
        uint64_t page = uaddr & ~((uint64_t)PAGE_SIZE - 1);
        uint64_t off  = uaddr & (PAGE_SIZE - 1);
        size_t chunk = PAGE_SIZE - off;
        if (chunk > len) chunk = len;

        uint64_t phys = user_virt_to_phys(current_process->pml4_phys, page);
        if (!phys) return -1;
        void *uvirt = (void *)(phys + hhdm_offset + off);
        for (size_t i = 0; i < chunk; i++)
            ((char *)kdest)[i] = ((const char *)uvirt)[i];

        uaddr += chunk;
        kdest = (char *)kdest + chunk;
        len -= chunk;
    }
    return 0;
}

/* Write a kernel buffer to the console / serial. */
static uint64_t do_write(uint64_t fd, const char *buf, uint64_t count) {
    if (fd == 1 || fd == 2) {
        for (uint64_t i = 0; i < count; i++) {
            serial_putc(buf[i]);
            console_putc(buf[i]);    /* also write to framebuffer */
        }
        return count;
    }
    return 0;
}

static void do_sleep(uint64_t seconds) {
    uint64_t now = timer_get_ticks();
    uint64_t deadline = now + seconds * 100;
    sleep_until(deadline);
}

static void do_nanosleep(uint64_t nanoseconds) {
    uint64_t ticks_to_wait = (nanoseconds + 9999999) / 10000000;
    uint64_t now = timer_get_ticks();
    sleep_until(now + ticks_to_wait);
}

void syscall_handler(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3,
                     uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;

    switch (nr) {
        case SYS_read:
            if (a1 == 0) {
                /* stdin: read one character from keyboard buffer */
                char c = kbd_buf_get();
                if (copy_to_user(a2, &c, 1) == 0)
                    syscall_retval = 1;
                else
                    syscall_retval = (uint64_t)-1;
            } else {
                syscall_retval = 0;
            }
            break;

        case SYS_write: {
            /* a1=fd, a2=user buffer, a3=count */
            uint64_t count = a3;
            if (count > 4096) count = 4096;    /* sanity limit */
            char kbuf[4096];
            if (copy_from_user(kbuf, a2, count) == 0)
                syscall_retval = do_write(a1, kbuf, count);
            else
                syscall_retval = (uint64_t)-1;
            break;
        }

        case SYS_open:
            syscall_retval = 10;   /* dummy fd */
            break;

        case SYS_close:
            syscall_retval = 0;
            break;

        case SYS_gettid:
            syscall_retval = (uint64_t)current_thread;
            break;

        case SYS_sleep:
            do_sleep(a1);
            syscall_retval = 0;
            break;

        case SYS_nanosleep:
            do_nanosleep(a1);
            syscall_retval = 0;
            break;

        case SYS_fork:
            syscall_retval = sys_fork();
            break;

        case SYS_iopl:
            if (a1 == 3) current_thread->iopl = 3;
            syscall_retval = 0;
            break;

        case SYS_irq_register:
            syscall_retval = irq_register((uint8_t)a1, a2);
            break;

        case SYS_exec: {
            /* a1 = user pointer to binary data, a2 = size */
            uint64_t size = a2;
            if (size > 1024 * 1024) {   /* 1 MB sanity limit */
                syscall_retval = (uint64_t)-1;
                break;
            }
            uint8_t *kbuf = kmalloc(size);
            if (!kbuf) {
                syscall_retval = (uint64_t)-1;
                break;
            }
            if (copy_from_user(kbuf, a1, size) == 0) {
                sys_exec(kbuf, size);
                syscall_retval = 0;
            } else {
                syscall_retval = (uint64_t)-1;
            }
            kfree(kbuf);
            break;
        }

        case SYS_get_binary: {
            /* a1 = user pointer to name, a2 = user buffer, a3 = max size */
            char name[32];
            if (copy_from_user(name, a1, sizeof(name) - 1) != 0) {
                syscall_retval = (uint64_t)-1;
                break;
            }
            name[31] = '\0';
            for (int i = 0; embedded_binaries[i].name; i++) {
                if (strcmp(name, embedded_binaries[i].name) == 0) {
                    size_t size = (size_t)(embedded_binaries[i].end - embedded_binaries[i].start);
                    if (size <= a3) {
                        if (copy_to_user(a2, embedded_binaries[i].start, size) == 0) {
                            syscall_retval = size;
                        } else {
                            syscall_retval = (uint64_t)-2;
                        }
                    } else {
                        syscall_retval = (uint64_t)-2;
                    }
                    return;
                }
            }
            syscall_retval = (uint64_t)-1;
            break;
        }

        case SYS_yield:
            schedule();
            syscall_retval = 0;
            break;

        case SYS_exit:
            thread_exit();
            break;

        default:
            syscall_retval = (uint64_t)-1;
            break;
    }
}