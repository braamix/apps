#include "braam.h"

#include "kernel/alloc.h"
#include "math/ftoa.h"
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

extern "C" void *memchr(const void *s, int c, usize n)
{
    const unsigned char *p = (const unsigned char *)s;
    for (usize i = 0; i < n; i++)
        if (p[i] == (unsigned char)c)
            return (void *)(p + i);
    return nullptr;
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

extern "C" int strcasecmp(const char *a, const char *b)
{
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b))
        a++, b++;
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

extern "C" int strncasecmp(const char *a, const char *b, usize n)
{
    for (usize i = 0; i < n; i++) {
        int x = tolower((unsigned char)a[i]), y = tolower((unsigned char)b[i]);
        if (x != y)
            return x - y;
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

extern "C" char *strdup(const char *s)
{
    usize n = strlen(s) + 1;
    char *p = (char *)malloc(n);

    if (p)
        memcpy(p, s, n);
    return p;
}

extern "C" usize strspn(const char *s, const char *set)
{
    usize n = 0;
    while (s[n] && strchr(set, s[n]))
        n++;
    return n;
}

extern "C" usize strcspn(const char *s, const char *set)
{
    usize n = 0;
    while (s[n] && !strchr(set, s[n]))
        n++;
    return n;
}

extern "C" char *strpbrk(const char *s, const char *set)
{
    for (; *s; s++)
        if (strchr(set, *s))
            return (char *)s;
    return nullptr;
}

// One static cursor, as C's has.
extern "C" char *strtok(char *s, const char *sep)
{
    static char *rest;
    char *tok;

    if (!s)
        s = rest;
    if (!s)
        return nullptr;
    s += strspn(s, sep);
    if (!*s) {
        rest = nullptr;
        return nullptr;
    }
    tok = s;
    s += strcspn(s, sep);
    if (*s)
        *s++ = 0;
    rest = s;
    return tok;
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

extern "C" int isblank(int c)
{
    return c == ' ' || c == '\t';
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

extern "C" int isxdigit(int c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
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

extern "C" int isgraph(int c)
{
    return c > 0x20 && c < 0x7f;
}

extern "C" int ispunct(int c)
{
    return isgraph(c) && !isalnum(c);
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
    return (int)strtol(s, nullptr, 10);
}

extern "C" int abs(int n)
{
    return n < 0 ? -n : n;
}

extern "C" long labs(long n)
{
    return n < 0 ? -n : n;
}

// strtol and friends. base 0 is C's 0x/0 prefix rules.

namespace {

int digit_of(int c, int base)
{
    int v;

    if (isdigit(c))
        v = c - '0';
    else if (isalpha(c))
        v = tolower(c) - 'a' + 10;
    else
        return -1;
    return v < base ? v : -1;
}

unsigned long long scan_ull(const char *s, char **end, int base, int *neg)
{
    const char *start    = s;
    unsigned long long v = 0;
    int any              = 0;

    *neg = 0;
    while (isspace((unsigned char)*s))
        s++;
    if (*s == '-')
        *neg = 1, s++;
    else if (*s == '+')
        s++;

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X') &&
        digit_of((unsigned char)s[2], 16) >= 0) {
        base = 16;
        s += 2;
    } else if (base == 0 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B') &&
               digit_of((unsigned char)s[2], 2) >= 0) {
        base = 2;
        s += 2;
    } else if (base == 0) {
        base = s[0] == '0' ? 8 : 10;
    }

    for (int d; (d = digit_of((unsigned char)*s, base)) >= 0; s++) {
        v   = v * (unsigned)base + (unsigned)d;
        any = 1;
    }
    if (end)
        *end = (char *)(any ? s : start);
    return v;
}

} // namespace

extern "C" long strtol(const char *s, char **end, int base)
{
    int neg;
    unsigned long long v = scan_ull(s, end, base, &neg);
    return neg ? -(long)v : (long)v;
}

extern "C" unsigned long strtoul(const char *s, char **end, int base)
{
    int neg;
    unsigned long long v = scan_ull(s, end, base, &neg);
    return neg ? (unsigned long)-(long long)v : (unsigned long)v;
}

extern "C" long long strtoll(const char *s, char **end, int base)
{
    int neg;
    unsigned long long v = scan_ull(s, end, base, &neg);
    return neg ? -(long long)v : (long long)v;
}

extern "C" unsigned long long strtoull(const char *s, char **end, int base)
{
    int neg;
    unsigned long long v = scan_ull(s, end, base, &neg);
    return neg ? (unsigned long long)-(long long)v : v;
}

// Insertion sort. The callers sort a directory listing and a colour table.
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

// fnmatch. Backtracking on * rather than recursion, so a pattern of stars
// cannot blow the stack.
extern "C" int fnmatch(const char *pattern, const char *s, int)
{
    const char *star = nullptr, *back = nullptr;

    while (*s) {
        if (*pattern == '?') {
            pattern++, s++;
            continue;
        }
        if (*pattern == '*') {
            star = pattern++;
            back = s;
            continue;
        }
        if (*pattern == '[') {
            const char *p = pattern + 1;
            int neg       = (*p == '!' || *p == '^');
            int hit       = 0;

            if (neg)
                p++;
            for (int first = 1; *p && (*p != ']' || first); first = 0, p++) {
                if (p[1] == '-' && p[2] && p[2] != ']') {
                    if ((unsigned char)*s >= (unsigned char)p[0] &&
                        (unsigned char)*s <= (unsigned char)p[2])
                        hit = 1;
                    p += 2;
                } else if (*p == *s) {
                    hit = 1;
                }
            }
            if (*p == ']' && hit != neg) {
                pattern = p + 1;
                s++;
                continue;
            }
        } else if (*pattern == *s) {
            pattern++, s++;
            continue;
        }
        // No match here: give the last star one more character.
        if (!star)
            return FNM_NOMATCH;
        pattern = star + 1;
        s       = ++back;
    }
    while (*pattern == '*')
        pattern++;
    return *pattern ? FNM_NOMATCH : 0;
}

// printf. Truncation is C99's: the count is what a big enough buffer would
// have taken.

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

// Digits of `v` in `base`, most significant last.
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
        int width = 0, prec = -1, lng = 0, left = 0, zero = 0, alt = 0;
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
        for (; *fmt == '-' || *fmt == '0' || *fmt == '#' || *fmt == '+' || *fmt == ' '; fmt++) {
            if (*fmt == '-')
                left = 1;
            else if (*fmt == '0')
                zero = 1;
            else if (*fmt == '#')
                alt = 1;
        }
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
            if (width < 0)
                left = 1, width = -width;
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
        for (; *fmt == 'l' || *fmt == 'z' || *fmt == 'L'; fmt++)
            lng++;

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

        // musl's engine, into a buffer of our own; the flags above still apply.
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'a':
        case 'A': {
            char fb[64];
            Str s   = fmt_f64(fb, sizeof(fb), va_arg(ap, double), prec, *fmt);
            int len = (int)s.size();

            if (!left)
                o.pad(zero ? '0' : ' ', width - len);
            for (int i = 0; i < len; i++)
                o.put(s.data()[i]);
            if (left)
                o.pad(' ', width - len);
            continue;
        }

        case 'd':
        case 'i':
            base = 10;
            {
                i64 sv = lng > 1 ? (i64)va_arg(ap, long long)
                         : lng   ? (i64)va_arg(ap, long)
                                 : (i64)va_arg(ap, int);

                neg = sv < 0;
                v   = neg ? (u64)(-sv) : (u64)sv;
            }
            break;

        case 'u':
            base = 10;
            goto unsigned_arg;

        case 'o':
            base = 8;
            goto unsigned_arg;

        case 'p':
            alt  = 0;
            base = 16;
            v    = (u64)(usize)va_arg(ap, void *);
            o.put('0'), o.put('x');
            break;

        case 'X':
            upper = true;
            [[fallthrough]];
        case 'x':
            base = 16;
        unsigned_arg:
            v = lng > 1 ? (u64)va_arg(ap, unsigned long long)
                : lng   ? (u64)va_arg(ap, unsigned long)
                        : (u64)va_arg(ap, unsigned);
            break;

        default:
            // Not ours: put it back as it was typed.
            o.put('%');
            o.put(*fmt);
            continue;
        }

        if (alt && base == 8 && v != 0)
            o.put('0');
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
// upstream had already sized for the widest conversion.
extern "C" int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, (usize)-1, fmt, ap);
    va_end(ap);
    return n;
}
