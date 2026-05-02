#include "tss.h"
#include "gdt.h"
#include "vmm.h"    // for kmalloc

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} __attribute__((packed));

static struct tss64 *tss;

void tss_init(void) {
    tss = (struct tss64 *)kmalloc(sizeof(struct tss64));
    if (!tss) return;
    for (unsigned i = 0; i < sizeof(struct tss64); i++)
        ((char*)tss)[i] = 0;

    gdt_set_tss((uint64_t)tss);
    extern void tss_flush(void);
    tss_flush();
}

void tss_set_rsp0(uint64_t rsp0) {
    if (tss) tss->rsp0 = rsp0;
}

void tss_set_io_bitmap(int full) {
    if (!tss) return;
    tss->io_map_base = full ? 0xFFFF : sizeof(struct tss64);
}