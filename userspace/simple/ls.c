#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

static char dir_buffer[512];

int
main(int argc, char *argvp[])
{
    char *path = ".";
    int fd, rc;

    if (argc == 2)
        path = argvp[1];
    else if (argc > 2) {
        printf("ls - list files and directories\n");
        printf("Usage: %s [dir]\n", argvp[0]);
        exit(1);
    }

    fd = open(path, 0, 0);
    //printf("fd is %d, errno is %d\n", fd, errno);
    if (fd > 0) {
        do {
            rc = readdir(fd, (struct dirent *) dir_buffer, 0);
            if (rc != -1) {
                struct dirent *dir = (struct dirent *) dir_buffer;
                printf("%s\n", dir->d_name);
            }
        } while (rc != -1);
        if (errno == ENOENT) {
            //printf("--- end ---\n");
        }
    }
}
