#include "ramfs.h"
#include "string.h"

#define RAMFS_MAX   16
#define RAMFS_NAME  16
#define RAMFS_DATA  192

struct ramfs_file {
    int used;
    char name[RAMFS_NAME];
    char data[RAMFS_DATA];
    size_t len;
};

static struct ramfs_file files[RAMFS_MAX];

static struct ramfs_file *find(const char *name)
{
    for (int i = 0; i < RAMFS_MAX; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            return &files[i];
        }
    }
    return 0;
}

void ramfs_init(void)
{
    memset(files, 0, sizeof(files));
}

int ramfs_write(const char *name, const char *data, size_t len)
{
    if (!name || !name[0] || strlen(name) >= RAMFS_NAME) {
        return -1;
    }
    struct ramfs_file *f = find(name);
    if (!f) {
        for (int i = 0; i < RAMFS_MAX; i++) {
            if (!files[i].used) {
                f = &files[i];
                break;
            }
        }
    }
    if (!f) {
        return -1;
    }
    if (len >= RAMFS_DATA) {
        len = RAMFS_DATA - 1;
    }
    f->used = 1;
    strncpy(f->name, name, RAMFS_NAME - 1);
    f->name[RAMFS_NAME - 1] = '\0';
    memcpy(f->data, data, len);
    f->data[len] = '\0';
    f->len = len;
    return 0;
}

int ramfs_unlink(const char *name)
{
    struct ramfs_file *f = find(name);
    if (!f) {
        return -1;
    }
    f->used = 0;
    f->len = 0;
    f->name[0] = '\0';
    return 0;
}

int ramfs_rename(const char *old_name, const char *new_name)
{
    struct ramfs_file *f = find(old_name);
    if (!f || !new_name || !new_name[0] || strlen(new_name) >= RAMFS_NAME) {
        return -1;
    }
    if (find(new_name)) {
        return -1;
    }
    strncpy(f->name, new_name, RAMFS_NAME - 1);
    f->name[RAMFS_NAME - 1] = '\0';
    return 0;
}

const char *ramfs_read(const char *name, size_t *len)
{
    struct ramfs_file *f = find(name);
    if (!f) {
        return 0;
    }
    if (len) {
        *len = f->len;
    }
    return f->data;
}

int ramfs_count(void)
{
    int n = 0;
    for (int i = 0; i < RAMFS_MAX; i++) {
        if (files[i].used) {
            n++;
        }
    }
    return n;
}

static struct ramfs_file *nth(int index)
{
    for (int i = 0; i < RAMFS_MAX; i++) {
        if (!files[i].used) {
            continue;
        }
        if (index == 0) {
            return &files[i];
        }
        index--;
    }
    return 0;
}

const char *ramfs_name(int index)
{
    struct ramfs_file *f = nth(index);
    return f ? f->name : 0;
}

size_t ramfs_size(int index)
{
    struct ramfs_file *f = nth(index);
    return f ? f->len : 0;
}
