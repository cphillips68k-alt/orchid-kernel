#include "idt.h"
#include <stdint.h>

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

extern void isr0();
extern void isr1();
// ... we'll use a stub array in assembly; for brevity we just declare all 48 entries
extern uint64_t isr_stub_table[];

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_mid  = (base >> 16) & 0xFFFF;
    idt[num].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].sel       = sel;
    idt[num].ist       = 0;
    idt[num].flags     = flags;
    idt[num].reserved  = 0;
}

void idt_init() {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    // Remap PIC - master to 0x20, slave to 0x28
    __asm__ volatile (
        "movb $0x11, %%al\n"
        "outb %%al, $0x20\n"
        "outb %%al, $0xA0\n"
        "movb $0x20, %%al\n"
        "outb %%al, $0x21\n"
        "movb $0x28, %%al\n"
        "outb %%al, $0xA1\n"
        "movb $0x04, %%al\n"
        "outb %%al, $0x21\n"
        "movb $0x02, %%al\n"
        "outb %%al, $0xA1\n"
        "movb $0x01, %%al\n"
        "outb %%al, $0x21\n"
        "outb %%al, $0xA1\n"
        ::: "al"
    );

    // Mask all interrupts initially
    __asm__ volatile (
        "movb $0xFF, %%al\n"
        "outb %%al, $0x21\n"
        "outb %%al, $0xA1\n"
        ::: "al"
    );

    // Install exception handlers (0-31)
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);
    }

    // Install IRQ handlers (32-47)
    for (int i = 32; i < 48; i++) {
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);
    }

    // Load IDT
    __asm__ volatile ("lidt (%0)" : : "r"(&idtp));
}