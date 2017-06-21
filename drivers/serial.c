#include <levos/kernel.h>
#include <levos/console.h>
#include <levos/fs.h>
#include <levos/x86.h>
#include <levos/tty.h>

#define SERIAL_PORT 0x3F8

#define MODULE_NAME serial

static struct tty_device *tty_notify;

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

int
serial_file_close(struct file *f)
{
    free(f->full_path);
    free(f);
}

int
serial_file_truncate(struct file *f, int len)
{
    return 0;
}

void
serial_irq(struct pt_regs *regs)
{
    //mprintk("IRQ\n");
    if (tty_notify)
        tty_notify->tty_ldisc->write_input(tty_notify, serial_read_data());
}

struct console serial_console = {
    .putc = serial_out_data,
    .readc = serial_read_data,
};

struct file_operations serial_fops = {
    .read = serial_file_read,
    .write = serial_file_write,
    .fstat = serial_file_fstat,
    .close = serial_file_close,
    .truncate = serial_file_truncate,
};

struct file serial_base_file = {
    .fops = &serial_fops,
    .fs = NULL,
    .fpos = 0,
    .type = DEV_TYPE_CHAR,
    .isdir = 0,
    .respath = "ttyS0",
    .full_path = "/dev/ttyS0",
};

void
serial_signup(struct tty_device *tty)
{
    tty_notify = tty;
}

int serial_init() {
    tty_notify = NULL;
    //outportb(SERIAL_PORT + 1, 0x00);    // Disable all interrupts
    //outportb(SERIAL_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    //outportb(SERIAL_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    //outportb(SERIAL_PORT + 1, 0x00);    //                  (hi byte)
    //outportb(SERIAL_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    //outportb(SERIAL_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    //outportb(SERIAL_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outportb(SERIAL_PORT + 1, 0x01);
    intr_register_hw(0x20 + 0x04, serial_irq);
    mprintk("setup done\n");
    return 0;
}


