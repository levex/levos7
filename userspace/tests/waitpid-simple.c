#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>

#include "test.h"

int
run_test()
{
    int pid;

    if (pid = fork()) {
        int status = 0, rc = 0;

        CHECK_VAL(waitpid(pid, &status, 0), "%d", pid);

        CHECK(WIFEXITED(status), 1);
        CHECK(WEXITSTATUS(status), 14);

        return 0;
    } else
        exit(14);
}
