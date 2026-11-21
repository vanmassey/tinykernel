#pragma once

#include <stdint.h>

#define MB2_TAG_END     0
#define MB2_TAG_MMAP    6
#define MB2_MEMORY_AVAILABLE 1

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));

struct mb2_mmap_tag {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct mb2_mmap_entry entries[];
} __attribute__((packed));

const struct mb2_tag *mb2_find_tag(const void *mbi, uint32_t type);
const struct mb2_mmap_tag *mb2_mmap(const void *mbi);
