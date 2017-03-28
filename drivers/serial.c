#include <levos/kernel.h>
#include <levos/console.h>
#include <levos/fs.h>
#include <levos/x86.h>

#define SERIAL_PORT 0x3F8

static void serial_out(int o, char d)
{
    outportb(SERIAL_PORT + o, d);
}

static int serial_is_ready_to_send()
{
    return inportb(SERIAL_PORT + 5) & 0x20;
}

static int serial_has_recv()
{
    return inportb(SERIAL_PORT + 5) & (1 << 0);
}

void serial_out_data(char c)
{
    while (!serial_is_ready_to_send())
        ;

    serial_out(0, c);
}

char serial_read_data()
{
    while (!serial_has_recv())
        ;

    return inportb(SERIAL_PORT + 0);
}

char serial_try_read_data()
{
    if (!serial_has_recv())
        return 0;

    return inportb(SERIAL_PORT + 0);
}

int serial_file_read(struct file *f, void *_buf, size_t count)
{
    char *buf = _buf;
    int a = count;

    while (count--)
        *buf++ = serial_read_data();

    return a;
}

int serial_file_write(struct file *f, void *_buf, size_t count)
{
    char *buf = _buf;
    int a = count;
    while (count--)
        serial_out_data(*buf ++);

    return a;
}

int serial_file_fstat(struct file *f, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

struct console serial_console = {
    .putc = serial_out_data,
    .readc = serial_read_data,
};

struct file_operations serial_fops = {
    .read = serial_file_read,
    .write = serial_file_write,
    .fstat = serial_file_fstat,
};

struct file serial_base_file = {
    .fops = &serial_fops,
    .fs = NULL,
    .fpos = 0,
    .isdir = 0,
    .respath = NULL,
};

int serial_init() {
    return 0;
}


