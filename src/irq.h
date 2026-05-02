#ifndef IRQ_H
#define IRQ_H
#include <stdint.h>

void irq_init(void);
int  irq_register(uint8_t irq, uint64_t port);
void irq_handler(uint8_t irq);
#endif