#pragma once

#include <stddef.h>
#include <stdint.h>

void kernel_panic(const char *msg) __attribute__((noreturn));
void halt(void) __attribute__((noreturn));
