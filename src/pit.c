#include "pit.h"
#include <stdint.h>

void pit_init(void) {
    uint16_t divisor = 1193180 / 100; // ~100 Hz
    __asm__ volatile (
        "movb $0x36, %%al\n"
        "outb %%al, $0x43\n"
        "movb %0, %%al\n"
        "outb %%al, $0x40\n"
        "movb %1, %%al\n"
        "outb %%al, $0x40\n"
        : : "c"((uint8_t)(divisor & 0xFF)), "d"((uint8_t)(divisor >> 8)) : "al"
    );
}