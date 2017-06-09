#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

int
main (int argc, char **argv)
{
    int words = 0;
    int lines = 0;
    char buffer[1024];

    while(fgets(buffer, sizeof buffer, stdin))
    {
        lines++;
        if(buffer[0] == '\n')
            continue;
        char *tmp = buffer-1;
        while(tmp = strchr(tmp+1, ' '))
            words++;
        words++; /* count last word before \0*/
    }

    printf("lines: %d, words: %d\n", lines, words);
    return 0;
}
