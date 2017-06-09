#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "test.h"


static int should_be_delivered = 0;
static volatile int delivered = 0;

void
sigusr1_handler(int sig)
{
    printf("SIGUSR1 handler\n");
    if (should_be_delivered) {
        delivered = 1;
        return;
    }

    printf("SIGUSR1 was delivered too early\n");
    test_failure();
}

int
run_test()
{
    sigset_t set;
    int rc;

    CHECK(signal(SIGUSR1, sigusr1_handler), SIG_DFL);

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);

    CHECK(sigprocmask(SIG_BLOCK, &set, NULL), 0);

    should_be_delivered = 0;
    raise(SIGUSR1);

    sigemptyset(&set);
    should_be_delivered = 1;
    CHECK(sigprocmask(SIG_SETMASK, &set, NULL), 0);

    int i = 1000000;
    while (i --) {
        sleep(1); /* force delivery of signal */
        if (delivered)
            test_success();
    }

    test_failure();
}
