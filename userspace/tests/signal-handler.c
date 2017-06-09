#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <stdio.h>

#include "test.h"

void
sigsegv_handle(int sig)
{
    printf("hi from signal\n");
    test_success();
}

int
run_test()
{
    int rc;

    CHECK((int) signal(SIGSEGV, sigsegv_handle), (int) SIG_DFL);

    *(int *)0 = 0x1337;
}
