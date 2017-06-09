#ifndef __LEVOS_MODULE_H
#define __LEVOS_MODULE_H

#define __MODULE_NAME __stringify(MODULE_NAME) ": "
#define mprintk(fmt, ...) printk(__MODULE_NAME fmt,##__VA_ARGS__);

#endif
