// The streams of Braam's stdio (docs/Braam.md §7.3): an output buffer each of BUFSIZ
// bytes, which the output coroutines fill and write out when it is full, and an input
// buffer, which the reading coroutines refill a chunk at a time.
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

static FILE *opened; // the streams fopen made, newest first

// The buffer of f, made on first use; NULL with errno set.
static char *buffer(FILE *f)
{
    if (!f->buf && !(f->buf = malloc(BUFSIZ))) {
        f->flags |= __F_ERR;
        errno = ENOMEM;
    }
    return f->buf;
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
    // One write at a time: another task's may be waiting.
    while (f->flags & __F_BUSY)
        await braam_yield();
    size_t len = f->len;
    if (len == 0)
        return 0;
    f->flags |= __F_BUSY;
    ssize_t n = await write(f->fd, f->buf, len);
    f->flags &= ~__F_BUSY;
    // Another task may have added to the buffer while the write waited.
    memmove(f->buf, f->buf + len, f->len - len);
    f->len -= len;
    if (n < 0) {
        f->flags |= __F_ERR;
        return EOF;
    }
    return 0;
}

// The end of an output call: stderr is written out at once.
static coro(braam_call *) int done(FILE *f, int result)
{
    if (f == stderr && await fflush(f) == EOF)
        return EOF;
    return result;
}

// n bytes at p into f's buffer, writing it out each time it fills; one that does not
// fit an empty buffer is written directly.
static coro(braam_call *) int put(FILE *f, const char *p, size_t n)
{
    if (!buffer(f))
        return EOF;
    while (n > 0) {
        if (f->len == 0 && n >= BUFSIZ && !(f->flags & __F_BUSY)) {
            f->flags |= __F_BUSY;
            ssize_t w = await write(f->fd, p, n);
            f->flags &= ~__F_BUSY;
            if (w < 0) {
                f->flags |= __F_ERR;
                return EOF;
            }
            return 0;
        }
        if (f->len == BUFSIZ && await fflush(f) == EOF)
            return EOF;
        size_t k = BUFSIZ - f->len < n ? BUFSIZ - f->len : n;
        memcpy(f->buf + f->len, p, k);
        f->len += k;
        p += k;
        n -= k;
    }
    return 0;
}

coro(braam_call *) int fputc(int c, FILE *f)
{
    char b = (char)c;
    if (f->buf && f->len < BUFSIZ && f != stderr) {
        f->buf[f->len++] = b; // what nearly every call is
        return (unsigned char)c;
    }
    if (await put(f, &b, 1) == EOF)
        return EOF;
    return await done(f, (unsigned char)c);
}

coro(braam_call *) int putc(int c, FILE *f)
{
    return await fputc(c, f);
}

coro(braam_call *) int putchar(int c)
{
    return await fputc(c, stdout);
}

coro(braam_call *) int fputs(const char *s, FILE *f)
{
    if (await put(f, s, strlen(s)) == EOF)
        return EOF;
    return await done(f, 0);
}

coro(braam_call *) int puts(const char *s)
{
    if (await put(stdout, s, strlen(s)) == EOF || await put(stdout, "\n", 1) == EOF)
        return EOF;
    return 0;
}

coro(braam_call *) size_t fwrite(const void *ptr, size_t size, size_t n, FILE *f)
{
    if (size == 0 || n == 0)
        return 0;
    if (await put(f, ptr, size * n) == EOF)
        return 0;
    return await done(f, 0) == EOF ? 0 : n;
}

// Formatted into the free room of the buffer when it fits; else into the buffer just
// written out, or, too long for any, into a block of its own.
coro(braam_call *) int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    if (!buffer(f))
        return EOF;
    va_list again;
    va_copy(again, ap);
    size_t room = BUFSIZ - f->len;
    int n       = vsnprintf(f->buf + f->len, room, fmt, again);
    va_end(again);
    if (n < 0)
        return n;
    if ((size_t)n < room) {
        f->len += (size_t)n;
        return await done(f, n);
    }
    if (await fflush(f) == EOF)
        return EOF;
    room = BUFSIZ - f->len;
    if ((size_t)n < room) {
        vsnprintf(f->buf + f->len, room, fmt, ap);
        f->len += (size_t)n;
        return await done(f, n);
    }
    char *block = malloc((size_t)n + 1);
    if (!block) {
        f->flags |= __F_ERR;
        errno = ENOMEM;
        return EOF;
    }
    vsnprintf(block, (size_t)n + 1, fmt, ap);
    int st = await put(f, block, (size_t)n);
    free(block);
    if (st == EOF)
        return EOF;
    return await done(f, n);
}

coro(braam_call *) int vprintf(const char *fmt, va_list ap)
{
    return await vfprintf(stdout, fmt, ap);
}

coro(braam_call *) int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = await vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

coro(braam_call *) int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = await vfprintf(stdout, fmt, ap);
    va_end(ap);
    return n;
}

coro(braam_call *) void perror(const char *s)
{
    if (s && *s)
        await fprintf(stderr, "%s: %s\n", s, strerror(errno));
    else
        await fprintf(stderr, "%s\n", strerror(errno));
}

void __braam_post(unsigned op, const void *ptr, unsigned len); // rt.c

// A stream's buffer as Writes nobody waits for, a chunk each.
static void post(FILE *f)
{
    for (size_t at = 0; at < f->len;) {
        unsigned n = f->len - at > BRAAM_READ_MAX ? BRAAM_READ_MAX : (unsigned)(f->len - at);
        __braam_post(BRAAM_SYS_OP(BRAAM_SYS_WRITE, f->fd), f->buf + at, n);
        at += n;
    }
    f->len = 0;
}

// Every stream's buffer, for an exit that cannot wait: the kernel performs them
// before the process goes.
void __stdio_post(void)
{
    for (FILE *g = opened; g; g = g->next)
        post(g);
    post(stdout);
    post(stderr);
}

// The next chunk into the input buffer: 1, or 0 at the end or on an error.  One byte
// is kept free below the chunk for ungetc.
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
            return EOF; // a second ungetc before a read
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

// fopen's mode as Sys::Open's flags, as Braam's compat layer reads it; 0 for a mode
// that starts with none of r, w and a.
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
    f->len = f->pos = f->end = 0;
    if (f->flags & __F_OPENED) {
        FILE **p = &opened;
        while (*p != f)
            p = &(*p)->next;
        *p = f->next;
        free(f);
    }
    return bad ? EOF : 0;
}

int setvbuf(FILE *f, char *buf, int mode, size_t size)
{
    (void)f, (void)buf, (void)mode, (void)size;
    return 0;
}

void setbuffer(FILE *f, char *buf, int size)
{
    (void)f, (void)buf, (void)size;
}

void setbuf(FILE *f, char *buf)
{
    (void)f, (void)buf;
}

// What was read ahead and not taken goes back to the descriptor's position.
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
