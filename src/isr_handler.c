#include "isr_handler.h"
#include "serial.h"
#include "scheduler.h"

void isr_handler(struct regs *r) {
    // Handle timer IRQ (0)
    if (r->int_no == 32) {
        schedule();   // Preemptive switch
    }

    // Acknowledge PIC if needed (for IRQs)
    if (r->int_no >= 32 && r->int_no <= 47) {
        if (r->int_no >= 40) {
            __asm__ volatile ("outb %%al, $0xA0" : : "a"(0x20));
        }
        __asm__ volatile ("outb %%al, $0x20" : : "a"(0x20));
    }
}