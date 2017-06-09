#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int
main(int argc, char *argvp)
{
    int fd, rc;
    char buffer[16];

    memset(buffer, 0, 16);

    fd = open("/proc/memtotal", 0, 0);
    if (fd < 0) {
        printf("failed to open /proc/memtotal: %s\n", strerror(errno));
        exit(1);
    } else {
        memset(buffer, 0, 16);
        rc = read(fd, buffer, 16);
        printf("Total RAM: %s bytes\n", buffer);
    }

    fd = open("/proc/memused", 0, 0);
    if (fd < 0) {
        printf("failed to open /proc/memused: %s\n", strerror(errno));
        exit(1);
    } else {
        memset(buffer, 0, 16);
        rc = read(fd, buffer, 16);
        printf("Used RAM: %s bytes\n", buffer);
    }

    fd = open("/proc/memfree", 0, 0);
    if (fd < 0) {
        printf("failed to open /proc/memfree: %s\n", strerror(errno));
        exit(1);
    } else {
        memset(buffer, 0, 16);
        rc = read(fd, buffer, 16);
        printf("Free RAM: %s bytes\n", buffer);
    }
    return 0;
}
