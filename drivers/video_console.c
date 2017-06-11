#include <levos/kernel.h>
#include <levos/intr.h>
#include <levos/device.h>
#include <levos/tty.h>
#include <levos/vga/font.h>
#include <levos/spinlock.h>
//#include <levos/bga.h> /* FIXME: convert to fb_device */

#define MODULE_NAME videocon

static uint32_t *__lfb = 0;
static int __vx = 0, __vy = 0, __vt_fg_color = 0x00ffffff;
static int __vt_bg_color = 0x00000000;
volatile int __in_ansi = 0;
static int __save_x = 0, __save_y = 0;

static int __cursor_shown = 0;
static int show_cursor = 1;

#define ANSI_BOLD (1 << 0)
static int ansi_flags = 0;

static spinlock_t scroll_lock;
static spinlock_t cursor_lock;

static inline void __putpix(int x, int y, uint32_t c)
{
    //printk("x: %d, y: %d\n", x, y);
    __lfb[y * 1024 + x] = c;
}

static void
do_scroll()
{
    //spin_lock(&scroll_lock);
    memcpy(__lfb, __lfb + (1024 * 1 + 0) * 8, 1024 * 8 * 4 * 95);
    //spin_unlock(&scroll_lock);
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
    if (!show_cursor)
        return;
    spin_lock(&cursor_lock);
    do_cursor(0x00333333);
    __cursor_shown = 1;
    spin_unlock(&cursor_lock);
}

static void
clear_cursor()
{
    if (!show_cursor)
        return;

    spin_lock(&cursor_lock);
    do_cursor(__vt_bg_color);
    __cursor_shown = 0;
    spin_unlock(&cursor_lock);
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

    spin_lock(&scroll_lock);
    for(cy = 0; cy < 8; cy ++){
		for(cx = 0; cx < 8; cx ++){
			__putpix(__vx + (8 - cx), __vy + cy, glyph[cy] & mask[cx] ? __vt_fg_color : __vt_bg_color);
		}
	}
    spin_unlock(&scroll_lock);

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
__do_ansi_sgr(char *buf, size_t len)
{
    int ret;

    if (len == -1)
        ret = atoi_10(buf);
    else
        ret = atoi_10n(buf, len);

    printk("%s (len: %d): %d\n", __func__, len, ret);


    if (ret == 1) {
        ansi_flags |= ANSI_BOLD;
        return;
    }
    if (ret == 22) {
        ansi_flags &= ~ANSI_BOLD;
        return;
    }

#define RGB(r, g, b) (0x00000000 | b | g << 8 | r << 16)
#define SGR_SET_FG(rt, col, col_b) \
    if (ret == (rt) && (ansi_flags & ANSI_BOLD)) { __vt_fg_color = (col_b); return; } else \
            if (ret == (rt)) { __vt_fg_color = (col); return; }
    SGR_SET_FG(0,  0x00ffffff, 0x00ffffff);
    SGR_SET_FG(30, RGB(0, 0, 0), RGB(85, 85, 85));
    SGR_SET_FG(31, RGB(170, 0, 0), RGB(255, 85, 85));
    SGR_SET_FG(32, RGB(0, 170, 0), RGB(85, 255, 85));
    SGR_SET_FG(33, RGB(170, 85, 0), RGB(255, 255, 85));
    SGR_SET_FG(34, RGB(0, 0, 170), RGB(85, 85, 255));
    SGR_SET_FG(35, RGB(170, 0, 170), RGB(255, 85, 255));
    SGR_SET_FG(36, RGB(0, 170, 170), RGB(85, 255, 255));
    SGR_SET_FG(37, RGB(170, 170, 170), RGB(255, 255, 255));

#define SGR_SET_BG(rt, col, col_b) \
    if (ret == (rt) && (ansi_flags & ANSI_BOLD)) { __vt_bg_color = (col_b); return; } else \
            if (ret == (rt)) { __vt_bg_color = (col); return; }
    SGR_SET_BG(40, RGB(0, 0, 0), RGB(85, 85, 85));
    SGR_SET_BG(41, RGB(170, 0, 0), RGB(255, 85, 85));
    SGR_SET_BG(42, RGB(0, 170, 0), RGB(85, 255, 85));
    SGR_SET_BG(43, RGB(170, 85, 0), RGB(255, 255, 85));
    SGR_SET_BG(44, RGB(0, 0, 170), RGB(85, 85, 255));
    SGR_SET_BG(45, RGB(170, 0, 170), RGB(255, 85, 255));
    SGR_SET_BG(46, RGB(0, 170, 170), RGB(85, 255, 255));
    SGR_SET_BG(47, RGB(170, 170, 170), RGB(255, 255, 255));
}

void
do_ansi_sgr(char *buf, size_t len)
{
    char *pch, *lasts;

    /* start splitting the line */
    if (strchr(buf, ';') == NULL) {
        __do_ansi_sgr(buf, len);
        return;
    }

    pch = strtok_r(buf, ";", &lasts);
    while (pch != NULL) {
        __do_ansi_sgr(pch, -1);
        pch = strtok_r(NULL, ";m", &lasts);
    }
}

void
do_ansi_ed(char *buf, size_t len)
{
    int ret;

    if (len == 0) {
        /* clear from cursor to end of screen */
        memsetl(__lfb + (__vy * 1024 + __vx), __vt_bg_color, 4 * (1024 - __vy) * (768 - __vx));
        return;
    }

    ret = atoi_10n(buf, len);

    if (ret == 0) {
        /* clear from cursor to end of screen */
        memsetl(__lfb + (__vy * 1024 + __vx), __vt_bg_color, 4 * (1024 - __vy) * (768 - __vx));
        return;
    } else if (ret == 1) {
        /* clear from cursor to beginning of screen FIXME */
        memsetl(__lfb, __vt_bg_color, 4 * (__vy * 1024 + __vx));
        return;
    } else if (ret == 2) {
        /* erase the whole display */
        memsetl(__lfb, __vt_bg_color, 4 * 1024 * 768);
        __vx = __vy = 0;
        return;
    }
}

void
do_ansi_cup(char *buf, size_t len)
{
    int ret;
    int target_x = -1, target_y = -1;
    char *pch, *lasts;

    printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* handle the case where there is no ';' */
    if (strnchr(buf, len, ';') == NULL) {
        target_x = 1;
        /* this means that the number means the y */
        target_y = atoi_10n(buf, len);
        goto end;
    }

    if (buf[0] == ';') {
        /* no x position given, default to pos 1 */
        target_x = 1;
        /* the rest is the Y coordinate */
        target_y = atoi_10n(buf + 1, len - 1);
        goto end;
    } else {
        /* the sequence contains a ';', and it has both numbers */
        pch = strtok_r(buf, ";", &lasts);
        target_x = atoi_10(pch);
        pch = strtok_r(NULL, ";H", &lasts);
        if (pch == NULL) {
            /* the ';' was not found */
            target_y = 1;
        } else
            target_y = atoi_10(pch);
    }

end:
    printk("%s: target (%d, %d)\n", __func__, target_x, target_y);
    __vx = (target_x - 1) * 8;
    __vy = (target_y - 1) * 8;
}

void
do_ansi_cuu(char *buf, size_t len)
{
    int ret;
    printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (__vy >= ret * 8)
        __vy -= ret * 8;
    else
        __vy = 0;
}

void
do_ansi_cud(char *buf, size_t len)
{
    int ret;
    printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (__vy <= 768 - ret * 8)
        __vy += ret * 8;
    else
        __vy = 768 - 8;
}

void
do_ansi_cuf(char *buf, size_t len)
{
    int ret;
    printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (__vx <= 1024 - ret * 8)
        __vx += ret * 8;
    else
        __vx = 1024 - 8;
}

void
do_ansi_cub(char *buf, size_t len)
{
    int ret;
    printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (__vx >= ret * 8)
        __vx -= ret * 8;
    else
        __vx = 0;
}

void
do_ansi_cnl(char *buf, size_t len)
{
    int current_line = __vy / 8;
    int ret;
    printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (current_line + ret <= 95) {
        __vy = (current_line + ret) * 8;
    } else
        __vy = 768 - 8;
}

void
do_ansi_cpl(char *buf, size_t len)
{
    int current_line = __vy / 8;
    int ret;
    printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (current_line - ret >= 0) {
        __vy = (current_line - ret) * 8;
    } else
        __vy = 0;
}

void
do_ansi_cha(char *buf, size_t len)
{
    int current_line = __vy / 8;
    int ret;
    printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (ret >= 0 && ret <= 128) {
        __vx = ret * 8;
    } else
        __vy = 0;
}

void
do_ansi_scp(char *buf, size_t len)
{
    __save_x = __vx;
    __save_y = __vy;
}

void
do_ansi_rcp(char *buf, size_t len)
{
    __vx = __save_x;
    __vy = __save_y;
}

void
do_ansi_showcur(char *buf, size_t len)
{
    show_cursor = 1;
}

void
do_ansi_hidecur(char *buf, size_t len)
{
    show_cursor = 0;
}

void
do_ansi(char *buf, size_t len)
{
#define ANSI_DO(c, name) if (buf[len] == c) { do_ansi_##name (buf, len); return; }
    ANSI_DO('A', cuu); /* move cursor up N cells */
    ANSI_DO('B', cud); /* move cursor down N cells */
    ANSI_DO('C', cuf); /* move cursor forward N cells */
    ANSI_DO('D', cub); /* move cursor back N cells */
    ANSI_DO('E', cnl); /* move cursor N lines down (x = 0) */
    ANSI_DO('F', cpl); /* move cursor N lines up (x = 0) */
    ANSI_DO('f', cup); /* CUP, but ANSI.SYS version */
    ANSI_DO('G', cha); /* move curson to column N */
    ANSI_DO('H', cup); /* set cursor position */
    ANSI_DO('h', showcur); /* show cursor */
    ANSI_DO('J', ed);  /* erase display */
    ANSI_DO('l', hidecur); /* hide cursor */
    ANSI_DO('m', sgr); /* set graphics representation */
    ANSI_DO('s', scp); /* save cursor position */
    ANSI_DO('r', rcp); /* restore cursor position */

    printk("----------> FOUND UNKNOWN ANSI ESCAPE %s\n", buf);
}

int
ansi_try_parse(char *buf, size_t len)
{
    int ansi_state = 0, ansi_size = 0;
    int i, escape_at = -1, begin_at = -1;
    char ansi_buffer[16];

    //printk("looking for ANSI... len: %d", len);

    if (len < 2)
        return 0;

    /* look for the ESC (0x1B) */
    if (ansi_state == 0) {
        i = 0;
        //for (i = 0; i < len; i ++) {
            if (buf[i] == 0x1b) {
                ansi_state = 1;
                escape_at = i;
                printk("ANSI: found ESC at %d\n", escape_at);
                goto as1;
            }
        //}
        /* ESC was not found, no ANSI escape sequence in this segment */
        return 0;
    }

as1:
    if (ansi_state == 1 && escape_at != -1) {
        if (escape_at + 1 > len) {
            /* there are not enough bytes for an ANSI sequence, bail */
            return 0;
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
        return 0;
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
                return ansi_size;
            }

            /* else this byte is part of the ANSI sequence */
            ansi_buffer[ansi_size] = this;

            printk("ANSI: added %c (%x) to size %d\n", this, this, ansi_size);

            /* proceed to the next byte */
            ansi_size ++;
        }

        printk("Abrupt end!\n");
        /* the stream ended before the ANSI sequence ended, thus don't parse */
        return 0;
    }
}

size_t
videocon_write(struct device *dev, void *_buf, size_t len)
{
    char *buf = _buf;
    int ansis = 0;

    clear_cursor();

    for (int i = 0; i < len; i ++) {
        /* find ANSI escape sequences */
        ansis = ansi_try_parse(_buf + i, len - i);
        if (ansis) {
            i += ansis + 2;
            continue;
        }
        videocon_emit(buf[i]);
    }

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

static void
blink_cursor(void *aux)
{
    if (__cursor_shown)
        clear_cursor();
    else
        draw_cursor();

    work_reschedule(50);
}

int video_console_init()
{
    bga_set_video(1024, 768, 32, /* LFB */ 1, /* CLEAR */ 1);
    extern struct device *default_user_device;
    default_user_device = &videocon_device;
    __lfb = bga_get_lfb();
    struct work *blinker = work_create(blink_cursor, NULL);
    //schedule_work_delay(blinker, 50);
    spin_lock_init(&scroll_lock);
    spin_lock_init(&cursor_lock);
    mprintk("initialized.\n");
}

