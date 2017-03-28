#include <levos/uname.h>
#include <levos/string.h>

struct uname levos_uname = {
    .sysname = "LevOS",
    .nodename = "localhost",
    .release = "7",
    .version = ".0",
    .machine = "x86_32",
};

void
do_uname(struct uname *uname)
{
    memcpy(uname, &levos_uname, sizeof(*uname));
}
