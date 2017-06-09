#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <stdio.h>

#include "test.h"

static volatile int the_value = 0;

void
sig_handle(int sig)
{
    printf("hi from signal\n");
    the_value = 1;
}

int
run_test()
{
    int rc;

    CHECK((int) signal(SIGUSR1, sig_handle), (int) SIG_DFL);

    raise(SIGUSR1);

    if (the_value == 1)
        test_success();
    
    test_failure();
}
