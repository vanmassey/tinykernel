#pragma once

#include <stddef.h>

#define PIPE_SYS 0

void pipe_init(void);
int pipe_open(void);
int pipe_write(int id, const char *buf, size_t n);
int pipe_read(int id, char *buf, size_t n);
unsigned pipe_used(int id);
