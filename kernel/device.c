#include <levos/kernel.h>
#include <levos/device.h>

struct device *block_devices[32];
struct device *net_devices[32];
static struct device *other_devices[32];

void
dev_init(void)
{
    memset(block_devices, 0, sizeof(block_devices));
    memset(other_devices, 0, sizeof(other_devices));
    printk("dev: subsystem initizalized\n");
}

void
dev_seek(struct device *dev, int pos)
{
    dev->pos = pos;
}

void
device_register(struct device *dev)
{
    if (dev->type == DEV_TYPE_BLOCK) {
        for (int i = 0; i < 32; i ++) {
            if (block_devices[i] == NULL) {
                block_devices[i] = dev;
                return;
            }
        }
    } else {
        for (int i = 0; i < 32; i ++) {
            if (other_devices[i] == NULL) {
                other_devices[i] = dev;
                return;
            }
        }
    }
}
