#include "mb2.h"

const struct mb2_tag *mb2_find_tag(const void *mbi, uint32_t type)
{
    if (!mbi) {
        return 0;
    }

    const uint8_t *ptr = (const uint8_t *)mbi + 8; /* skip total_size + reserved */
    for (;;) {
        const struct mb2_tag *tag = (const struct mb2_tag *)ptr;
        if (tag->type == MB2_TAG_END) {
            return 0;
        }
        if (tag->type == type) {
            return tag;
        }
        uint32_t size = tag->size;
        if (size < 8) {
            return 0;
        }
        ptr += (size + 7u) & ~7u;
    }
}

const struct mb2_mmap_tag *mb2_mmap(const void *mbi)
{
    return (const struct mb2_mmap_tag *)mb2_find_tag(mbi, MB2_TAG_MMAP);
}
