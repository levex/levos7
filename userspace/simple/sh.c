#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/utsname.h>

#define STRCMP(c, s, n) strlen(c) == n && strncmp(c, s, n)

char *the_prompt = "lOS $ ";

void
process_cmd(char *b)
{
    pid_t pid;
    int rc;
    char *arg;
    if (strncmp(b, "fork ", 5) == 0) {
        arg = b + 5;
        if (pid = fork()) {
            int status;

            /* parent */
            //printf("parent\n");
            rc = waitpid(pid, &status, 0);
            if (WIFEXITED(status))
                printf("Exit code: %d\n", WEXITSTATUS(status));
            else
                printf("Weird!\n");
            return;
        } else {
            //memcpy(the_prompt, "oops $ ", 7);

            /* child */
            printf("child\n");
            char *argvp[] = {
                "/lol",
                arg,
                NULL,
            };

            execve("/lol", argvp, environ);
            exit(13);
        }
    }
    if (STRCMP(b, "uname", 5) == 0) {
        struct uname name;

        uname(&name);

        printf("%s version %s%s, on %s machine named %s\n",
                name.sysname, name.release, name.version,
                name.machine, name.nodename);
        return;
    }

    if (*b != 0)
        printf("unknown command\n");
}

int
main(int argc, char **argvp)
{
    char cmdbuf[128];
    int i = 0;

    /* TODO: tty set raw mode */

    memset(cmdbuf, 0, 128);
    printf("There are %d args\n", argc);

    for (i = 0; i < argc; i ++)
        printf("arg %d: %s\n", i, argvp[i]);

    while (1) {
        volatile char c = 0;
prompt:
        printf(the_prompt);
        //write(1, "lOS $ ", 6);
read_more:
        read(0, (char *) &c, 1);
        if (c == '\r') {
            printf("\n");
            cmdbuf[i] = 0;
            process_cmd(cmdbuf);
            i = 0;
            goto prompt;
        }
        cmdbuf[i++] = c;
        printf("%c", c);
        goto read_more;
    }
}
