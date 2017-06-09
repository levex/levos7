#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "test.h"

char *test_string = "Hello pipely world";

int run_test()
{
    int rc, status, pipefd[2];
    pid_t cpid;
    char buf;

    CHECK(pipe(pipefd), 0);

    cpid = fork();
    if (cpid == -1) {
        perror("fork");
        test_failure();
    }

    if (cpid == 0) {    /* Child reads from pipe */
        int total_len = 0;

        close(pipefd[1]);          /* Close unused write end */
        close(pipefd[0]);

        test_success();
    } else {            /* Parent writes test_string to pipe */
        close(pipefd[0]);          /* Close unused read end */
        CHECK_ERR(lseek(pipefd[1], 0, SEEK_SET), ESPIPE);
        close(pipefd[1]);          /* Reader will see EOF */

        waitpid(cpid, &status, 0);                /* Wait for child */
        test_success();
    }
}
