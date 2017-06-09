#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

int
main(int argc, char **argvp)
{
    char buf[6];

    memset(buf, 0, 6);

    int fd = open("/etc/release", O_RDONLY, 0);
    if (fd < 0) {
        perror("fd");
        return 1;
    }

    void *addr = mmap(NULL, 0x1000, PROT_READ, MAP_SHARED, fd, 0);
    printf("addr = 0x%x\n", addr);
    if (addr == MAP_FAILED)
        return 1;

    printf("about to read the data\n");
    memcpy(buf, addr, 5);
    printf("read: \"%s\"\n", buf);

    return 0;
}

