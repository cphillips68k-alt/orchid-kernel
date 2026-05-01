#ifndef GDT_H
#define GDT_H
#include <stdint.h>

void gdt_init(void);
void gdt_set_tss(uint64_t base);
#endif