/*
 * DureMark Porting Layer Implementation
 * Platform-specific implementations for timing and I/O
 *
 * Braam: the clock is Sys::Now, monotonic and counted in whole milliseconds,
 * so a tick is a millisecond. printf does not exist, so the conversions
 * DureMark uses are written out here; %f is braam::math's.
 */
#include "duremark.h"
#include "kernel/fmt.h"
#include "math/ftoa.h"

/* Timing variables */
static u32 start_ms;

void du_start_time(void)
{
    start_ms = proc_now();
}

du_ticks_t du_get_time(void)
{
    return (du_ticks_t)(proc_now() - start_ms);
}

double du_get_sec_per_tick(void)
{
    return 1.0 / 1000.0;
}

namespace {

/* One report's worth of text. Trivially destructible, so it may be a global. */
Buf<4096> out_buf;

/* A failed write, as the status to exit with: 130 for a ^C, 1 otherwise. */
i32 write_status;

} // namespace

/* The conversions DureMark uses, and no more: %lu, %.1f, %.2f, %%. %d, %u, %s
   and %c are here because a format is easier to read than a special case. */
void du_printf(const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    for (const char *p = fmt; *p != 0; p++) {
        if (*p != '%') {
            out_buf.put(*p);
            continue;
        }
        p++;
        if (*p == '%' || *p == 0) {
            out_buf.put('%');
            continue;
        }

        /* Precision, as in %.1f — the only width DureMark asks for. None is
           printf's default, which fmt_f64 spells -1. */
        i32 decimals = -1;
        if (*p == '.') {
            p++;
            decimals = 0;
            while (*p >= '0' && *p <= '9')
                decimals = decimals * 10 + i32(*p++ - '0');
        }

        /* Length, as in %lu. wasm32 is LP32, so long is what int is. */
        bool is_long = false;
        while (*p == 'l') {
            is_long = true;
            p++;
        }

        switch (*p) {
        case 'd':
            out_buf.put(is_long ? int(__builtin_va_arg(ap, long)) : __builtin_va_arg(ap, int));
            break;
        case 'u':
            out_buf.put(is_long ? u32(__builtin_va_arg(ap, unsigned long))
                                : __builtin_va_arg(ap, unsigned));
            break;
        case 'f':
            put_f64(out_buf, __builtin_va_arg(ap, double), decimals, 'f');
            break;
        case 's': {
            /* Character by character: Str's const char * constructor reaches
               for strlen, which does not exist here. */
            const char *s = __builtin_va_arg(ap, const char *);
            while (*s != 0)
                out_buf.put(*s++);
            break;
        }
        case 'c':
            out_buf.put(char(__builtin_va_arg(ap, int)));
            break;
        default:
            out_buf.put('%').put(*p);
            break;
        }
    }

    __builtin_va_end(ap);
}

Task<void> du_flush(void)
{
    if (out_buf.str().empty())
        co_return;

    Result<void> r = co_await write_all(SYS_STDOUT, out_buf.str());
    out_buf.clear();
    if (r.is_err() && !write_status)
        write_status = r.error() == Error::Cancelled ? 130 : 1;
    co_return;
}

Task<i32> proc_main(Args)
{
    i32 rc = co_await du_main();

    if (Task<void> t = du_flush())
        co_await t;
    co_return write_status ? write_status : rc;
}
