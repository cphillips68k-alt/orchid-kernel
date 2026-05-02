#ifndef TSS_H
#define TSS_H
#include <stdint.h>

void tss_init(void);
void tss_set_rsp0(uint64_t rsp0);
void tss_set_io_bitmap(int full);

#endif