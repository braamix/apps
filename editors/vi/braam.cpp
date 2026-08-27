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

// proc_env answers a Str, which is not NUL-terminated. One buffer per call
// site would be safer, but ex reads SHELL, HOME and EXINIT once each at
// startup and copies what it wants immediately.
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

// Insertion sort. The only caller is the tag search over an arg list, which is
// a few dozen names at most.
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
