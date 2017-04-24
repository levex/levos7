#include <levos/kernel.h>
#include <levos/tty.h>
#include <levos/device.h>
#include <levos/ring.h>

#if 0

/* write to the pty's buffer from buf (sz. len), passing through the
 * line discipline
 */
static int
do_pty_write(struct tty_device *tty, struct pty *pty, void *buf, size_t len)
{
    struct tty_line_discipline *ldisc = tty->tty_ldisc;

    while (len --)
        ldisc->write_byte(tty, *(uint8_t *)buf++);

    /* XXX: check for errors */
    return len;
}

static int
do_pty_read(struct tty_device *tty, struct pty *pty, void *buf, size_t len)
{
    /* read from the out buffer of the pty */
    return ring_buffer_read(&pty->pty_out_rb, buf, len);
}

int
pty_slave_read(struct device *dev, void *buf, size_t len)
{
    struct pty *pty = dev->priv;
    struct tty_device *tty = pty->pty_tty;

    return do_pty_read(tty, pty, buf, len);
}

int
pty_slave_write(struct device *dev, void *buf, size_t len)
{
    struct pty *pty = dev->priv;
    struct tty_device *tty = pty->pty_tty;

    return do_pty_write(tty, pty->pty_other, buf, len);
}

int
pty_master_read(struct device *dev, void *buf, size_t len)
{
    struct pty *pty = dev->priv;
    struct tty_device *tty = pty->pty_tty;

    return do_pty_read(tty, pty, buf, len);
}

int
pty_master_write(struct device *dev, void *buf, size_t len)
{
    struct pty *pty = dev->priv;
    struct tty_device *tty = pty->pty_tty;

    return do_pty_write(tty, pty->pty_other, buf, len);
}

int
pty_side_init(struct tty_device *tty, struct pty *pty, int side)
{
    pty->pty_side = side;
    pty->pty_tty = tty;
    ring_buffer_init(&pty->pty_out_rb, PTY_BUF_SIZE);
    ring_buffer_set_flags(&pty->pty_out_rb, RB_FLAG_NONBLOCK);
}

struct device pty_slave_dev_tmpl = {
   .read = pty_slave_read,
   .write = pty_slave_write,
   .type = DEV_TYPE_CHAR,
   .pos = 0,
   .fs = NULL,
   .priv = NULL,
};

struct device pty_master_dev_tmpl = {
   .read = pty_master_read,
   .write = pty_master_write,
   .type = DEV_TYPE_CHAR,
   .pos = 0,
   .fs = NULL,
   .priv = NULL,
};

#endif
