#include "io.h"
#include "serial.h"

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00); /* disable IRQs */
    outb(COM1 + 3, 0x80); /* DLAB */
    outb(COM1 + 0, 0x03); /* 38400 baud divisor lo */
    outb(COM1 + 1, 0x00); /* divisor hi */
    outb(COM1 + 3, 0x03); /* 8N1 */
    outb(COM1 + 2, 0xC7); /* FIFO */
    outb(COM1 + 4, 0x0B); /* IRQs off, RTS/DSR */
}

static int serial_tx_ready(void)
{
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c)
{
    if (c == '\n') {
        while (!serial_tx_ready()) {
        }
        outb(COM1, '\r');
    }
    while (!serial_tx_ready()) {
    }
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s) {
        serial_putc(*s++);
    }
}
