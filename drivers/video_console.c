#include <levos/kernel.h>
#include <levos/intr.h>
#include <levos/device.h>
#include <levos/tty.h>
#include <levos/vga/font.h>
//#include <levos/bga.h> /* FIXME: convert to fb_device */

#define MODULE_NAME videocon

static uint32_t *__lfb = 0;
static int __vx = 0, __vy = 0, __vt_fg_color = 0x00ffffff;
static int __vt_bg_color = 0x00000000;
volatile int __in_ansi = 0;

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

static void
do_cursor(uint32_t col)
{
    int cx, cy;

    for (cy = 0; cy < 8; cy ++) {
        for (cx = 0; cx < 8; cx ++) {
            __putpix(__vx + (8 - cx), __vy + cy, col);
        }
    }
}

static void
draw_cursor()
{
    do_cursor(0x00333333);
}

static void
clear_cursor()
{
    do_cursor(__vt_bg_color);
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
			__putpix(__vx + (8 - cx), __vy + cy, glyph[cy] & mask[cx] ? __vt_fg_color : __vt_bg_color);
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

void
do_ansi_sgr(char *buf, size_t len)
{
    int ret = atoi_10n(buf, len);

    printk("%s (len: %d): %d\n", __func__, len, ret);

#define RGB(r, g, b) (0x00000000 | b | g << 8 | r << 16)
#define SGR_SET_FG(rt, col) if (ret == (rt)) { __vt_fg_color = (col); return; }
    SGR_SET_FG(0,  0x00ffffff);
    SGR_SET_FG(30, RGB(0, 0, 0));
    SGR_SET_FG(31, RGB(170, 0, 0));
    SGR_SET_FG(32, RGB(0, 170, 0));
    SGR_SET_FG(33, RGB(170, 85, 0));
    SGR_SET_FG(34, RGB(0, 0, 170));
    SGR_SET_FG(35, RGB(170, 0, 170));
    SGR_SET_FG(36, RGB(0, 170, 170));
    SGR_SET_FG(37, RGB(170, 170, 170));

#define SGR_SET_BG(rt, col) if (ret == (rt)) { __vt_bg_color = (col); return; }
    SGR_SET_BG(40, RGB(0, 0, 0));
    SGR_SET_BG(41, RGB(170, 0, 0));
    SGR_SET_BG(42, RGB(0, 170, 0));
    SGR_SET_BG(43, RGB(170, 85, 0));
    SGR_SET_BG(44, RGB(0, 0, 170));
    SGR_SET_BG(45, RGB(170, 0, 170));
    SGR_SET_BG(46, RGB(0, 170, 170));
    SGR_SET_BG(47, RGB(170, 170, 170));
}

void
do_ansi(char *buf, size_t len)
{
    if (buf[len] == 'm') {
        do_ansi_sgr(buf, len);
        return;
    }
}

void
ansi_try_parse(char *buf, size_t len)
{
    int ansi_state = 0, ansi_size = 0;
    int i, escape_at = -1, begin_at = -1;
    char ansi_buffer[16];

    //printk("looking for ANSI... len: %d", len);

    if (len < 2)
        return;

    /* look for the ESC (0x1B) */
    if (ansi_state == 0) {
        for (i = 0; i < len; i ++) {
            if (buf[i] == 0x1b) {
                ansi_state = 1;
                escape_at = i;
                printk("ANSI: found ESC at %d\n", escape_at);
                goto as1;
            }
        }
        /* ESC was not found, no ANSI escape sequence in this segment */
        return;
    }

as1:
    if (ansi_state == 1 && escape_at != -1) {
        if (escape_at + 1 > len) {
            /* there are not enough bytes for an ANSI sequence, bail */
            return;
        }
        /* check if the next char is the left bracket (0x5b) */
        if (buf[escape_at + 1] == 0x5B) {
            ansi_state = 2;
            begin_at = escape_at + 1;
            printk("ANSI: found LB at %d\n", begin_at);
            goto as2;
        }

        printk("ANSI: LB was %c (%x) at %d\n", buf[escape_at + 1], buf[escape_at + 1], begin_at);
        /* nope, bail */
        return;
    }
as2:
    if (ansi_state == 2 && begin_at != -1) {
        /* we are in an ANSI sequence, start fetching the bytes */
        while (begin_at + 1 + ansi_size < len) {
            printk("parsing ANSI remainder...\n");
            char this = buf[begin_at + 1 + ansi_size];
            if (0x40 <= this && this <= 0x7e) {
                /* found the end of the ANSI sequence */
                ansi_buffer[ansi_size] = this;
                ansi_buffer[ansi_size + 1] = 0;
                printk("Found ANSI sequence: \"%s\"\n", ansi_buffer);
                do_ansi(ansi_buffer, ansi_size);
                return;
            }

            /* else this byte is part of the ANSI sequence */
            ansi_buffer[ansi_size] = this;

            printk("ANSI: added %c (%x) to size %d\n", this, this, ansi_size);

            /* proceed to the next byte */
            ansi_size ++;
        }

        printk("Abrupt end!\n");
        /* the stream ended before the ANSI sequence ended, thus don't parse */
        return;
    }
}

size_t
videocon_write(struct device *dev, void *_buf, size_t len)
{
    char *buf = _buf;

    clear_cursor();

    /* find ANSI escape sequences */
    ansi_try_parse(_buf, len);

    for (int i = 0; i < len; i ++)
        videocon_emit(buf[i]);

    draw_cursor();

    return len;
}

size_t videocon_tty_interrupt_output(struct device *dev, struct tty_device *tty, int len)
{
    /* a TTY interrupted the console, which means there is data in
     * the output buffer for us to read
     */

    char *kbuf = malloc(len);
    if (!kbuf)
        return -ENOMEM;

    ring_buffer_read(&tty->tty_out, kbuf, len);

    videocon_write(dev, kbuf, len);

    free(kbuf);
}

struct device videocon_device = {
    .type = DEV_TYPE_CHAR,
    .read = videocon_read,
    .write = videocon_write,
    .tty_interrupt_output = videocon_tty_interrupt_output,
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

