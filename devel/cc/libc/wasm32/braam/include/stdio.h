/*
 * <stdio.h> for Braam (docs/Braam.md §7.3).  The output half never blocks:
 * printf, puts, putchar, fputs, fputc, putc and fwrite append to the stream's buffer,
 * which grows rather than waits.  What can wait is a coroutine yielding braam_call *,
 * called with await: fflush writes a buffer out, fgetc, getc, getchar, fgets and fread
 * read through a buffer of BRAAM_CHUNK bytes (a read of stdin flushes stdout first),
 * and so are fopen, fclose, fseek, ftell, rewind, remove and rename.  The runtime
 * flushes every stream when main returns; exit() cannot (stdlib.h).
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
#define BUFSIZ       BRAAM_CHUNK
#define FILENAME_MAX 256

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct __FILE {
    int fd;
    int flags; /* __F_EOF, __F_ERR, __F_OPENED */
    char *buf; /* what is waiting to be written */
    size_t len, cap;
    char *in; /* what was read and not taken: in[pos] up to in[end] */
    size_t pos, end;
    struct __FILE *next; /* the streams fopen made, for fflush(NULL) */
} FILE;

#define __F_EOF    1
#define __F_ERR    2
#define __F_OPENED 4

typedef long long fpos_t;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Formatting, into stdout's buffer. */
int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, int size, const char *fmt, ...);
int puts(const char *s);
int putchar(int c);

/* Into a stream's buffer. */
int fprintf(FILE *stream, const char *fmt, ...);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int vprintf(const char *fmt, va_list ap);
int fputc(int c, FILE *stream);
int putc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t n, FILE *stream);
/* "s: strerror(errno)" on stderr. */
void perror(const char *s);

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

coro(braam_call *) int remove(const char *path);
coro(braam_call *) int rename(const char *from, const char *to);

/* The runtime's console primitives: putbyte and putch append a byte to stdout's
   buffer; flush does nothing (a write is a syscall, so only fflush may make one). */
void putbyte(int b);
void putch(unsigned ch);
void flush(void);

#endif /* _STDIO_H */
