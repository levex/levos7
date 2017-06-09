#include <levos/kernel.h>
#include <levos/intr.h>
#include <levos/device.h>
#include <levos/vga/font.h>
//#include <levos/bga.h> /* FIXME: convert to fb_device */

#define MODULE_NAME videocon

static uint32_t *__lfb = 0;
static int __vx = 0, __vy = 0;

static inline void __putpix(int x, int y, uint32_t c)
{
    //printk("x: %d, y: %d\n", x, y);
    __lfb[y * 1024 + x] = c;
}

static void
do_scroll()
{
    memcpy(__lfb, __lfb + (1024 * 1 + 0) * 8, 1024 * 8 * 4 * 95);
    //memset(__lfb + (1024 * 95 * 4 * 8), 0, 1024 * 8 * 4);
}

static void
do_post_print(char c)
{
    /* check if we need to scroll */
    if (__vy >= 768 - 8) {
        do_scroll();
        __vy -= 8;
        return;
    }

    /* otherwise just simply go to the next character */
    if (c == '\n' || c == '\r')
        goto end;

    if (__vx >= 1024 - 8) {
        __vx = 0;
        __vy += 8;
        /* check if we need to scroll */
        if (__vy >= 768 - 8) {
            do_scroll();
            return;
        }
    } else
        __vx += 8;

end: return;
}

void
videocon_emit(char c)
{
    int cx, cy;
    unsigned char *glyph = g_8x8_font + (int)(c * 8);
    int mask[8] = {1, 2, 4, 8, 16, 32, 64, 128};

    /* FIXME: investigate */
    if (!c)
        return;
    
    if (c == '\t') {
        for (int i = 0; i < 4; i ++)
            videocon_emit(' ');
        return;
    }

    if (c == '\n' || c == '\r') {
        __vy += 8;
        __vx = 0;
        goto end;
    } 

    if (c == '\b') {
        if (__vx == 0)
            return;
        __vx -= 8;
        return;
    }

    for(cy = 0; cy < 8; cy ++){
		for(cx = 0; cx < 8; cx ++){
			__putpix(__vx + (8 - cx), __vy + cy, glyph[cy] & mask[cx] ? 0x00ffffff : 0x00);
		}
	}

end:
    do_post_print(c);
}

void videocon_puts(char *s)
{
    while (*s)
        videocon_emit(*s++);
}

char
videocon_getchar(void)
{
    char c = 0;

    kbd_file_read(NULL, &c, 1);

    printk("read: %c\n", c);

    return c;
}

size_t
videocon_read(struct device *dev, void *_buf, size_t len)
{
    return 0;
    char *buf = _buf;

    for (int i = 0; i < len; i ++)
        buf[i] = videocon_getchar();

    return len;
}

size_t
videocon_write(struct device *dev, void *_buf, size_t len)
{
    char *buf = _buf;

    for (int i = 0; i < len; i ++)
        videocon_emit(buf[i]);

    return len;
}

struct device videocon_device = {
    .type = DEV_TYPE_CHAR,
    .read = videocon_read,
    .write = videocon_write,
    .pos = 0,
    .fs = NULL,
    .name = "videoconsole",
    .priv = NULL,
};

int video_console_init()
{
    bga_set_video(1024, 768, 32, /* LFB */ 1, /* CLEAR */ 1);
    extern struct device *default_user_device;
    default_user_device = &videocon_device;
    __lfb = bga_get_lfb();
    mprintk("initialized.\n");
}

