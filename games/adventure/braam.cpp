// The porting layer: printf, getchar, rand and time, none of which exist here.
#include "braam.h"

#include "edit.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/host.h"
#include "proc/io.h"
#include "proc/time.h"

namespace {

// A turn's output. Trivially destructible, so it may be a global.
struct OutBuf {
    char b[8192];
    u32 n;
};

OutBuf out;
bool write_failed;

void put(char c)
{
    if (out.n < sizeof out.b)
        out.b[out.n++] = c;
}

void put_str(const char *s)
{
    while (*s)
        put(*s++);
}

void put_u32(u32 v)
{
    char t[10];
    u32 k = 0;
    do {
        t[k++] = char('0' + v % 10);
        v /= 10;
    } while (v);
    while (k)
        put(t[--k]);
}

void put_i32(int v)
{
    if (v < 0) {
        put('-');
        put_u32(u32(-(v + 1)) + 1);
    } else
        put_u32(u32(v));
}

// With a console the editor runs; a pipe or a file reads stdin instead.
bool have_keys;

// Both outlive every read, so neither sits in a coroutine frame. Built on
// first use and never torn down: the kernel drops the instance.
LineEditor *editor;
Input *input;
LineReader *lines;

// Read when something wants a day: clock_now() is a syscall, datime() is not a
// coroutine.
u64 clock_ms;
i32 clock_tz;

u32 rng_state = 1;

constexpr Str WHO    = "adventure";
constexpr Str PROMPT = "> ";

} // namespace

// Str's const char * constructor calls this, and where the pointer is not a
// literal clang cannot fold it away. Built with -fno-builtin-strlen, or the
// definition becomes a call to itself.
extern "C" usize strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return usize(p - s);
}

void adv_putc(char c)
{
    put(c);
}

void adv_puts(Str s)
{
    for (usize i = 0; i < s.size(); i++)
        put(s[i]);
}

void adv_printf(const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            put(*p);
            continue;
        }
        switch (*++p) {
        case 'd':
            put_i32(__builtin_va_arg(ap, int));
            break;
        case 'u':
            put_u32(__builtin_va_arg(ap, unsigned));
            break;
        case 'c':
            put(char(__builtin_va_arg(ap, int)));
            break;
        case 's':
            // Character by character: Str's const char * constructor reaches
            // for strlen, which is not here.
            put_str(__builtin_va_arg(ap, const char *));
            break;
        case '%':
            put('%');
            break;
        case 0:
            p--; // a trailing %; the loop must not run past it
            break;
        default:
            put('%');
            put(*p);
            break;
        }
    }
    __builtin_va_end(ap);
}

Task<void> adv_flush(void)
{
    if (!out.n)
        co_return;
    Str s = Str(out.b, out.n);
    out.n = 0;
    if (Task<Result<void>> t = write_all(SYS_STDOUT, s)) {
        if ((co_await t).is_err())
            write_failed = true;
    } else
        write_failed = true;
    co_return;
}

bool adv_write_failed(void)
{
    return write_failed;
}

Task<void> adv_input_init(void)
{
    Result<TtyInfo> tty = Err(Error::Unsupported);
    if (Task<Result<TtyInfo>> t = tty_of(SYS_STDIN))
        tty = co_await t;

    if (tty.is_ok() && tty.value().console) {
        // Err(Perm) if somebody already holds them, which a background job does
        // not get past either.
        Result<Geometry> g = Err(Error::Perm);
        if (Task<Result<Geometry>> t = keys_claim(true))
            g = co_await t;
        have_keys = g.is_ok();
    }

    if (have_keys)
        editor = heap_new<LineEditor>();
    else {
        input = heap_new<Input>(Args{}, SYS_STDIN, WHO);
        if (input)
            lines = heap_new<LineReader>(*input);
    }
    co_return;
}

Task<void> adv_input_done(void)
{
    if (have_keys) {
        have_keys = false;
        if (Task<Result<Geometry>> t = keys_claim(false))
            co_await t;
    }
    co_return;
}

Task<AdvEnd> adv_readline(String &out_line)
{
    out_line.clear();

    if (have_keys && editor) {
        Result<Line> r = Err(Error::NoMemory);
        if (Task<Result<Line>> t = editor->read_line(PROMPT))
            r = co_await t;
        if (r.is_err())
            co_return AdvEnd::Eof;
        if (r.value().how == LineEnd::Interrupt)
            co_return AdvEnd::Interrupt;
        if (!out_line.assign(r.value().text.str()))
            co_return AdvEnd::Eof;
        co_return AdvEnd::Enter;
    }

    if (!lines)
        co_return AdvEnd::Eof;
    Result<bool> r = Err(Error::NoMemory);
    if (Task<Result<bool>> t = lines->next(out_line))
        r = co_await t;
    if (r.is_err() || !r.value())
        co_return AdvEnd::Eof;
    co_return AdvEnd::Enter;
}

// Upstream printed this and exited; a trap is what a can't-happen deserves, and
// it lives here because hdr.h makes `panic` an alias for a struct field.
void bug(int n)
{
    Buf<64> b;
    b.put("adventure: fatal bug ").put(n);
    panic(b.str());
}

// xorshift32, where upstream had the C library's rand().
int ran(int range) // uses unix rng
{                  // can't div by 32768 because
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return range > 0 ? int((rng_state >> 1) % u32(range)) : 0;
}

Task<void> adv_clock(void)
{
    Result<Clock> c = Err(Error::Unsupported);
    if (Task<Result<Clock>> t = clock_now())
        c = co_await t;
    if (c.is_ok()) {
        clock_ms = c.value().epoch_ms;
        clock_tz = c.value().tz_min;
    }
    co_return;
}

Task<void> adv_seed(void)
{
    if (Task<void> t = adv_clock())
        co_await t;
    u32 seed  = u32(clock_ms) ^ (proc_pid() << 16);
    rng_state = seed ? seed : 1;
    co_return;
}

// Days before each month, for the tm_yday civil() does not carry.
static const u32 CUM_DAYS[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

void datime(int *d, int *t)
{
    i64 secs  = i64(clock_ms / 1000) + i64(clock_tz) * 60;
    Civil now = civil(secs);

    bool leap = (now.year % 4 == 0 && now.year % 100 != 0) || now.year % 400 == 0;
    u32 yday  = CUM_DAYS[now.month - 1] + now.day - 1;
    if (leap && now.month > 2)
        yday++;

    *d = int(yday) + // day since 1977 (mod leap)
         365 * (now.year - 1977);
    // bug: this will overflow in the year 2066 AD
    // it will be attributed to Wm the C's millenial celebration
    *t = int(now.hour) * 60 + // and minutes since midnite
         int(now.min);        // pretty painless
}
