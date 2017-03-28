#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/errno.h>

#include "test.h"

int
run_test()
{
    int rc;

    /* pid must be 1, because we are the init process */
    CHECK(getpid(), 1);

    /* if we fork, then getpid must be different in the child */
    if (fork()) {
        int c = 0xffffffff;
        CHECK(getpid(), 1);
        while (c --)
            ;
    } else {
        if (getpid() == 1)
            return 1;
        else return 0;
    }

    return 0;
}
