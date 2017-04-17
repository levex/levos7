#ifndef __LEVOS_ERRNO_H
#define __LEVOS_ERRNO_H

#define    EPERM          1    /* Operation not permitted */
#define    ENOENT         2    /* No such file or directory */
#define    ESRCH          3    /* No such process */
#define    EINTR          4    /* Interrupted system call */
#define    EIO            5    /* I/O error */
#define    ENXIO          6    /* No such device or address */
#define    E2BIG          7    /* Argument list too long */
#define    ENOEXEC        8    /* Exec format error */
#define    EBADF          9    /* Bad file number */
#define    ECHILD        10    /* No child processes */
#define    EAGAIN        11    /* Try again */
#define    ENOMEM        12    /* Out of memory */
#define    EACCES        13    /* Permission denied */
#define    EFAULT        14    /* Bad address */
#define    ENOTBLK       15    /* Block device required */
#define    EBUSY         16    /* Device or resource busy */
#define    EEXIST        17    /* File exists */
#define    EXDEV         18    /* Cross-device link */
#define    ENODEV        19    /* No such device */
#define    ENOTDIR       20    /* Not a directory */
#define    EISDIR        21    /* Is a directory */
#define    EINVAL        22    /* Invalid argument */
#define    ENFILE        23    /* File table overflow */
#define    EMFILE        24    /* Too many open files */
#define    ENOTTY        25    /* Not a typewriter */
#define    ETXTBSY       26    /* Text file busy */
#define    EFBIG         27    /* File too large */
#define    ENOSPC        28    /* No space left on device */
#define    ESPIPE        29    /* Illegal seek */
#define    EROFS         30    /* Read-only file system */
#define    EMLINK        31    /* Too many links */
#define    EPIPE         32    /* Broken pipe */
#define    EDOM          33    /* Math argument out of domain of func */
#define    ERANGE        34    /* Math result not representable */
#define    ENOSYS        35    /* No such system call */

#define    ENOTSOCK      88    /* Socket operation on non-socket */
#define    EAFNOSUPPORT  97    /* Address family not supported by protocol */
#define    EADDRINUSE    98    /* Address already in use */
#define    ECONNRESET    104   /* Connection reset by peer */
#define    ENOTCONN      107   /* Transport endpoint is not connected */
#define    ETIMEDOUT     110   /* Connection timed out */
#define    ECONNREFUSED  111   /* Connection refused */

inline const char *errno_to_string(int errno)
{
    if (errno == 0)
        return "SUCCESS";
    if (errno < 0)
        errno *= -1;

    /* shoutout to vim recording feature! */
    switch (errno) {
        case EPERM: return "EPERM";
        case ENOENT: return "ENOENT";
        case ESRCH: return "ESRCH";
        case EINTR: return "EINTR";
        case EIO: return "EIO";
        case ENXIO: return "ENXIO";
        case E2BIG: return "E2BIG";
        case ENOEXEC: return "ENOEXEC";
        case EBADF: return "EBADF";
        case ECHILD: return "ECHILD";
        case EAGAIN: return "EAGAIN";
        case ENOMEM: return "ENOMEM";
        case EACCES: return "EACCES";
        case EFAULT: return "EFAULT";
        case ENOTBLK: return "ENOTBLK";
        case EBUSY: return "EBUSY";
        case EEXIST: return "EEXIST";
        case EXDEV: return "EXDEV";
        case ENODEV: return "ENODEV";
        case ENOTDIR: return "ENOTDIR";
        case EISDIR: return "EISDIR";
        case EINVAL: return "EINVAL";
        case ENFILE: return "ENFILE";
        case EMFILE: return "EMFILE";
        case ENOTTY: return "ENOTTY";
        case ETXTBSY: return "ETXTBSY";
        case EFBIG: return "EFBIG";
        case ENOSPC: return "ENOSPC";
        case ESPIPE: return "ESPIPE";
        case EROFS: return "EROFS";
        case EMLINK: return "EMLINK";
        case EPIPE: return "EPIPE";
        case EDOM: return "EDOM";
        case ERANGE: return "ERANGE";
        case ENOSYS: return "ENOSYS";
        case EADDRINUSE: return "EADDRINUSE";
        case ETIMEDOUT: return "ETIMEDOUT";
        case ECONNREFUSED: return "ECONNREFUSED";
        case ENOTCONN: return "ENOTCONN";
        case ECONNRESET: return "ECONNRESET";
    }
    return "UNKNOWN";
}


#endif /* __LEVOS_ERRNO_H */
