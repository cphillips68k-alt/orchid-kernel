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

/* Limine request/response declarations */
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_kernel_file_request kernel_file_request;

/* Simple spinlock for serial output */
static volatile int serial_lock = 0;
void spin_lock(volatile int *lock) { while (__sync_lock_test_and_set(lock, 1)); }
void spin_unlock(volatile int *lock) { __sync_lock_release(lock); }

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

void _start(void) {
    /* Fetch Limine responses */
    struct limine_framebuffer_response *fb_resp =
        (struct limine_framebuffer_response *)framebuffer_request.response;
    struct limine_framebuffer *fb = NULL;
    if (fb_resp != NULL && fb_resp->framebuffer_count > 0) {
        fb = fb_resp->framebuffers[0];
    }

    struct limine_memmap_response *mm_resp =
        (struct limine_memmap_response *)memmap_request.response;

    uint64_t hhdm_offset = 0;
    struct limine_hhdm_response *hhdm_resp =
        (struct limine_hhdm_response *)hhdm_request.response;
    if (hhdm_resp != NULL) {
        hhdm_offset = hhdm_resp->offset;
    }

    /* Initialize core subsystems */
    serial_init();
    serial_write("\n========================================\n");
    serial_write("Orchid Microkernel v0.2.0 (preemptive)\n");
    serial_write("========================================\n\n");

    if (fb != NULL) {
        console_init(fb);
        console_write("Orchid Microkernel v0.2.0\n========================\n");
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
    serial_printf("[boot] HHDM offset: 0x%x\n", hhdm_offset);

    /* Setup scheduling infrastructure */
    gdt_init();
    idt_init();
    pit_init();
    scheduler_init();

    /* Unmask IRQ0 (timer) */
    __asm__ volatile (
        "movb $0xFC, %%al\n"
        "outb %%al, $0x21\n"
        "movb $0xFF, %%al\n"
        "outb %%al, $0xA1\n"
        ::: "al"
    );

    /* Create demo threads */
    thread_create(thread_a, "thread_a");
    thread_create(thread_b, "thread_b");

    serial_write("[boot] Preemptive scheduler started.\n");
    enable_interrupts();

    /* Idle loop */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}