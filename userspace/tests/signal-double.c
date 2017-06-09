#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <stdio.h>

#include "test.h"

static int num = 0;

void
sigusr1_handle(int sig)
{
    printf("hi from signal\n");
    num ++;
}

int
run_test()
{
    int rc;

    CHECK((int) signal(SIGUSR1, sigusr1_handle), (int) SIG_DFL);

    if (fork()) {
        while (num != 2)
            sleep(1);

        test_success();
    } else {
        pid_t parent = getppid();
        CHECK(kill(parent, SIGUSR1), 0);
        CHECK(kill(parent, SIGUSR1), 0);

        while(1);
    }

}
