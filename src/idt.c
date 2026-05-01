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

extern uint64_t isr_stub_table[];
extern void syscall_entry(void);

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

    __asm__ volatile (
        "movb $0xFF, %%al\n"
        "outb %%al, $0x21\n"
        "outb %%al, $0xA1\n"
        ::: "al"
    );

    for (int i = 0; i < 32; i++)
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);
    for (int i = 32; i < 48; i++)
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);

    __asm__ volatile ("lidt (%0)" : : "r"(&idtp));

    /* Enable SYSCALL/SYSRET */
    __asm__ volatile (
        "mov $0xC0000080, %%ecx\n"
        "rdmsr\n"
        "or $1, %%eax\n"
        "wrmsr\n"
        "mov $0xC0000081, %%ecx\n"
        "mov $0x00180008, %%eax\n"   /* STAR: kernel CS 0x08, user CS base 0x18 */
        "xor %%edx, %%edx\n"
        "wrmsr\n"
        "mov $0xC0000082, %%ecx\n"
        "mov %0, %%rax\n"
        "mov %%rax, %%rdx\n"
        "shr $32, %%rdx\n"
        "wrmsr\n"
        "mov $0xC0000084, %%ecx\n"
        "mov $0x300, %%eax\n"        /* SFMASK: disable interrupts during syscall */
        "xor %%edx, %%edx\n"
        "wrmsr\n"
        : : "r"(syscall_entry) : "rax","rcx","rdx","memory"
    );
}