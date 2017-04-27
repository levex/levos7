#include <levos/kernel.h>
#include <levos/pipe.h>
#include <levos/fs.h>
#include <levos/task.h>

size_t
do_pipe_read(struct pipe *pip, void *buf, size_t len)
{
    while (ring_buffer_size(&pip->pipe_buffer) == 0 || pip->pipe_flags & PIPFLAG_WRITE_CLOSED)
        ;

    if (pip->pipe_flags & PIPFLAG_WRITE_CLOSED)
        return 0;

    return ring_buffer_read(&pip->pipe_buffer, buf, len);
}

size_t
do_pipe_write(struct pipe *pip, void *buf, size_t len)
{
    if (pip->pipe_flags & PIPFLAG_READ_CLOSED)
        send_signal(current_task, SIGPIPE);
    
    return ring_buffer_write(&pip->pipe_buffer, buf, len);
}

size_t
pipe_read(struct file *filp, void *buf, size_t len)
{
    struct pipe *pip = filp->priv;

    if (filp == pip->pipe_read)
        return do_pipe_read(pip, buf, len);

    return -EINVAL;
}

size_t
pipe_write(struct file *filp, void *buf, size_t len)
{
    struct pipe *pip = filp->priv;

    if (filp == pip->pipe_write)
        return do_pipe_write(pip, buf, len);

    return -EINVAL;
}

int
pipe_fstat(struct file *filp, struct stat *st)
{
    struct pipe *pip = filp->priv;

    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR;

    return 0;
}

int 
pipe_close(struct file *filp)
{
    struct pipe *pip = filp->priv;

    if (filp == pip->pipe_read) {
        pip->pipe_flags |= PIPFLAG_READ_CLOSED;
        pip->pipe_read = NULL;
    } else {
        pip->pipe_flags |= PIPFLAG_WRITE_CLOSED;
        pip->pipe_write = NULL;
    }
    free(filp);
    return 0;
}

int
pipe_readdir(struct file *filp, struct linux_dirent *de)
{
    return -EINVAL;
}

int
pipe_ioctl(struct file *filp, unsigned long req, unsigned long arg)
{
    return -EINVAL;
}

struct file_operations pipe_fops = {
    .read = pipe_read,
    .write = pipe_write,
    .fstat = pipe_fstat,
    .close = pipe_close,
    .readdir = pipe_readdir,
    .ioctl = pipe_ioctl,
};

struct file *
pipe_create_file(struct pipe *pip)
{
    struct file *filp = malloc(sizeof(*filp));
    if (!filp)
        return NULL;

    filp->fops = &pipe_fops;
    filp->fs = NULL;
    filp->fpos = 0;
    filp->isdir = 0;
    filp->type = FILE_TYPE_PIPE;
    filp->refc = 1;
    filp->respath = NULL;
    filp->priv = pip;
    return filp;
}

struct pipe *
pipe_create(void)
{
    struct pipe *pip;

    pip = malloc(sizeof(*pip));
    if (!pip)
        return NULL;

    /* XXX: this may need checking later on */
    ring_buffer_init(&pip->pipe_buffer, PIPE_BUF);
    ring_buffer_set_flags(&pip->pipe_buffer, RB_FLAG_NONBLOCK);

    pip->pipe_read = pipe_create_file(pip);
    if (pip->pipe_read == NULL) {
        free(pip);
        return NULL;
    }

    pip->pipe_write = pipe_create_file(pip);
    if (pip->pipe_write == NULL) {
        free(pip);
        return NULL;
    }

    pip->pipe_flags = 0;
    return pip;
}

int
do_pipe(struct task *task, int *fds)
{
    struct pipe *pip = pipe_create();
    if (!pip)
        return -ENOMEM;

    task->file_table[fds[0]] = pip->pipe_read;
    task->file_table[fds[1]] = pip->pipe_write;

    return 0;
}
