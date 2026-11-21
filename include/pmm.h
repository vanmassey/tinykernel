#pragma once

#include <stddef.h>
#include <stdint.h>

#define PMM_PAGE_SIZE 4096u

void pmm_init(const void *mbi);
void *pmm_alloc_page(void);
void *pmm_alloc_pages(size_t count);
void pmm_free_page(void *page);
void pmm_free_pages(void *page, size_t count);
uint64_t pmm_total_pages(void);
uint64_t pmm_free_pages(void);
