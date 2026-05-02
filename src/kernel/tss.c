#include "tss.h"
#include "gdt.h"
#include <stddef.h>

struct tss {
    uint32_t reserved0;
    uint32_t rsp0_low;
    uint32_t rsp0_high;
    uint32_t rsp1_low;
    uint32_t rsp1_high;
    uint32_t rsp2_low;
    uint32_t rsp2_high;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t ist1_low;
    uint32_t ist1_high;
    uint32_t ist2_low;
    uint32_t ist2_high;
    uint32_t ist3_low;
    uint32_t ist3_high;
    uint32_t ist4_low;
    uint32_t ist4_high;
    uint32_t ist5_low;
    uint32_t ist5_high;
    uint32_t ist6_low;
    uint32_t ist6_high;
    uint32_t ist7_low;
    uint32_t ist7_high;
    uint32_t reserved3;
    uint32_t reserved4;
    uint16_t reserved5;
    uint16_t iopb_offset;
} __attribute__((packed));

static struct tss tss_data;

static uint8_t io_bitmap_zero[4096] __attribute__((aligned(4096))) = {0};
static uint8_t io_bitmap_full[4096] __attribute__((aligned(4096))) = {
    [0 ... 4095] = 0xFF
};

extern void tss_flush(uint16_t sel);

void tss_init(void) {
    /* Zero the TSS */
    for (size_t i = 0; i < sizeof(tss_data); i++)
        ((volatile uint8_t*)&tss_data)[i] = 0;

    /* Tell the GDT about our TSS */
    gdt_set_tss((uint64_t)&tss_data);

    /* Load the TSS selector (0x28 = 5th GDT entry * 8) */
    tss_flush(0x28);
}

void tss_set_rsp0(uint64_t rsp0) {
    tss_data.rsp0_low  = (uint32_t)(rsp0 & 0xFFFFFFFF);
    tss_data.rsp0_high = (uint32_t)(rsp0 >> 32);
}

void tss_set_io_bitmap(int full) {
    tss_data.iopb_offset = full
        ? (uint16_t)((uint64_t)io_bitmap_full - (uint64_t)&tss_data)
        : (uint16_t)((uint64_t)io_bitmap_zero - (uint64_t)&tss_data);
}