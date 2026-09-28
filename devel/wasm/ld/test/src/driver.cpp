// The Braam half of every fixture: proc_main runs fx_main, then writes what it
// collected. Plain functions, so constructors can print before proc_main runs.
#include "fixture.h"
#include "proc/io.h"
#include "proc/rt.h"

namespace {

char out[8192];
usize len;

} // namespace

extern "C" void fx_puts(const char *s)
{
    while (*s && len < sizeof out)
        out[len++] = *s++;
}

extern "C" void fx_putn(long n)
{
    char digits[16];
    int i = 0;
    unsigned long u = n < 0 ? 0ul - (unsigned long)n : (unsigned long)n;
    do
        digits[i++] = char('0' + u % 10);
    while (u /= 10);
    if (n < 0)
        fx_puts("-");
    char s[2] = {0, 0};
    while (i > 0) {
        s[0] = digits[--i];
        fx_puts(s);
    }
}

extern "C" unsigned fx_pid(void)
{
    return proc_pid();
}

Task<i32> proc_main(Args)
{
    fx_main();
    auto r = co_await write_all(SYS_STDOUT, Str(out, len));
    co_return r.is_err() ? 1 : 0;
}
