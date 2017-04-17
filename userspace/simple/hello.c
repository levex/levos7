#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

/* TODO: need to provide htons */

#define IP(a, b, c, d) (a << 24 | b << 16 | c << 8 | d)

char *hello = "Hello from userspace LevOS 7\n";

struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port   = 31773, /* 7548 in BE */
    .sin_addr   = IP(10, 33, 33, 145),
};

int
main(void)
{
    int fd, rc;

    printf("Hello, world!\n");

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    printf("Got socket ret: %d (%d)\n", fd, errno);

    rc = connect(fd, (void *) &addr, sizeof(addr));
    printf("Got connect ret: %d (%d)\n", rc, errno);

    write(fd, hello, strlen(hello));

    return 0x13;
}
