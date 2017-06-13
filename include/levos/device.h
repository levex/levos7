#ifndef __LEVOS_DEVICE_H
#define __LEVOS_DEVICE_H

#include <levos/types.h>

struct filesystem;
struct tty_device;

struct device
{
#define DEV_TYPE_UNKNOWN 0
#define DEV_TYPE_BLOCK   1
#define DEV_TYPE_CHAR    2
    int type;

#define DEV_TYPE_BLOCK_UNKNOWN 0
#define DEV_TYPE_BLOCK_ATA     1
    int subtype;

    size_t (*read)(struct device *, void *, size_t);
    size_t (*write)(struct device *, void *, size_t);

    int (*tty_interrupt_output)(struct device *, struct tty_device *, int);
    int (*tty_signup_input)(struct device *, struct tty_device *);

    unsigned long pos;

    struct filesystem *fs;

    char *name;
    void *priv;
};

extern struct device *block_devices[];
#define for_each_blockdev(dev) \
        for (int __i__ = 0; \
                (dev = block_devices[__i__]) != NULL && __i__ < 32;\
                __i__++)

void device_register(struct device *);
void dev_init(void);
void dev_seek(struct device *, int);

#endif /* __LEVOS_DEVICE_H */
