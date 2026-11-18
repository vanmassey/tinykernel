#include "gdt.h"
#include "idt.h"
#include "kernel.h"
#include "serial.h"
#include "vga.h"

extern char kernel_end[];

void halt(void)
{
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void kernel_panic(const char *msg)
{
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_printf("\nPANIC: %s\n", msg);
    serial_write("PANIC: ");
    serial_write(msg);
    serial_write("\n");
    halt();
}

void kernel_main(uint32_t magic, void *multiboot_info)
{
    vga_init();
    serial_init();
    gdt_init();
    idt_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("tinykernel\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_printf("  arch:     x86_64\n");
    vga_printf("  magic:    0x%x\n", magic);
    vga_printf("  mbi:      0x%x\n", (uint32_t)(uintptr_t)multiboot_info);
    vga_printf("  kernel*:  0x%x\n", (uint32_t)(uintptr_t)kernel_end);

    if (magic != 0x36d76289u) {
        kernel_panic("bad Multiboot2 magic");
    }

    vga_write("\nGDT, IDT, VGA, and serial are up. Halted.\n");
    serial_write("tinykernel: boot complete\n");
    halt();
}
