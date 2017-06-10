#ifndef __LEVOS_TTY_H
#define __LEVOS_TTY_H

#include <levos/kernel.h>
#include <levos/types.h>
#include <levos/ring.h>
#include <levos/task.h>

typedef unsigned int  tcflag_t;
typedef unsigned int  speed_t;
typedef unsigned char cc_t;

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
#define NCCS 32
	cc_t     c_cc[NCCS];
};

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;   /* unused */
    unsigned short ws_ypixel;   /* unused */
};

#define VEOF 1
#define VEOL 2
#define VERASE 3
#define VINTR 4
#define VKILL 5
#define VMIN 6
#define VQUIT 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VTIME 11
#define VWERASE 12
//#define NCCS 24

#define __CONTROL(c) (((c) - 64) & 127)

/* c_iflag */
#define BRKINT  0000001
#define ICRNL   0000002
#define IGNBRK  0000004
#define IGNCR   0000010
#define IGNPAR  0000020
#define INLCR   0000040
#define INPCK   0000100
#define ISTRIP  0000200
#define IUCLC   0000400
#define IXANY   0001000
#define IXOFF   0002000
#define IXON    0004000
#define PARMRK  0010000

/* c_oflag */
#define OPOST   0000001
#define OLCUC   0000002
#define ONLCR   0000004
#define OCRNL   0000010
#define ONOCR   0000020
#define ONLRET  0000040
#define OFILL   0000100
#define OFDEL   0000200
#define NLDLY   0000400
#define   NL0   0000000
#define   NL1   0000400
#define CRDLY   0003000
#define   CR0   0000000
#define   CR1   0001000
#define   CR2   0002000
#define   CR3   0003000
#define TABDLY  0014000
#define   TAB0  0000000
#define   TAB1  0004000
#define   TAB2  0010000
#define   TAB3  0014000
#define BSDLY   0020000
#define   BS0   0000000
#define   BS1   0020000
#define FFDLY   0100000
#define   FF0   0000000
#define   FF1   0100000
#define VTDLY   0040000
#define   VT0   0000000
#define   VT1   0040000

/* baud rates, meh */

#define B0      0000000
#define B50     0000001
#define B75     0000002
#define B110    0000003
#define B134    0000004
#define B150    0000005
#define B200    0000006
#define B300    0000007
#define B600    0000010
#define B1200   0000011
#define B1800   0000012
#define B2400   0000013
#define B4800   0000014
#define B9600   0000015
#define B19200  0000016
#define B38400  0000017

/* c_cflag */

#define CSIZE   0000060
#define   CS5   0000000
#define   CS6   0000020
#define   CS7   0000040
#define   CS8   0000060
#define CSTOPB  0000100
#define CREAD   0000200
#define PARENB  0000400
#define PARODD  0001000
#define HUPCL   0002000
#define CLOCAL  0004000

/* c_lflag */

#define ISIG    0000001
#define ICANON  0000002
#define XCASE   0000004
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define ECHOCTL 0001000
#define NOFLSH  0000200
#define TOSTOP  0000400
#define IEXTEN  0001000

/* termios ioctls, copied from linux */
#define TCGETS		0x5401
#define TCSETS		0x5402
#define TCSETSW		0x5403
#define TCSETSF		0x5404
#define TCGETA		0x5405
#define TCSETA		0x5406
#define TCSETAW		0x5407
#define TCSETAF		0x5408
#define TCSBRK		0x5409
#define TCXONC		0x540A
#define TCFLSH		0x540B
#define TIOCEXCL	0x540C
#define TIOCNXCL	0x540D
#define TIOCSCTTY	0x540E
#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410
#define TIOCOUTQ	0x5411
#define TIOCSTI		0x5412
#define TIOCGWINSZ	0x5413
#define TIOCSWINSZ	0x5414
#define TIOCMGET	0x5415
#define TIOCMBIS	0x5416
#define TIOCMBIC	0x5417
#define TIOCMSET	0x5418
#define TIOCGSOFTCAR	0x5419
#define TIOCSSOFTCAR	0x541A

#define MAX_CANON 4096

/* XXX: what is this on linux? should we use a ringbuffer? */
#define PTY_BUF_SIZE MAX_CANON

#define PTY_SIDE_MASTER 0
#define PTY_SIDE_SLAVE  1

struct tty_device;

struct pty {
	int pty_side;
    struct pty *pty_other;

    struct ring_buffer pty_out_rb;

	struct tty_device *pty_tty;
};

#define TTY_STATE_UNKNOWN 0
#define TTY_STATE_CLOSED  1

struct tty_device {
	/* the line discipline that manipulates the stream */
    struct tty_line_discipline *tty_ldisc;

    /* the char device */
    struct device *tty_device;

	/* various flags */
	struct termios tty_termios;

    /* output buffer */
    struct ring_buffer tty_out;

    /* state */
    int tty_state;

    /* tty id */
    int tty_id;

    /* foreground process */
    pid_t tty_fg_proc;

    struct winsize tty_winsize;

    /* private data for the line discipline */
    void *priv_ldisc;
};

struct tty_line_discipline {
	int (*write_output)(struct tty_device *, uint8_t);
	int (*write_input)(struct tty_device *, uint8_t);
    int (*read_buf)(struct tty_device *, uint8_t *, size_t);

    int (*flush)(struct pty *);

    int (*init)(struct tty_line_discipline *);

	void *priv;
};

struct n_tty_priv {
    struct ring_buffer line_buffer;
    char line_editing[512];
    int line_len;
};

#endif /* __LEVOS_TTY_H */
