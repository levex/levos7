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
    bool success = false;

    /* bad buffer for open must fail */
    CHECK_ERR(open(NULL, 0, 0), EFAULT);

    /* can't open a not existing file */
    CHECK_ERR(open("/dont-make-me-exist", 0, 0), ENOENT);

    /* should be able to open an existing file */
    rc = open("/init", 0, 0);
    if (rc <= 1)
        return 1;

    /* can't open too many files */
    for (int i = 0; i < 1024; i ++) {
        rc = open("/init", 0, 0);
        if (rc == -1 && errno == EMFILE) {
            success = true;
            break;
        } else if (rc == -1) {
            printf("errno %d\n", errno);
            success = 0;
            break;
        }
    }
    if (!success) {
        printf("wtf\n");
        return 1;
    }

    return 0;
}
