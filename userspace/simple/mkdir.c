#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
    int rc;

    if (argc == 1) {
        printf("usage: %s <path>\n", argv[0]);
        exit(1);
    }

    rc = mkdir(argv[1], 0777);
    if (rc == -1) {
        perror(argv[0]);
        return 1;
    }

    return 0;
}
