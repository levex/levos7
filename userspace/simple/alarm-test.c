#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void
alarm_here(int sig)
{
    printf("alarm!\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    signal(SIGALRM, alarm_here);

    alarm(1);

    while (1);
}
