#pragma once

#include <stdint.h>

void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t value);
uint16_t inw(uint16_t port);

static inline void io_wait(void)
{
    outb(0x80, 0);
}

/* Read/write the control registers used by the virtual memory code. */
static inline uint64_t read_cr0(void)
{
    uint64_t value;
    __asm__ volatile ("movq %%cr0, %0" : "=r"(value));
    return value;
}

static inline uint64_t read_cr2(void)
{
    uint64_t value;
    __asm__ volatile ("movq %%cr2, %0" : "=r"(value));
    return value;
}

static inline uint64_t read_cr3(void)
{
    uint64_t value;
    __asm__ volatile ("movq %%cr3, %0" : "=r"(value));
    return value;
}

static inline uint64_t read_cr4(void)
{
    uint64_t value;
    __asm__ volatile ("movq %%cr4, %0" : "=r"(value));
    return value;
}

static inline void write_cr3(uint64_t value)
{
    __asm__ volatile ("movq %0, %%cr3" : : "r"(value) : "memory");
}

static inline void cli(void)
{
    __asm__ volatile ("cli");
}

static inline void sti(void)
{
    __asm__ volatile ("sti");
}
