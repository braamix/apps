#include "braam.h"

#include "kernel/alloc.h"
#include "proc/rt.h"

// heap_alloc has no realloc and does not remember a block's size, so malloc
// carries an eight-byte header. Eight rather than four keeps u64 alignment.

namespace {

struct Head {
    u32 cap;
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
        return p;

    void *q = malloc(n);
    if (!q)
        return nullptr;
    memcpy(q, p, h->cap);
    free(p);
    return q;
}

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
    usize k = strlen(n);

    if (k == 0)
        return (char *)h;
    for (; *h; h++)
        if (*h == *n && strncmp(h, n, k) == 0)
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

extern "C" int islower(int c)
{
    return c >= 'a' && c <= 'z';
}

extern "C" int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

extern "C" int isalpha(int c)
{
    return islower(c) || isupper(c);
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

// proc_env answers a Str, which is not NUL-terminated. One static buffer: every
// caller copies what it wants before asking again.
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
    int sign = 1, n = 0;
    while (isspace(*s))
        s++;
    if (*s == '-')
        sign = -1, s++;
    else if (*s == '+')
        s++;
    while (isdigit(*s))
        n = n * 10 + (*s++ - '0');
    return n * sign;
}

// Insertion sort. The only callers sort the name table and a buffer list.
extern "C" void qsort(void *base, usize n, usize size, int (*cmp)(const void *, const void *))
{
    char *a = (char *)base;
    for (usize i = 1; i < n; i++)
        for (usize j = i; j > 0 && cmp(a + (j - 1) * size, a + j * size) > 0; j--)
            for (usize k = 0; k < size; k++) {
                char t                = a[(j - 1) * size + k];
                a[(j - 1) * size + k] = a[j * size + k];
                a[j * size + k]       = t;
            }
}

// printf, the conversions the message line and the file-name builders use.
// Truncation is C99's: the count is what a big enough buffer would have taken.

namespace {

struct Out {
    char *buf;
    usize size;
    usize n = 0;

    void put(char c)
    {
        if (n + 1 < size)
            buf[n] = c;
        n++;
    }

    void pad(char c, int k)
    {
        while (k-- > 0)
            put(c);
    }
};

// Digits of `v` in `base`, into a caller's buffer, most significant last.
int digits(char *d, u64 v, u32 base, bool upper)
{
    const char *set = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int k           = 0;

    do {
        d[k++] = set[v % base];
        v /= base;
    } while (v);
    return k;
}

} // namespace

extern "C" int vsnprintf(char *buf, usize size, const char *fmt, va_list ap)
{
    Out o{ buf, size };

    for (; *fmt; fmt++) {
        int width = 0, prec = -1, lng = 0, left = 0, zero = 0;
        char d[24];
        int k;
        u64 v;
        int neg = 0;
        u32 base;
        bool upper = false;

        if (*fmt != '%') {
            o.put(*fmt);
            continue;
        }
        fmt++;
        for (; *fmt == '-' || *fmt == '0'; fmt++) {
            if (*fmt == '-')
                left = 1;
            else
                zero = 1;
        }
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
        } else
            for (; isdigit(*fmt); fmt++)
                width = width * 10 + (*fmt - '0');
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                fmt++;
            } else
                for (; isdigit(*fmt); fmt++)
                    prec = prec * 10 + (*fmt - '0');
        }
        for (; *fmt == 'l'; fmt++)
            lng = 1;

        switch (*fmt) {
        case 0:
            fmt--;
            continue;

        case '%':
            o.put('%');
            continue;

        case 'c':
            o.put((char)va_arg(ap, int));
            continue;

        case 's': {
            const char *s = va_arg(ap, const char *);
            int len       = 0;

            if (!s)
                s = "(null)";
            while (s[len] && (prec < 0 || len < prec))
                len++;
            if (!left)
                o.pad(' ', width - len);
            for (int i = 0; i < len; i++)
                o.put(s[i]);
            if (left)
                o.pad(' ', width - len);
            continue;
        }

        case 'd':
            base = 10;
            {
                i64 sv = lng ? (i64)va_arg(ap, long) : (i64)va_arg(ap, int);

                neg = sv < 0;
                v   = neg ? (u64)(-sv) : (u64)sv;
            }
            break;

        case 'u':
            base = 10;
            v    = lng ? (u64)va_arg(ap, unsigned long) : (u64)va_arg(ap, unsigned);
            break;

        case 'o':
            base = 8;
            v    = lng ? (u64)va_arg(ap, unsigned long) : (u64)va_arg(ap, unsigned);
            break;

        case 'X':
            upper = true;
            [[fallthrough]];
        case 'x':
            base = 16;
            v    = lng ? (u64)va_arg(ap, unsigned long) : (u64)va_arg(ap, unsigned);
            break;

        default:
            // Not ours: put it back as it was typed.
            o.put('%');
            o.put(*fmt);
            continue;
        }

        k = digits(d, v, base, upper);
        if (!left)
            o.pad(zero ? '0' : ' ', width - k - neg);
        if (neg)
            o.put('-');
        for (int i = k; i > 0; i--)
            o.put(d[i - 1]);
        if (left)
            o.pad(' ', width - k - neg);
    }

    if (o.size)
        buf[o.n < o.size ? o.n : o.size - 1] = 0;
    return (int)o.n;
}

extern "C" int snprintf(char *buf, usize size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

// No size, so nothing can be checked; every caller here writes into a buffer
// that upstream had already sized for the widest conversion.
extern "C" int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, (usize)-1, fmt, ap);
    va_end(ap);
    return n;
}
