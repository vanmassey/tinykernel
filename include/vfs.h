#pragma once

#include <stddef.h>
#include <stdint.h>

#define VFS_READ   1
#define VFS_WRITE  2
#define VFS_CREATE 4

void vfs_init(void);
int vfs_open(const char *name, int flags);
int vfs_close(int fd);
int vfs_read(int fd, char *buf, size_t n);
int vfs_write(int fd, const char *buf, size_t n);
int vfs_seek(int fd, size_t pos);
const char *vfs_name(int fd);
