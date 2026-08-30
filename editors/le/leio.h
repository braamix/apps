// The file syscalls, in the shape LE calls them, over proc/io.h.
//
// Upstream called open/read/write/lseek/stat directly and they blocked; here
// each one is a syscall and so a co_await. The names carry an le_ prefix
// because these differ from POSIX's in exactly one way -- the co_await -- and
// a call that lost it would otherwise still compile.
//
// Failure is -1 with le_errno set to the kernel's own Error, which is what
// lesys.h's E* are.
#pragma once

#include "kernel/task.h"
#include "lesys.h"

// open()'s flags, mapped onto SYS_O_* by le_open.
enum {
    O_RDONLY  = 0,
    O_WRONLY  = 1,
    O_RDWR    = 2,
    O_ACCMODE = 3,
    O_CREAT   = 0100,
    O_TRUNC   = 01000,
    O_APPEND  = 02000,
    O_EXCL    = 0200,
    O_BINARY  = 0, // there is no text mode to be the other of
};

enum { SEEK_SET = 0, SEEK_CUR = 1, SEEK_END = 2 };

// access()'s modes. There is no permission bit to test against, so le_access
// answers whether the path is there.
enum { F_OK = 0, X_OK = 1, W_OK = 2, R_OK = 4 };

Task<int> le_open(const char *path, int flags, mode_t mode = 0666);
Task<int> le_close(int fd);
Task<ssize_t> le_read(int fd, void *buf, size_t n);
Task<ssize_t> le_write(int fd, const void *buf, size_t n);
Task<off_t> le_lseek(int fd, off_t off, int whence);
Task<int> le_ftruncate(int fd, off_t n);
Task<int> le_stat(const char *path, struct stat *st);
Task<int> le_lstat(const char *path, struct stat *st);
Task<int> le_fstat(int fd, struct stat *st);
Task<int> le_unlink(const char *path);
Task<int> le_rename(const char *from, const char *to);
Task<int> le_mkdir(const char *path, mode_t mode = 0777);
Task<int> le_access(const char *path, int mode);

// Every byte, or -1: a short write is a failure to the callers here, which
// write a whole block or give up.
Task<int> le_write_all(int fd, const void *buf, size_t n);
