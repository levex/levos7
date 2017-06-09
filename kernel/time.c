#include <levos/kernel.h>
#include <levos/time.h>

#define MODULE_NAME time

static struct timesource *current_timesource;

static uint64_t __boot_time;

void
timesource_set(struct timesource *ts)
{
    mprintk("current timesource set to %s\n", ts->name);
    current_timesource = ts;
}

int
time_init(void)
{
    mprintk("initializing\n");

    rtc_init();

    timesource_set(rtc_get_timesource());

    __boot_time = rtc_get_unix_seconds();

    mprintk("UNIX timestamp of boot: %d\n", __boot_time);

    return 0;
}

uint64_t
time_get_uptime()
{
    return rtc_get_unix_seconds() - __boot_time;
}

/*
 * gettimeofday - query the current timesource
 *                  for a timeval
 */
int
gettimeofday(struct timeval *tv, void *tz)
{
    return current_timesource->gettimeofday(tv, tz);
}
