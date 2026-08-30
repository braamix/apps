/*
 * DureMark Porting Layer Implementation
 * Platform-specific implementations for timing and I/O
 *
 * Braam: the clock is Sys::Now, monotonic and counted in whole milliseconds,
 * so a tick is a millisecond. printf writes to a stream and a write is a
 * syscall a coroutine has to await, so du_printf formats into a buffer with
 * the port kit's vsnprintf and du_flush sends it.
 */
#include "duremark.h"
#include "kernel/fmt.h"

#include <stdio.h>

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

/* One report line is the longest thing DureMark formats. */
void du_printf(const char *fmt, ...)
{
    char line[512];

    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    __builtin_va_end(ap);

    if (n < 0)
        return;
    /* n is what the whole conversion wanted, not what fitted. */
    usize len = usize(n) < sizeof line ? usize(n) : sizeof line - 1;
    out_buf.put(Str(line, len));
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
