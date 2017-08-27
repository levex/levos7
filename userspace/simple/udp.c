#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

char *hello = "Hello there!\n";

#define IP(a, b, c, d) (a << 24 | b << 16 | c << 8 | d)

struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port   = 7548, /* 7548 in BE */
    .sin_addr   = IP(192, 168, 1, 79),
};

int
main(int argc, char **argvp)
{
    char *input = malloc(4096);
    char *reply= malloc(4096);
    int len;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("Got socket ret: %d (%d)\n", fd, errno);

    int rc = connect(fd, (void *) &addr, sizeof(addr));
    printf("Got connect ret: %d (%d)\n", rc, errno);

round:
    memset(input, 0, 4096);
    len = read(0, input, 4096);

    write(fd, input, strlen(input));

    memset(reply, 0, 4096);
    read(fd, reply, 4096);

    printf("> %s", reply);

    goto round;

    return 0;
}
