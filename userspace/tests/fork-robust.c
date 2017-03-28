#include <stdlib.h>
#include <unistd.h>

#include "test.h"

int
run_test()
{
    if (fork()) {
        int c = 0xffffffff;
        while (c --)
            ;
        return 1;
    } else {
        return 0;
    }
}
