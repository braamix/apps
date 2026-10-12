/*
 * <sys/stat.h> for Braam: stat, lstat, fstat and mkdir, coroutines yielding
 * braam_call *.  Braam keeps a kind, a size and a modification time; the rest of a
 * struct stat is a constant, as in Braam's own compat layer: no permissions (a file
 * is 0644, a directory 0755), no owners, one link, and st_ino a hash of the path.
 */
#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <braam.h>
#include <sys/types.h>
#include <time.h>

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    off_t st_size;
    time_t st_atime; /* seconds since the epoch, 0 when the filesystem keeps none */
    time_t st_mtime;
    time_t st_ctime;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

#define S_IFMT     0170000
#define S_IFDIR    0040000
#define S_IFREG    0100000
#define S_IFLNK    0120000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)

/* 0, or -1 with errno set.  lstat reports a link itself; fstat's st_ino is 0. */
coro(braam_call *) int stat(const char *path, struct stat *st);
coro(braam_call *) int lstat(const char *path, struct stat *st);
coro(braam_call *) int fstat(int fd, struct stat *st);

/* The mode is not kept. */
coro(braam_call *) int mkdir(const char *path, mode_t mode);

#endif /* _SYS_STAT_H */
