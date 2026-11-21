#include "reboot.h"

#include "io.h"
#include "vga.h"

static void kb_wait(void)
{
    for (int i = 0; i < 100000; i++) {
        if ((inb(0x64) & 0x02) == 0) {
            return;
        }
    }
}

void reboot(void)
{
    vga_write("rebooting\n");
    kb_wait();
    outb(0x64, 0xFE);
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void poweroff(void)
{
    vga_write("poweroff\n");
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
