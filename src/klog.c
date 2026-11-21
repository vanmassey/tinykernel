#include "klog.h"

#include "printf.h"
#include "serial.h"
#include "string.h"
#include "vga.h"

#include <stdarg.h>

#define KLOG_SIZE 2048

static char ring[KLOG_SIZE];
static unsigned head;
static unsigned used;

void klog_init(void)
{
    head = 0;
    used = 0;
}

static void putc(char c)
{
    ring[head] = c;
    head = (head + 1) % KLOG_SIZE;
    if (used < KLOG_SIZE) {
        used++;
    }
}

static void write_raw(const char *s)
{
    while (*s) {
        putc(*s++);
    }
}

void klog(const char *msg)
{
    if (!msg) {
        return;
    }
    write_raw(msg);
    serial_write(msg);
    size_t n = strlen(msg);
    if (n == 0 || msg[n - 1] != '\n') {
        putc('\n');
        serial_putc('\n');
    }
}

void klogf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    klog(buf);
}

void klog_dump(void)
{
    unsigned start = (head + KLOG_SIZE - used) % KLOG_SIZE;
    for (unsigned i = 0; i < used; i++) {
        vga_putc(ring[(start + i) % KLOG_SIZE]);
    }
}

unsigned klog_lines(void)
{
    unsigned start = (head + KLOG_SIZE - used) % KLOG_SIZE;
    unsigned n = 0;
    for (unsigned i = 0; i < used; i++) {
        if (ring[(start + i) % KLOG_SIZE] == '\n') {
            n++;
        }
    }
    return n;
}
