#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>

#include "test.h"

int
run_test()
{
    int rc;

    /* using a simple kernel buffer should fail */
    CHECK_ERR(write(1, (char *) 0xC0001337, 51), EFAULT);

    /* using a kernel buffer that overlaps should fail */
    CHECK_ERR(write(1, (char *) 0xC0001337, 8192), EFAULT);

    /* using a user buffer that overlaps into the kernel should fail*/
    CHECK_ERR(write(1, (char *) (0xC0000000 - 1), 4), EFAULT);

    /* using a user buffer that is not mapped should fail */
    CHECK_ERR(write(1, (char *) 0x11223344, 4), EFAULT);

    /* using a user buffer that is not mapped and overlaps should fail */
    CHECK_ERR(write(1, (char *) 0x11223344, 4097), EFAULT);

    return 0;
}
