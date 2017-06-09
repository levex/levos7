#include <levos/kernel.h>
#include <levos/console.h>
#include <levos/arch.h>
#include <levos/device.h>

static struct console *console;

void
console_init(void)
{
    console = arch_get_console();
}

void
console_emit(char c)
{
    console->putc(c);
    if (c == '\r')
        console->putc('\n');
}

void console_puts(char *s)
{
    while (*s)
        console_emit(*s++);
}

char
console_getchar(void)
{
    return console->readc();
}

size_t
console_read(struct device *dev, void *_buf, size_t len)
{
    char *buf = _buf;

    for (int i = 0; i < len; i ++)
        buf[i] = console_getchar();
}

size_t
console_write(struct device *dev, void *_buf, size_t len)
{
    char *buf = _buf;

    for (int i = 0; i < len; i ++)
        console_emit(buf[i]);
}

struct device console_device = {
    .type = DEV_TYPE_CHAR,
    .read = console_read,
    .write = console_write,
    .pos = 0,
    .fs = NULL,
    .name = "ttyconsole",
    .priv = NULL,
};

struct device *default_user_device = &console_device;
