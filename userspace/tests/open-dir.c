#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>

#include "test.h"

int
run_test()
{
    int fd, rc;

    /* try to open a directory */
    fd = open("/", 0, 0);
    if (fd <= 1) {
        printf("Failed to open the root directory\n");
        return 1;
    }

    /* try to write to it now */
    CHECK_ERR(write(fd, "HELLO_WORLD", 11), EISDIR);

    return 0;
}
