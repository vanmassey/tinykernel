#include "io.h"
#include "kbd.h"

#define KBD_DATA 0x60

static const char keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

#define KBD_BUF 128

static char ring[KBD_BUF];
static unsigned ring_head;
static unsigned ring_tail;

void kbd_init(void)
{
    ring_head = ring_tail = 0;
}

int kbd_getc(void)
{
    if (ring_head == ring_tail) {
        return -1;
    }
    char c = ring[ring_tail];
    ring_tail = (ring_tail + 1) % KBD_BUF;
    return (unsigned char)c;
}

void kbd_irq(void)
{
    uint8_t sc = inb(KBD_DATA);
    if (sc & 0x80) {
        return;
    }
    if (sc >= sizeof(keymap) || keymap[sc] == 0) {
        return;
    }
    unsigned next = (ring_head + 1) % KBD_BUF;
    if (next == ring_tail) {
        return;
    }
    ring[ring_head] = keymap[sc];
    ring_head = next;
}
