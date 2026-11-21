#pragma once

#include <stddef.h>

void ramfs_init(void);
int ramfs_write(const char *name, const char *data, size_t len);
int ramfs_unlink(const char *name);
int ramfs_rename(const char *old_name, const char *new_name);
const char *ramfs_read(const char *name, size_t *len);
int ramfs_count(void);
const char *ramfs_name(int index);
size_t ramfs_size(int index);
