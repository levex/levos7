#include <levos/kernel.h>
#include <levos/tty.h>
#include <levos/device.h>
#include <levos/ring.h>

/* a master writing should write the input */
size_t
pty_master_write(struct pty *pty, char *buf, size_t count)
{
    struct tty_device *tty = pty->pty_tty;
    int i;

    for (i = 0; i < count; i ++)
        tty->tty_ldisc->write_input(tty, buf[i]);

    return count;
}

size_t
pty_master_read(struct pty *pty, char *buf, size_t count)
{
    /* read from the output buffer of the tty */
    return -EIO;
}

/* A slave reading should read the processed input */
size_t
pty_slave_read(struct pty *pty, char *buf, size_t count)
{
    struct tty_device *tty = pty->pty_tty;

    return tty->tty_ldisc->read_buf(tty, buf, count);
}

/* A slave writing should write to the output buffer */
size_t
pty_slave_write(struct pty *pty, char *buf, size_t count)
{
    struct tty_device *tty = pty->pty_tty;
    int i;

    for (i = 0; i < count; i ++)
        tty->tty_ldisc->write_output(tty, buf[i]);
    
    return count;
}
