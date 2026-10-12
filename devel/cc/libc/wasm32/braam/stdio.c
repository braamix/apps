/*
 * The streams of Braam's stdio (docs/Braam.md §7.3): an output buffer each,
 * which the formatting functions fill without blocking and fflush writes out, and an
 * input buffer, which the reading coroutines refill a chunk at a time.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE in_file = { 0 }, out_file = { 1 }, err_file = { 2 };
FILE *stdin  = &in_file;
FILE *stdout = &out_file;
FILE *stderr = &err_file;

static FILE *opened; /* the streams fopen made, newest first */

int fputc(int c, FILE *f)
{
    if (f->len == f->cap) {
        size_t cap = f->cap ? 2 * f->cap : 256;
        char *buf  = realloc(f->buf, cap);
        if (!buf) {
            f->flags |= __F_ERR;
            return EOF;
        }
        f->buf = buf;
        f->cap = cap;
    }
    f->buf[f->len++] = (char)c;
    return (unsigned char)c;
}

int putc(int c, FILE *f)
{
    return fputc(c, f);
}

int fputs(const char *s, FILE *f)
{
    for (; *s; s++)
        if (fputc(*s, f) == EOF)
            return EOF;
    return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t n, FILE *f)
{
    const char *p = ptr;
    size_t bytes  = size * n, k;
    for (k = 0; k < bytes; k++)
        if (fputc(p[k], f) == EOF)
            break;
    return size ? k / size : 0;
}

void perror(const char *s)
{
    if (s && *s)
        fprintf(stderr, "%s: ", s);
    fprintf(stderr, "%s\n", strerror(errno));
}

/* Where putbyte writes: stdout, or the stream of the fprintf running. */
static FILE *current = &out_file;

void putbyte(int b)
{
    fputc(b, current);
}

void putch(unsigned c)
{
    fputc((int)c, current);
}

extern int __doprnt(const char *fmt, va_list ap, char *buf, int size, int to_buf);

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    FILE *was = current;
    current   = f;
    int n     = __doprnt(fmt, ap, 0, 0, 0);
    current   = was;
    return n;
}

int vprintf(const char *fmt, va_list ap)
{
    return vfprintf(stdout, fmt, ap);
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

void flush(void)
{
}

coro(braam_call *) int fflush(FILE *f)
{
    if (!f) {
        int bad = 0;
        for (FILE *g = opened; g; g = g->next)
            bad |= await fflush(g) == EOF;
        bad |= await fflush(stdout) == EOF;
        bad |= await fflush(stderr) == EOF;
        return bad ? EOF : 0;
    }
    if (f->len == 0)
        return 0;
    ssize_t n = await write(f->fd, f->buf, f->len);
    f->len    = 0;
    if (n < 0) {
        f->flags |= __F_ERR;
        return EOF;
    }
    return 0;
}

/* The next chunk into the input buffer: 1, or 0 at the end or on an error.  One byte
   is kept free below the chunk for ungetc. */
static coro(braam_call *) int refill(FILE *f)
{
    if (f->flags & (__F_EOF | __F_ERR))
        return 0;
    if (!f->in && !(f->in = malloc(BUFSIZ + 1))) {
        f->flags |= __F_ERR;
        errno = ENOMEM;
        return 0;
    }
    ssize_t n = await read(f->fd, f->in + 1, BUFSIZ);
    if (n <= 0) {
        f->flags |= n < 0 ? __F_ERR : __F_EOF;
        f->pos = f->end = 0;
        return 0;
    }
    f->pos = 1;
    f->end = 1 + (size_t)n;
    return 1;
}

coro(braam_call *) int fgetc(FILE *f)
{
    if (f->pos == f->end && !await refill(f))
        return EOF;
    return (unsigned char)f->in[f->pos++];
}

coro(braam_call *) int getc(FILE *f)
{
    return await fgetc(f);
}

coro(braam_call *) int getchar(void)
{
    return await fgetc(stdin);
}

coro(braam_call *) char *fgets(char *s, int size, FILE *f)
{
    int k = 0;
    while (k < size - 1) {
        if (f->pos == f->end && !await refill(f))
            break;
        char c = f->in[f->pos++];
        s[k++] = c;
        if (c == '\n')
            break;
    }
    if (k == 0 || (f->flags & __F_ERR))
        return NULL;
    s[k] = 0;
    return s;
}

coro(braam_call *) size_t fread(void *ptr, size_t size, size_t n, FILE *f)
{
    char *p      = ptr;
    size_t bytes = size * n, k = 0;
    while (k < bytes) {
        if (f->pos == f->end && !await refill(f))
            break;
        size_t take = f->end - f->pos < bytes - k ? f->end - f->pos : bytes - k;
        memcpy(p + k, f->in + f->pos, take);
        f->pos += take;
        k += take;
    }
    return size ? k / size : 0;
}

int ungetc(int c, FILE *f)
{
    if (c == EOF)
        return EOF;
    if (!f->in && !(f->in = malloc(BUFSIZ + 1)))
        return EOF;
    if (f->pos == 0) {
        if (f->end > BUFSIZ)
            return EOF; /* a second ungetc before a read */
        memmove(f->in + 1, f->in, f->end);
        f->end++;
    } else {
        f->pos--;
    }
    f->in[f->pos] = (char)c;
    f->flags &= ~__F_EOF;
    return (unsigned char)c;
}

int feof(FILE *f)
{
    return f->flags & __F_EOF;
}

int ferror(FILE *f)
{
    return f->flags & __F_ERR;
}

void clearerr(FILE *f)
{
    f->flags &= ~(__F_EOF | __F_ERR);
}

int fileno(FILE *f)
{
    return f->fd;
}

/* fopen's mode as Sys::Open's flags, as Braam's compat layer reads it; 0 for a mode
   that starts with none of r, w and a. */
static int mode_flags(const char *mode)
{
    int flags;
    switch (mode[0]) {
    case 'r':
        flags = O_RDONLY;
        break;
    case 'w':
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case 'a':
        flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
    default:
        return 0;
    }
    for (const char *m = mode + 1; *m; m++) {
        if (*m == '+')
            flags |= O_RDWR;
        else if (*m == 'x')
            flags |= O_EXCL;
    }
    return flags;
}

coro(braam_call *) FILE *fopen(const char *path, const char *mode)
{
    int flags = mode_flags(mode);
    if (!flags) {
        errno = EINVAL;
        return NULL;
    }
    FILE *f = calloc(1, sizeof *f);
    if (!f) {
        errno = ENOMEM;
        return NULL;
    }
    f->fd = await open(path, flags);
    if (f->fd < 0) {
        free(f);
        return NULL;
    }
    f->flags = __F_OPENED;
    f->next  = opened;
    opened   = f;
    return f;
}

coro(braam_call *) int fclose(FILE *f)
{
    int bad = await fflush(f) == EOF;
    bad |= await close(f->fd) < 0;
    free(f->buf);
    free(f->in);
    f->buf = f->in = NULL;
    f->len = f->cap = f->pos = f->end = 0;
    if (f->flags & __F_OPENED) {
        FILE **p = &opened;
        while (*p != f)
            p = &(*p)->next;
        *p = f->next;
        free(f);
    }
    return bad ? EOF : 0;
}

/* What was read ahead and not taken goes back to the descriptor's position. */
coro(braam_call *) int fseek(FILE *f, long offset, int whence)
{
    if (await fflush(f) == EOF)
        return -1;
    if (whence == SEEK_CUR)
        offset -= (long)(f->end - f->pos);
    if (await lseek(f->fd, offset, whence) < 0)
        return -1;
    f->pos = f->end = 0;
    f->flags &= ~__F_EOF;
    return 0;
}

coro(braam_call *) long ftell(FILE *f)
{
    off_t at = await lseek(f->fd, 0, SEEK_CUR);
    if (at < 0)
        return -1;
    return (long)(at - (off_t)(f->end - f->pos) + (off_t)f->len);
}

coro(braam_call *) void rewind(FILE *f)
{
    await fseek(f, 0, SEEK_SET);
    f->flags &= ~__F_ERR;
}
