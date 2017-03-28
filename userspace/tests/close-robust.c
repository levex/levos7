#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>

#include "test.h"

int
run_test()
{
    int rc;

    /* closing fd 0 must succeed */
    CHECK(close(0), 0);

    /* opening a file now will use fd 0 */
    CHECK(open("/init", 0, 0), 0);

    /* opening a non existing file will not consume an fd */
    CHECK_ERR(open("/non-existing", 0, 0),  ENOENT);
    CHECK(open("/init", 0, 0), 2);

    /* closing an invalid FD should fail */
    CHECK_ERR(close(0xffffff), EBADF);

    /* closing a not yet open fd should fail */
    CHECK_ERR(close(3), EBADF);

    /* closing valid fds should succeed */
    CHECK(close(0), 0);
    CHECK(close(2), 0);

    return 0;
}
