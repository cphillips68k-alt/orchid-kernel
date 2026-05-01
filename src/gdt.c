#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt[5];
static struct gdt_ptr   gdtp;

extern void gdt_flush(uint64_t);

void gdt_init() {
    // Null descriptor
    gdt[0].limit_low = 0;
    gdt[0].base_low = 0;
    gdt[0].base_mid = 0;
    gdt[0].access = 0;
    gdt[0].granularity = 0;
    gdt[0].base_high = 0;

    // Kernel code (64-bit)
    gdt[1].limit_low = 0;
    gdt[1].base_low = 0;
    gdt[1].base_mid = 0;
    gdt[1].access = 0x9A;    // present, ring 0, code, readable
    gdt[1].granularity = 0x20; // long mode = 1
    gdt[1].base_high = 0;

    // Kernel data
    gdt[2].limit_low = 0;
    gdt[2].base_low = 0;
    gdt[2].base_mid = 0;
    gdt[2].access = 0x92;    // present, ring 0, data, writable
    gdt[2].granularity = 0;
    gdt[2].base_high = 0;

    // NULL TSS (not used)
    gdt[3] = (struct gdt_entry){0};
    gdt[4] = (struct gdt_entry){0};

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint64_t)&gdt;

    // Load GDT and reload segment registers
    __asm__ volatile (
        "lgdt (%0)\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        // Do a far return to reload CS
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        : : "r"(&gdtp) : "ax", "memory"
    );
}