#include "tss.h"
#include <stddef.h>

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

static struct tss tss_data;

extern void tss_flush(uint16_t sel);

void tss_init(void) {
    /* Zero the TSS */
    for (int i = 0; i < sizeof(tss_data); i++)
        ((volatile uint8_t*)&tss_data)[i] = 0;

    /* 
     * Later, when we switch to user mode, tss_set_rsp0 will be called
     * with the kernel stack for the current task.
     */
    // Set IST entries if we want interrupt stack switching (optional)

    /* The TSS selector is 0x18 (5th GDT entry, after NULL, code, data, NULL * 2) */
    tss_flush(0x18);
}

void tss_set_rsp0(uint64_t rsp0) {
    tss_data.rsp0 = rsp0;
}