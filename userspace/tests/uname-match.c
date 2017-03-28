#include <sys/utsname.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "test.h"

struct uname match = {
    .sysname = "LevOS",
    .nodename = "localhost",
    .release = "7",
    .version = ".0",
    .machine = "x86_32",
};

int
run_test()
{
    struct uname un;
    int rc;

    CHECK(uname(&un), 0);

    CHECK(strcmp(un.sysname, match.sysname), 0);
    CHECK(strcmp(un.nodename, match.nodename), 0);
    CHECK(strcmp(un.release, match.release), 0);
    CHECK(strcmp(un.version, match.version), 0);
    CHECK(strcmp(un.machine, match.machine), 0);

    return 0;
}
