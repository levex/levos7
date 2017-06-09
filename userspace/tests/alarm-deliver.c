#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <stdio.h>
#include <errno.h>

#include "test.h"

static int __alarm_tick = 0;

void
alarm_tick(int sig)
{
    printf("alarm ticks\n");
    __alarm_tick ++;
}

int
run_test()
{
    int rc;

    CHECK((int) signal(SIGALRM, alarm_tick), (int) SIG_DFL);

    CHECK(alarm(3), 0);

    CHECK_ERR(sleep(10), EINTR);

    test_success();
}
