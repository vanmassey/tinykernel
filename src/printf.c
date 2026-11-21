#include "printf.h"

static void emit(char *out, size_t cap, size_t *pos, char c)
{
    if (*pos + 1 < cap) {
        out[*pos] = c;
    }
    (*pos)++;
}

static void emit_str(char *out, size_t cap, size_t *pos, const char *s)
{
    if (!s) {
        s = "(null)";
    }
    while (*s) {
        emit(out, cap, pos, *s++);
    }
}

static void emit_uint(char *out, size_t cap, size_t *pos, unsigned long value, unsigned base)
{
    char tmp[32];
    const char *digits = "0123456789abcdef";
    int i = 0;

    if (value == 0) {
        emit(out, cap, pos, '0');
        return;
    }
    while (value) {
        tmp[i++] = digits[value % base];
        value /= base;
    }
    while (i--) {
        emit(out, cap, pos, tmp[i]);
    }
}

static int format(char *out, size_t cap, const char *fmt, va_list ap)
{
    size_t pos = 0;

    for (; fmt && *fmt; fmt++) {
        if (*fmt != '%') {
            emit(out, cap, &pos, *fmt);
            continue;
        }
        fmt++;
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
        }
        switch (*fmt) {
        case 's':
            emit_str(out, cap, &pos, va_arg(ap, const char *));
            break;
        case 'c':
            emit(out, cap, &pos, (char)va_arg(ap, int));
            break;
        case 'd': {
            long v = is_long ? va_arg(ap, long) : va_arg(ap, int);
            if (v < 0) {
                emit(out, cap, &pos, '-');
                v = -v;
            }
            emit_uint(out, cap, &pos, (unsigned long)v, 10);
            break;
        }
        case 'u':
            emit_uint(out, cap, &pos,
                      is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 10);
            break;
        case 'x':
        case 'p':
            if (*fmt == 'p') {
                emit_str(out, cap, &pos, "0x");
            }
            emit_uint(out, cap, &pos,
                      is_long || *fmt == 'p' ? va_arg(ap, unsigned long) : va_arg(ap, unsigned),
                      16);
            break;
        case '%':
            emit(out, cap, &pos, '%');
            break;
        case '\0':
            fmt--;
            break;
        default:
            emit(out, cap, &pos, '%');
            emit(out, cap, &pos, *fmt);
            break;
        }
    }

    if (cap > 0) {
        size_t end = pos < cap ? pos : cap - 1;
        out[end] = '\0';
    }
    return (int)pos;
}

int kvsnprintf(char *out, size_t cap, const char *fmt, va_list ap)
{
    return format(out, cap, fmt, ap);
}

int ksnprintf(char *out, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = format(out, cap, fmt, ap);
    va_end(ap);
    return n;
}
