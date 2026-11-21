#pragma once

#include <stddef.h>

void kheap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t n, size_t size);
void kfree(void *ptr);
size_t kheap_used(void);
