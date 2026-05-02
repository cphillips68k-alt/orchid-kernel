#ifndef TIMER_H
#define TIMER_H
#include <stdint.h>
#include "scheduler.h"

void timer_init(void);
void timer_tick(void);
void sleep_until(uint64_t tick_deadline);
uint64_t timer_get_ticks(void);

#endif