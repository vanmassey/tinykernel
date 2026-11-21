#pragma once

#include <stdarg.h>
#include <stddef.h>

int kvsnprintf(char *out, size_t cap, const char *fmt, va_list ap);
int ksnprintf(char *out, size_t cap, const char *fmt, ...);
