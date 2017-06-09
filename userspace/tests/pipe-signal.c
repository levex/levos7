#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "test.h"

char *test_string = "Hello pipely world";

void
sigpipe_handler(int sig)
{
    printf("Got SIGPIPE\n");

    test_success();
}

int run_test()
{
    int rc, status, pipefd[2];
    pid_t cpid;
    char buf;

    CHECK(pipe(pipefd), 0);

    CHECK((int) signal(SIGPIPE, sigpipe_handler), (int) SIG_DFL);

    cpid = fork();
    if (cpid == -1) {
        perror("fork");
        test_failure();
    }

    if (cpid == 0) {    /* Child reads from pipe */
        close(pipefd[1]);          /* Close unused write end */
        close(pipefd[0]); // close read end

        sleep(3); // sleep 3 secs

        test_failure();
    } else {            /* Parent writes test_string to pipe */
        close(pipefd[0]);          /* Close unused read end */
        sleep(1);

        rc = write(pipefd[1], test_string, strlen(test_string));
        printf("rc %d errno %d\n", rc, errno);

        printf("should have got SIGPIPE\n");
        test_failure();
    }
}
