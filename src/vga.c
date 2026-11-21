#include "serial.h"
#include "vga.h"

#include <stdarg.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_ADDR   0xB8000

static volatile uint16_t *vga = (volatile uint16_t *)VGA_ADDR;
static size_t row;
static size_t col;
static uint8_t color;

static uint8_t make_color(uint8_t fg, uint8_t bg)
{
    return (uint8_t)(fg | (bg << 4));
}

static uint16_t entry(char c, uint8_t attr)
{
    return (uint16_t)c | ((uint16_t)attr << 8);
}

void vga_set_color(uint8_t fg, uint8_t bg)
{
    color = make_color(fg, bg);
}

void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga[y * VGA_WIDTH + x] = entry(' ', color);
        }
    }
    row = 0;
    col = 0;
}

void vga_init(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

static void scroll(void)
{
    if (row < VGA_HEIGHT) {
        return;
    }
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga[(y - 1) * VGA_WIDTH + x] = vga[y * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = entry(' ', color);
    }
    row = VGA_HEIGHT - 1;
}

void vga_putc(char c)
{
    serial_putc(c);

    if (c == '\n') {
        col = 0;
        row++;
        scroll();
        return;
    }
    if (c == '\r') {
        col = 0;
        return;
    }
    if (c == '\t') {
        col = (col + 4) & ~(size_t)3;
        if (col >= VGA_WIDTH) {
            col = 0;
            row++;
            scroll();
        }
        return;
    }

    vga[row * VGA_WIDTH + col] = entry(c, color);
    col++;
    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
        scroll();
    }
}

void vga_backspace(void)
{
    if (col > 0) {
        col--;
    } else if (row > 0) {
        row--;
        col = VGA_WIDTH - 1;
    } else {
        return;
    }
    vga[row * VGA_WIDTH + col] = entry(' ', color);
}

void vga_write(const char *s)
{
    while (*s) {
        vga_putc(*s++);
    }
}

static void print_uint(unsigned long value, unsigned base)
{
    char buf[32];
    const char *digits = "0123456789abcdef";
    int i = 0;

    if (value == 0) {
        vga_putc('0');
        return;
    }
    while (value) {
        buf[i++] = digits[value % base];
        value /= base;
    }
    while (i--) {
        vga_putc(buf[i]);
    }
}

void vga_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            vga_putc(*p);
            continue;
        }
        p++;
        if (*p == 'l') {
            p++;
            if (*p == 'x') {
                print_uint(va_arg(ap, unsigned long), 16);
                continue;
            }
            if (*p == 'u') {
                print_uint(va_arg(ap, unsigned long), 10);
                continue;
            }
        }
        switch (*p) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            vga_write(s ? s : "(null)");
            break;
        }
        case 'c':
            vga_putc((char)va_arg(ap, int));
            break;
        case 'd': {
            int v = va_arg(ap, int);
            if (v < 0) {
                vga_putc('-');
                print_uint((unsigned long)(-(long)v), 10);
            } else {
                print_uint((unsigned long)v, 10);
            }
            break;
        }
        case 'u':
            print_uint(va_arg(ap, unsigned), 10);
            break;
        case 'x':
            print_uint(va_arg(ap, unsigned), 16);
            break;
        case '%':
            vga_putc('%');
            break;
        default:
            vga_putc('%');
            vga_putc(*p);
            break;
        }
    }

    va_end(ap);
}
