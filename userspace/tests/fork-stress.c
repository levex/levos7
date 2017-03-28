#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "test.h"

int
run_test()
{
    int n = 100;
    while (n --)
        if (fork())
            continue;
        else {
            printf("%d\n", getpid());
            while (1)
                ;
        }

    return 0;
}
