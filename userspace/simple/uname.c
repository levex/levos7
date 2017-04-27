#include <stdio.h>
#include <sys/utsname.h>

int
main(int argc, char **argvp)
{
    struct uname name;

    uname(&name);

    printf("%s version %s%s, on %s machine named %s\n",
            name.sysname, name.release, name.version,
            name.machine, name.nodename);

    return 0;
}
