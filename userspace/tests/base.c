#include <stdlib.h>
#include <stdio.h>

#include "test.h"

void
test_success(void)
{
    printf("SUCCESS\n");
    exit(0);
}

void
test_failure(void)
{
    printf("FAILURE\n");
    exit(1);
}

int
main(void)
{
    if (run_test())
        test_failure();
    else test_success();
}
