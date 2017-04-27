#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>

/* TODO: need to provide htons */

#define IP(a, b, c, d) (a << 24 | b << 16 | c << 8 | d)

char *hello = "Hello from userspace LevOS 7\n";

struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port   = 31773, /* 7548 in BE */
    .sin_addr   = IP(10, 33, 33, 145),
};

static char dir_buffer[512];

volatile int static_ptr = 0;
static int target;

void
segv_handler(int signum)
{
    static_ptr = &target;
    printf("Hello from signal handler, set to 0x%x\n", static_ptr);
    return;
}

int
main(int argc, char **argvp)
{
    int fd, rc;

    if (strcmp(argvp[argc - 1], "args") == 0) {
        printf("Hello, world! I have %d args, and the last is \"%s\"\n",
                argc, argvp[argc - 1]);
        return 0;
    }

    if (strcmp(argvp[argc - 1], "pipe") == 0) {
        int fds[2];
        int rfd, wfd;
        char buf[17];

        memset(buf, 0, 17);

        rc = pipe(fds);
        rfd = fds[0];
        wfd = fds[1];
        printf("created pipe: rc %d rfd %d wfd %d errno %d\n",
                rc, rfd, wfd, errno);
 
        if (fork()) {
            rc = write(wfd, "Pipe test worked", 16);
            printf("after write in %d: rc %d errno %d\n", getpid(), rc, errno);
        } else {
            rc = read(rfd, buf, 16);
            printf("after read : rc %d errno %d\n", rc, errno);
            printf("read in %d: \"%s\"\n", getpid(), buf);
            exit(1);
        }

        return 0;
    }

    if (strcmp(argvp[argc - 1], "sig") == 0) {
        signal(SIGSEGV, segv_handler);
        static_ptr = 0;

        kill(getpid(), SIGSEGV);

        printf("returned from signal\n");

        kill(getpid(), SIGKILL);
        return 0;
    }

    if (strcmp(argvp[argc - 1], "group") == 0) {
        printf("Sending SIGKILL to my group, bye\n");
        rc = kill(0, SIGKILL);
        printf("rc is %d, errno is %d\n", rc, errno);
        return 0;
    }

    if (strcmp(argvp[argc - 1], "ls") == 0) {
        fd = open("/etc", 0, 0);
        printf("fd is %d, errno is %d\n", fd, errno);
        if (fd > 0) {
            do {
                rc = readdir(fd, (struct dirent *) dir_buffer, 0);
                if (rc != -1) {
                    struct dirent *dir = (struct dirent *) dir_buffer;
                    printf("entry: %s\n", dir->d_name);
                }
            } while (rc != -1);
            if (errno == ENOENT) {
                printf("--- end ---\n");
            }
        }
        return 0;
    }

    if (strcmp(argvp[argc - 1], "create") == 0) {
        printf("ABOUT TO CREATE A FILE YAY\n");
        fd = open("/etc/release", O_CREAT | O_EXCL | O_TRUNC, 0);
        printf("fd is %d, errno is %d\n", fd, errno);
        if (fd > 0) {
            rc = write(fd, "LevOS 7.0\n", 10);
            printf("rc is %d, errno is %d\n", rc, errno);
        }
        return 0;
    }

    if (strcmp(argvp[argc - 1], "socket") == 0) {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        printf("Got socket ret: %d (%d)\n", fd, errno);

        rc = connect(fd, (void *) &addr, sizeof(addr));
        printf("Got connect ret: %d (%d)\n", rc, errno);

        write(fd, hello, strlen(hello));
        return 0;
    }

    return 0x13;
}
