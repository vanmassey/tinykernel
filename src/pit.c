#include "io.h"
#include "pit.h"
#include "sched.h"

#define PIT_CH0 0x40
#define PIT_CMD 0x43
#define PIT_HZ  1193182u

static volatile uint64_t ticks;
static uint32_t pit_frequency = 100;

void pit_init(uint32_t hz)
{
    if (hz == 0) {
        hz = 100;
    }
    uint32_t divisor = PIT_HZ / hz;
    if (divisor == 0) {
        divisor = 1;
    }
    if (divisor > 0xFFFF) {
        divisor = 0xFFFF;
    }

    outb(PIT_CMD, 0x36); /* ch0, lobyte/hibyte, mode 3 */
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    ticks = 0;
    pit_frequency = hz ? (PIT_HZ / divisor) : 100;
}

void pit_irq(void)
{
    ticks++;
}

uint64_t pit_ticks(void)
{
    return ticks;
}

uint32_t pit_hz(void)
{
    return pit_frequency;
}

uint64_t pit_uptime_ms(void)
{
    return (ticks * 1000ull) / pit_frequency;
}