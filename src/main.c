#include <stdint.h>
#include <stddef.h>
#include "limine.h"
#include "serial.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "idt.h"
#include "pit.h"
#include "scheduler.h"
#include "tss.h"

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;

uint64_t hhdm_offset = 0;
uint64_t kernel_cr3;
uint64_t syscall_retval;

void kernel_panic(const char *msg) {
    serial_write("\n\n[KERNEL PANIC] ");
    serial_write(msg);
    serial_write("\nSystem halted.\n");
    for (;;) __asm__ volatile ("hlt");
}

void _start(void) {
    serial_init();
    serial_write("\n========================================\n");
    serial_write("Orchid Microkernel v0.2.0 (Skeleton + TSS)\n");
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
    serial_write("[boot] GDT OK\n");

    idt_init();
    serial_write("[boot] IDT OK\n");

    pit_init();
    serial_write("[boot] PIT OK\n");

    scheduler_init();
    serial_write("[boot] Scheduler OK\n");

    tss_init();
    serial_write("[boot] TSS OK\n");

    /* Unmask IRQ0 (timer) only */
    __asm__ volatile (
        "movb $0xFC, %%al\n"
        "outb %%al, $0x21\n"
        "movb $0xFF, %%al\n"
        "outb %%al, $0xA1\n"
        ::: "al"
    );
    enable_interrupts();
    serial_write("[boot] Interrupts enabled\n");

    serial_write("[boot] Interrupts enabled.\n");
    for (;;) __asm__ volatile ("hlt");
}