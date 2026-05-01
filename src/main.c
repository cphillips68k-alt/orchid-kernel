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

extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;

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

/* Demo threads */
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
    for (volatile int i = 0; i < 1000000; i++);

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
    serial_init();
    serial_write("\n========================================\n");
    serial_write("Orchid Microkernel v0.2.0 (Skeleton + Threads)\n");
    serial_write("========================================\n\n");

    struct limine_hhdm_response *hhdm_resp =
        (struct limine_hhdm_response *)hhdm_request.response;
    if (hhdm_resp) {
        hhdm_offset = hhdm_resp->offset;
        serial_printf("[boot] HHDM offset: %x\n", hhdm_offset);
    } else {
        serial_write("[boot] No HHDM response!\n");
        for (;;) __asm__ volatile ("hlt");
    }

    pmm_init();
    vmm_init();
    __asm__ volatile ("mov %%cr3, %0" : "=r"(kernel_cr3));
    serial_printf("[boot] Kernel CR3: %x\n", kernel_cr3);

    gdt_init();
    idt_init();
    pit_init();
    scheduler_init();
    tss_init();

    /* Unmask IRQ0 (timer) only */
    __asm__ volatile (
        "movb $0xFC, %%al\n"
        "outb %%al, $0x21\n"
        "movb $0xFF, %%al\n"
        "outb %%al, $0xA1\n"
        ::: "al"
    );

    /* Create kernel demo threads */
    uint64_t krnl_cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(krnl_cr3));
    thread_create(thread_a, "thread_a", krnl_cr3);
    thread_create(thread_b, "thread_b", krnl_cr3);
    thread_create(echo_service, "echo_svc", krnl_cr3);
    thread_create(echo_client, "echo_cli", krnl_cr3);

    serial_write("[boot] Preemptive scheduler started.\n");
    enable_interrupts();

    for (;;) __asm__ volatile ("hlt");
}