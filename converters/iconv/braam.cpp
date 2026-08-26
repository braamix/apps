// The C library citrus calls, over the kernel's own. Naive byte loops
// throughout: the hot path here is table lookup, not string handling.

#include "braam.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <wchar.h>

#include "kernel/alloc.h"
#include "kernel/host.h"
#include "kernel/text.h"
#include "proc/io.h"

extern "C" int errno = 0;

// ------------------------------------------------------------------- the heap
//
// heap_alloc has no realloc and does not remember a block's requested size, so
// malloc carries an eight-byte header. Eight rather than four keeps the u64 and
// f64 alignment the payload may want.

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
    if (!h) {
        errno = ENOMEM;
        return nullptr;
    }
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

    // Not C's realloc(p, 0): citrus calls this with a count that can be zero
    // and then treats null as a failure, so a zero request keeps a block.
    if (n == 0)
        n = 1;

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

extern "C" void *reallocarray(void *p, usize n, usize size)
{
    if (n && size > (usize)-1 / n) {
        errno = ENOMEM;
        return nullptr;
    }
    return realloc(p, n * size);
}

// ----------------------------------------------------------------------- mem*

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
    if (dp < sp)
        for (usize i = 0; i < n; i++)
            dp[i] = sp[i];
    else
        for (usize i = n; i > 0; i--)
            dp[i - 1] = sp[i - 1];
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

extern "C" void bzero(void *d, usize n)
{
    memset(d, 0, n);
}

extern "C" void *memchr(const void *s, int c, usize n)
{
    const unsigned char *p = (const unsigned char *)s;
    for (usize i = 0; i < n; i++)
        if (p[i] == (unsigned char)c)
            return (void *)(p + i);
    return nullptr;
}

// ----------------------------------------------------------------------- str*

extern "C" usize strlen(const char *s)
{
    usize n = 0;
    while (s[n])
        n++;
    return n;
}

extern "C" char *strcpy(char *d, const char *s)
{
    char *r = d;
    while ((*d++ = *s++))
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

// BSD's: the length it tried to make, so a caller can see truncation. Citrus
// relies on that at two sites.
extern "C" usize strlcpy(char *d, const char *s, usize n)
{
    usize len = strlen(s);
    if (n) {
        usize k = len < n - 1 ? len : n - 1;
        memcpy(d, s, k);
        d[k] = 0;
    }
    return len;
}

extern "C" int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

extern "C" int strncmp(const char *a, const char *b, usize n)
{
    for (usize i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (!a[i])
            return 0;
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
    const char *last = nullptr;
    for (;; s++) {
        if (*s == (char)c)
            last = s;
        if (!*s)
            return (char *)last;
    }
}

extern "C" char *strstr(const char *h, const char *n)
{
    if (!*n)
        return (char *)h;
    for (; *h; h++) {
        usize i = 0;
        while (n[i] && h[i] == n[i])
            i++;
        if (!n[i])
            return (char *)h;
    }
    return nullptr;
}

extern "C" char *strdup(const char *s)
{
    usize n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}

extern "C" char *strndup(const char *s, usize n)
{
    usize len = 0;
    while (len < n && s[len])
        len++;
    char *p = (char *)malloc(len + 1);
    if (p) {
        memcpy(p, s, len);
        p[len] = 0;
    }
    return p;
}

namespace {

char fold(char c)
{
    return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
}

} // namespace

extern "C" int strcasecmp(const char *a, const char *b)
{
    while (*a && fold(*a) == fold(*b)) {
        a++;
        b++;
    }
    return (int)(unsigned char)fold(*a) - (int)(unsigned char)fold(*b);
}

extern "C" int strncasecmp(const char *a, const char *b, usize n)
{
    for (usize i = 0; i < n; i++) {
        if (fold(a[i]) != fold(b[i]))
            return (int)(unsigned char)fold(a[i]) - (int)(unsigned char)fold(b[i]);
        if (!a[i])
            return 0;
    }
    return 0;
}

extern "C" char *strcasestr(const char *h, const char *n)
{
    if (!*n)
        return (char *)h;
    for (; *h; h++) {
        usize i = 0;
        while (n[i] && fold(h[i]) == fold(n[i]))
            i++;
        if (!n[i])
            return (char *)h;
    }
    return nullptr;
}

// ---------------------------------------------------------------- conversions

extern "C" unsigned long strtoul(const char *s, char **end, int base)
{
    const char *p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '+')
        p++;
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        base = 16;
    } else if (base == 0) {
        base = p[0] == '0' ? 8 : 10;
    }

    unsigned long v    = 0;
    const char *digits = p;
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
        v = v * (unsigned long)base + (unsigned long)d;
    }
    if (end)
        *end = (char *)(p == digits ? s : p);
    return v;
}

extern "C" int atoi(const char *s)
{
    return (int)strtol(s, nullptr, 10);
}

extern "C" long strtol(const char *s, char **end, int base)
{
    const char *p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    bool neg = *p == '-';
    if (*p == '-' || *p == '+')
        p++;
    unsigned long v = strtoul(p, end, base);
    return neg ? -(long)v : (long)v;
}

extern "C" int isspace(int c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}
extern "C" int isdigit(int c)
{
    return c >= '0' && c <= '9';
}
extern "C" int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}
extern "C" int islower(int c)
{
    return c >= 'a' && c <= 'z';
}
extern "C" int isalpha(int c)
{
    return isupper(c) || islower(c);
}
extern "C" int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}
extern "C" int toupper(int c)
{
    return islower(c) ? c - 'a' + 'A' : c;
}
extern "C" int tolower(int c)
{
    return isupper(c) ? c - 'A' + 'a' : c;
}

// -------------------------------------------------------------------- snprintf
//
// Citrus's forty calls between them use %s, %.*s and %d. The return is C's:
// the length it would have written, which is how a caller sees truncation.

namespace {

usize put_num(char *out, usize cap, unsigned long v, bool neg)
{
    char tmp[24];
    usize n = 0;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    if (neg)
        tmp[n++] = '-';

    usize k = 0;
    while (n > 0 && k < cap)
        out[k++] = tmp[--n];
    return k;
}

} // namespace

extern "C" int vsnprintf(char *out, usize cap, const char *fmt, __builtin_va_list ap)
{
    usize n = 0; // what has been written
    usize w = 0; // what would have been

    // Room for the terminator, and a cap of zero writes nothing at all.
    usize room = cap ? cap - 1 : 0;

    auto emit = [&](char c) {
        if (n < room)
            out[n++] = c;
        w++;
    };

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            emit(*p);
            continue;
        }
        p++;
        if (*p == '%') {
            emit('%');
            continue;
        }

        bool left = false;
        while (*p == '-') {
            left = true;
            p++;
        }

        int width = 0;
        while (*p >= '0' && *p <= '9')
            width = width * 10 + (*p++ - '0');

        int prec = -1;
        if (*p == '.') {
            p++;
            if (*p == '*') {
                prec = __builtin_va_arg(ap, int);
                p++;
            } else {
                prec = 0;
                while (*p >= '0' && *p <= '9')
                    prec = prec * 10 + (*p++ - '0');
            }
            if (prec < 0)
                prec = -1;
        }

        while (*p == 'l' || *p == 'h' || *p == 'z')
            p++;

        char body[24];
        usize len        = 0;
        const char *text = body;

        switch (*p) {
        case 'd':
        case 'i': {
            int v = __builtin_va_arg(ap, int);
            len   = put_num(body, sizeof(body), v < 0 ? (unsigned long)-(long)v : (unsigned long)v,
                            v < 0);
            break;
        }
        case 'u': {
            unsigned v = __builtin_va_arg(ap, unsigned);
            len        = put_num(body, sizeof(body), v, false);
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
            len  = 0;
            while (s[len] && (prec < 0 || len < (usize)prec))
                len++;
            break;
        }
        default:
            emit('%');
            if (*p)
                emit(*p);
            continue;
        }

        usize pad = (usize)width > len ? (usize)width - len : 0;
        if (!left)
            for (; pad > 0; pad--)
                emit(' ');
        for (usize i = 0; i < len; i++)
            emit(text[i]);
        if (left)
            for (; pad > 0; pad--)
                emit(' ');
    }

    if (cap)
        out[n] = 0;
    return (int)w;
}

extern "C" int snprintf(char *out, usize cap, const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int n = vsnprintf(out, cap, fmt, ap);
    __builtin_va_end(ap);
    return n;
}

// ---------------------------------------------------------------- wide chars
//
// The locale is UTF-8 and wchar_t is UTF-32, so the Apple wchar_t extension's
// two conversions are the kernel's own codec. A sequence that straddles a call
// is held in the state, which is what makes these restartable.

extern "C" usize mbrtowc(wchar_t *pwc, const char *s, usize n, mbstate_t *ps)
{
    static mbstate_t own;
    if (!ps)
        ps = &own;

    if (!s) { // reset
        ps->len = 0;
        return 0;
    }

    usize used = 0;
    while (used < n) {
        if (ps->len >= sizeof(ps->buf))
            return (usize)-1;
        ps->buf[ps->len++] = (unsigned char)s[used++];

        char32_t ch  = 0;
        usize wanted = utf8_decode(Str((const char *)ps->buf, ps->len), 0, ch);

        // Nothing consumed means the sequence runs past what is held; ask for
        // another byte rather than deciding.
        if (wanted == 0)
            continue;
        if (wanted != ps->len)
            continue;

        ps->len = 0;
        if (pwc)
            *pwc = (wchar_t)ch;
        return ch == 0 ? 0 : used;
    }
    return (usize)-2; // incomplete, and every byte was taken
}

extern "C" usize wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
    if (ps)
        ps->len = 0;
    if (!s)
        return 1; // the reset sequence, which UTF-8 does not have
    return utf8_encode((char32_t)wc, s);
}

// ------------------------------------------------------------------ the stubs

// There is one locale and it is UTF-8. Reached only for the empty, "char" and
// "wchar_t" encoding names.
extern "C" char *nl_langinfo(int)
{
    return (char *)"UTF-8";
}
extern "C" char *locale_charset(void)
{
    return (char *)"UTF-8";
}

// No setuid here, so the environment is always trusted.
extern "C" int issetugid(void)
{
    return 0;
}

extern "C" char *getenv(const char *name)
{
    // proc_env answers a Str; citrus wants a C string, and there is no setenv
    // to invalidate one, so a static buffer per call site is safe enough for
    // the two names citrus reads.
    static char val[128];
    Str v = proc_env(Str(name, strlen(name)));
    if (v.empty())
        return nullptr;
    usize n = v.size() < sizeof(val) - 1 ? v.size() : sizeof(val) - 1;
    memcpy(val, v.data(), n);
    val[n] = 0;
    return val;
}

extern "C" void iconv_assert_fail(const char *what, const char *file, int line)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "iconv: assertion failed: %s at %s:%d", what, file, line);
    panic(Str(buf, n < 0 ? 0 : (usize)n));
}

extern "C" void abort(void)
{
    panic("iconv: internal error");
}

// Nothing in the library exits; the command returns a status up its call chain.
extern "C" void exit(int)
{
    panic("iconv: exit from the library");
}

Error iconv_error(int err)
{
    switch (err) {
    case 0:
        return Error::Invalid;
    case ENOENT:
        return Error::NotFound;
    case ENOMEM:
        return Error::NoMemory;
    case EILSEQ:
    case EINVAL:
    case EFTYPE:
        return Error::Invalid;
    case E2BIG:
        return Error::Again;
    case EOPNOTSUPP:
        return Error::Unsupported;
    default:
        return Error::Io;
    }
}

extern "C" void qsort(void *base, usize n, usize size, int (*cmp)(const void *, const void *))
{
    // Heapsort: n log n whatever the input, no recursion, no scratch.
    char *a  = (char *)base;
    auto swp = [&](usize i, usize j) {
        char *x = a + i * size, *y = a + j * size;
        for (usize k = 0; k < size; k++) {
            char t = x[k];
            x[k]   = y[k];
            y[k]   = t;
        }
    };
    auto sift = [&](usize root, usize end) {
        for (;;) {
            usize child = 2 * root + 1;
            if (child > end)
                return;
            if (child + 1 <= end && cmp(a + child * size, a + (child + 1) * size) < 0)
                child++;
            if (cmp(a + root * size, a + child * size) >= 0)
                return;
            swp(root, child);
            root = child;
        }
    };

    if (n < 2)
        return;
    for (usize i = n / 2; i > 0; i--)
        sift(i - 1, n - 1);
    for (usize end = n - 1; end > 0; end--) {
        swp(0, end);
        sift(0, end - 1);
    }
}
