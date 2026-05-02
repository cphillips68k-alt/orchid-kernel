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

static uint8_t io_bitmap_zero[4096] __attribute__((aligned(4096))) = {0};
static uint8_t io_bitmap_full[4096] __attribute__((aligned(4096))) = {
    [0 ... 4095] = 0xFF
};

extern void tss_flush(uint16_t sel);

void tss_init(void) {
    for (size_t i = 0; i < sizeof(tss_data); i++)
        ((volatile uint8_t*)&tss_data)[i] = 0;
    gdt_set_tss((uint64_t)&tss_data);
    tss_flush(0x28);
}

void tss_set_rsp0(uint64_t rsp0) {
    tss_data.rsp0 = rsp0;
}

void tss_set_io_bitmap(int full) {
    tss_data.iopb_offset = full
        ? (uint16_t)((uint64_t)io_bitmap_full - (uint64_t)&tss_data)
        : (uint16_t)((uint64_t)io_bitmap_zero - (uint64_t)&tss_data);
}