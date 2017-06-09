#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <stdio.h>

#include "test.h"

static int got_sigchld = 0;

void
sigchld_handler(int sig)
{
    printf("got SIGCHLD\n");
    got_sigchld = 1;
}

int
run_test()
{
    int pid, rc;

    CHECK((int) signal(SIGCHLD, sigchld_handler), (int) SIG_DFL);

    if (pid = fork()) {
        /* force the other thread to run */
        sleep(1);

        if (!got_sigchld)
            test_failure();

        test_success();
    } else {
        exit(0);
    }
}
