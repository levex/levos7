#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/utsname.h>

#define STRCMP(c, s, n) strlen(c) == n && strncmp(c, s, n)

char *the_prompt = "lOS $ ";

void
process_cmd(char *b)
{
    pid_t pid;
    int rc;
    if (STRCMP(b, "fork", 4) == 0) {
        if (pid = fork()) {
            int status;

            /* parent */
            //printf("parent\n");
            rc = waitpid(pid, &status, 0);
            //printf("PARENT PID %d STATUS 0x%x rc %d errno %d\n", pid, status, rc, errno);
            return;
        } else {
            //memcpy(the_prompt, "oops $ ", 7);

            /* child */
            //printf("child\n");
            execve("/lol", 0, 0);
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
main(void)
{
    char cmdbuf[128];
    int i = 0;

    /* TODO: tty set raw mode */

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
