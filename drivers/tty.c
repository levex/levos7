#include <levos/kernel.h>
#include <levos/tty.h>
#include <levos/device.h>
#include <levos/fs.h>

extern struct tty_line_discipline *n_tty_ldisc;

static int __last_tty_id;

#define MODULE_NAME tty

static
int tty_get_id()
{
    return __last_tty_id ++;
}

void
termios_init(struct termios *tm)
{
    memset(tm, 0, sizeof(*tm));
    tm->c_iflag = BRKINT | ICRNL | IXANY | IXON;
    tm->c_oflag = OPOST | ONLCR;
    tm->c_cflag = CS8 | CREAD | HUPCL;
    tm->c_lflag = ECHO | ECHOE | ECHOK | ECHOCTL | ICANON | IEXTEN | ISIG;
    tm->c_cc[VEOF] = __CONTROL('D');
    tm->c_cc[VEOL] = __CONTROL('?');
    tm->c_cc[VERASE] = 0x7F;
    tm->c_cc[VINTR] = __CONTROL('X'); /* XXX: should be ^C */
    tm->c_cc[VKILL] = __CONTROL('U');
    tm->c_cc[VMIN] = 1;
    tm->c_cc[VQUIT] = __CONTROL('\\');
    tm->c_cc[VSTART] = __CONTROL('Q');
    tm->c_cc[VSTOP] = __CONTROL('S');
    tm->c_cc[VSUSP] = __CONTROL('K'); /* XXX: should be ^Z */
    tm->c_cc[VTIME] = 0;
    tm->c_cc[VWERASE] = __CONTROL('W');
}

struct tty_device *
tty_new(struct device *dev)
{
    int rc;
	struct tty_device *tty = malloc(sizeof(*tty));

    if (!tty)
        return ERR_PTR(-ENOMEM);

    mprintk("creating new TTY device with base %s\n", dev->name);

    tty->tty_device = dev;
    tty->tty_device->tty_signup_input(dev, tty);
    tty->tty_state = TTY_STATE_UNKNOWN;
    tty->tty_ldisc = &n_tty_ldisc;
    tty->tty_id = tty_get_id();
    ring_buffer_init(&tty->tty_out, PTY_BUF_SIZE);
    termios_init(&tty->tty_termios);
    tty->tty_winsize.ws_row = 80;
    tty->tty_winsize.ws_col = 25;
    tty->tty_winsize.ws_xpixel = 0;
    tty->tty_winsize.ws_ypixel = 0;
    rc = tty->tty_ldisc->init(tty->tty_ldisc);
    if (rc) {
        free(tty);
        return rc;
    }

    return tty;
}

size_t tty_fread(struct file *f, void *buf, size_t len)
{
    struct tty_device *tty = f->priv;

    if (tty->tty_state == TTY_STATE_CLOSED) {
        tty->tty_state = TTY_STATE_UNKNOWN;
        return 0;
    }

    return tty->tty_ldisc->read_buf(tty, buf, len);
}

size_t tty_fwrite(struct file *f, void *_buf, size_t len)
{
    uint8_t *buf = _buf;
    struct tty_device *tty = f->priv;
    int n = 0;

    for (int i = 0; i < len; i ++)
        n += tty->tty_ldisc->write_output(tty, buf[i]);

    tty->tty_device->tty_interrupt_output(tty->tty_device, tty, n);

    return n;
}

int tty_ffstat(struct file *f, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR;
    return 0;
}

int tty_fclose(struct file *f)
{
    //free(f);
}

int tty_freaddir(struct file *f, struct linux_dirent *de)
{
    return 0;
}

int tty_ioctl(struct tty_device *tty, unsigned long req, unsigned long arg)
{
    //mprintk("ioctl 0x%x with arg 0x%x\n", req, arg);
    /* TODO: verify the buffers before memcpying into */

    if (req == TCGETS) {
        //mprintk("TCGETS\n");
        struct termios *tm = (void *) arg;
        memcpy(tm, &tty->tty_termios, sizeof(*tm));
        return 0;
    } else if (req == TCSETS) {
        //mprintk("TCSETS\n");
        struct termios *tm = (void *) arg;
        memcpy(&tty->tty_termios, tm, sizeof(*tm));
        return 0;
    } else if (req == TIOCGWINSZ) {
        struct winsize *ws = (void *) arg;
        memcpy(ws, &tty->tty_winsize, sizeof(*ws));
        return 0;
    } else if (req == TIOCSWINSZ) {
        struct winsize *ws = (void *) arg;
        int should_sig = 0;
        if (ws->ws_row != tty->tty_winsize.ws_row ||
                ws->ws_col != tty->tty_winsize.ws_col ||
                ws->ws_xpixel != tty->tty_winsize.ws_xpixel ||
                ws->ws_ypixel != tty->tty_winsize.ws_ypixel)
            should_sig = 1;
        memcpy(&tty->tty_winsize, ws, sizeof(*ws));
        if (should_sig)
            send_signal(current_task, SIGWINCH);
        return 0;
    } else if (req == TIOCGPGRP) {
        pid_t *tg = arg;
        *tg = tty->tty_fg_proc;
        return 0;
    } else if (req == TIOCSPGRP) {
        pid_t *tg = arg;
        //printk("tty: fg pgid %d\n", *tg);
        tty->tty_fg_proc = *tg;
        return 0;
    } else if (req == TCFLSH) {
        /* FIXME */
        return 0;
    }

    return -EINVAL;
}

int tty_fioctl(struct file *f, unsigned long req, unsigned long arg)
{
    struct tty_device *tty = f->priv;

    return tty_ioctl(tty, req, arg);
}

struct file_operations tty_fops = {
    .read = tty_fread,
    .write = tty_fwrite,
    .fstat = tty_ffstat,
    .close = tty_fclose,
    .readdir = tty_freaddir,
    .ioctl = tty_fioctl,
};

struct file *
tty_get_file(struct tty *tty)
{
    struct file *filp = malloc(sizeof(*filp));
    if (!filp)
        return NULL;

    filp->fops = &tty_fops,
    filp->fs = NULL;
    filp->fpos = 0;
    filp->isdir = 0;
    filp->type = FILE_TYPE_TTY;
    filp->refc = 1;
    filp->respath = "console";
    filp->full_path = strdup("/dev/console");
    filp->priv = tty;

    return filp;
};

struct tty_device *
tty_get(int id)
{
	return NULL;
}

void
tty_flush_output(struct tty_device *tty)
{
    ring_buffer_flush(&tty->tty_out);
}

int
tty_init(void)
{
	printk("tty: initializing layer\n");
    return 0;
}

int ctty_file_write(struct file *f, char *_buf, size_t count)
{
    struct tty_device *tty = current_task->ctty;

    if (tty) {
        for (int i = 0; i < count; i ++)
            tty->tty_ldisc->write_output(tty, _buf[i]);

        tty->tty_device->tty_interrupt_output(tty->tty_device, tty, count);
        return count;
    }

    return -ENOTTY;
}

int ctty_file_read(struct file *f, void *_buf, size_t count)
{
    struct tty_device *tty = current_task->ctty;

    if (tty)
        return tty->tty_ldisc->read_buf(tty, _buf, count);

    return -ENOTTY;
}

int ctty_file_fstat(struct file *f, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int ctty_file_ioctl(struct file *f, unsigned long req, unsigned long arg)
{
    struct tty_device *tty = current_task->ctty;

    if (tty)
        return tty_ioctl(tty, req, arg);

    return -ENOTTY;
}

int
ctty_file_truncate(struct file *f, int pos)
{
    return 0;
}

int
ctty_file_close(struct file *f)
{
    free(f->full_path);
    free(f);
}

struct file_operations ctty_fops = {
    .read = ctty_file_read,
    .write = ctty_file_write,
    .fstat = ctty_file_fstat,
    .close = ctty_file_close,
    .ioctl = ctty_file_ioctl,
    .truncate = ctty_file_truncate,
};

struct file ctty_base_file = {
    .fops = &ctty_fops,
    .fs = NULL,
    .fpos = 0,
    .isdir = 0,
    .respath = "tty",
    .full_path = "/dev/tty",
};
