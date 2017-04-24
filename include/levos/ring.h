#ifndef __LEVOS_RING_H
#define __LEVOS_RING_H

#define RB_FLAG_NONBLOCK (1 << 0) /* if set, return -1 on operations that
                                     can't do anything */
#define RB_FLAG_SLEEP    (1 << 1) /* if this and NONBLOCK are set, then the
                                     caller is put to sleep instead of
                                     busywaiting */

struct ring_buffer {
    volatile int head; /* write pointer */
    volatile int tail; /* read pointer */
    int flags; /* settings of the ring buffer */
    volatile size_t capacity; /* capacity of the ring buffer */
    volatile size_t size; /* how many bytes do we have already? */
    uint8_t *buffer; /* the buffer */
};

int ring_buffer_size(struct ring_buffer *);
void ring_buffer_init(struct ring_buffer *, int);
void ring_buffer_set_flags(struct ring_buffer *, int);
int ring_buffer_write(struct ring_buffer *, uint8_t *, size_t);
int ring_buffer_read(struct ring_buffer *, void *, size_t);
void ring_buffer_destroy(struct ring_buffer *);

#endif /* __LEVOS_RING_H */
