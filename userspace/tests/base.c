#include <stdlib.h>
#include <stdio.h>

#include "test.h"

int
main(void)
{
    if (run_test())
        printf("FAILURE\n");
    else printf("SUCCESS\n");

    exit(0);
}
