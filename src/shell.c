#include "commands.h"
#include "kbd.h"
#include "serial.h"
#include "kheap.h"
#include "pit.h"
#include "pmm.h"
#include "ramfs.h"
#include "sched.h"
#include "shell.h"
#include "string.h"
#include "vga.h"

#define LINE_MAX 96

static int getc_wait(void)
{
    for (;;) {
        int c = kbd_getc();
        if (c >= 0) {
            return c;
        }
        __asm__ volatile ("sti; hlt");
    }
}

static void read_line(char *buf, size_t cap)
{
    size_t n = 0;
    buf[0] = '\0';
    for (;;) {
        int c = getc_wait();
        if (c == '\n') {
            vga_putc('\n');
            buf[n] = '\0';
            return;
        }
        if (c == '\b') {
            if (n > 0) {
                n--;
                buf[n] = '\0';
                vga_backspace();
            }
            continue;
        }
        if (c < 32 || n + 1 >= cap) {
            continue;
        }
        buf[n++] = (char)c;
        buf[n] = '\0';
        vga_putc((char)c);
    }
}

static int starts(const char *line, const char *cmd, const char **rest)
{
    size_t n = strlen(cmd);
    if (strncmp(line, cmd, n) != 0) {
        return 0;
    }
    if (line[n] != '\0' && line[n] != ' ') {
        return 0;
    }
    const char *r = line + n;
    while (*r == ' ') {
        r++;
    }
    *rest = r;
    return 1;
}

static void cmd_ls(void)
{
    int n = ramfs_count();
    if (n == 0) {
        vga_write("(empty)\n");
        return;
    }
    for (int i = 0; i < n; i++) {
        vga_printf("  %s  %u\n", ramfs_name(i), (unsigned)ramfs_size(i));
    }
}

static void cmd_cat(const char *name)
{
    if (!name[0]) {
        vga_write("usage: cat <name>\n");
        return;
    }
    size_t len = 0;
    const char *data = ramfs_read(name, &len);
    if (!data) {
        vga_printf("no such file: %s\n", name);
        return;
    }
    vga_write(data);
    if (len == 0 || data[len - 1] != '\n') {
        vga_putc('\n');
    }
}

static void cmd_write(const char *rest)
{
    char name[16];
    size_t i = 0;
    while (rest[i] && rest[i] != ' ' && i < sizeof(name) - 1) {
        name[i] = rest[i];
        i++;
    }
    name[i] = '\0';
    if (!name[0]) {
        vga_write("usage: write <name> <text>\n");
        return;
    }
    const char *text = rest + i;
    while (*text == ' ') {
        text++;
    }
    size_t len = strlen(text);
    char tmp[160];
    if (len + 2 > sizeof(tmp)) {
        len = sizeof(tmp) - 2;
    }
    memcpy(tmp, text, len);
    tmp[len++] = '\n';
    tmp[len] = '\0';
    if (ramfs_write(name, tmp, len) != 0) {
        vga_write("write failed\n");
        return;
    }
    vga_printf("wrote %s (%u)\n", name, (unsigned)len);
}

static void dispatch(char *line)
{
    const char *rest = line;
    if (line[0] == '\0') {
        return;
    }
    shell_note(line);
    if (shell_extra(line)) {
        return;
    }
    if (starts(line, "help", &rest) || strcmp(line, "?") == 0) {
        vga_write("help ls cat write echo ps mem uptime sleep clear\n");
        vga_write("date dmesg rm mv cp wc stat hex logcat capture\n");
        vga_write("regs history reboot poweroff\n");
        return;
    }
    if (starts(line, "ls", &rest)) {
        cmd_ls();
        return;
    }
    if (starts(line, "cat", &rest)) {
        cmd_cat(rest);
        return;
    }
    if (starts(line, "write", &rest)) {
        cmd_write(rest);
        return;
    }
    if (starts(line, "echo", &rest)) {
        vga_write(rest);
        vga_putc('\n');
        return;
    }
    if (starts(line, "ps", &rest)) {
        sched_dump();
        return;
    }
    if (starts(line, "mem", &rest)) {
        vga_printf("pages free %lu / %lu   heap %u bytes\n",
                   (unsigned long)pmm_free_pages(),
                   (unsigned long)pmm_total_pages(),
                   (unsigned)kheap_used());
        return;
    }
    if (starts(line, "uptime", &rest)) {
        vga_printf("%lu ms  (%lu ticks)\n",
                   (unsigned long)pit_uptime_ms(),
                   (unsigned long)pit_ticks());
        return;
    }
    if (starts(line, "sleep", &rest)) {
        uint64_t ms = 0;
        while (*rest >= '0' && *rest <= '9') {
            ms = ms * 10u + (uint64_t)(*rest - '0');
            rest++;
        }
        sched_sleep(ms ? ms : 1000);
        return;
    }
    if (starts(line, "clear", &rest)) {
        vga_clear();
        return;
    }
    vga_printf("unknown: %s\n", line);
}

void shell_task(void *arg)
{
    (void)arg;
    vga_write("init shell\n");
    vga_write("type help\n");
    serial_write("shell ready\n");

    char line[LINE_MAX];
    for (;;) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_write("tk> ");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        read_line(line, sizeof(line));
        dispatch(line);
    }
}
