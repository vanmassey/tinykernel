#pragma once

#include <stdint.h>

void pit_init(uint32_t hz);
void pit_irq(void);
uint64_t pit_ticks(void);
uint32_t pit_hz(void);
uint64_t pit_uptime_ms(void);