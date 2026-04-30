#include <stdint.h>
#include <stddef.h>
#include "limine.h"
#include "serial.h"
#include "console.h"

/* Limine request/response declarations */
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_kernel_file_request kernel_file_request;

/* Panic handler - called when something goes catastrophically wrong */
__attribute__((noreturn))
void kernel_panic(const char *msg) {
    /* Last-ditch output to serial (no framebuffer required) */
    serial_write("\n\n[KERNEL PANIC] ");
    serial_write(msg);
    serial_write("\nSystem halted.\n");

    /* Infinite halt */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* 
 * _start - Kernel entry point called by Limine.
 * We're in 64-bit long mode with:
 * - An identity-mapped or higher-half mapped address space
 * - A valid stack
 * - Interrupts disabled
 * - No CPUID/SSE state we can count on (Limine handles the basics)
 */
void _start(void) {
    /* --- Fetch Limine responses --- */

       /* Framebuffer */
    struct limine_framebuffer_response *fb_resp = 
        (struct limine_framebuffer_response *)framebuffer_request.response;
    struct limine_framebuffer *fb = NULL;
    if (fb_resp != NULL && fb_resp->framebuffer_count > 0) {
        fb = fb_resp->framebuffers[0];
    }

    /* Memory map */
    struct limine_memmap_response *mm_resp = 
        (struct limine_memmap_response *)memmap_request.response;

    /* HHDM offset for physical memory access */
    uint64_t hhdm_offset = 0;
    struct limine_hhdm_response *hhdm_resp = 
        (struct limine_hhdm_response *)hhdm_request.response;
    if (hhdm_resp != NULL) {
        hhdm_offset = hhdm_resp->offset;
    }

    /* --- Initialize core subsystems --- */

    /* Serial first - it's the simplest and most reliable debug output */
    serial_init();
    serial_write("\n========================================\n");
    serial_write("Orchid Microkernel v0.1.0 (C rewrite)\n");
    serial_write("========================================\n\n");

    serial_write("[boot] Limine boot protocol detected\n");

    /* Initialize console if we have a framebuffer */
    if (fb != NULL) {
        console_init(fb);
        console_write("Orchid Microkernel v0.1.0\n");
        console_write("========================\n\n");
        console_printf("Framebuffer: %dx%d, BPP: %d\n", fb->width, fb->height, fb->bpp);
    } else {
        serial_write("[boot] No framebuffer available, console output disabled\n");
    }

    /* Print memory map summary */
    if (mm_resp != NULL) {
        serial_printf("[boot] Memory map entries: %d\n", mm_resp->entry_count);
        console_printf("Memory map entries: %d\n", mm_resp->entry_count);

        uint64_t total_usable = 0;
        for (uint64_t i = 0; i < mm_resp->entry_count; i++) {
            struct limine_memmap_entry *entry = mm_resp->entries[i];
            if (entry->type == LIMINE_MEMMAP_USABLE) {
                total_usable += entry->length;
            }
        }
        serial_printf("[boot] Total usable RAM: %d MB\n", total_usable / (1024 * 1024));
        console_printf("Total usable RAM: %d MB\n", total_usable / (1024 * 1024));
    }

    serial_printf("[boot] HHDM offset: 0x%x\n", hhdm_offset);

    /* --- Kernel startup complete --- */
    serial_write("\n[boot] Kernel initialization complete.\n");
    console_write("\nKernel initialization complete.\n");

    /* For now, just halt. The scheduler and IPC come next. */
    console_write("\n[Note: This is the bootstrap kernel. Scheduler, IPC, and\n");
    console_write(" task management will be added in the next iteration.]\n\n");
    console_write("System halted.\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}