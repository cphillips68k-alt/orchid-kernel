#include "gdt.h"
#include <stddef.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct tss_entry {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt_entries[5];
static struct tss_entry tss_entry;
static struct gdt_ptr   gdtp;

extern void gdt_flush(uint64_t);

void gdt_init() {
    // Null descriptor
    gdt_entries[0] = (struct gdt_entry){0};

    // Kernel code (64-bit)
    gdt_entries[1] = (struct gdt_entry){
        .limit_low = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = 0x9A,      // present, ring 0, code, readable
        .granularity = 0x20, // long mode
        .base_high = 0
    };

    // Kernel data
    gdt_entries[2] = (struct gdt_entry){
        .limit_low = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = 0x92,      // present, ring 0, data, writable
        .granularity = 0,
        .base_high = 0
    };

    // User code (ring 3) – for later
    gdt_entries[3] = (struct gdt_entry){
        .limit_low = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = 0xFA,      // present, ring 3, code, readable
        .granularity = 0x20,
        .base_high = 0
    };

    // User data
    gdt_entries[4] = (struct gdt_entry){
        .limit_low = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = 0xF2,      // present, ring 3, data, writable
        .granularity = 0,
        .base_high = 0
    };

    // TSS entry (we'll define it after knowing the TSS address)
    // For now, set to zero, tss_init() will configure and flush.
    tss_entry = (struct tss_entry){0};

    gdtp.limit = sizeof(gdt_entries) + sizeof(tss_entry) - 1;
    gdtp.base  = (uint64_t)&gdt_entries;

    // Load GDT and reload segment registers
    __asm__ volatile (
        "lgdt (%0)\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        : : "r"(&gdtp) : "ax", "memory"
    );
}

// Function to set the TSS and load it
void gdt_set_tss(uint64_t base) {
    tss_entry.length = sizeof(struct tss);
    tss_entry.base_low = base & 0xFFFF;
    tss_entry.base_mid = (base >> 16) & 0xFF;
    tss_entry.flags1 = 0x89;    // present, 64-bit TSS
    tss_entry.flags2 = 0;
    tss_entry.base_high = (base >> 24) & 0xFF;
    tss_entry.base_upper = base >> 32;
    tss_entry.reserved = 0;
}