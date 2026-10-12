/*
 * <unistd.h> for Braam: the descriptor calls, coroutines yielding braam_call *, so a
 * program calls them with await (docs/Braam.md §7.3).  On an error they return
 * -1 and set errno.
 */
#ifndef _UNISTD_H
#define _UNISTD_H

#include <braam.h>
#include <stddef.h>
#include <sys/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Up to n bytes; 0 at the end of the input. */
coro(braam_call *) ssize_t read(int fd, void *buf, size_t n);
/* All n bytes, unless an error stops it. */
coro(braam_call *) ssize_t write(int fd, const void *buf, size_t n);
coro(braam_call *) int close(int fd);
/* The new position; a file only (not 0, 1 or 2). */
coro(braam_call *) off_t lseek(int fd, off_t offset, int whence);

/* A file, or an empty directory. */
coro(braam_call *) int unlink(const char *path);
coro(braam_call *) int rmdir(const char *path);

/* The working directory: set it, or copy it to buf (NULL if it does not fit). */
coro(braam_call *) int chdir(const char *path);
coro(braam_call *) char *getcwd(char *buf, size_t size);

pid_t getpid(void);

#endif /* _UNISTD_H */
