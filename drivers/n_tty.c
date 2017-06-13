#include <levos/kernel.h>
#include <levos/tty.h>
#include <levos/device.h>

/* N_TTY line discipline */

static int
n_tty_flush(struct tty_device *tty)
{
    struct n_tty_priv *priv = tty->tty_ldisc->priv;
    int ret;
    
    ret = ring_buffer_write(&priv->line_buffer, priv->line_editing, priv->line_len);
    priv->line_len = 0;
    return ret;
}

static int
n_tty_write_output(struct tty_device *tty, uint8_t byte)
{
    struct n_tty_priv *priv = tty->tty_ldisc->priv;
    struct termios *tm = &tty->tty_termios;

    /* if output post processing is not enabled, then pass through */
    if (!(tm->c_oflag & OPOST)) {
        //tty->tty_device->write(tty->tty_device, &byte, 1);
        ring_buffer_write(&tty->tty_out, &byte, 1);
        //tty->tty_device->tty_interrupt_output(tty->tty_device, tty, 1);
        return 1;
    }

    if (tm->c_oflag & OLCUC && byte >= 'a' && byte <= 'z')
        byte = byte + ('A' - 'a');

    if (tm->c_oflag & ONLCR
            && byte == '\n') {
        /* XXX: if this is \n\r, then stuff breaks, why? */
        //tty->tty_device->write(tty->tty_device, "\r", 1);
        ring_buffer_write(&tty->tty_out, "\r", 1);
        //tty->tty_device->tty_interrupt_output(tty->tty_device, tty, 1);
        return 1;
    }

    //printk("wrte!\n");
    //tty->tty_device->write(tty->tty_device, &byte, 1);
    ring_buffer_write(&tty->tty_out, &byte, 1);
    //tty->tty_device->tty_interrupt_output(tty->tty_device, tty, 1);
    return 1;
}

static int
n_tty_output(struct tty_device *tty, char *buf, size_t len)
{
    int i;
    int nr = 0;

    for (i = 0; i < len; i ++)
        nr += n_tty_write_output(tty, buf[i]);

    tty->tty_device->tty_interrupt_output(tty->tty_device, tty, len);

    return nr;
}

static void
n_tty_push(struct tty_device *tty, uint8_t byte)
{
    struct n_tty_priv *priv = tty->tty_ldisc->priv;
    struct termios *tm = &tty->tty_termios;

    priv->line_editing[priv->line_len ++] = byte;

    /* XXX: if ICANON is not set, line editing is disabled, so just flush
     * immediately; this can and should be optimized
     */
    if (!(tm->c_lflag & ICANON))
        n_tty_flush(tty);
}

static int
n_tty_write_input(struct tty_device *tty, uint8_t byte)
{
    struct n_tty_priv *priv = tty->tty_ldisc->priv;
    struct termios *tm = &tty->tty_termios;

    if (tm->c_lflag & ISIG
            && byte == tm->c_cc[VINTR]) {
        if (tm->c_lflag & ECHOCTL) {
            n_tty_write_output(tty, '^');
            n_tty_write_output(tty, '@' + tm->c_cc[VINTR]);
            n_tty_write_output(tty, '\n');
        }
        //printk("SIGINT to pg%d\n", tty->tty_fg_proc);
        send_signal_group(tty->tty_fg_proc, SIGINT);
        return 1;
    }

    if (tm->c_lflag & ISIG
            && byte == tm->c_cc[VQUIT]) {
        if (tm->c_lflag & ECHOCTL) {
            n_tty_write_output(tty, '^');
            n_tty_write_output(tty, '@' + tm->c_cc[VQUIT]);
            n_tty_write_output(tty, '\n');
        }
        send_signal_group(tty->tty_fg_proc, SIGQUIT);
        return 1;
    }

    /* XXX: VEOF handling is not entirely correct */
    if (tm->c_lflag & ICANON
            && byte == tm->c_cc[VEOF]) {
        if (priv->line_len == 0) {
            tty->tty_state = TTY_STATE_CLOSED;
            n_tty_push(tty, '\n');
            n_tty_flush(tty);
            return 1;
        }
        byte = '\r';
        //n_tty_flush(tty);
        //return 1;
    }

    if (tm->c_lflag & ISIG
            && byte == tm->c_cc[VSUSP]) {
        if (tm->c_lflag & ECHOCTL) {
            n_tty_write_output(tty, '^');
            n_tty_write_output(tty, '@' + tm->c_cc[VSUSP]);
            n_tty_write_output(tty, '\n');
        }
        send_signal_group(tty->tty_fg_proc, SIGTSTP);
        return 1;
    }

    /* do quick transforms */
    if (tm->c_iflag & INLCR && byte == '\n') {
        byte = '\r';
    }

    if (tm->c_iflag & IGNCR && byte == '\r')
        return 1;

    if (!(tm->c_iflag & IGNCR) && tm->c_iflag & ICRNL
            && byte == '\r')
        byte = '\n';

    /* DEL */
    if (tm->c_lflag & ICANON && tm->c_lflag & ECHOE
            && byte == tm->c_cc[VERASE]) {
        if (priv->line_len == 0)
            return 1;
        priv->line_len --;
        //tty->tty_device->write(tty->tty_device, "\b \b", 3);
        n_tty_output(tty, "\b \b", 3);
        return 1;
    }

    /* VWERASE ^W */
    if (byte == tm->c_cc[VWERASE]) {
        if (priv->line_len == 0)
            return 1;

        while (priv->line_len > 0) {
            //tty->tty_device->write(tty->tty_device, "\b \b", 3);
            n_tty_output(tty, "\b \b", 3);
            priv->line_len --;
            if (priv->line_editing[priv->line_len] == ' ')
                break;
        }
        return 1;
    }

    /* VKILL ^U */
    if (tm->c_lflag & ICANON && tm->c_lflag & ECHOK
            && byte == tm->c_cc[VKILL]) {
        while (priv->line_len > 0) {
            priv->line_len --;
            //tty->tty_device->write(tty->tty_device, "\b \b", 3);
            n_tty_output(tty, "\b \b", 3);
        }
        return 1;
    }

    if (tm->c_iflag & IUCLC && byte >= 'A' && byte <= 'Z')
        byte = byte - ('A' - 'a');

    /* the default discipline is the NULL discipline: echo the byte back */
    if (byte != '\n') {
        if (tm->c_lflag & ICANON && tm->c_lflag & ECHO) {
            //tty->tty_device->write(tty->tty_device, &byte, 1);
            n_tty_output(tty, &byte, 1);
            //tty->tty_device->tty_blit(byte);
        }
        n_tty_push(tty, byte);
        //priv->line_editing[priv->line_len ++] = byte;
    } else {
        if ((tm->c_lflag & ICANON && tm->c_lflag & ECHO) ||
                (tm->c_lflag & ICANON && tm->c_lflag & ECHONL)) {
            //tty->tty_device->write(tty->tty_device, "\n", 1);
            n_tty_output(tty, "\n", 1);
        }
        //priv->line_editing[priv->line_len ++] = '\n';
        n_tty_push(tty, '\n');
        n_tty_flush(tty);
    }

    return 1;
}

static int
n_tty_read_buf(struct tty_device *tty, uint8_t *buf, size_t len)
{
    struct n_tty_priv *priv = tty->tty_ldisc->priv;
    uint8_t preproc;
    int rc;

    while (tty->tty_state != TTY_STATE_CLOSED && ring_buffer_size(&priv->line_buffer) == 0) {
        //rc = tty->tty_device->read(tty->tty_device, &preproc, 1);
        //if (rc)
            //return rc;

        //n_tty_write_input(tty, preproc);
    }

    return ring_buffer_read(&priv->line_buffer, buf, len);
}

static int
n_tty_init(struct tty_line_discipline *ldisc)
{
    struct n_tty_priv *priv = malloc(sizeof(*priv));
    if (!priv)
        return -ENOMEM;

    ring_buffer_init(&priv->line_buffer, PTY_BUF_SIZE);
    ring_buffer_set_flags(&priv->line_buffer, RB_FLAG_NONBLOCK);

    priv->line_len = 0;

    ldisc->priv = priv;
    return 0;
}

struct tty_line_discipline n_tty_ldisc = {
    .write_output = n_tty_write_output,
    .write_input = n_tty_write_input,
    .read_buf = n_tty_read_buf,
    .flush = n_tty_flush,
    .init = n_tty_init,
};
