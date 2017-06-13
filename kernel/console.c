#include <levos/kernel.h>
#include <levos/console.h>
#include <levos/arch.h>
#include <levos/tty.h>
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

int
console_do_signup(struct device *dev, struct tty_device *tty)
{
    /* the serial console's default input is the serial port */
    serial_signup(tty);
}

size_t console_tty_interrupt_output(struct device *dev, struct tty_device *tty, int len)
{
    /* a TTY interrupted the console, which means there is data in
     * the output buffer for us to read
     */

    char *kbuf = malloc(len);
    if (!kbuf)
        return -ENOMEM;

    ring_buffer_read(&tty->tty_out, kbuf, len);

    console_write(dev, kbuf, len);

    free(kbuf);
}

struct device console_device = {
    .type = DEV_TYPE_CHAR,
    .read = console_read,
    .write = console_write,
    .tty_signup_input = console_do_signup,
    .tty_interrupt_output = console_tty_interrupt_output,
    .pos = 0,
    .fs = NULL,
    .name = "ttyconsole",
    .priv = NULL,
};

struct device *default_user_device;
