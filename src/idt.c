#include "idt.h"
#include "kernel.h"
#include "vga.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

static struct idt_entry idt[256];
static struct idt_ptr idtr;

void idt_set_gate(int vec, void (*handler)(void), uint8_t flags)
{
    uint64_t addr = (uint64_t)(uintptr_t)handler;
    idt[vec].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[vec].selector    = 0x08;
    idt[vec].ist         = 0;
    idt[vec].flags       = flags;
    idt[vec].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vec].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vec].reserved    = 0;
}

void exception_handler(struct interrupt_frame *frame)
{
    static const char *names[] = {
        "Divide Error", "Debug", "NMI", "Breakpoint",
        "Overflow", "BOUND", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment", "Invalid TSS", "Segment Not Present",
        "Stack Fault", "General Protection", "Page Fault", "Reserved",
        "x87 FP", "Alignment Check", "Machine Check", "SIMD FP",
        "Virtualization", "Control Protection", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor Injection", "VMM Communication", "Security", "Reserved"
    };

    const char *name = (frame->vector < 32) ? names[frame->vector] : "Unknown";
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_printf("\nexception %u (%s) err=0x%x rip=0x%x\n",
               (unsigned)frame->vector, name,
               (unsigned)frame->error, (unsigned)frame->rip);
    kernel_panic("unhandled CPU exception");
}

void idt_init(void)
{
    void (*handlers[32])(void) = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 256; i++) {
        idt[i].offset_low = idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].flags = 0;
        idt[i].reserved = 0;
    }

    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, handlers[i], 0x8E); /* present, DPL0, interrupt gate */
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)(uintptr_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
