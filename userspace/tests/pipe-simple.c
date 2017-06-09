#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <stdio.h>
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
        printf("child: reading...\n");
        while (read(pipefd[0], &buf, 1) > 0) {
            total_len ++;
            write(1, &buf, 1);
        }
        write(1, "\n", 1);
        printf("child: done...\n");
        if (total_len != strlen(test_string))
            test_failure();
        close(pipefd[0]);
        test_success();
    } else {            /* Parent writes test_string to pipe */
        close(pipefd[0]);          /* Close unused read end */
        printf("parent: writing...\n");
        CHECK(write(pipefd[1], test_string, strlen(test_string)), strlen(test_string));
        printf("parent: done...\n");
        close(pipefd[1]);          /* Reader will see EOF */
        waitpid(cpid, &status, 0);                /* Wait for child */
        test_success();
    }
}
