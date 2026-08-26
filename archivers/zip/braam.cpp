#include "braam.h"

#include "kernel/alloc.h"
#include "proc/rt.h"
#include "proc/time.h"
#include "zip.h"

// ------------------------------------------------------------------- the heap
//
// heap_alloc has no realloc and does not remember a block's requested size, so
// malloc carries an eight-byte header. Eight rather than four keeps the u64 and
// f64 alignment the payload may want.

namespace {

struct Head {
    u32 cap; // bytes of payload the block holds
    u32 pad;
};

} // namespace

extern "C" void *malloc(usize n)
{
    if (n == 0)
        n = 1;
    Head *h = (Head *)heap_alloc(n + sizeof(Head));
    if (!h)
        return nullptr;
    h->cap = (u32)n;
    h->pad = 0;
    return h + 1;
}

extern "C" void free(void *p)
{
    if (p)
        heap_free((Head *)p - 1);
}

extern "C" void *calloc(usize n, usize size)
{
    usize want = n * size;
    void *p    = malloc(want);
    if (p)
        memset(p, 0, want);
    return p;
}

extern "C" void *realloc(void *p, usize n)
{
    if (!p)
        return malloc(n);
    if (n == 0) {
        free(p);
        return nullptr;
    }

    Head *h = (Head *)p - 1;
    if (n <= h->cap)
        return p; // the block already holds it; keep the capacity

    void *q = malloc(n);
    if (!q)
        return nullptr;
    memcpy(q, p, h->cap);
    free(p);
    return q;
}

// ----------------------------------------------------------------- mem*, str*

extern "C" void *memcpy(void *d, const void *s, usize n)
{
    char *dp       = (char *)d;
    const char *sp = (const char *)s;
    for (usize i = 0; i < n; i++)
        dp[i] = sp[i];
    return d;
}

extern "C" void *memmove(void *d, const void *s, usize n)
{
    char *dp       = (char *)d;
    const char *sp = (const char *)s;
    if (dp < sp) {
        for (usize i = 0; i < n; i++)
            dp[i] = sp[i];
    } else if (dp > sp) {
        for (usize i = n; i > 0; i--)
            dp[i - 1] = sp[i - 1];
    }
    return d;
}

extern "C" void *memset(void *d, int c, usize n)
{
    char *dp = (char *)d;
    for (usize i = 0; i < n; i++)
        dp[i] = (char)c;
    return d;
}

extern "C" int memcmp(const void *a, const void *b, usize n)
{
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    for (usize i = 0; i < n; i++)
        if (x[i] != y[i])
            return x[i] < y[i] ? -1 : 1;
    return 0;
}

extern "C" usize strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return usize(p - s);
}

extern "C" char *strcpy(char *d, const char *s)
{
    char *r = d;
    while ((*d++ = *s++) != 0)
        ;
    return r;
}

extern "C" char *strncpy(char *d, const char *s, usize n)
{
    usize i = 0;
    for (; i < n && s[i]; i++)
        d[i] = s[i];
    for (; i < n; i++)
        d[i] = 0;
    return d;
}

extern "C" char *strcat(char *d, const char *s)
{
    strcpy(d + strlen(d), s);
    return d;
}

extern "C" char *strncat(char *d, const char *s, usize n)
{
    char *e = d + strlen(d);
    usize i = 0;
    for (; i < n && s[i]; i++)
        e[i] = s[i];
    e[i] = 0;
    return d;
}

extern "C" int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

extern "C" int strncmp(const char *a, const char *b, usize n)
{
    for (usize i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (!a[i])
            break;
    }
    return 0;
}

extern "C" char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c)
            return (char *)s;
        if (!*s)
            return nullptr;
    }
}

extern "C" char *strrchr(const char *s, int c)
{
    const char *found = nullptr;
    for (;; s++) {
        if (*s == (char)c)
            found = s;
        if (!*s)
            return (char *)found;
    }
}

extern "C" char *strstr(const char *h, const char *n)
{
    if (!*n)
        return (char *)h;
    usize ln = strlen(n);
    for (; *h; h++)
        if (*h == *n && strncmp(h, n, ln) == 0)
            return (char *)h;
    return nullptr;
}

extern "C" int toupper(int c)
{
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

extern "C" int tolower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

extern "C" int isspace(int c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

extern "C" int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

extern "C" int isalpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

extern "C" int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

extern "C" int isprint(int c)
{
    return c >= 0x20 && c < 0x7f;
}

extern "C" int iscntrl(int c)
{
    return (c >= 0 && c < 0x20) || c == 0x7f;
}

// proc_env answers a Str, which is not NUL-terminated; the copy is, and the
// environment cannot change, so one buffer per name is enough for zip, which
// reads ZIP and ZIPOPT once each.
extern "C" char *getenv(const char *name)
{
    static char val[512];
    Str v = proc_env(Str(name, strlen(name)));
    if (v.empty())
        return nullptr;
    usize n = v.size() < sizeof(val) - 1 ? v.size() : sizeof(val) - 1;
    memcpy(val, v.data(), n);
    val[n] = 0;
    return val;
}

extern "C" int atoi(const char *s)
{
    return (int)strtol(s, nullptr, 10);
}

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

extern "C" long strtol(const char *s, char **end, int base)
{
    const char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    int neg = 0;
    if (*p == '+' || *p == '-')
        neg = *p++ == '-';

    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        base = 16;
    } else if (base == 0) {
        base = p[0] == '0' ? 8 : 10;
    }

    long v  = 0;
    int any = 0;
    for (;; p++) {
        int d;
        if (*p >= '0' && *p <= '9')
            d = *p - '0';
        else if (*p >= 'a' && *p <= 'z')
            d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z')
            d = *p - 'A' + 10;
        else
            break;
        if (d >= base)
            break;
        v   = v * base + d;
        any = 1;
    }

    if (end)
        *end = (char *)(any ? p : s);
    return neg ? -v : v;
}

// Heapsort: guaranteed n log n, no recursion and no scratch, where a quicksort
// would want both. zip sorts the entry list, which can be long.

namespace {

void swap_bytes(char *a, char *b, usize n)
{
    for (usize i = 0; i < n; i++) {
        char t = a[i];
        a[i]   = b[i];
        b[i]   = t;
    }
}

void sift(char *base, usize n, usize size, usize root, int (*cmp)(const void *, const void *))
{
    for (;;) {
        usize big = root;
        usize l   = 2 * root + 1;
        usize r   = l + 1;
        if (l < n && cmp(base + l * size, base + big * size) > 0)
            big = l;
        if (r < n && cmp(base + r * size, base + big * size) > 0)
            big = r;
        if (big == root)
            return;
        swap_bytes(base + root * size, base + big * size, size);
        root = big;
    }
}

} // namespace

extern "C" void qsort(void *base, usize n, usize size, int (*cmp)(const void *, const void *))
{
    char *b = (char *)base;
    if (n < 2)
        return;
    for (usize i = n / 2; i > 0; i--)
        sift(b, n, size, i - 1, cmp);
    for (usize i = n; i > 1; i--) {
        swap_bytes(b, b + (i - 1) * size, size);
        sift(b, i - 1, size, 0, cmp);
    }
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

Task<Result<void>> zfclose(FILE *f)
{
    Result<void> r = Err(Error::NoMemory);
    if (Task<Result<void>> t = f->close())
        r = co_await t;
    co_return r;
}

Task<i32> zfseeko(FILE *f, i64 off, int whence)
{
    Result<u64> r = Err(Error::NoMemory);
    if (Task<Result<u64>> t = f->seek(off, (u32)whence))
        r = co_await t;
    co_return r.is_ok() ? 0 : -1;
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
