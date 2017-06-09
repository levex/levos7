#include <levos/kernel.h>
#include <levos/time.h>
#include <levos/arch.h>

#define MODULE_NAME rtc

#define SECONDS_IN_A_YEAR (365 * 24 * 60 * 60)

#define from_bcd(val)  ((val / 16) * 10 + (val & 0xf))

#define CMOS_GET(reg) ({ outportb(0x70, reg); from_bcd(inportb(0x71));});

uint8_t __rtc_get_seconds() { return CMOS_GET(0x00); }
uint8_t __rtc_get_minutes() { return CMOS_GET(0x02); }
uint8_t __rtc_get_hours() { return CMOS_GET(0x04); }
uint8_t __rtc_get_weekday() { return CMOS_GET(0x06); }
uint8_t __rtc_get_dayofmonth() { return CMOS_GET(0x07); }
uint8_t __rtc_get_month() { return CMOS_GET(0x08); }
uint8_t __rtc_get_year() { return CMOS_GET(0x09); }

uint32_t secs_of_years(int years) {
    uint32_t days = 0;
    years += 2000;
    while (years > 1969) {
        days += 365;
        if (years % 4 == 0) {
            if (years % 100 == 0) {
                if (years % 400 == 0) {
                    days++;
                }
            } else {
                days++;
            }
        }
        years--;
    }

    return days * 86400;
}

uint32_t secs_of_month(int months, int year) {
    year += 2000;

    uint32_t days = 0;
    switch(months) {
        case 11: days += 30;
        case 10: days += 31;
        case 9: days += 30;
        case 8: days += 31;
        case 7: days += 31;
        case 6: days += 30;
        case 5: days += 31;
        case 4: days += 30;
        case 3: days += 31;
        case 2: days += 28;
            if ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)))
                days++;
        case 1: days += 31;
        default: break;
    }

    return days * 86400;
}

uint64_t
rtc_get_unix_seconds()
{
    return secs_of_years(__rtc_get_year() - 1) +
        secs_of_month(__rtc_get_month() - 1, __rtc_get_year()) +
        (__rtc_get_dayofmonth() - 1) * 86400 +
        __rtc_get_hours() * 3600 +
        __rtc_get_minutes() * 60 +
        __rtc_get_seconds();
}

int
rtc_gettimeofday(struct timeval *tv, void *tz)
{
    tv->tv_sec = rtc_get_unix_seconds();
    tv->tv_usec = 0;

    return 0;
}

struct timesource rtc_timesource = {
    .name = "RTC",
    .gettimeofday = rtc_gettimeofday,
};

struct timesource *rtc_get_timesource(void)
{
    return &rtc_timesource;
}

int
rtc_init()
{
    mprintk("initializing\n");
}
