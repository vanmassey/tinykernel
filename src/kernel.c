#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "kernel.h"
#include "kheap.h"
#include "klog.h"
#include "pipe.h"
#include "pmm.h"
#include "ramfs.h"
#include "sched.h"
#include "serial.h"
#include "shell.h"
#include "string.h"
#include "syscall.h"
#include "ticker.h"
#include "vga.h"
#include "vfs.h"
#include "vmm.h"

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

static void idle_loop(void *arg)
{
    (void)arg;
    for (;;) {
        __asm__ volatile ("sti; hlt");
    }
}

static void seed_ramfs(void)
{
    static const char readme[] =
        "ramfs is in memory. commands: ls cat write echo ps mem uptime\n";
    static const char hello[] = "hello from ramfs\n";
    ramfs_write("readme", readme, sizeof(readme) - 1);
    ramfs_write("hello", hello, sizeof(hello) - 1);
}

void kernel_main(uint32_t magic, void *multiboot_info)
{
    vga_init();
    serial_init();
    klog_init();
    gdt_init();
    idt_init();
    syscall_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("tinykernel\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_printf("  arch:     x86_64\n");
    vga_printf("  magic:    0x%x\n", magic);
    vga_printf("  mbi:      0x%lx\n", (unsigned long)(uintptr_t)multiboot_info);
    vga_printf("  kernel*:  0x%lx\n", (unsigned long)(uintptr_t)kernel_end);

    if (magic != 0x36d76289u) {
        kernel_panic("bad Multiboot2 magic");
    }

    pmm_init(multiboot_info);
    kheap_init();

    char *msg = kmalloc(32);
    if (msg) {
        strcpy(msg, "heap ok");
        vga_printf("  kmalloc:  0x%lx \"%s\" used=%u\n",
                   (unsigned long)(uintptr_t)msg, msg, (unsigned)kheap_used());
        kfree(msg);
    }
    void *big = kmalloc(8192);
    vga_printf("  large:    0x%lx used=%u\n",
               (unsigned long)(uintptr_t)big, (unsigned)kheap_used());
    kfree(big);

    vmm_init();
    ramfs_init();
    seed_ramfs();
    vfs_init();
    pipe_init();
    sched_init();
    klog("kernel ready");

    irq_init();
    __asm__ volatile ("sti");

    vga_write("\nIRQs on (PIT, PS/2). Shell is next.\n");
    serial_write("tinykernel: interrupts enabled\n");

    sched_spawn("shell", shell_task, NULL);
    sched_spawn("ticker", ticker_task, NULL);
    sched_spawn("idle", idle_loop, NULL);
    sched_adopt_idle();
    sched_run();
}
