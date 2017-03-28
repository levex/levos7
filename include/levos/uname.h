#ifndef __LEVOS_UNAME_H
#define __LEVOS_UNAME_H

#include <levos/compiler.h>

struct uname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
} __packed;

void do_uname(struct uname *);

#endif /* __LEVOS_UNAME_H */
