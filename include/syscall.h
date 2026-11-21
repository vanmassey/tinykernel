#pragma once

#include <stddef.h>
#include <stdint.h>

/* Syscall numbers. */
#define SYS_WRITE   0
#define SYS_EXIT    1
#define SYS_READ    2
#define SYS_YIELD   3
#define SYS_OPEN    4
#define SYS_CLOSE   5
#define SYS_UNLINK  6
#define SYS_GETTIME 7

void syscall_init(void);

/* Called from syscall entry stub in isr.S */
uint64_t syscall_handler(uint64_t num, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3);

/* User-mode entry helper. */
int task_spawn_user(const char *name, uint64_t entry, uint64_t ustack_top);