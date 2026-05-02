#include "tss.h"
#include "gdt.h"    // for gdt_set_tss
#include "vmm.h"    // for kmalloc
#include <stddef.h>

// The actual 64-bit TSS layout (only fields we care about)
struct tss64 {
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
    uint16_t io_map_base;
} __attribute__((packed));

static struct tss64 *tss = NULL;

void tss_init(void) {
    // Allocate and zero a page for the TSS
    tss = (struct tss64 *)kmalloc(sizeof(struct tss64));
    if (!tss) return;
    // Zero it
    for (unsigned i = 0; i < sizeof(struct tss64); i++)
        ((char*)tss)[i] = 0;

    // Tell the GDT where the TSS lives
    gdt_set_tss((uint64_t)tss);

    // Load the TSS selector (0x28 as defined in tssflush.S)
    extern void tss_flush(void);
    tss_flush();
}

void tss_set_rsp0(uint64_t rsp0) {
    if (tss)
        tss->rsp0 = rsp0;
}

void tss_set_io_bitmap(int full) {
    if (!tss) return;
    if (full)
        tss->io_map_base = 0xFFFF;  // deny all I/O
    else
        tss->io_map_base = sizeof(struct tss64); // allow all I/O
}