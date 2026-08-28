// The POSIX types and errno LE names, and nothing behind them but the kernel.
//
// errno is vi's answer, not glibc's: the last syscall's Error as an int. There
// is no table of messages here -- error_name() in kernel/result.h names them.
#pragma once

#ifdef __cplusplus
#include "kernel/result.h"
#include "kernel/text.h" // the Str scanners, in place of sscanf
#include "kernel/types.h"
#else
/* regex.c and wcwidth.c are C and want only the widths and the limits. */
typedef unsigned long usize;
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
#endif

/* What a path buffer holds. alloca is gone -- a variable-length array in a
   coroutine would live in its frame, and a frame past 512 bytes costs a
   whole 64 KiB span -- so the buffers it sized are fixed and, where the
   holder is a coroutine, at file scope. */
enum { LE_PATHMAX = 512 };

// <limits.h>, the three of it that are named here.
enum { UCHAR_MAX = 255, INT_MAX = 0x7fffffff, LONG_MAX = 0x7fffffff };

typedef usize size_t;
typedef unsigned mode_t;
typedef long time_t;
typedef long off_t;
typedef long ssize_t;
typedef u64 ino_t;
typedef u32 dev_t;
typedef u32 uid_t;
typedef u32 gid_t;
typedef long clock_t;

#ifndef EOF
#define EOF (-1)
#endif

// The last syscall's Error, as an int.
extern int errno;

// Only the five LE compares against; each is the kernel's own number so that
// errno = int(r.error()) needs no mapping.
#ifdef __cplusplus
enum {
    ENOENT      = int(Error::NotFound),
    EACCES      = int(Error::Perm),
    EEXIST      = int(Error::Exists),
    ENOMEM      = int(Error::NoMemory),
    EINTR       = int(Error::Intr),
    EAGAIN      = int(Error::Again),
    EWOULDBLOCK = EAGAIN,
};
#endif

// The stat LE looks at. Everything else in struct stat went with the syscalls
// that filled it.
struct stat {
    mode_t st_mode;
    off_t st_size;
    time_t st_mtime;
    ino_t st_ino;
    dev_t st_dev;
};

enum : mode_t {
    S_IFMT  = 0170000,
    S_IFREG = 0100000,
    S_IFDIR = 0040000,
    S_IFLNK = 0120000,
    // No device is open-able as a file here, so these two match nothing.
    S_IFCHR = 0020000,
    S_IFBLK = 0060000,
    S_IRUSR = 0400,
    S_IWUSR = 0200,
    S_IXUSR = 0100,
    S_IRGRP = 0040,
    S_IWGRP = 0020,
    S_IXGRP = 0010,
    S_IROTH = 0004,
    S_IWOTH = 0002,
    S_IXOTH = 0001,
    S_ISUID = 04000,
    S_ISGID = 02000,
};

#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISBLK(m)  (0)
#define S_ISCHR(m)  (0)
#define S_ISFIFO(m) (0)
#define S_ISSOCK(m) (0)
