/*
 * <stdio.h> for Braam (docs/Braam.md §7.3).  Every call that can wait is a coroutine
 * yielding braam_call *, called with await.  Output goes into the stream's buffer of
 * BUFSIZ bytes, and a call that finds it full waits while it is written out: printf,
 * fprintf, vprintf, vfprintf, puts, putchar, fputs, fputc, putc, fwrite and perror.
 * stderr is written out at the end of each of them.  fflush writes a buffer out;
 * fgetc, getc, getchar, fgets and fread read through it (a read of stdin flushes stdout
 * first); and so are fopen, fclose, fseek, ftell, rewind, remove and rename.  The
 * formatting into memory, sprintf, snprintf, vsprintf and vsnprintf, never waits.  The
 * runtime flushes every stream when main returns, and exit() posts what is left
 * (stdlib.h).
 *
 * As in C, output is not directly followed by input on one stream without an fflush,
 * fseek or rewind between them, and input not by output without an fseek or rewind.
 */
#ifndef _STDIO_H
#define _STDIO_H

#include <braam.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>

#define EOF          (-1)
#define BUFSIZ       4096
#define FILENAME_MAX 256

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct __FILE {
    int fd;
    int flags; /* __F_EOF, __F_ERR, __F_OPENED, __F_BUSY */
    char *buf; /* what is waiting to be written, BUFSIZ bytes */
    size_t len;
    char *in; /* what was read and not taken: in[pos] up to in[end] */
    size_t pos, end;
    struct __FILE *next; /* the streams fopen made, for fflush(NULL) */
} FILE;

#define __F_EOF    1
#define __F_ERR    2
#define __F_OPENED 4
#define __F_BUSY   8 /* a write of the buffer is waiting */

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

typedef long long fpos_t;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Formatting into memory, which never waits. */
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, int size, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

/* Output to stdout. */
coro(braam_call *) int printf(const char *fmt, ...);
coro(braam_call *) int vprintf(const char *fmt, va_list ap);
coro(braam_call *) int puts(const char *s);
coro(braam_call *) int putchar(int c);

/* Output to a stream. */
coro(braam_call *) int fprintf(FILE *stream, const char *fmt, ...);
coro(braam_call *) int vfprintf(FILE *stream, const char *fmt, va_list ap);
coro(braam_call *) int fputc(int c, FILE *stream);
coro(braam_call *) int putc(int c, FILE *stream);
coro(braam_call *) int fputs(const char *s, FILE *stream);
coro(braam_call *) size_t fwrite(const void *ptr, size_t size, size_t n, FILE *stream);
/* "s: strerror(errno)" on stderr. */
coro(braam_call *) void perror(const char *s);

/* Write the buffer out: 0, or EOF with errno set.  NULL flushes every stream. */
coro(braam_call *) int fflush(FILE *stream);

/* Reading.  EOF at the end of the input or on an error; feof and ferror tell which. */
coro(braam_call *) int fgetc(FILE *stream);
coro(braam_call *) int getc(FILE *stream);
coro(braam_call *) int getchar(void);
/* Up to size - 1 bytes, through the first newline; NULL if nothing was read. */
coro(braam_call *) char *fgets(char *s, int size, FILE *stream);
coro(braam_call *) size_t fread(void *ptr, size_t size, size_t n, FILE *stream);
/* One byte back, read again next; not EOF. */
int ungetc(int c, FILE *stream);

int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);

/* "r", "w", "a", with "+" for update, "x" for a new file only; "b" is ignored. */
coro(braam_call *) FILE *fopen(const char *path, const char *mode);
/* Flushes and closes; a stream fopen made is freed. */
coro(braam_call *) int fclose(FILE *stream);
coro(braam_call *) int fseek(FILE *stream, long offset, int whence);
coro(braam_call *) long ftell(FILE *stream);
coro(braam_call *) void rewind(FILE *stream);

/* Accepted and ignored: a stream's buffer is BUFSIZ bytes. */
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
void setbuffer(FILE *stream, char *buf, int size);
void setbuf(FILE *stream, char *buf);

coro(braam_call *) int remove(const char *path);
coro(braam_call *) int rename(const char *from, const char *to);

#endif /* _STDIO_H */
