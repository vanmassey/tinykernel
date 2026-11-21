#include "kbd.h"
#include "ramfs.h"
#include "rtc.h"
#include "sched.h"
#include "syscall.h"
#include "vga.h"
#include "vfs.h"
#include "serial.h"
#include "string.h"

static uint64_t sys_write(uint64_t fd, const char *buf, size_t len)
{
    if (!buf) {
        return (uint64_t)-1;
    }
    if (fd == 1 || fd == 2) {
        for (size_t i = 0; i < len; i++) {
            vga_putc(buf[i]);
        }
        return len;
    }
    int n = vfs_write((int)fd, buf, len);
    return n < 0 ? (uint64_t)-1 : (uint64_t)n;
}

static uint64_t sys_read(uint64_t fd, char *buf, size_t len)
{
    if (!buf) {
        return (uint64_t)-1;
    }
    if (fd == 0) {
        size_t n = 0;
        while (n < len) {
            int c = kbd_getc();
            if (c < 0) {
                break;
            }
            buf[n++] = (char)c;
            if (c == '\n') {
                break;
            }
        }
        return n;
    }
    int n = vfs_read((int)fd, buf, len);
    return n < 0 ? (uint64_t)-1 : (uint64_t)n;
}

static void sys_exit(int code)
{
    (void)code;
    sched_exit();
}

static void sys_yield(void)
{
    sched_yield();
}

uint64_t syscall_handler(uint64_t num, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
    (void)a3;

    switch (num) {
    case SYS_WRITE:
        return sys_write(a0, (const char *)a1, (size_t)a2);
    case SYS_READ:
        return sys_read(a0, (char *)a1, (size_t)a2);
    case SYS_EXIT:
        sys_exit((int)a0);
        return 0;
    case SYS_YIELD:
        sys_yield();
        return 0;
    case SYS_OPEN:
        return (uint64_t)vfs_open((const char *)a0, (int)a1);
    case SYS_CLOSE:
        return (uint64_t)vfs_close((int)a0);
    case SYS_UNLINK:
        return (uint64_t)ramfs_unlink((const char *)a0);
    case SYS_GETTIME:
        return (uint64_t)rtc_read((struct rtc_time *)a0);
    default:
        serial_write("syscall: unknown ");
        /* Simple hex print */
        char hex[17] = "0x0000000000000000";
        for (int i = 15; i >= 2; i--) {
            uint8_t nibble = num & 0xF;
            hex[i] = nibble < 10 ? '0' + nibble : 'a' + (nibble - 10);
            num >>= 4;
        }
        serial_write(hex);
        serial_write("\n");
        return -1;
    }
}

void syscall_init(void)
{
    extern void syscall_entry(void);
    uint64_t addr = (uint64_t)(uintptr_t)syscall_entry;

    /* IA32_LSTAR MSR = 0xC0000082 */
    __asm__ volatile ("wrmsr" : : "c"(0xC0000082), "a"(addr & 0xFFFFFFFF), "d"(addr >> 32));

    /* IA32_STAR MSR = 0xC0000081:
     * Bits 47:32 = kernel CS (0x08) for syscall target
     * Bits 63:48 = user CS (0x18|3) for sysret target
     * User SS = user CS + 8 = 0x23 */
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)(0x18 | 3) << 48);
    __asm__ volatile ("wrmsr" : : "c"(0xC0000081), "a"(star & 0xFFFFFFFF), "d"(star >> 32));

    /* IA32_FMASK MSR = 0xC0000084: RFLAGS to clear on syscall (IF=1<<9, TF=1<<8) */
    __asm__ volatile ("wrmsr" : : "c"(0xC0000084), "a"((1u << 9) | (1u << 8)), "d"(0));
}