#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * Virtual memory.
 *
 * The kernel is linked at KERNEL_VMA and mapped there with the higher
 * half direct map (HHDM) giving access to all of physical memory at
 * HHDM_BASE + phys. The boot loader's identity map is kept until the
 * higher-half mappings are installed, then removed.
 */

#define PAGE_SIZE        4096u
#define PAGE_SHIFT       12
#define PAGE_MASK        (~(PAGE_SIZE - 1))

/* 512 entries per page table level, 9 bits of index each. */
#define PT_ENTRIES       512

#define PML4_INDEX(v)    (((uintptr_t)(v) >> 39) & 0x1FF)
#define PDPT_INDEX(v)    (((uintptr_t)(v) >> 30) & 0x1FF)
#define PD_INDEX(v)      (((uintptr_t)(v) >> 21) & 0x1FF)
#define PT_INDEX(v)      (((uintptr_t)(v) >> 12) & 0x1FF)

/* Where the kernel image lives, and where physical memory is reachable. */
#define KERNEL_VMA       0xffffffff80000000ull
#define HHDM_BASE        0xffff800000000000ull
#define HHDM_PCI_BASE    0xffffff0000000000ull

/* The boot loader identity-maps the first 1 GiB; keep it around briefly. */
#define IDENTITY_MAP_BYTES (1ull << 30)

/* Page table entry flags. */
#define PAGE_PRESENT     (1ull << 0)
#define PAGE_WRITE       (1ull << 1)
#define PAGE_USER        (1ull << 2)
#define PAGE_PWT         (1ull << 3)
#define PAGE_PCD         (1ull << 4)
#define PAGE_ACCESSED    (1ull << 5)
#define PAGE_DIRTY       (1ull << 6)
#define PAGE_HUGE        (1ull << 7)
#define PAGE_GLOBAL      (1ull << 8)
#define PAGE_NX          (1ull << 63)

#define PAGE_FLAGS_USER  (PAGE_PRESENT | PAGE_USER | PAGE_NX)
#define PAGE_FLAGS_USER_W (PAGE_PRESENT | PAGE_WRITE | PAGE_USER | PAGE_NX)
#define PAGE_FLAGS_KERNEL (PAGE_PRESENT | PAGE_WRITE | PAGE_NX)
#define PAGE_FLAGS_RO    (PAGE_PRESENT | PAGE_NX)

/* phys_to_virt / virt_to_phys: the direct map makes these free. */
static inline void *phys_to_virt(uint64_t phys)
{
    return (void *)(HHDM_BASE + phys);
}

static inline uint64_t virt_to_phys(const void *virt)
{
    return (uint64_t)(uintptr_t)virt - HHDM_BASE;
}

void vmm_init(void);
int vmm_map(uint64_t phys, uint64_t virt, uint64_t flags);
int vmm_unmap(uint64_t virt);
int vmm_map_range(uint64_t phys, uint64_t virt, size_t pages, uint64_t flags);
uint64_t vmm_get_phys(uint64_t virt);
uint64_t vmm_new_address_space(void);
void vmm_set_address_space(uint64_t pml4_phys);
uint64_t vmm_current_address_space(void);
int vmm_is_mapped(uint64_t virt);