#include "rtc.h"

#include "io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int update_in_progress(void)
{
    return (cmos_read(0x0A) & 0x80) != 0;
}

static uint8_t bcd(uint8_t v, int binary)
{
    if (binary) {
        return v;
    }
    return (uint8_t)((v & 0x0F) + ((v >> 4) * 10));
}

int rtc_read(struct rtc_time *out)
{
    if (!out) {
        return -1;
    }

    uint8_t sec = 0, min = 0, hour = 0, day = 0, month = 0, year = 0;
    uint8_t century = 0, status_b = 0;
    for (int try = 0; try < 4; try++) {
        while (update_in_progress()) {
        }
        sec = cmos_read(0x00);
        min = cmos_read(0x02);
        hour = cmos_read(0x04);
        day = cmos_read(0x07);
        month = cmos_read(0x08);
        year = cmos_read(0x09);
        century = cmos_read(0x32);
        status_b = cmos_read(0x0B);
        while (update_in_progress()) {
        }
        if (sec == cmos_read(0x00) && min == cmos_read(0x02)) {
            break;
        }
    }

    int binary = (status_b & 0x04) != 0;
    int hour24 = (status_b & 0x02) != 0;

    sec = bcd(sec, binary);
    min = bcd(min, binary);
    hour = bcd((uint8_t)(hour & 0x7F), binary);
    day = bcd(day, binary);
    month = bcd(month, binary);
    year = bcd(year, binary);

    if (!hour24 && (cmos_read(0x04) & 0x80)) {
        hour = (uint8_t)((hour % 12) + 12);
    }

    unsigned full = 2000u + year;
    if (century != 0 && century != 0xFF) {
        unsigned c = bcd(century, binary);
        if (c >= 19 && c <= 99) {
            full = c * 100u + year;
        }
    }

    out->sec = sec;
    out->min = min;
    out->hour = hour;
    out->day = day;
    out->month = month;
    out->year = (uint16_t)full;
    return 0;
}
