#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct gdt_ptr gdtr;

static void set_gate(int idx, uint8_t access, uint8_t gran)
{
    gdt[idx].limit_low = 0;
    gdt[idx].base_low  = 0;
    gdt[idx].base_mid  = 0;
    gdt[idx].access    = access;
    gdt[idx].gran      = gran;
    gdt[idx].base_high = 0;
}

void gdt_init(void)
{
    set_gate(0, 0x00, 0x00); /* null */
    set_gate(1, 0x9A, 0x20); /* kernel code, L=1 */
    set_gate(2, 0x92, 0x00); /* kernel data */
    set_gate(3, 0xFA, 0x20); /* user code for iret, DPL3 */
    set_gate(4, 0xF2, 0x00); /* user data, DPL3 */
    set_gate(5, 0xFA, 0x20); /* user code for sysret, DPL3 */

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)(uintptr_t)&gdt;

    __asm__ volatile (
        "lgdt %0\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        :
        : "m"(gdtr)
        : "rax", "memory"
    );
}
