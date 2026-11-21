#include "io.h"
#include "pic.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20
#define ICW1_INIT 0x11
#define ICW4_8086 0x01

void pic_init(void)
{
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT);
    io_wait();
    outb(PIC1_DATA, 32); /* IRQ 0-7  → vectors 32-39 */
    io_wait();
    outb(PIC2_DATA, 40); /* IRQ 8-15 → vectors 40-47 */
    io_wait();
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    (void)mask1;
    (void)mask2;
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_unmask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t line = (uint8_t)(irq & 7);
    uint8_t mask = inb(port) & (uint8_t)~(1u << line);
    outb(port, mask);
}
