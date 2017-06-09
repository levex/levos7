#include <levos/uname.h>
#include <levos/string.h>

struct uname levos_uname = {
    .sysname = "Helwyr",
    .nodename = "localhost",
    .release = "seren",
    .version = "-debug",
    .machine = "x86_32",
};

void
do_uname(struct uname *uname)
{
    memcpy(uname, &levos_uname, sizeof(*uname));
}
