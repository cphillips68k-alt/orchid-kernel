#include "isr_handler.h"
#include "serial.h"
#include "scheduler.h"

static void page_fault_handler(uint64_t error_code, uint64_t addr) {
    serial_printf("[PF] Page fault at %x, error %x, killing thread\n", addr, error_code);
    thread_exit();
}

void isr_handler(struct regs *r) {
    if (r->int_no == 14) {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        page_fault_handler(r->err_code, cr2);
        return;
    }

    if (r->int_no == 32) {
        schedule();
    }

    if (r->int_no >= 32 && r->int_no <= 47) {
        if (r->int_no >= 40)
            __asm__ volatile ("outb %%al, $0xA0" : : "a"(0x20));
        __asm__ volatile ("outb %%al, $0x20" : : "a"(0x20));
    }
}