#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>

#include "test.h"

int
run_test()
{
    int rc;

    /* check for invalid fds */
    CHECK_ERR(write(999, NULL, 0), EBADF);
    CHECK_ERR(write(-1, NULL, 0), EBADF);

    /* simple write must succeed */
    CHECK(write(1, "1234\n", 5), 5);

    /* writing a zero length buffer must return zero */
    CHECK(write(1, "1234", 0), 0);

    /* write to existing fd with NULL buffer should fail */
    CHECK_ERR(write(1, NULL, 1337), EFAULT);

    /* write to nonexistant fd must fail */
    CHECK_ERR(write(5, "hello", 4), EBADF);

    /* using a kernel buffer should fail */
    CHECK_ERR(write(1, (char *) 0xC0001337, 51), EFAULT);

    return 0;
}
