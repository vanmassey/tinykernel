#pragma once

#include <stdint.h>

void irq_init(void);
void irq_register(uint8_t irq, void (*handler)(void));
void irq_handler(struct interrupt_frame *frame);