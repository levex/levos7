#include <levos/kernel.h>
#include <levos/intr.h>
#include <levos/device.h>
#include <levos/tty.h>
#include <levos/vga/font.h>
#include <levos/spinlock.h>
//#include <levos/bga.h> /* FIXME: convert to fb_device */

#define MODULE_NAME videocon

#define VT_DISPLAY_WIDTH 1024
#define VT_DISPLAY_HEIGHT 768
#define VT_CHAR_SIZE 8
#define VT_TABSIZE 4

struct vconsole {
    int vc_px; /* pixel X */
    int vc_py; /* pixel Y */
    int vc_spx; /* saved pixel X */
    int vc_spy; /* saved pixel Y */
    uint32_t vc_fg_col; /* foreground color */
    uint32_t vc_bg_col; /* background color */
};

struct vconsole __vc_0 = {
    .vc_px = 0,
    .vc_py = 0,
    .vc_spx = 0,
    .vc_spy = 0,
    .vc_fg_col = 0x00ffffff,
    .vc_bg_col = 0x00000000,
};

#define VT_WIDTH_CHARS (VT_DISPLAY_WIDTH / VT_CHAR_SIZE)
#define VT_HEIGHT_CHARS (VT_DISPLAY_HEIGHT / VT_CHAR_SIZE)

static uint32_t *__lfb = 0;
volatile int __in_ansi = 0;

static int __cursor_shown = 0;
static int show_cursor = 1;

#define ANSI_BOLD             (1 << 0) /* bold colors */
#define ANSI_EXPECT_COLOR     (1 << 1) /* expecting an SGR color id */
#define ANSI_EXPECT_COLOR_FG  (1 << 2) /* expectation is for FG (ifset) or BG */
#define ANSI_EXPECT_255_COLOR (1 << 3) /* 255 color mode */
static int ansi_flags = 0;

char vt_array[(VT_DISPLAY_WIDTH / VT_CHAR_SIZE) * (VT_DISPLAY_HEIGHT / VT_CHAR_SIZE)];
#define PUT_CHAR_VT(vc, c) \
    vt_array[(vc->vc_px / VT_CHAR_SIZE) * VT_WIDTH_CHARS + \
            (vc->vc_py / VT_CHAR_SIZE)] = c;

static spinlock_t scroll_lock;
static spinlock_t cursor_lock;

static inline void __putpix(int x, int y, uint32_t c)
{
    //printk("x: %d, y: %d\n", x, y);
    __lfb[y * VT_DISPLAY_WIDTH + x] = c;
}

static void
do_scroll()
{
    //spin_lock(&scroll_lock);
    mg_memcpy(__lfb,
              __lfb + (VT_DISPLAY_WIDTH * 1 + 0) * VT_CHAR_SIZE,
              VT_DISPLAY_WIDTH * VT_CHAR_SIZE * 4 * (VT_HEIGHT_CHARS - 1));
    //spin_unlock(&scroll_lock);
}

static void
do_post_print(struct vconsole *vc, char c)
{
    /* check if we need to scroll */
    if (vc->vc_py >= VT_DISPLAY_HEIGHT - VT_CHAR_SIZE) {
        do_scroll();
        vc->vc_py -= VT_CHAR_SIZE;
        return;
    }

    /* otherwise just simply go to the next character */
    if (c == '\n' || c == '\r')
        goto end;

    if (vc->vc_px >= VT_DISPLAY_WIDTH - VT_CHAR_SIZE) {
        vc->vc_px = 0;
        vc->vc_py += VT_CHAR_SIZE;
        /* check if we need to scroll */
        if (vc->vc_py >= VT_DISPLAY_HEIGHT - VT_CHAR_SIZE) {
            do_scroll();
            return;
        }
    } else
        vc->vc_px += VT_CHAR_SIZE;

end: return;
}

static void
do_cursor(struct vconsole *vc, uint32_t col)
{
    int cx, cy;

    for (cy = 0; cy < VT_CHAR_SIZE; cy ++) {
        for (cx = 0; cx < VT_CHAR_SIZE; cx ++) {
            __putpix(vc->vc_px + (VT_CHAR_SIZE - cx), vc->vc_py + cy, col);
        }
    }
}

static void
draw_cursor(struct vconsole *vc)
{
    if (!show_cursor)
        return;
    spin_lock(&cursor_lock);
    do_cursor(vc, 0x00333333);
    __cursor_shown = 1;
    spin_unlock(&cursor_lock);
}

static void
clear_cursor(struct vconsole *vc)
{
    if (!show_cursor)
        return;

    spin_lock(&cursor_lock);
    do_cursor(vc, vc->vc_bg_col);
    __cursor_shown = 0;
    spin_unlock(&cursor_lock);
}

void
videocon_emit(struct vconsole *vc, char c)
{
    int cx, cy;
    unsigned char *glyph = g_8x8_font + (int)(c * 8);
    int mask[8] = {1, 2, 4, 8, 16, 32, 64, 128};

    /* FIXME: investigate */
    if (!c)
        return;

    //printk("%s: %c (0x%x)\n", __func__, c, c);

    if (c == '\t') {
        for (int i = 0; i < VT_TABSIZE; i ++)
            videocon_emit(vc, ' ');
        return;
    }

    if (c == '\n' || c == '\r') {
        vc->vc_py += VT_CHAR_SIZE;
        vc->vc_px = 0;
        goto end;
    } 

    if (c == '\b') {
        if (vc->vc_px == 0)
            return;
        PUT_CHAR_VT(vc, ' ');
        vc->vc_px -= VT_CHAR_SIZE;
        return;
    }

    spin_lock(&scroll_lock);
    PUT_CHAR_VT(vc, c);
    for(cy = 0; cy < VT_CHAR_SIZE; cy ++){
		for(cx = 0; cx < VT_CHAR_SIZE; cx ++){
			__putpix(vc->vc_px + (VT_CHAR_SIZE - cx),
                     vc->vc_py + cy,
                     glyph[cy] & mask[cx] ? vc->vc_fg_col : vc->vc_bg_col);
		}
	}
    spin_unlock(&scroll_lock);

end:
    do_post_print(vc, c);
}

void videocon_puts(struct vconsole *vc, char *s)
{
    while (*s)
        videocon_emit(vc, *s++);
}

char
videocon_getchar(void)
{
    char c = 0;

    kbd_file_read(NULL, &c, 1);

    //printk("read: %c\n", c);

    return c;
}

size_t
videocon_read(struct device *dev, void *_buf, size_t len)
{
    return 0;
}

void
ansi_set_color(uint32_t *ret, int val)
{
    //printk("%s: val %d\n", __func__, val);

#define DEFCOL(col, rr) if (val == col) { *ret = rr; return; }
    DEFCOL(255, 0x00eeeeee);
    DEFCOL(0, 0x00000000);
    DEFCOL(1, 0x00800000);
    DEFCOL(2, 0x00008000);
    DEFCOL(3, 0x00808000);
    DEFCOL(4, 0x00000080);
    DEFCOL(5, 0x00800080);
    DEFCOL(6, 0x00008080);
    DEFCOL(7, 0x00c0c0c0);
    DEFCOL(8, 0x00808080);

    printk("%s: unknown color %d\n", __func__, val);
}

void
__do_ansi_sgr(struct vconsole *vc, char *buf, size_t len)
{
    int ret;

    if (len == -1)
        ret = atoi_10(buf);
    else
        ret = atoi_10n(buf, len);

    //printk("%s %s%s%s (len: %d): (buf[0]: %c) %d\n",
            //__func__,
            //ansi_flags & ANSI_EXPECT_COLOR ? "col " : "",
            //ansi_flags & ANSI_EXPECT_255_COLOR ? "255 " : "",
            //ansi_flags & ANSI_EXPECT_COLOR_FG ? "fg " : "bg ",
            //len, buf[0], ret);

    if (!(ansi_flags & ANSI_EXPECT_COLOR) && !(ansi_flags & ANSI_EXPECT_255_COLOR)) {
        if (ret == 1) {
            ansi_flags |= ANSI_BOLD;
            return;
        }
        if (ret == 22) {
            ansi_flags &= ~ANSI_BOLD;
            return;
        }

        if (ret == 38) {
            ansi_flags |= ANSI_EXPECT_COLOR;
            ansi_flags |= ANSI_EXPECT_COLOR_FG;
            return;
        }

        if (ret == 48) {
            ansi_flags |= ANSI_EXPECT_COLOR;
            ansi_flags &= ~ANSI_EXPECT_COLOR_FG;
            return;
        }
    }

    if (ansi_flags & ANSI_EXPECT_COLOR) {
        if (ret == 5) {
            /* 255 color mode */
            ansi_flags |= ANSI_EXPECT_255_COLOR;
            ansi_flags &= ~ANSI_EXPECT_COLOR;
            return;
        } else if (ret == 2) {
            /* r;g;b mode TODO */
            printk("requested RGB mode, we do not yet support\n");
            ansi_flags &= ~ANSI_EXPECT_COLOR_FG;
            ansi_flags &= ~ANSI_EXPECT_COLOR;
            ansi_flags &= ~ANSI_EXPECT_255_COLOR;
            return;
        }

        printk("invalid 38 expectation\n");

        return;
    }

    if (ansi_flags & ANSI_EXPECT_255_COLOR) {
        /* this value is the 255 color value */
        if (ansi_flags & ANSI_EXPECT_COLOR_FG) {
            /* for foreground */
            ansi_set_color(&vc->vc_fg_col, ret);
        } else {
            /* for background */
            ansi_set_color(&vc->vc_bg_col, ret);
        }
        ansi_flags &= ~ANSI_EXPECT_COLOR_FG;
        ansi_flags &= ~ANSI_EXPECT_COLOR;
        ansi_flags &= ~ANSI_EXPECT_255_COLOR;
        return;
    }

#define RGB(r, g, b) (0x00000000 | b | g << 8 | r << 16)
#define SGR_SET_FG(rt, col, col_b) \
    if (ret == (rt) && (ansi_flags & ANSI_BOLD)) { vc->vc_fg_col = (col_b); return; } else \
            if (ret == (rt)) { vc->vc_fg_col = (col); return; }
#define SGR_SET_BG(rt, col, col_b) \
    if (ret == (rt) && (ansi_flags & ANSI_BOLD)) { vc->vc_bg_col = (col_b); return; } else \
            if (ret == (rt)) { vc->vc_bg_col = (col); return; }

    if (ret == 0) {
        vc->vc_fg_col = RGB(0xff, 0xff, 0xff);
        vc->vc_bg_col = RGB(0, 0, 0);
        return;
    }

    SGR_SET_FG(30, RGB(0, 0, 0), RGB(85, 85, 85));
    SGR_SET_FG(31, RGB(170, 0, 0), RGB(255, 85, 85));
    SGR_SET_FG(32, RGB(0, 170, 0), RGB(85, 255, 85));
    SGR_SET_FG(33, RGB(170, 85, 0), RGB(255, 255, 85));
    SGR_SET_FG(34, RGB(0, 0, 170), RGB(85, 85, 255));
    SGR_SET_FG(35, RGB(170, 0, 170), RGB(255, 85, 255));
    SGR_SET_FG(36, RGB(0, 170, 170), RGB(85, 255, 255));
    SGR_SET_FG(37, RGB(170, 170, 170), RGB(255, 255, 255));

    SGR_SET_BG(40, RGB(0, 0, 0), RGB(85, 85, 85));
    SGR_SET_BG(41, RGB(170, 0, 0), RGB(255, 85, 85));
    SGR_SET_BG(42, RGB(0, 170, 0), RGB(85, 255, 85));
    SGR_SET_BG(43, RGB(170, 85, 0), RGB(255, 255, 85));
    SGR_SET_BG(44, RGB(0, 0, 170), RGB(85, 85, 255));
    SGR_SET_BG(45, RGB(170, 0, 170), RGB(255, 85, 255));
    SGR_SET_BG(46, RGB(0, 170, 170), RGB(85, 255, 255));
    SGR_SET_BG(47, RGB(170, 170, 170), RGB(255, 255, 255));

    printk("%s: unhandled SGR %d\n", __func__, ret);
}

void
do_ansi_sgr(struct vconsole *vc, char *buf, size_t len)
{
    char *pch, *lasts;

    //printk("%s: len %d \"%s\"\n", __func__, len, buf);

    /* start splitting the line */
    if (strchr(buf, ';') == NULL) {
        __do_ansi_sgr(vc, buf, len);
        return;
    }

    pch = strtok_r(buf, ";", &lasts);
    while (pch != NULL) {
        __do_ansi_sgr(vc, pch, -1);
        pch = strtok_r(NULL, ";m", &lasts);
    }
}

void
do_ansi_ed(struct vconsole *vc, char *buf, size_t len)
{
    int ret;

    if (len == 0) {
        /* clear from cursor to end of screen */
        memsetl(__lfb + (vc->vc_py * VT_DISPLAY_WIDTH+ vc->vc_px),
                vc->vc_bg_col,
                4 * (VT_DISPLAY_WIDTH - vc->vc_py) * (VT_DISPLAY_HEIGHT - vc->vc_px));
        return;
    }

    ret = atoi_10n(buf, len);

    if (ret == 0) {
        /* clear from cursor to end of screen */
        memsetl(__lfb + (vc->vc_py * VT_DISPLAY_WIDTH + vc->vc_px),
                vc->vc_bg_col,
                4 * (VT_DISPLAY_WIDTH - vc->vc_py) * (VT_DISPLAY_HEIGHT - vc->vc_px));
        return;
    } else if (ret == 1) {
        /* clear from cursor to beginning of screen FIXME */
        memsetl(__lfb, vc->vc_bg_col, 4 * (vc->vc_py * VT_DISPLAY_WIDTH + vc->vc_px));
        return;
    } else if (ret == 2) {
        /* erase the whole display */
        memsetl(__lfb, vc->vc_bg_col, 4 * VT_DISPLAY_WIDTH * VT_DISPLAY_HEIGHT);
        vc->vc_px = vc->vc_py = 0;
        return;
    }
}

void
do_ansi_cup(struct vconsole *vc, char *buf, size_t len)
{
    int ret;
    int target_x = -1, target_y = -1;
    char *pch, *lasts;

    //printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* handle the case where there is no ';' */
    if (strnchr(buf, len, ';') == NULL) {
        target_x = 1;
        if (len == 0)
            target_y = 1;
        else {
            /* this means that the number means the y */
            target_y = atoi_10n(buf, len);
        }
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
    //printk("%s: target (%d, %d)\n", __func__, target_x, target_y);
    vc->vc_px = (target_x - 1) * VT_CHAR_SIZE;
    vc->vc_py = (target_y - 1) * VT_CHAR_SIZE;
}

void
do_ansi_cuu(struct vconsole *vc, char *buf, size_t len)
{
    int ret;
    //printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (vc->vc_py >= ret * VT_CHAR_SIZE)
        vc->vc_py -= ret * VT_CHAR_SIZE;
    else
        vc->vc_py = 0;
}

void
do_ansi_cud(struct vconsole *vc, char *buf, size_t len)
{
    int ret;
    //printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (vc->vc_py <= VT_DISPLAY_HEIGHT - ret * VT_CHAR_SIZE)
        vc->vc_py += ret * VT_CHAR_SIZE;
    else
        vc->vc_py = VT_DISPLAY_HEIGHT - VT_CHAR_SIZE;
}

void
do_ansi_cuf(struct vconsole *vc, char *buf, size_t len)
{
    int ret;
    //printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (vc->vc_px <= VT_DISPLAY_WIDTH - ret * VT_CHAR_SIZE)
        vc->vc_px += ret * VT_CHAR_SIZE;
    else
        vc->vc_px = VT_DISPLAY_WIDTH - VT_CHAR_SIZE;
}

void
do_ansi_cub(struct vconsole *vc, char *buf, size_t len)
{
    int ret;
    //printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (vc->vc_px >= ret * VT_CHAR_SIZE)
        vc->vc_px -= ret * VT_CHAR_SIZE;
    else
        vc->vc_px = 0;
}

void
do_ansi_cnl(struct vconsole *vc, char *buf, size_t len)
{
    int current_line = vc->vc_py / VT_CHAR_SIZE;
    int ret;
    //printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (current_line + ret <= (VT_HEIGHT_CHARS - 1)) {
        vc->vc_py = (current_line + ret) * VT_CHAR_SIZE;
    } else
        vc->vc_py = VT_DISPLAY_HEIGHT - VT_CHAR_SIZE;
}

void
do_ansi_cpl(struct vconsole *vc, char *buf, size_t len)
{
    int current_line = vc->vc_py / VT_CHAR_SIZE;
    int ret;
    //printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (current_line - ret >= 0) {
        vc->vc_py = (current_line - ret) * VT_CHAR_SIZE;
    } else
        vc->vc_py = 0;
}

void
do_ansi_cha(struct vconsole *vc, char *buf, size_t len)
{
    int current_line = vc->vc_py / VT_CHAR_SIZE;
    int ret;
    //printk("%s: len %d buf \"%s\"\n", __func__, len, buf);

    /* no argument was supplied, default to 1 */
    if (len == 0)
        ret = 1;
    else
        ret = atoi_10n(buf, len);

    if (ret >= 0 && ret <= VT_WIDTH_CHARS) {
        vc->vc_px = ret * VT_CHAR_SIZE;
    } else
        vc->vc_py = 0;
}

void
do_ansi_scp(struct vconsole *vc, char *buf, size_t len)
{
    vc->vc_spx = vc->vc_px;
    vc->vc_spy = vc->vc_py;
}

void
do_ansi_rcp(struct vconsole *vc, char *buf, size_t len)
{
    vc->vc_px = vc->vc_spx;
    vc->vc_py = vc->vc_spy;
}

void
do_ansi_showcur(struct vconsole *vc, char *buf, size_t len)
{
    show_cursor = 1;
}

void
do_ansi_hidecur(struct vconsole *vc, char *buf, size_t len)
{
    show_cursor = 0;
}

void
do_ansi(struct vconsole *vc, char *buf, size_t len)
{
#define ANSI_DO(c, name) if (buf[len] == c) { do_ansi_##name (vc, buf, len); return; }
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
ansi_try_parse(struct vconsole *vc, int *isansi, char *buf, size_t len)
{
    int ansi_state = 0, ansi_size = 0;
    int i, escape_at = -1, begin_at = -1;
    char ansi_buffer[64];

    *isansi = 0;

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
                //printk("ANSI: found ESC at %d\n", escape_at);
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
            //printk("ANSI: found LB at %d\n", begin_at);
            goto as2;
        }

        //printk("ANSI: LB was %c (%x) at %d\n", buf[escape_at + 1], buf[escape_at + 1], begin_at);
        /* nope, bail */
        return 0;
    }
as2:
    if (ansi_state == 2 && begin_at != -1) {
        /* we are in an ANSI sequence, start fetching the bytes */
        while (begin_at + 1 + ansi_size < len) {
            //printk("parsing ANSI remainder...\n");
            char this = buf[begin_at + 1 + ansi_size];
            if (0x40 <= this && this <= 0x7e) {
                /* found the end of the ANSI sequence */
                ansi_buffer[ansi_size] = this;
                ansi_buffer[ansi_size + 1] = 0;
                //printk("Found ANSI sequence: \"%s\"\n", ansi_buffer);
                do_ansi(vc, ansi_buffer, ansi_size);
                *isansi = 1;
                return ansi_size;
            }

            /* else this byte is part of the ANSI sequence */
            ansi_buffer[ansi_size] = this;

            //printk("ANSI: added %c (%x) to size %d\n", this, this, ansi_size);

            /* proceed to the next byte */
            ansi_size ++;
        }

        //printk("Abrupt end!\n");
        /* the stream ended before the ANSI sequence ended, thus don't parse */
        return 0;
    }
}

size_t
videocon_write(struct device *dev, void *_buf, size_t len)
{
    char *buf = _buf;
    int ansis = 0;
    int isansi = 0;
    struct vconsole *vc = dev->priv;

    clear_cursor(vc);

    for (int i = 0; i < len;) {
        /* find ANSI escape sequences */
        ansis = ansi_try_parse(vc, &isansi, _buf + i, len - i);
        if (isansi) {
            i += ansis + 3;
            continue;
        }
        videocon_emit(vc, buf[i]);
        i ++;
    }

    draw_cursor(vc);

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
    .priv = &__vc_0, /* FIXME */
};

#if 0
static void
blink_cursor(void *aux)
{
    if (__cursor_shown)
        clear_cursor(aux);
    else
        draw_cursor(aux);

    work_reschedule(50);
}
#endif

int video_console_init()
{
    bga_set_video(VT_DISPLAY_WIDTH, VT_DISPLAY_HEIGHT, 32, /* LFB */ 1, /* CLEAR */ 1);
    extern struct device *default_user_device;
    default_user_device = &videocon_device;
    __lfb = bga_get_lfb();
    //struct work *blinker = work_create(blink_cursor, );
    //schedule_work_delay(blinker, 50);
    spin_lock_init(&scroll_lock);
    spin_lock_init(&cursor_lock);
    mprintk("initialized.\n");
}

