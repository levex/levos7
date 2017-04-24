#include <levos/kernel.h>
#include <levos/ring.h>

void
ring_buffer_init(struct ring_buffer *rb, int capacity)
{
    rb->capacity = capacity;
    rb->buffer = malloc(rb->capacity);
    if (!rb->buffer) {
        printk("CRITICAL: ring buffer initialization ran out of memory\n");
        return;
    }
    rb->size = 0;
    rb->flags = 0;
    rb->head = rb->tail = 0;
}

void
ring_buffer_set_flags(struct ring_buffer *rb, int flags)
{
    rb->flags = flags;
}

void
ring_buffer_destroy(struct ring_buffer *rb)
{
    free(rb->buffer);
}

int
__ring_buffer_write(struct ring_buffer *rb, uint8_t c)
{
    if (rb->size == rb->capacity)
        if (rb->flags & RB_FLAG_NONBLOCK)
            return 1;
        else
            while (rb->size == rb->capacity)
                ;

    rb->buffer[rb->head ++] = c;
    if (rb->head == rb->capacity)
        rb->head = 0;

    rb->size ++;
    return 0;
}

int
ring_buffer_write(struct ring_buffer *rb, uint8_t *buf, size_t len)
{
    int rc;
    int n_wrote = 0;

    /* meh, this is really really really not well optimized :-) */
    while (len --) {
        rc = __ring_buffer_write(rb, *buf++);
        if (rc)
            return n_wrote;
        else
            n_wrote ++;
    }

    return n_wrote;
}

int
__ring_buffer_read(struct ring_buffer *rb, uint8_t *buf)
{
    if (rb->size == 0)
        if (rb->flags & RB_FLAG_NONBLOCK)
            return 1;
        else
            while (rb->size == 0)
                ;

    *buf = rb->buffer[rb->tail ++];
    if (rb->tail == rb->capacity)
        rb->tail = 0;

    rb->size --;
    return 0;
}

int
ring_buffer_read(struct ring_buffer *rb, void *buf, size_t len)
{
    int rc;
    int n_read = 0;

    while (len --) {
        rc = __ring_buffer_read(rb, buf ++);
        if (rc) return n_read;
        else n_read ++;
    }

    return n_read;
}

int
ring_buffer_size(struct ring_buffer *rb)
{
    return rb->size;
}
