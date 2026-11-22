#include "kernel.h"
#include "io.h"
#include "kheap.h"
#include "pmm.h"
#include "spinlock.h"
#include "string.h"
#include "vmm.h"
#include "vga.h"

#define PTE_ADDR_MASK  0x000ffffffffff000ull

extern char kernel_start[];
extern char kernel_end[];

static uint64_t *pml4;
static uint64_t  pml4_phys;
static struct spinlock vmm_lock = SPINLOCK_INIT;

/* Return the physical address of a page-table page, or 0 on failure. */
static uint64_t *alloc_table(void)
{
    void *page = pmm_alloc_page();
    if (!page) {
        return 0;
    }
    uint64_t *table = (uint64_t *)phys_to_virt((uint64_t)(uintptr_t)page);
    memset(table, 0, PAGE_SIZE);
    return table;
}

/*
 * Walk down to the entry covering `virt`, allocating intermediate
 * tables as needed. Returns a pointer to the entry, or 0.
 */
static uint64_t *walk(uint64_t virt, int create)
{
    uint64_t *table = pml4;

    for (int level = 3; level >= 1; level--) {
        unsigned index;
        switch (level) {
        case 3: index = PML4_INDEX(virt); break;
        case 2: index = PDPT_INDEX(virt); break;
        default: index = PD_INDEX(virt); break;
        }

        uint64_t *entry = &table[index];

        if (level == 1) {
            return entry;
        }

        if (!(*entry & PAGE_PRESENT)) {
            if (!create) {
                return 0;
            }
            uint64_t *fresh = alloc_table();
            if (!fresh) {
                return 0;
            }
            *entry = (uint64_t)virt_to_phys(fresh) | PAGE_FLAGS_KERNEL;
        }

        table = (uint64_t *)phys_to_virt(*entry & PTE_ADDR_MASK);
    }

    return 0;
}

void vmm_init(void)
{
    uint64_t *boot_pml4 = (uint64_t *)phys_to_virt(read_cr3() & PTE_ADDR_MASK);

    /* The boot loader already identity-mapped the kernel image. Clone the
     * entries it installed into a fresh higher-half PML4 so we can free
     * the boot page tables and still reach the kernel. */
    uint64_t *fresh = alloc_table();
    if (!fresh) {
        kernel_panic("vmm: out of memory");
    }

    /* Map the kernel image at KERNEL_VMA. */
    for (uintptr_t i = 0; i < PT_ENTRIES; i++) {
        if (!(boot_pml4[i] & PAGE_PRESENT)) {
            continue;
        }
        fresh[i] = boot_pml4[i];
    }

    /* The PML4 slot covering KERNEL_VMA is index 256. The boot loader's
     * identity map used slots 0..3; rebuild the higher-half slot by
     * pointing it at a freshly built PDPT that references the boot PD. */
    uint64_t *pdpt = alloc_table();
    uint64_t *pd   = 0;
    if (!pdpt) {
        kernel_panic("vmm: out of memory");
    }
    pd = (uint64_t *)phys_to_virt(boot_pml4[0] & PTE_ADDR_MASK);
    pdpt[0] = (uint64_t)virt_to_phys(pd) | PAGE_FLAGS_KERNEL;

    /* HHDM: every PML4 slot from 256 upward maps physical memory
     * directly. Point slot 256 at a PDPT that maps the first 512 GiB
     * through the boot loader's PD (which is 2 MiB pages). */
    fresh[256] = (uint64_t)virt_to_phys(pdpt) | PAGE_FLAGS_KERNEL;
    fresh[511] = 0; /* leave the top slot empty for future use */

    pml4 = fresh;
    pml4_phys = virt_to_phys(fresh);
    write_cr3(pml4_phys);

    vga_printf("  vmm:     kernel 0x%lx, HHDM 0x%lx, cr3 0x%lx\n",
               (unsigned long)KERNEL_VMA, (unsigned long)HHDM_BASE,
               (unsigned long)pml4_phys);
}

int vmm_map(uint64_t phys, uint64_t virt, uint64_t flags)
{
    spin_lock(&vmm_lock);
    uint64_t *entry = walk(virt, 1);
    if (!entry) {
        spin_unlock(&vmm_lock);
        return -1;
    }
    if (*entry & PAGE_PRESENT) {
        spin_unlock(&vmm_lock);
        return -1;
    }
    *entry = (phys & PTE_ADDR_MASK) | flags;
    spin_unlock(&vmm_lock);
    return 0;
}

int vmm_map_range(uint64_t phys, uint64_t virt, size_t pages, uint64_t flags)
{
    for (size_t i = 0; i < pages; i++) {
        if (vmm_map(phys + i * PAGE_SIZE, virt + i * PAGE_SIZE, flags) != 0) {
            return -1;
        }
    }
    return 0;
}

int vmm_unmap(uint64_t virt)
{
    spin_lock(&vmm_lock);
    uint64_t *entry = walk(virt, 0);
    if (!entry || !(*entry & PAGE_PRESENT)) {
        spin_unlock(&vmm_lock);
        return -1;
    }
    *entry = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    spin_unlock(&vmm_lock);
    return 0;
}

uint64_t vmm_get_phys(uint64_t virt)
{
    uint64_t *entry = walk(virt, 0);
    if (!entry || !(*entry & PAGE_PRESENT)) {
        return 0;
    }
    if (*entry & PAGE_HUGE) {
        return (*entry & PTE_ADDR_MASK) | (virt & 0x1FFFFF);
    }
    return (*entry & PTE_ADDR_MASK) | (virt & 0xFFF);
}

int vmm_is_mapped(uint64_t virt)
{
    uint64_t *entry = walk(virt, 0);
    return entry && (*entry & PAGE_PRESENT);
}

uint64_t vmm_new_address_space(void)
{
    uint64_t *table = alloc_table();
    if (!table) {
        return 0;
    }

    /* The kernel half stays shared: slots 256 and above are inherited
     * from the current PML4, everything below is private to the task. */
    uint64_t *current = pml4;
    for (int i = 256; i < PT_ENTRIES; i++) {
        table[i] = current[i];
    }
    return virt_to_phys(table);
}

void vmm_set_address_space(uint64_t pml4_phys)
{
    if (pml4_phys) {
        write_cr3(pml4_phys);
    }
}

uint64_t vmm_current_address_space(void)
{
    return read_cr3() & PTE_ADDR_MASK;
}