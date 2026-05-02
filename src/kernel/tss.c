#include "tss.h"

void tss_init(void) {
    /* TSS not needed yet — syscall/sysret doesn't use it,
       and I/O port bitmap only needed when kbd moves to ring 3 */
}

void tss_set_rsp0(uint64_t rsp0) {
    (void)rsp0;
}

void tss_set_io_bitmap(int full) {
    (void)full;
}