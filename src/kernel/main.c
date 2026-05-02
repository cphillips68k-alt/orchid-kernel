#include <stdint.h>
#include <stddef.h>
#include "limine.h"
#include "serial.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "isr_handler.h"
#include "scheduler.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include "tss.h"
#include "ipc.h"
#include "bus.h"
#include "sync.h"
#include "proc.h"
#include "irq.h"
#include "elf.h"
#include "kbd_buf.h"

extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;

extern uint8_t _binary_init_bin_start[];
extern uint8_t _binary_init_bin_end[];

uint64_t hhdm_offset = 0;
uint64_t kernel_cr3;
uint64_t syscall_retval;

static spinlock_t serial_lock = 0;

void kernel_panic(const char *msg) {
    serial_write("\n\n[KERNEL PANIC] ");
    serial_write(msg);
    serial_write("\nSystem halted.\n");
    for (;;) __asm__ volatile ("hlt");
}

void thread_a(void) {
    for (int i = 0; i < 10; i++) {
        spin_lock(&serial_lock);
        serial_printf("Thread A: %d\n", i);
        spin_unlock(&serial_lock);
        for (volatile int j = 0; j < 1000000; j++);
    }
    thread_exit();
}

void thread_b(void) {
    for (int i = 0; i < 10; i++) {
        spin_lock(&serial_lock);
        serial_printf("Thread B: %d\n", i);
        spin_unlock(&serial_lock);
        for (volatile int j = 0; j < 1000000; j++);
    }
    thread_exit();
}

void echo_service(void) {
    int port = bus_register("echo");
    if (port < 0) {
        serial_write("Failed to register echo service\n");
        thread_exit();
        return;
    }
    spin_lock(&serial_lock);
    serial_printf("Echo service started on port %d\n", port);
    spin_unlock(&serial_lock);
    for (;;) {
        struct ipc_message msg;
        ipc_recv(port, &msg);
        ipc_send(port, &msg);
    }
}

void echo_client(void) {
    // Wait longer for the echo service to start
    for (volatile int i = 0; i < 5000000; i++);  // 5x longer delay
    
    int port = bus_lookup("echo");
    if (port < 0) {
        serial_write("Echo service not found!\n");
        thread_exit();
        return;
    }
    struct ipc_message msg;
    msg.length = 13;
    for (int i = 0; i < 13; i++) msg.data[i] = "Hello, world!"[i];
    spin_lock(&serial_lock);
    serial_printf("Client sending to port %d: %s\n", port, msg.data);
    spin_unlock(&serial_lock);
    ipc_send(port, &msg);
    ipc_recv(port, &msg);
    spin_lock(&serial_lock);
    serial_printf("Client received reply: %s\n", msg.data);
    spin_unlock(&serial_lock);
    thread_exit();
}

void _start(void) {
    struct limine_framebuffer_response *fb_resp =
        (struct limine_framebuffer_response *)framebuffer_request.response;
    struct limine_framebuffer *fb = NULL;
    if (fb_resp != NULL && fb_resp->framebuffer_count > 0)
        fb = fb_resp->framebuffers[0];

    struct limine_memmap_response *mm_resp =
        (struct limine_memmap_response *)memmap_request.response;

    struct limine_hhdm_response *hhdm_resp =
        (struct limine_hhdm_response *)hhdm_request.response;
    if (hhdm_resp != NULL) hhdm_offset = hhdm_resp->offset;

    serial_init();
    serial_write("\n========================================\n");
    serial_write("Orchid Microkernel v0.4.0 (shell-ready)\n");
    serial_write("========================================\n\n");

    if (fb != NULL) {
        console_init(fb);
        console_write("Orchid Microkernel v0.4.0\n========================\n");
        console_printf("Framebuffer: %dx%d, BPP: %d\n", fb->width, fb->height, fb->bpp);
    } else {
        serial_write("[boot] No framebuffer available\n");
    }

    if (mm_resp != NULL) {
        serial_printf("[boot] Memory map entries: %d\n", mm_resp->entry_count);
        uint64_t total_usable = 0;
        for (uint64_t i = 0; i < mm_resp->entry_count; i++) {
            struct limine_memmap_entry *entry = mm_resp->entries[i];
            if (entry->type == LIMINE_MEMMAP_USABLE)
                total_usable += entry->length;
        }
        serial_printf("[boot] Total usable RAM: %d MB\n", total_usable / (1024 * 1024));
    }

    pmm_init();
    vmm_init();
    __asm__ volatile ("mov %%cr3, %0" : "=r"(kernel_cr3));
    serial_printf("[boot] HHDM offset: %x\n", hhdm_offset);

    gdt_init();
    idt_init();
    pit_init();
    scheduler_init();
    tss_init();
    irq_init();
    kbd_buf_init();
    proc_init();

    __asm__ volatile (
        "movb $0xFC, %%al\n"
        "outb %%al, $0x21\n"
        "movb $0xFF, %%al\n"
        "outb %%al, $0xA1\n"
        ::: "al"
    );

    uint64_t krnl_cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(krnl_cr3));

    thread_t *t;
    t = thread_create(thread_a, "thread_a", krnl_cr3, NULL);
    if (!t) serial_write("[boot] Failed to create thread_a\n");

    t = thread_create(thread_b, "thread_b", krnl_cr3, NULL);
    if (!t) serial_write("[boot] Failed to create thread_b\n");

    t = thread_create(echo_service, "echo_svc", krnl_cr3, NULL);
    if (!t) serial_write("[boot] Failed to create echo_svc\n");

    t = thread_create(echo_client, "echo_cli", krnl_cr3, NULL);
    if (!t) serial_write("[boot] Failed to create echo_cli\n");

    /* TODO: Load init user process once fork/exec is fixed */

    serial_write("[boot] Preemptive scheduler started.\n");
    enable_interrupts();

    for (;;) __asm__ volatile ("hlt")

        /* Load init as the first user process */
    size_t init_size = _binary_init_bin_end - _binary_init_bin_start;
    int init_pid = elf_load(_binary_init_bin_start, init_size);
    if (init_pid < 0)
        serial_write("[boot] Failed to load init!\n");
    else
        serial_printf("[boot] Init loaded, PID %d\n", init_pid);

    serial_write("[boot] Preemptive scheduler started.\n");
    enable_interrupts();

    for (;;) __asm__ volatile ("hlt");

}