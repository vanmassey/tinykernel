#include "commands.h"

#include "io.h"
#include "klog.h"
#include "pipe.h"
#include "ramfs.h"
#include "reboot.h"
#include "rtc.h"
#include "string.h"
#include "vga.h"

#define HIST_MAX 8
#define HIST_LEN 96

static char history[HIST_MAX][HIST_LEN];
static unsigned history_count;

void shell_note(const char *line)
{
    if (!line || !line[0]) {
        return;
    }
    char *slot = history[history_count % HIST_MAX];
    strncpy(slot, line, HIST_LEN - 1);
    slot[HIST_LEN - 1] = '\0';
    history_count++;
}

static int take(const char *line, const char *cmd, const char **rest)
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

static void split_name(const char *rest, char *name, size_t cap, const char **after)
{
    size_t i = 0;
    while (rest[i] && rest[i] != ' ' && i + 1 < cap) {
        name[i] = rest[i];
        i++;
    }
    name[i] = '\0';
    const char *p = rest + i;
    while (*p == ' ') {
        p++;
    }
    *after = p;
}

static void cmd_history(void)
{
    if (history_count == 0) {
        vga_write("(no history)\n");
        return;
    }
    unsigned start = history_count > HIST_MAX ? history_count - HIST_MAX : 0;
    for (unsigned i = start; i < history_count; i++) {
        vga_printf("  %u  %s\n", i + 1, history[i % HIST_MAX]);
    }
}

static void cmd_date(void)
{
    struct rtc_time t;
    if (rtc_read(&t) != 0) {
        vga_write("rtc read failed\n");
        return;
    }
    vga_printf("%u-%u-%u %u:%u:%u\n",
               (unsigned)t.year, (unsigned)t.month, (unsigned)t.day,
               (unsigned)t.hour, (unsigned)t.min, (unsigned)t.sec);
}

static void cmd_rm(const char *name)
{
    if (!name[0]) {
        vga_write("usage: rm <name>\n");
        return;
    }
    if (ramfs_unlink(name) != 0) {
        vga_printf("no such file: %s\n", name);
        return;
    }
    vga_printf("removed %s\n", name);
}

static void cmd_mv(const char *rest)
{
    char src[16];
    const char *dst = rest;
    split_name(rest, src, sizeof(src), &dst);
    if (!src[0] || !dst[0]) {
        vga_write("usage: mv <old> <new>\n");
        return;
    }
    if (ramfs_rename(src, dst) != 0) {
        vga_write("mv failed\n");
        return;
    }
    vga_printf("renamed %s -> %s\n", src, dst);
}

static void cmd_cp(const char *rest)
{
    char src[16];
    const char *dst = rest;
    split_name(rest, src, sizeof(src), &dst);
    if (!src[0] || !dst[0]) {
        vga_write("usage: cp <src> <dst>\n");
        return;
    }
    size_t len = 0;
    const char *data = ramfs_read(src, &len);
    if (!data) {
        vga_printf("no such file: %s\n", src);
        return;
    }
    if (ramfs_write(dst, data, len) != 0) {
        vga_write("cp failed\n");
        return;
    }
    vga_printf("copied %s -> %s (%u)\n", src, dst, (unsigned)len);
}

static void cmd_wc(const char *name)
{
    if (!name[0]) {
        vga_write("usage: wc <name>\n");
        return;
    }
    size_t len = 0;
    const char *data = ramfs_read(name, &len);
    if (!data) {
        vga_printf("no such file: %s\n", name);
        return;
    }
    unsigned lines = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            lines++;
        }
    }
    vga_printf("%u lines  %u bytes  %s\n", lines, (unsigned)len, name);
}

static void cmd_stat(const char *name)
{
    if (!name[0]) {
        vga_write("usage: stat <name>\n");
        return;
    }
    size_t len = 0;
    if (!ramfs_read(name, &len)) {
        vga_printf("no such file: %s\n", name);
        return;
    }
    vga_printf("%s  %u bytes\n", name, (unsigned)len);
}

static void cmd_hex(const char *name)
{
    if (!name[0]) {
        vga_write("usage: hex <name>\n");
        return;
    }
    size_t len = 0;
    const char *data = ramfs_read(name, &len);
    if (!data) {
        vga_printf("no such file: %s\n", name);
        return;
    }
    for (size_t i = 0; i < len; i++) {
        if ((i & 15) == 0) {
            vga_printf("%u:", (unsigned)i);
        }
        vga_printf(" %x", (unsigned)(unsigned char)data[i]);
        if ((i & 15) == 15 || i + 1 == len) {
            vga_putc('\n');
        }
    }
    if (len == 0) {
        vga_write("(empty)\n");
    }
}

static void cmd_logcat(void)
{
    char buf[64];
    unsigned left = pipe_used(PIPE_SYS);
    if (left == 0) {
        vga_write("(pipe empty)\n");
        return;
    }
    while (pipe_used(PIPE_SYS) > 0) {
        int n = pipe_read(PIPE_SYS, buf, sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';
        vga_write(buf);
    }
}

static void cmd_regs(void)
{
    vga_printf("cr0 %lx\n", (unsigned long)read_cr0());
    vga_printf("cr2 %lx\n", (unsigned long)read_cr2());
    vga_printf("cr3 %lx\n", (unsigned long)read_cr3());
    vga_printf("cr4 %lx\n", (unsigned long)read_cr4());
}

static void cmd_capture(const char *name)
{
    if (!name[0]) {
        vga_write("usage: capture <name>\n");
        return;
    }
    char buf[160];
    size_t n = 0;
    while (n + 1 < sizeof(buf) && pipe_used(PIPE_SYS) > 0) {
        int got = pipe_read(PIPE_SYS, buf + n, sizeof(buf) - 1 - n);
        if (got <= 0) {
            break;
        }
        n += (size_t)got;
    }
    buf[n] = '\0';
    if (ramfs_write(name, buf, n) != 0) {
        vga_write("capture failed\n");
        return;
    }
    vga_printf("captured %u bytes into %s\n", (unsigned)n, name);
}

int shell_extra(const char *line)
{
    const char *rest = line;
    if (take(line, "date", &rest)) {
        cmd_date();
        return 1;
    }
    if (take(line, "dmesg", &rest)) {
        klog_dump();
        return 1;
    }
    if (take(line, "rm", &rest)) {
        cmd_rm(rest);
        return 1;
    }
    if (take(line, "mv", &rest)) {
        cmd_mv(rest);
        return 1;
    }
    if (take(line, "cp", &rest)) {
        cmd_cp(rest);
        return 1;
    }
    if (take(line, "wc", &rest)) {
        cmd_wc(rest);
        return 1;
    }
    if (take(line, "stat", &rest)) {
        cmd_stat(rest);
        return 1;
    }
    if (take(line, "hex", &rest)) {
        cmd_hex(rest);
        return 1;
    }
    if (take(line, "logcat", &rest)) {
        cmd_logcat();
        return 1;
    }
    if (take(line, "regs", &rest)) {
        cmd_regs();
        return 1;
    }
    if (take(line, "history", &rest)) {
        cmd_history();
        return 1;
    }
    if (take(line, "capture", &rest)) {
        cmd_capture(rest);
        return 1;
    }
    if (take(line, "reboot", &rest)) {
        reboot();
    }
    if (take(line, "poweroff", &rest)) {
        poweroff();
    }
    return 0;
}
