/*
 * <fcntl.h> for Braam: open, a coroutine yielding braam_call *.  The flags are
 * Sys::Open's bits, so O_RDONLY is not 0 here.
 */
#ifndef _FCNTL_H
#define _FCNTL_H

#include <braam.h>

#define O_RDONLY BRAAM_O_READ
#define O_WRONLY BRAAM_O_WRITE
#define O_RDWR   (BRAAM_O_READ | BRAAM_O_WRITE)
#define O_CREAT  BRAAM_O_CREATE
#define O_TRUNC  BRAAM_O_TRUNC
#define O_APPEND BRAAM_O_APPEND
#define O_EXCL   BRAAM_O_EXCL

/* The descriptor, or -1 with errno set.  A third argument (a mode) is not taken. */
coro(braam_call *) int open(const char *path, int flags);

#endif /* _FCNTL_H */
