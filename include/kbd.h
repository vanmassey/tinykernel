#pragma once

void kbd_init(void);
void kbd_irq(void);
/* Next queued character, or -1 if the buffer is empty. */
int kbd_getc(void);
