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

/* All descriptors in one contiguous, packed block */
static struct {
    struct gdt_entry entries[5];
    struct tss_entry tss;
} __attribute__((packed)) gdt;

static struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtp;

extern void gdt_flush(uint64_t);

void gdt_init() {
    gdt.entries[0] = (struct gdt_entry){0};

    gdt.entries[1] = (struct gdt_entry){
        .limit_low = 0, .base_low = 0, .base_mid = 0,
        .access = 0x9A, .granularity = 0x20, .base_high = 0
    };
    gdt.entries[2] = (struct gdt_entry){
        .limit_low = 0, .base_low = 0, .base_mid = 0,
        .access = 0x92, .granularity = 0, .base_high = 0
    };
    gdt.entries[3] = (struct gdt_entry){
        .limit_low = 0, .base_low = 0, .base_mid = 0,
        .access = 0xFA, .granularity = 0x20, .base_high = 0
    };
    gdt.entries[4] = (struct gdt_entry){
        .limit_low = 0, .base_low = 0, .base_mid = 0,
        .access = 0xF2, .granularity = 0, .base_high = 0
    };

    gdt.tss = (struct tss_entry){0};

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint64_t)&gdt;

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

void gdt_set_tss(uint64_t base) {
    gdt.tss.length    = 104;
    gdt.tss.base_low  = base & 0xFFFF;
    gdt.tss.base_mid  = (base >> 16) & 0xFF;
    gdt.tss.flags1    = 0x89;
    gdt.tss.flags2    = 0;
    gdt.tss.base_high = (base >> 24) & 0xFF;
    gdt.tss.base_upper = base >> 32;
    gdt.tss.reserved  = 0;
}