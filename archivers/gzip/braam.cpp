#include "braam.h"

#include "kernel/fmt.h"

#include <limits.h>

#ifndef SSIZE_MAX
#define SSIZE_MAX ((ssize_t)(SIZE_MAX / 2))
#endif
#include <stdarg.h>

int cflag, dflag, lflag, numflag = 6, fflag, kflag, nflag, Nflag, qflag, rflag, tflag, vflag;
int exit_value = 0;
const char *remove_file = NULL;
const char *infile = NULL;
off_t infile_total, infile_current;
int gzip_fatal = 0;

static char progname_buf[256];
static time_t gzip_boot_time;

static char gzip_diag[512];
static int gzip_diag_len;
static int gzip_diag_pending;

const char *
gzip_progname(Str argv0)
{
    if (argv0.size() == 0)
        return "gzip";
    Str leaf = path_basename(argv0);
    usize n = leaf.size();
    if (n >= sizeof progname_buf)
        n = sizeof progname_buf - 1;
    memcpy(progname_buf, leaf.data(), n);
    progname_buf[n] = '\0';
    return progname_buf;
}

void
gzip_clear_fatal()
{
    gzip_fatal = 0;
}

void
gzip_set_time(time_t t)
{
    gzip_boot_time = t;
}

time_t
ztime(time_t *t)
{
    if (t)
        *t = gzip_boot_time;
    return gzip_boot_time;
}

Task<void>
gzip_flush_diag()
{
    if (!gzip_diag_pending)
        co_return;
    gzip_diag_pending = 0;
    co_await write_all(SYS_STDERR, Str(gzip_diag, (usize)gzip_diag_len));
}

void
infile_newdata(size_t newdata)
{
    infile_current += (off_t)newdata;
}

void
infile_set(const char *newinfile, off_t total)
{
    if (newinfile)
        infile = newinfile;
    infile_total = total;
}

void
infile_clear(void)
{
    infile = NULL;
    infile_total = infile_current = 0;
}

Task<void>
gzip_on_int()
{
    if (remove_file != NULL)
        co_await b_unlink(remove_file);
}

Task<ssize_t>
read_retry(int fd, void *buf, size_t sz)
{
    co_await gzip_flush_diag();
    if (gzip_fatal)
        co_return -1;

    char *cp = (char *)buf;
    size_t left = sz;
    if (left > (size_t)SSIZE_MAX)
        left = (size_t)SSIZE_MAX;

    while (left > 0) {
        if (sig_take(SIG_INT)) {
            co_await gzip_on_int();
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

Task<ssize_t>
write_retry(int fd, const void *buf, size_t sz)
{
    co_await gzip_flush_diag();
    if (gzip_fatal)
        co_return -1;

    const char *cp = (const char *)buf;
    size_t left = sz;
    if (left > (size_t)SSIZE_MAX)
        left = (size_t)SSIZE_MAX;

    while (left > 0) {
        if (sig_take(SIG_INT)) {
            co_await gzip_on_int();
            errno = EINTR;
            co_return -1;
        }
        ssize_t ret = co_await b_write(fd, cp, left);
        if (ret < 0)
            co_return ret;
        if (ret == 0) {
            exit_value = 2;
            gzip_fatal = 1;
            co_return -1;
        }
        cp += ret;
        left -= (size_t)ret;
    }
    co_return (ssize_t)sz;
}

Task<ssize_t>
gzip_pread(int fd, void *buf, size_t sz, off_t pos)
{
    if (co_await b_lseek(fd, pos, SEEK_SET) < 0)
        co_return -1;
    co_return co_await read_retry(fd, buf, sz);
}

static void
diag_vfmt(const char *fmt, va_list ap)
{
    gzip_diag_len = vsnprintf(gzip_diag, sizeof gzip_diag, fmt, ap);
    if (gzip_diag_len < 0)
        gzip_diag_len = 0;
    if ((usize)gzip_diag_len >= sizeof gzip_diag)
        gzip_diag_len = (int)sizeof gzip_diag - 1;
    gzip_diag_pending = 1;
}

void
maybe_warn(const char *fmt, ...)
{
    if (qflag == 0) {
        va_list ap;
        va_start(ap, fmt);
        diag_vfmt(fmt, ap);
        va_end(ap);
        int n = gzip_diag_len;
        gzip_diag_len = snprintf(gzip_diag + n, sizeof gzip_diag - (usize)n, ": %s\n",
                                 strerror(errno));
        if (gzip_diag_len > 0)
            gzip_diag_len += n;
    }
    if (exit_value == 0)
        exit_value = 1;
}

void
maybe_warnx(const char *fmt, ...)
{
    if (qflag == 0) {
        va_list ap;
        va_start(ap, fmt);
        diag_vfmt(fmt, ap);
        va_end(ap);
        int n = gzip_diag_len;
        if (n < (int)sizeof gzip_diag - 1) {
            gzip_diag[n] = '\n';
            gzip_diag_len = n + 1;
        }
    }
    if (exit_value == 0)
        exit_value = 1;
}

void
maybe_err(const char *fmt, ...)
{
    if (qflag == 0) {
        va_list ap;
        va_start(ap, fmt);
        diag_vfmt(fmt, ap);
        va_end(ap);
        int n = gzip_diag_len;
        gzip_diag_len = snprintf(gzip_diag + n, sizeof gzip_diag - (usize)n, ": %s\n",
                                 strerror(errno));
        if (gzip_diag_len > 0)
            gzip_diag_len += n;
    }
    exit_value = 2;
    gzip_fatal = 1;
}

void
maybe_errx(const char *fmt, ...)
{
    if (qflag == 0) {
        va_list ap;
        va_start(ap, fmt);
        diag_vfmt(fmt, ap);
        va_end(ap);
        int n = gzip_diag_len;
        if (n < (int)sizeof gzip_diag - 1) {
            gzip_diag[n] = '\n';
            gzip_diag_len = n + 1;
        }
    }
    exit_value = 2;
    gzip_fatal = 1;
}

Task<i32> proc_main(Args args)
{
    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    gzip_clear_fatal();
    exit_value = 0;

    int st = co_await gzip_main(args);

    co_await gzip_flush_diag();
    co_await b_fflush(stdout);
    co_await b_fflush(stderr);
    if (st == 130)
        co_return 130;
    if (exit_value == 2)
        co_return 2;
    if (st != 0)
        co_return st;
    co_return exit_value;
}
