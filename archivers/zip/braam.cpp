#include "braam.h"

#include "kernel/alloc.h"
#include "proc/rt.h"
#include "proc/time.h"
#include "zip.h"

// -------------------------------------------------------- what the kit leaves

// "%4d-%2d-%2d" or "%2d%2d%4d", which is what -t and -tt accept.
int zparse_date(const char *s, int *yyyy, int *mm, int *dd)
{
    auto num = [](const char *p, int n, int &out) {
        out = 0;
        for (int i = 0; i < n; i++) {
            if (p[i] < '0' || p[i] > '9')
                return false;
            out = out * 10 + (p[i] - '0');
        }
        return true;
    };

    if (strlen(s) >= 10 && s[4] == '-' && s[7] == '-') {
        if (num(s, 4, *yyyy) && num(s + 5, 2, *mm) && num(s + 8, 2, *dd))
            return 3;
    }
    if (strlen(s) >= 8) {
        if (num(s, 2, *mm) && num(s + 2, 2, *dd) && num(s + 4, 4, *yyyy))
            return 3;
    }
    return 0;
}

// ------------------------------------------------------------------- stdio

FILE *zstdout = nullptr;
FILE *zstderr = nullptr;
char zfmtbuf[ZFMT_MAX];
char zputbuf = 0;
char zgetbuf = 0;

namespace {

// Digits of `v` in `base`, into `out`, at least `pad` of them.
usize put_num(char *out, usize cap, u64 v, unsigned base, int pad, bool upper)
{
    char tmp[24];
    usize n = 0;
    do {
        unsigned d = (unsigned)(v % base);
        tmp[n++]   = (char)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10);
        v /= base;
    } while (v);
    while (n < (usize)pad && n < sizeof(tmp))
        tmp[n++] = '0';

    usize k = 0;
    while (n > 0 && k < cap)
        out[k++] = tmp[--n];
    return k;
}

} // namespace

usize zvformat(char *out, usize cap, const char *fmt, __builtin_va_list ap)
{
    usize n = 0;
    if (cap == 0)
        return 0;
    cap--; // room for the terminator

    for (const char *p = fmt; *p && n < cap; p++) {
        if (*p != '%') {
            out[n++] = *p;
            continue;
        }
        p++;
        if (*p == '%') {
            out[n++] = '%';
            continue;
        }

        bool left = false, zero = false;
        for (;; p++) {
            if (*p == '-')
                left = true;
            else if (*p == '0')
                zero = true;
            else
                break;
        }

        int width = 0;
        while (*p >= '0' && *p <= '9')
            width = width * 10 + (*p++ - '0');

        // A precision is parsed and ignored: upstream uses it only on %s, and
        // never to truncate.
        if (*p == '.') {
            p++;
            while (*p >= '0' && *p <= '9')
                p++;
        }

        int longs = 0;
        while (*p == 'l' || *p == 'h') {
            if (*p == 'l')
                longs++;
            p++;
        }
        if (*p == 'z' || *p == 'j' || *p == 't') {
            longs = 1;
            p++;
        }

        char body[24];
        usize len        = 0;
        const char *text = body;

        switch (*p) {
        case 'd':
        case 'i': {
            i64 v = longs >= 2 ? __builtin_va_arg(ap, long long)
                    : longs    ? (long long)__builtin_va_arg(ap, long)
                               : (long long)__builtin_va_arg(ap, int);
            if (v < 0) {
                body[len++] = '-';
                len += put_num(body + len, sizeof(body) - len, (u64)(-v), 10, 0, false);
            } else {
                len = put_num(body, sizeof(body), (u64)v, 10, 0, false);
            }
            break;
        }
        case 'u': {
            u64 v = longs >= 2 ? __builtin_va_arg(ap, unsigned long long)
                    : longs    ? (unsigned long long)__builtin_va_arg(ap, unsigned long)
                               : (unsigned long long)__builtin_va_arg(ap, unsigned);
            len   = put_num(body, sizeof(body), v, 10, 0, false);
            break;
        }
        case 'o': {
            u64 v = longs ? (u64) __builtin_va_arg(ap, unsigned long)
                          : (u64) __builtin_va_arg(ap, unsigned);
            len   = put_num(body, sizeof(body), v, 8, 0, false);
            break;
        }
        case 'x':
        case 'X': {
            u64 v = longs >= 2 ? __builtin_va_arg(ap, unsigned long long)
                    : longs    ? (unsigned long long)__builtin_va_arg(ap, unsigned long)
                               : (unsigned long long)__builtin_va_arg(ap, unsigned);
            len   = put_num(body, sizeof(body), v, 16, zero ? width : 0, *p == 'X');
            if (zero)
                width = 0;
            break;
        }
        case 'c':
            body[0] = (char)__builtin_va_arg(ap, int);
            len     = 1;
            break;
        case 's': {
            const char *s = __builtin_va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            text = s;
            len  = strlen(s);
            break;
        }
        default:
            // An unknown conversion is printed as it was written.
            out[n++] = '%';
            if (*p && n < cap)
                out[n++] = *p;
            continue;
        }

        usize pad = (usize)width > len ? (usize)width - len : 0;
        if (!left)
            for (; pad > 0 && n < cap; pad--)
                out[n++] = zero ? '0' : ' ';
        for (usize i = 0; i < len && n < cap; i++)
            out[n++] = text[i];
        if (left)
            for (; pad > 0 && n < cap; pad--)
                out[n++] = ' ';
    }

    out[n] = 0;
    return n;
}

FileWrite zfprintf(FILE *f, const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    usize n = zvformat(zfmtbuf, sizeof(zfmtbuf), fmt, ap);
    __builtin_va_end(ap);
    return f->write(Str(zfmtbuf, n));
}

int zsprintf(char *out, const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    usize n = zvformat(out, ZFMT_MAX, fmt, ap);
    __builtin_va_end(ap);
    return (int)n;
}

Task<usize> zfread(void *buf, usize size, usize count, FILE *f)
{
    usize want = size * count;
    usize got  = 0;
    while (got < want) {
        Result<usize> r = co_await f->read(Span<char>((char *)buf + got, want - got));
        if (r.is_err())
            break; // Closed is the end of input, and not a failure
        if (r.value() == 0)
            break;
        got += r.value();
    }
    co_return size ? got / size : 0;
}

Task<int> zfputs_nl(const char *s)
{
    if ((co_await zstdout->write(Str(s, strlen(s)))).is_err())
        co_return EOF_;
    co_await zfputc('\n', zstdout);
    co_return 0;
}

Task<Result<void>> zfflush(FILE *f)
{
    Result<void> r = Err(Error::NoMemory);
    if (Task<Result<void>> t = f->flush())
        r = co_await t;
    co_return r;
}

Task<bool> zgets(char *buf, usize cap, FILE *f)
{
    String line;
    bool got = false;
    if (Task<Result<bool>> t = f->getline(line, false))
        if (Result<bool> r = co_await t; r.is_ok())
            got = r.value();
    if (!got) {
        buf[0] = 0;
        co_return false;
    }

    usize n = line.size() < cap - 2 ? line.size() : cap - 2;
    memcpy(buf, line.data(), n);
    buf[n]     = '\n'; // getline() stripped it and upstream tests for it
    buf[n + 1] = 0;
    co_return true;
}

Task<FILE *> zfopen(const char *path, const char *mode)
{
    u32 flags    = SYS_O_READ;
    FileMode how = FileMode::Read;
    if (mode[0] == 'w') {
        flags = SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC;
        how   = FileMode::Write;
    } else if (mode[0] == 'a') {
        flags = SYS_O_WRITE | SYS_O_CREATE | SYS_O_APPEND;
        how   = FileMode::Append;
    }
    if (strchr(mode, '+')) {
        flags |= SYS_O_READ | SYS_O_WRITE;
        how = FileMode::Update;
    }

    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_at(Str(path, strlen(path)), flags))
        fd = co_await t;
    if (fd.is_err())
        co_return nullptr;

    FILE *f = heap_new<File>(File::of((u32)fd.value(), how));
    if (f == nullptr) {
        if (Task<void> t = close_fd((u32)fd.value()))
            co_await t;
        co_return nullptr;
    }
    f->set_buffering(Buffering::Full);
    f->reserve(SYS_READ_MAX);
    co_return f;
}

Task<int> zfclose(FILE *f)
{
    if (f == nullptr || f == zstdout || f == zstderr)
        co_return 0;

    Result<void> r = Err(Error::NoMemory);
    if (Task<Result<void>> t = f->flush())
        r = co_await t;

    u32 fd = f->fd();
    if (Task<void> t = close_fd(fd))
        co_await t;
    heap_delete(f);
    co_return r.is_ok() ? 0 : EOF_;
}

Task<i32> zfseeko(FILE *f, i64 off, int whence)
{
    // fseek() clears the end-of-file indicator, and a failed one does not
    // leave the stream unusable. A File's error is sticky and a seek does not
    // touch it, so both are said outright — upstream seeks past the start of
    // a short archive on purpose and reads on when that fails.
    f->clear_err();

    Result<u64> r = Err(Error::NoMemory);
    if (Task<Result<u64>> t = f->seek(off, (u32)whence))
        r = co_await t;
    if (r.is_err()) {
        f->clear_err();
        co_return -1;
    }
    co_return 0;
}

Task<i64> zftello(FILE *f)
{
    Result<u64> r = Err(Error::NoMemory);
    if (Task<Result<u64>> t = f->seek(0, SYS_SEEK_CUR))
        r = co_await t;
    co_return r.is_ok() ? (i64)r.value() : -1;
}

// -------------------------------------------------------------- the calendar

i32 ztz_min     = 0;
i64 zstart_time = 0;

int zargv(Args argv, char ***out)
{
    // Every main calls this first, so it is where the streams are named. The
    // runtime flushes stdout and stderr after proc_main returns.
    zstdout = &File::stdout();
    zstderr = &File::stderr();
    if (mesg == nullptr)
        mesg = zstdout;

    int n    = (int)argv.size();
    char **a = (char **)malloc((n + 2) * sizeof(char *));
    if (a == nullptr)
        return 0;
    for (int i = 0; i < n; i++) {
        if ((a[i] = (char *)malloc(argv[i].size() + 1)) == nullptr)
            return 0;
        memcpy(a[i], argv[i].data(), argv[i].size());
        a[i][argv[i].size()] = 0;
    }
    a[n] = nullptr;
    *out = a;
    return n;
}

Task<void> zclock_init()
{
    Result<Clock> c = Err(Error::NoMemory);
    if (Task<Result<Clock>> t = clock_now())
        c = co_await t;
    if (c.is_ok()) {
        ztz_min     = c.value().tz_min;
        zstart_time = (i64)(c.value().epoch_ms / 1000);
    }
}

extern "C" time_t time(time_t *t)
{
    time_t now = zstart_time + (time_t)(proc_now() / 1000);
    if (t)
        *t = now;
    return now;
}

u32 zdostime(u64 mtime_ms)
{
    if (mtime_ms == 0)
        return DOSTIME_MINIMUM_;

    Civil t = civil((i64)(mtime_ms / 1000) + ztz_min * 60);
    if (t.year < 1980)
        return DOSTIME_MINIMUM_;

    return ((u32)(t.year - 1980) << 25) | (t.month << 21) | (t.day << 16) | (t.hour << 11) |
           (t.min << 5) | (t.sec >> 1);
}

// Howard Hinnant's days_from_civil, the inverse of proc/time.h's civil().
static i64 days_from_civil(i32 y, u32 m, u32 d)
{
    y -= m <= 2;
    const i64 era = (y >= 0 ? y : y - 399) / 400;
    const u32 yoe = (u32)(y - era * 400);                           // [0, 399]
    const u32 doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    const u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          // [0, 146096]
    return era * 146097 + (i64)doe - 719468;
}

i64 zdos2unix(u32 dostime)
{
    i32 year = (i32)((dostime >> 25) & 0x7f) + 1980;
    u32 mon  = (dostime >> 21) & 0x0f;
    u32 day  = (dostime >> 16) & 0x1f;
    if (mon < 1 || mon > 12 || day < 1)
        return 0;

    i64 secs = days_from_civil(year, mon, day) * 86400 + ((dostime >> 11) & 0x1f) * 3600 +
               ((dostime >> 5) & 0x3f) * 60 + (dostime & 0x1f) * 2;
    return secs - (i64)ztz_min * 60;
}

// ------------------------------------------- what unix/zipup.h did: raw reads

int zread_failed = 0;

Task<ftype> zopen(const char *n, int how)
{
    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_read(Str(n, strlen(n))))
        fd = co_await t;
    co_return fd.is_ok() ? (ftype)fd.value() : fbad;
}

Task<unsigned> zread(ftype f, char *buf, unsigned len)
{
    if (len > SYS_READ_MAX)
        len = SYS_READ_MAX;

    Result<String> r = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_some((u32)f, len))
        r = co_await t;
    // This is where a long run parks, so this is where a ^C reaches it.
    // Upstream installed a handler that called ziperr(ZE_ABORT). There is no
    // handler here: a delivered signal abandons the parked call with
    // Err(Intr), or is simply pending if the call had already answered — so
    // it is sig_take() that says one arrived, not the error. The entry loop
    // unwinds on zip_fatal.
    if (sig_take(SIG_INT))
        zip_fail(ZE_ABORT, "aborting");

    if (r.is_err()) {
        if (r.error() == Error::Cancelled)
            zip_fail(ZE_ABORT, "aborting");
        else if (r.error() != Error::Closed && r.error() != Error::Intr)
            zread_failed = 1;
        co_return 0; // Closed is the end of input, not a failure
    }
    memcpy(buf, r.value().data(), r.value().size());
    co_return (unsigned) r.value().size();
}

Task<void> zclose(ftype f)
{
    if (f >= 0)
        if (Task<void> t = close_fd((u32)f))
            co_await t;
}
