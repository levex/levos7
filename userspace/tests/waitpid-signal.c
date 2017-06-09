#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <stdio.h>

#include "test.h"

int
run_test()
{
    int pid;

    if (pid = fork()) {
        int status = 0, rc = 0;

        CHECK_VAL(waitpid(pid, &status, 0), "%d", pid);

        CHECK(WIFSIGNALED(status), 1);
        CHECK_VAL(WTERMSIG(status), "%d", SIGSEGV);

        return 0;
    } else {
        /* dereference null to get a SIGSEGV */
        *(int *)0 = 0x1337;
    }
}
