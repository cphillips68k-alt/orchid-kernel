#include "tss.h"
#include "gdt.h"
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

    /* Set up the TSS entry in the GDT */
    gdt_set_tss((uint64_t)&tss_data);

    /* Load the TSS selector (0x18 = 5th entry: after NULL, code, data, user code, user data) */
    tss_flush(0x18);
}

void tss_set_rsp0(uint64_t rsp0) {
    tss_data.rsp0 = rsp0;
}