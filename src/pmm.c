#include "kernel.h"
#include "mb2.h"
#include "pmm.h"
#include "vga.h"

#define PAGE_SIZE      4096u
#define PMM_MAX_BYTES  (1ull << 30) /* matches the 1 GiB identity map */
#define PMM_MAX_PAGES  (PMM_MAX_BYTES / PAGE_SIZE)

extern char kernel_end[];

static uint64_t bitmap[(PMM_MAX_PAGES + 63) / 64];
static uint64_t total_pages;
static uint64_t used_pages;

static void set_bit(uint64_t i)
{
    bitmap[i / 64] |= (1ull << (i % 64));
}

static void clear_bit(uint64_t i)
{
    bitmap[i / 64] &= ~(1ull << (i % 64));
}

static int test_bit(uint64_t i)
{
    return (bitmap[i / 64] >> (i % 64)) & 1ull;
}

static void mark_used(uint64_t addr, uint64_t len)
{
    uint64_t start = addr / PAGE_SIZE;
    uint64_t end   = (addr + len + PAGE_SIZE - 1) / PAGE_SIZE;
    if (start >= PMM_MAX_PAGES) {
        return;
    }
    if (end > PMM_MAX_PAGES) {
        end = PMM_MAX_PAGES;
    }
    for (uint64_t i = start; i < end; i++) {
        if (!test_bit(i)) {
            set_bit(i);
            used_pages++;
        }
    }
}

static void mark_free(uint64_t addr, uint64_t len)
{
    uint64_t start = (addr + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t end   = (addr + len) / PAGE_SIZE;
    if (start >= PMM_MAX_PAGES) {
        return;
    }
    if (end > PMM_MAX_PAGES) {
        end = PMM_MAX_PAGES;
    }
    for (uint64_t i = start; i < end; i++) {
        if (test_bit(i)) {
            clear_bit(i);
            used_pages--;
        }
    }
}

void pmm_init(const void *mbi)
{
    total_pages = PMM_MAX_PAGES;
    used_pages  = PMM_MAX_PAGES;
    for (uint64_t i = 0; i < (PMM_MAX_PAGES + 63) / 64; i++) {
        bitmap[i] = ~0ull;
    }

    const struct mb2_mmap_tag *mmap = mb2_mmap(mbi);
    uint32_t regions = 0;

    if (mmap && mmap->entry_size >= sizeof(struct mb2_mmap_entry)) {
        const uint8_t *entry = (const uint8_t *)mmap->entries;
        const uint8_t *end   = (const uint8_t *)mmap + mmap->size;
        while (entry + mmap->entry_size <= end) {
            const struct mb2_mmap_entry *e = (const struct mb2_mmap_entry *)entry;
            if (e->type == MB2_MEMORY_AVAILABLE) {
                mark_free(e->addr, e->len);
                regions++;
            }
            entry += mmap->entry_size;
        }
    } else {
        mark_free(0x100000, 127ull * 1024 * 1024);
        regions = 1;
        vga_write("  pmm:     no mmap tag, assuming 128 MiB\n");
    }

    /* Keep the first megabyte, VGA, and the kernel image reserved. */
    mark_used(0, (uint64_t)(uintptr_t)kernel_end);
    mark_used(0xA0000, 0x60000);

    vga_printf("  mmap:    %u usable region(s)\n", regions);
    vga_printf("  pages:   %lu free / %lu total (4 KiB)\n",
               pmm_get_free_pages(), pmm_total_pages());
}

void *pmm_alloc_page(void)
{
    return pmm_alloc_pages(1);
}

void *pmm_alloc_pages(size_t count)
{
    if (count == 0) {
        return 0;
    }
    uint64_t run = 0;
    uint64_t start = 0;
    for (uint64_t i = 0; i < PMM_MAX_PAGES; i++) {
        if (!test_bit(i)) {
            if (run == 0) {
                start = i;
            }
            run++;
            if (run == count) {
                for (uint64_t j = 0; j < count; j++) {
                    set_bit(start + j);
                }
                used_pages += count;
                return (void *)(uintptr_t)(start * PAGE_SIZE);
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

void pmm_free_page(void *page)
{
    pmm_free_pages(page, 1);
}

void pmm_free_pages(void *page, size_t count)
{
    uint64_t addr = (uint64_t)(uintptr_t)page;
    if (!page || (addr % PAGE_SIZE) || count == 0) {
        return;
    }
    uint64_t i = addr / PAGE_SIZE;
    for (size_t n = 0; n < count; n++) {
        if (i + n >= PMM_MAX_PAGES || !test_bit(i + n)) {
            continue;
        }
        clear_bit(i + n);
        used_pages--;
    }
}

uint64_t pmm_total_pages(void)
{
    return total_pages;
}

uint64_t pmm_get_free_pages(void)
{
    return total_pages - used_pages;
}
