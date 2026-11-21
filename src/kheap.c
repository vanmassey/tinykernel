#include "kheap.h"
#include "pmm.h"
#include "string.h"

#include <stdint.h>

#define HEAP_MAGIC       0x48454150u /* "HEAP" */
#define LARGE_MAGIC      0x4C415247u /* "LARG" */
#define ALIGN            16u
#define SMALL_LIMIT      (PMM_PAGE_SIZE / 2)

struct block {
    size_t size;
    uint32_t magic;
    uint32_t free;
};

struct large {
    size_t pages;
    uint32_t magic;
    uint32_t pad;
};

struct heap_page {
    struct heap_page *next;
};

static struct heap_page *pages;
static size_t used_bytes;

static size_t align_up(size_t n)
{
    return (n + (ALIGN - 1)) & ~(size_t)(ALIGN - 1);
}

static struct block *first_block(struct heap_page *page)
{
    return (struct block *)((uint8_t *)page + align_up(sizeof(struct heap_page)));
}

static int in_page(struct heap_page *page, void *ptr)
{
    uint8_t *base = (uint8_t *)page;
    uint8_t *p = ptr;
    return p >= base && p < base + PMM_PAGE_SIZE;
}

static void init_page(struct heap_page *page)
{
    uint8_t *base = (uint8_t *)page;
    struct block *b = first_block(page);
    size_t header_off = (size_t)((uint8_t *)b - base);
    b->size  = PMM_PAGE_SIZE - header_off - sizeof(struct block);
    b->magic = HEAP_MAGIC;
    b->free  = 1;
    page->next = pages;
    pages = page;
}

static void split_block(struct block *b, size_t want)
{
    size_t leftover = b->size - want;
    if (leftover < sizeof(struct block) + ALIGN) {
        return;
    }
    uint8_t *payload = (uint8_t *)(b + 1);
    struct block *n = (struct block *)(payload + want);
    n->size  = leftover - sizeof(struct block);
    n->magic = HEAP_MAGIC;
    n->free  = 1;
    b->size  = want;
}

static void coalesce_page(struct heap_page *page)
{
    uint8_t *end = (uint8_t *)page + PMM_PAGE_SIZE;
    struct block *b = first_block(page);
    while ((uint8_t *)b < end) {
        uint8_t *nextp = (uint8_t *)(b + 1) + b->size;
        if (nextp >= end) {
            break;
        }
        struct block *n = (struct block *)nextp;
        if (b->free && n->magic == HEAP_MAGIC && n->free) {
            b->size += sizeof(struct block) + n->size;
            continue;
        }
        b = n;
    }
}

static void *alloc_small(size_t size)
{
    size = align_up(size);
    for (int grow = 0; grow < 8; grow++) {
        for (struct heap_page *page = pages; page; page = page->next) {
            uint8_t *end = (uint8_t *)page + PMM_PAGE_SIZE;
            struct block *b = first_block(page);
            while ((uint8_t *)b < end) {
                if (b->magic == HEAP_MAGIC && b->free && b->size >= size) {
                    split_block(b, size);
                    b->free = 0;
                    used_bytes += b->size;
                    return b + 1;
                }
                b = (struct block *)((uint8_t *)(b + 1) + b->size);
            }
        }
        struct heap_page *fresh = pmm_alloc_page();
        if (!fresh) {
            return 0;
        }
        init_page(fresh);
    }
    return 0;
}

static void *alloc_large(size_t size)
{
    size_t total = align_up(size) + sizeof(struct large);
    size_t pages_n = (total + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    struct large *hdr = pmm_alloc_pages(pages_n);
    if (!hdr) {
        return 0;
    }
    hdr->pages = pages_n;
    hdr->magic = LARGE_MAGIC;
    used_bytes += pages_n * PMM_PAGE_SIZE;
    return hdr + 1;
}

void kheap_init(void)
{
    pages = 0;
    used_bytes = 0;
    struct heap_page *page = pmm_alloc_page();
    if (page) {
        init_page(page);
    }
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return 0;
    }
    if (size > SMALL_LIMIT) {
        return alloc_large(size);
    }
    return alloc_small(size);
}

void *kcalloc(size_t n, size_t size)
{
    size_t total;
    if (n && size > ((size_t)-1) / n) {
        return 0;
    }
    total = n * size;
    void *p = kmalloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }

    struct large *large = (struct large *)ptr - 1;
    if (large->magic == LARGE_MAGIC) {
        used_bytes -= large->pages * PMM_PAGE_SIZE;
        pmm_free_pages(large, large->pages);
        return;
    }

    struct block *b = (struct block *)ptr - 1;
    if (b->magic != HEAP_MAGIC || b->free) {
        return;
    }

    for (struct heap_page *page = pages; page; page = page->next) {
        if (!in_page(page, b)) {
            continue;
        }
        used_bytes -= b->size;
        b->free = 1;
        coalesce_page(page);
        return;
    }
}

size_t kheap_used(void)
{
    return used_bytes;
}
