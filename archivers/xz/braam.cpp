#include "braam.h"

#include <stdarg.h>

#include "kernel/fmt.h"
#include "main.h"

volatile int user_abort   = 0;
const char *xz_remove_out = NULL;
int xz_fatal              = 0;
int xz_stop               = 0;

static char *xz_diag_buf = NULL;
static usize xz_diag_cap = 0;
static int xz_diag_len;
static int xz_diag_pending;

static bool xz_diag_reserve(usize need)
{
    if (xz_diag_cap >= need)
        return true;
    usize cap = xz_diag_cap ? xz_diag_cap : 512;
    while (cap < need)
        cap *= 2;
    char *p = (char *)realloc(xz_diag_buf, cap);
    if (p == NULL)
        return false;
    xz_diag_buf = p;
    xz_diag_cap = cap;
    return true;
}

Task<void> xz_flush_diag()
{
    if (!xz_diag_pending)
        co_return;
    xz_diag_pending = 0;
    co_await write_all(SYS_STDERR, Str(xz_diag_buf, (usize)xz_diag_len));
    xz_diag_len = 0;
}

void xz_diag_vprint(const char *fmt, va_list ap)
{
    if (!xz_diag_reserve((usize)xz_diag_len + 256))
        return;
    va_list copy;
    va_copy(copy, ap);
    int n = vsnprintf(xz_diag_buf + xz_diag_len, xz_diag_cap - (usize)xz_diag_len, fmt, copy);
    va_end(copy);
    if (n < 0)
        return;
    if ((usize)n >= xz_diag_cap - (usize)xz_diag_len) {
        if (!xz_diag_reserve((usize)xz_diag_len + (usize)n + 2))
            return;
        n = vsnprintf(xz_diag_buf + xz_diag_len, xz_diag_cap - (usize)xz_diag_len, fmt, ap);
        if (n < 0)
            return;
    }
    xz_diag_len += n;
    if ((usize)xz_diag_len + 1 >= xz_diag_cap && !xz_diag_reserve((usize)xz_diag_len + 2))
        return;
    xz_diag_buf[xz_diag_len++] = '\n';
    xz_diag_pending            = 1;
}

void xz_diag_print(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xz_diag_vprint(fmt, ap);
    va_end(ap);
}

static char xz_out[32768];
static int xz_out_len;
static int xz_out_pending;

Task<void> xz_flush_out()
{
    if (!xz_out_pending)
        co_return;
    xz_out_pending = 0;
    co_await write_all(SYS_STDOUT, Str(xz_out, (usize)xz_out_len));
    xz_out_len = 0;
}

void xz_out_print(const char *fmt, ...)
{
    if (xz_out_len >= (int)sizeof xz_out - 2)
        return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(xz_out + xz_out_len, sizeof xz_out - (usize)xz_out_len, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if ((usize)n >= sizeof xz_out - (usize)xz_out_len)
        n = (int)sizeof xz_out - xz_out_len - 1;
    xz_out_len += n;
    xz_out_pending = 1;
}

static void diag_vfmt(const char *fmt, va_list ap)
{
    if (!xz_diag_reserve(512))
        return;
    xz_diag_len = vsnprintf(xz_diag_buf, xz_diag_cap, fmt, ap);
    if (xz_diag_len < 0)
        xz_diag_len = 0;
    if ((usize)xz_diag_len >= xz_diag_cap)
        xz_diag_len = (int)xz_diag_cap - 1;
    xz_diag_pending = 1;
}

void xz_maybe_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vfmt(fmt, ap);
    va_end(ap);
    int n = xz_diag_len;
    if (!xz_diag_reserve((usize)n + 64))
        return;
    xz_diag_len = snprintf(xz_diag_buf + n, xz_diag_cap - (usize)n, ": %s\n", strerror(errno));
    if (xz_diag_len > 0)
        xz_diag_len += n;
    xz_fatal = 1;
    if (exit_status == E_SUCCESS)
        exit_status = E_ERROR;
}

void xz_maybe_errx(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vfmt(fmt, ap);
    va_end(ap);
    int n = xz_diag_len;
    if ((usize)n + 1 < xz_diag_cap) {
        xz_diag_buf[n] = '\n';
        xz_diag_len    = n + 1;
    }
    xz_fatal = 1;
    if (exit_status == E_SUCCESS)
        exit_status = E_ERROR;
}

Task<void> xz_on_int()
{
    if (xz_remove_out != NULL)
        co_await b_unlink(xz_remove_out);
}

Task<ssize_t> xz_read_retry(int fd, void *buf, size_t sz)
{
    co_await xz_flush_diag();
    if (xz_fatal)
        co_return -1;

    char *cp    = (char *)buf;
    size_t left = sz;
    while (left > 0) {
        if (sig_take(SIG_INT)) {
            user_abort = 1;
            co_await xz_on_int();
            errno = EINTR;
            co_return -1;
        }
        ssize_t ret = co_await b_read(fd, cp, left);
        if (ret < 0)
            co_return ret;
        if (ret == 0)
            break;
        cp += ret;
        left -= (size_t)ret;
    }
    co_return (ssize_t)(sz - left);
}

Task<ssize_t> xz_write_retry(int fd, const void *buf, size_t sz)
{
    co_await xz_flush_diag();
    if (xz_fatal)
        co_return -1;

    const char *cp = (const char *)buf;
    size_t left    = sz;
    while (left > 0) {
        if (sig_take(SIG_INT)) {
            user_abort = 1;
            co_await xz_on_int();
            errno = EINTR;
            co_return -1;
        }
        ssize_t ret = co_await b_write(fd, cp, left);
        if (ret < 0)
            co_return ret;
        if (ret == 0)
            break;
        cp += ret;
        left -= (size_t)ret;
    }
    co_return (ssize_t)(sz - left);
}

Task<off_t> xz_lseek_retry(int fd, off_t off, int whence)
{
    co_return co_await b_lseek(fd, off, whence);
}

void tuklib_exit(int status, int err_status, bool show_msg)
{
    (void)err_status;
    (void)show_msg;
    exit_status = (enum exit_status_type)status;
    xz_stop     = 1;
    if (status != E_SUCCESS)
        xz_fatal = 1;
}

Task<i32> proc_main(Args args)
{
    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    user_abort    = 0;
    xz_fatal      = 0;
    xz_stop       = 0;
    xz_remove_out = NULL;
    exit_status   = E_SUCCESS;

    int st = co_await xz_main(args);

    co_await xz_flush_diag();
    co_await xz_flush_out();
    co_await b_fflush(stdout);
    co_await b_fflush(stderr);
    if (st == 130)
        co_return 130;
    if (xz_fatal && st == 0)
        co_return 1;
    if (st != 0)
        co_return st;
    co_return 0;
}
