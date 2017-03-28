#ifndef __LEVOS_SIGNAL_H
#define __LEVOS_SIGNAL_H

#include <levos/task.h>

struct task;

#define SIGHUP       1   	/* Hangup (POSIX) */
#define SIGINT       2       /* Terminal interrupt (ANSI) */
#define SIGQUIT      3       /* Terminal quit (POSIX) */
#define SIGILL       4       /* Illegal instruction (ANSI) */
#define SIGTRAP      5       /* Trace trap (POSIX) */
#define SIGIOT       6       /* IOT Trap (4.2 BSD) */
#define SIGBUS       7       /* BUS error (4.2 BSD) */
#define SIGFPE       8       /* Floating point exception (ANSI) */
#define SIGKILL      9       /* Kill(can't be caught or ignored) (POSIX) */
#define SIGUSR1     10  	/* User defined signal 1 (POSIX) */
#define SIGSEGV     11  	/* Invalid memory segment access (ANSI) */
#define SIGUSR2     12  	/* User defined signal 2 (POSIX) */
#define SIGPIPE     13  	/* Write on a pipe with no reader, Broken pipe (POSIX) */
#define SIGALRM     14  	/* Alarm clock (POSIX) */
#define SIGTERM     15  	/* Termination (ANSI) */
#define SIGSTKFLT   16  /* Stack fault */
#define SIGCHLD     17	    /* Child process has stopped or exited, changed (POSIX) */
#define SIGCONT     18	    /* Continue executing, if stopped (POSIX) */
#define SIGSTOP     19	    /* Stop executing(can't be caught or ignored) (POSIX) */
#define SIGTSTP     20	    /* Terminal stop signal (POSIX) */
#define SIGTTIN     21	    /* Background process trying to read, from TTY (POSIX) */
#define SIGTTOU     22	    /* Background process trying to write, to TTY (POSIX) */
#define SIGURG      23	    /* Urgent condition on socket (4.2 BSD) */
#define SIGXCPU     24	    /* CPU limit exceeded (4.2 BSD) */
#define SIGXFSZ     25	    /* File size limit exceeded (4.2 BSD) */
#define SIGVTALRM   26  /* Virtual alarm clock (4.2 BSD) */
#define SIGPROF     27	    /* Profiling alarm clock (4.2 BSD) */
#define SIGWINCH    28  /* Window size change (4.3 BSD, Sun) */
#define SIGIO       29      /* I/O now possible (4.2 BSD) */
#define SIGPWR      30      /* Power failure restart (System V) */

inline char *signal_to_string(int sig)
{
    switch (sig)
    {
        case SIGHUP:return "SIGHUP";
        case SIGINT:return "SIGINT";
        case SIGQUIT:return "SIGQUIT";
        case SIGILL:return "SIGILL";
        case SIGTRAP:return "SIGTRAP";
        case SIGIOT:return "SIGIOT";
        case SIGBUS:return "SIGBUS";
        case SIGFPE:return "SIGFPE";
        case SIGKILL:return "SIGKILL";
        case SIGUSR1:return "SIGUSR1";
        case SIGSEGV:return "SIGSEGV";
        case SIGUSR2:return "SIGUSR2";
        case SIGPIPE:return "SIGPIPE";
        case SIGALRM:return "SIGALRM";
        case SIGTERM:return "SIGTERM";
        case SIGSTKFLT:return "SIGSTKFLT";
        case SIGCHLD:return "SIGCHLD";
        case SIGCONT:return "SIGCONT";
        case SIGSTOP:return "SIGSTOP";
        case SIGTSTP:return "SIGTSTP";
        case SIGTTIN:return "SIGTTIN";
        case SIGTTOU:return "SIGTTOU";
        case SIGURG:return "SIGURG";
        case SIGXCPU:return "SIGXCPU";
        case SIGXFSZ:return "SIGXFSZ";
        case SIGVTALRM:return "SIGVTALRM";
        case SIGPROF:return "SIGPROF";
        case SIGWINCH:return "SIGWINCH";
        case SIGIO:return "SIGIO";
        case SIGPWR:return "SIGPWR";
        default: return "UNKNOWN";
    }
}


void send_signal(struct task *, int);

#endif /* __LEVOS_SIGNAL_H */
