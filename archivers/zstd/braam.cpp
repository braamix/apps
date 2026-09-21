#include "braam.h"

#include "kernel/fmt.h"

const char *g_artefact = NULL;

static int g_status;
static bool g_fatal;
static bool g_int;

bool zstd_interrupted()
{
    if (sig_take(SIG_INT)) {
        g_int = true;
        // Sticky, and fatal with it: a loop that tests zstd_fatal() is the same
        // loop that has to end on a ^C, and there is nothing to unwind with.
        g_fatal = true;
    }
    return g_int;
}

void zstd_fail(int code)
{
    g_fatal = true;
    if (g_status == 0)
        g_status = code;
}

bool zstd_fatal()
{
    return g_fatal;
}

int zstd_status()
{
    return g_status;
}

// ------------------------------------------------------------------ display

// It has to grow rather than truncate: --help is twelve kilobytes written by a
// hundred DISPLAYOUT calls with no co_await among them, so there is no point
// between the first and the last at which anything could be drained.
struct Sink {
    char *buf;
    usize len;
    usize cap;
};

static Sink g_out;
static Sink g_err;

static bool reserve(Sink *s, usize want)
{
    if (s->cap >= want)
        return true;
    usize cap = s->cap ? s->cap : 4096;
    while (cap < want)
        cap *= 2;
    char *p = (char *)realloc(s->buf, cap);
    if (p == NULL)
        return false;
    s->buf = p;
    s->cap = cap;
    return true;
}

static void append(Sink *s, const char *fmt, va_list ap)
{
    // One line usually fits in what is already there; a longer one is measured
    // by the first vsnprintf and formatted again into room made for it.
    if (!reserve(s, s->len + 256))
        return;
    va_list copy;
    va_copy(copy, ap);
    int n = vsnprintf(s->buf + s->len, s->cap - s->len, fmt, copy);
    va_end(copy);
    if (n < 0)
        return;
    if ((usize)n < s->cap - s->len) {
        s->len += (usize)n;
        return;
    }
    if (!reserve(s, s->len + (usize)n + 1))
        return;
    n = vsnprintf(s->buf + s->len, s->cap - s->len, fmt, ap);
    if (n > 0)
        s->len += (usize)n;
}

void zstd_display(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    append(f == stdout ? &g_out : &g_err, fmt, ap);
    va_end(ap);
}

Task<void> zstd_flush(void)
{
    // stdout goes through stdio's own buffer, so a --list listing and a -c
    // payload keep their order; stderr is unbuffered and written straight out.
    if (g_out.len) {
        usize n   = g_out.len;
        g_out.len = 0;
        co_await b_fwrite(g_out.buf, 1, n, stdout);
    }
    if (g_err.len) {
        usize n   = g_err.len;
        g_err.len = 0;
        co_await write_all(SYS_STDERR, Str(g_err.buf, n));
    }
}

// ------------------------------------------------------------------ streams

Task<size_t> zstd_fread(void *p, size_t size, size_t n, FILE *f)
{
    co_await zstd_flush();
    if (zstd_fatal()) {
        errno = EIO;
        co_return 0;
    }
    if (zstd_interrupted()) {
        errno = EINTR;
        co_return 0;
    }
    co_return co_await b_fread(p, size, n, f);
}

Task<size_t> zstd_fwrite(const void *p, size_t size, size_t n, FILE *f)
{
    co_await zstd_flush();
    if (zstd_fatal()) {
        errno = EIO;
        co_return 0;
    }
    if (zstd_interrupted()) {
        errno = EINTR;
        co_return 0;
    }
    co_return co_await b_fwrite(p, size, n, f);
}

// ------------------------------------------------------------------ process

Task<i32> proc_main(Args args)
{
    co_await sig_catch(SIG_INT);

    g_artefact = NULL;
    g_status   = 0;
    g_fatal    = false;
    g_int      = false;

    i32 st = co_await zstd_main(args);

    // Upstream's INThandler removed the file it was writing and exited 2.
    if (g_int) {
        if (g_artefact != NULL)
            co_await b_unlink(g_artefact);
        zstd_display(stderr, "\n");
        st = 2;
    }

    co_await zstd_flush();
    co_await b_fflush(stdout);
    co_await b_fflush(stderr);

    if (st == 0 && g_status != 0)
        st = g_status;
    co_return st;
}
