#ifndef __LEVOS_TIME_H
#define __LEVOS_TIME_H

#include <levos/kernel.h>
#include <levos/types.h>

struct timeval {
    uint64_t tv_sec;
    uint64_t tv_usec;
};

struct timesource {
    char *name;
    int (*gettimeofday)(struct timeval *, void *);
};

int time_init();
int rtc_init();
int gettimeofday(struct timeval *, void *);

#endif
