#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

char buf[512];

int
main(int argc, char **argvp)
{
    struct termios tios, orig_tios;

    int fd = open("/etc/issue", O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    memset(buf, 0, sizeof(buf));
    read(fd, buf, 512);

    printf("\n");
    printf("%s", buf);
    printf("\n");
    printf("Press a key to login\n");

    if (tcgetattr(0, &orig_tios)){
        printf("Error getting current terminal settingsn");
        return 3;
    }

    /* Copy that to "tios" and play with it */
    tios = orig_tios;

    tios.c_lflag &= ~ICANON;
    tios.c_lflag &= ~ECHO;

    if (tcsetattr(0, TCSANOW, &tios)){
        printf("Error applying terminal settingsn");
        return 3;
    }

    char *shell = getenv("SHELL");
    if (shell == NULL) {
        shell = "/usr/bin/dash";
    }

    /* read a byte */
    read(0, &fd, 1);

    printf("\n");

    /* reset */
    tcsetattr(0, TCSANOW, &orig_tios);

    char *args[2] = {shell, NULL};
    execv(shell, args);

    printf("fatal error: failed to spawn shell\n");
    return 1;
}
