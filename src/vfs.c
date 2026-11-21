#include "vfs.h"

#include "ramfs.h"
#include "string.h"

#define VFS_MAX 16
#define VFS_FIRST 3
#define VFS_NAME 16
#define VFS_CAP 191

struct vfs_fd {
    int used;
    int flags;
    char name[VFS_NAME];
    size_t pos;
};

static struct vfs_fd table[VFS_MAX];

void vfs_init(void)
{
    memset(table, 0, sizeof(table));
}

static int valid(int fd)
{
    return fd >= VFS_FIRST && fd < VFS_MAX && table[fd].used;
}

int vfs_open(const char *name, int flags)
{
    if (!name || !name[0] || strlen(name) >= VFS_NAME) {
        return -1;
    }
    size_t len = 0;
    const char *existing = ramfs_read(name, &len);
    if (!existing) {
        if (!(flags & VFS_CREATE)) {
            return -1;
        }
        if (ramfs_write(name, "", 0) != 0) {
            return -1;
        }
    }

    for (int i = VFS_FIRST; i < VFS_MAX; i++) {
        if (table[i].used) {
            continue;
        }
        table[i].used = 1;
        table[i].flags = flags;
        table[i].pos = 0;
        strncpy(table[i].name, name, VFS_NAME - 1);
        table[i].name[VFS_NAME - 1] = '\0';
        return i;
    }
    return -1;
}

int vfs_close(int fd)
{
    if (!valid(fd)) {
        return -1;
    }
    table[fd].used = 0;
    table[fd].name[0] = '\0';
    table[fd].pos = 0;
    return 0;
}

int vfs_read(int fd, char *buf, size_t n)
{
    if (!valid(fd) || !buf) {
        return -1;
    }
    size_t len = 0;
    const char *data = ramfs_read(table[fd].name, &len);
    if (!data) {
        return -1;
    }
    if (table[fd].pos >= len || n == 0) {
        return 0;
    }
    if (table[fd].pos + n > len) {
        n = len - table[fd].pos;
    }
    memcpy(buf, data + table[fd].pos, n);
    table[fd].pos += n;
    return (int)n;
}

int vfs_write(int fd, const char *buf, size_t n)
{
    if (!valid(fd) || !buf) {
        return -1;
    }
    if (!(table[fd].flags & VFS_WRITE)) {
        return -1;
    }

    char tmp[VFS_CAP + 1];
    size_t oldlen = 0;
    const char *old = ramfs_read(table[fd].name, &oldlen);
    if (!old) {
        oldlen = 0;
    }
    if (oldlen > VFS_CAP) {
        oldlen = VFS_CAP;
    }
    if (oldlen) {
        memcpy(tmp, old, oldlen);
    }

    if (table[fd].pos > VFS_CAP) {
        return -1;
    }
    if (table[fd].pos + n > VFS_CAP) {
        n = VFS_CAP - table[fd].pos;
    }
    if (table[fd].pos > oldlen) {
        memset(tmp + oldlen, 0, table[fd].pos - oldlen);
    }
    memcpy(tmp + table[fd].pos, buf, n);
    size_t newlen = table[fd].pos + n;
    if (newlen < oldlen) {
        newlen = oldlen;
    }
    tmp[newlen] = '\0';
    if (ramfs_write(table[fd].name, tmp, newlen) != 0) {
        return -1;
    }
    table[fd].pos += n;
    return (int)n;
}

int vfs_seek(int fd, size_t pos)
{
    if (!valid(fd)) {
        return -1;
    }
    table[fd].pos = pos;
    return 0;
}

const char *vfs_name(int fd)
{
    if (!valid(fd)) {
        return 0;
    }
    return table[fd].name;
}
