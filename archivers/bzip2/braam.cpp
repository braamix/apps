#include "braam.h"

#include <stdarg.h>

#include "kernel/fmt.h"

int b2_fatal              = 0;
const char *b2_remove_out = NULL;

static char b2_diag[512];
static int b2_diag_len;
static int b2_diag_pending;

Task<void> b2_flush_diag()
{
    if (!b2_diag_pending)
        co_return;
    b2_diag_pending = 0;
    co_await write_all(SYS_STDERR, Str(b2_diag, (usize)b2_diag_len));
}

static void diag_vfmt(const char *fmt, va_list ap)
{
    b2_diag_len = vsnprintf(b2_diag, sizeof b2_diag, fmt, ap);
    if (b2_diag_len < 0)
        b2_diag_len = 0;
    if ((usize)b2_diag_len >= sizeof b2_diag)
        b2_diag_len = (int)sizeof b2_diag - 1;
    b2_diag_pending = 1;
}

void b2_maybe_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vfmt(fmt, ap);
    va_end(ap);
    int n       = b2_diag_len;
    b2_diag_len = snprintf(b2_diag + n, sizeof b2_diag - (usize)n, ": %s\n", strerror(errno));
    if (b2_diag_len > 0)
        b2_diag_len += n;
    b2_fatal = 1;
    if (exitValue == 0)
        exitValue = 1;
}

void b2_maybe_errx(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vfmt(fmt, ap);
    va_end(ap);
    int n = b2_diag_len;
    if (n < (int)sizeof b2_diag - 1) {
        b2_diag[n]  = '\n';
        b2_diag_len = n + 1;
    }
    b2_fatal = 1;
    if (exitValue == 0)
        exitValue = 1;
}

void b2_set_exit(Int32 v)
{
    if (v > exitValue)
        exitValue = v;
}

Task<void> b2_on_int()
{
    if (b2_remove_out != NULL)
        co_await b_unlink(b2_remove_out);
}

Task<i32> proc_main(Args args)
{
    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    b2_fatal      = 0;
    exitValue     = 0;
    b2_remove_out = NULL;

    int st = co_await bzip2_main(args);

    co_await b2_flush_diag();
    co_await b_fflush(stdout);
    co_await b_fflush(stderr);
    if (st == 130)
        co_return 130;
    if (b2_fatal && exitValue == 0)
        co_return 1;
    if (exitValue != 0)
        co_return exitValue;
    if (st != 0)
        co_return st;
    co_return 0;
}
