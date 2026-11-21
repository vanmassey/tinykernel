#include "idt.h"
#include "irq.h"
#include "kernel.h"
#include "kbd.h"
#include "pic.h"
#include "pit.h"
#include "sched.h"
#include "vga.h"

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

/*
 * A single dispatch table is cleaner than the old hardcoded switch and
 * makes it obvious that every IRQ has a handler slot.
 */
static void (*irq_table[16])(void);

void irq_register(uint8_t irq, void (*handler)(void))
{
    if (irq < 16) {
        irq_table[irq] = handler;
    }
}

void irq_handler(struct interrupt_frame *frame)
{
    uint8_t irq = (uint8_t)(frame->vector - 32);

    if (irq < 16 && irq_table[irq]) {
        irq_table[irq]();
    }

    /* Acknowledge before scheduling: the timer tick that follows must
     * not re-enter the handler while we are still inside it. */
    pic_eoi(irq);

    if (frame->cs & 0x3) {
        return;
    }

    if (irq == 0) {
        sched_tick();
        if (sched_current()) {
            sched_schedule(frame);
        }
    }
}

void irq_init(void)
{
    void (*handlers[16])(void) = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
    };

    for (int i = 0; i < 16; i++) {
        irq_table[i] = 0;
        idt_set_gate(32 + i, handlers[i], 0x8E);
    }

    pic_init();
    pit_init(SCHED_HZ);
    kbd_init();

    irq_register(0, pit_irq);
    irq_register(1, kbd_irq);

    pic_unmask(0);
    pic_unmask(1);
}