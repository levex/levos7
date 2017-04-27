#include <stdlib.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
    int i;
    if (argc <= 1) {
        printf("Echo version 0.1 for LevOS 7\n");
        printf("Usage: %s <args-to-echo-back>\n", argv[0]);
        return 1;
    }
    for (i = 1; i < argc; i ++)
        printf("%s%s", argv[i], i == argc - 1 ? "" : " ");

    printf("\n");
    return 0;
}
