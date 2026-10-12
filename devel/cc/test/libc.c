// The C library's pure half, printed: built for Braam by vcc and for the host by
// its own compiler, and the two outputs must be the same (libc.mjs).
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef __vcc_coroutines__
#define coro(Y)
#define await
#endif

static coro(braam_call *) void ctypes(void)
{
    int (*const is[])(int) = { isalnum, isalpha, isblank, iscntrl, isdigit, isgraph,
                               islower, isprint, ispunct, isspace, isupper, isxdigit };
    for (int k = 0; k < 12; k++) {
        unsigned long long bits[2] = { 0, 0 };
        for (int c = 0; c < 128; c++)
            if (is[k](c))
                bits[c / 64] |= 1ull << (c % 64);
        await printf("ctype %d: %016llx %016llx %d\n", k, bits[1], bits[0], is[k](EOF) != 0);
    }
    for (int c = 0; c < 128; c++)
        if (tolower(c) != c || toupper(c) != c)
            await printf("%c%c%c ", c, tolower(c), toupper(c));
    await printf("\n");
}

/* strtol's and strtoul's answers for s, from strtoull's: long is 64 bits on the host
   and 32 on Braam, so these are checked against a clamp rather than printed. */
static void expect_long(const char *s, int base, long *l, int *el, unsigned long *u, int *eu)
{
    const char *p = s;
    while (isspace((unsigned char)*p))
        p++;
    int neg = *p == '-';
    if (*p == '+' || *p == '-')
        p++;
    errno                = 0;
    unsigned long long m = *p == '+' || *p == '-' ? 0 : strtoull(p, NULL, base);
    int over             = errno == ERANGE;
    *el                  = over || m > (unsigned long long)LONG_MAX + neg;
    *l                   = *el ? (neg ? LONG_MIN : LONG_MAX) : neg ? (long)(0 - m) : (long)m;
    *eu                  = over || m > ULONG_MAX;
    *u                   = *eu ? ULONG_MAX : neg ? 0 - (unsigned long)m : (unsigned long)m;
}

static coro(braam_call *) void integers(void)
{
    static const char *const in[] = { "0",
                                      "  42z",
                                      "-17",
                                      "+0x1f",
                                      "0x",
                                      "077",
                                      "0b1",
                                      "zz",
                                      "",
                                      "  -",
                                      "4294967295",
                                      "4294967296",
                                      "-2147483648",
                                      "-2147483649",
                                      "9223372036854775807",
                                      "9223372036854775808",
                                      "18446744073709551615",
                                      "18446744073709551616",
                                      "-1",
                                      "- 5",
                                      "+-5" };
    static const int bases[]      = { 0, 10, 16, 36, 2 };
    for (size_t i = 0; i < sizeof in / sizeof *in; i++)
        for (size_t b = 0; b < sizeof bases / sizeof *bases; b++) {
            char *end;
            long wl;
            unsigned long wu;
            int wel, weu;
            expect_long(in[i], bases[b], &wl, &wel, &wu, &weu);
            errno   = 0;
            long l  = strtol(in[i], &end, bases[b]);
            int ok1 = l == wl && (errno == ERANGE) == wel, n1 = (int)(end - in[i]);
            errno           = 0;
            unsigned long u = strtoul(in[i], &end, bases[b]);
            int ok2 = u == wu && (errno == ERANGE) == weu, n2 = (int)(end - in[i]);
            errno        = 0;
            long long ll = strtoll(in[i], &end, bases[b]);
            int e3 = errno, n3 = (int)(end - in[i]);
            errno                 = 0;
            unsigned long long ul = strtoull(in[i], &end, bases[b]);
            int e4 = errno, n4 = (int)(end - in[i]);
            await printf("\"%s\"/%d: %d %d, %d %d, %lld %d %d, %llu %d %d\n", in[i], bases[b], ok1,
                         n1, ok2, n2, ll, n3, e3 == ERANGE, ul, n4, e4 == ERANGE);
        }
}

static coro(braam_call *) void floats(void)
{
    static const char *const in[] = {
        "0",
        "-0",
        "1",
        "0.1",
        "1e23",
        "8.98846567431158e307",
        "1.7976931348623157e308",
        "1.7976931348623159e308",
        "2.2250738585072011e-308",
        "4.9406564584124654e-324",
        "2.4703282292062327e-324",
        "2.4703282292062328e-324",
        "1e-400",
        "0x1.8p3",
        "0x.8",
        "  +3.5e+2x",
        "inf",
        "-Infinity",
        "nan",
        "nan(123)",
        ".e1",
        "1.",
        "-.5e-1",
        "abc",
        "3.4028235677973366e38",
        "1.17549435e-38",
        "1.4e-45",
        "7.0064923216240854e-46",
        "123456789012345678901234567890",
        "0.000000000000000000000000000001",
    };
    for (size_t i = 0; i < sizeof in / sizeof *in; i++) {
        char *end1, *end2;
        errno    = 0;
        double d = strtod(in[i], &end1);
        int e1   = errno;
        errno    = 0;
        float f  = strtof(in[i], &end2);
        int e2   = errno;
        // ERANGE where both libraries set it: macOS also does for a subnormal result.
        await printf("\"%s\": %a %d %d, %a %d %d\n", in[i], d, (int)(end1 - in[i]),
                     e1 == ERANGE && (d == 0 || isinf(d)), (double)f, (int)(end2 - in[i]),
                     e2 == ERANGE && (f == 0 || isinf(f)));
    }

    static const double v[]        = { 0.0,
                                       -0.0,
                                       1.0,
                                       0.1,
                                       -2.5,
                                       1e300,
                                       1e-300,
                                       123456.789,
                                       DBL_MAX,
                                       DBL_MIN,
                                       4.9406564584124654e-324,
                                       0.5,
                                       1.5,
                                       2.5,
                                       9.9999999,
                                       1e21,
                                       1e-5,
                                       6.02214076e23 };
    static const char *const fmt[] = { "%a",     "%A",       "%.3a",    "%e",    "%.0e",
                                       "%E",     "%f",       "%.0f",    "%.10f", "%g",
                                       "%.17g",  "%#g",      "%G",      "%+e",   "% f",
                                       "%12.4f", "%-12.4e|", "%012.3f", "%#.0f", "%.3g" };
    for (size_t i = 0; i < sizeof v / sizeof *v; i++) {
        for (size_t k = 0; k < sizeof fmt / sizeof *fmt; k++) {
            char buf[512];
            snprintf(buf, sizeof buf, fmt[k], v[i]);
            await printf("%s ", buf);
        }
        await printf("\n");
    }
    double inf = INFINITY, qnan = NAN;
    await printf("%f %e %g %a %F %5.1f| %-6g|\n", inf, -inf, inf, -inf, inf, qnan, -inf);

    await printf("%d %d %d %d %d %d\n", isnan(qnan), isnan(1.0), isinf(-inf), isinf(1e308),
                 isfinite(1.0), isfinite(inf));
    await printf("%d %d %d %d\n", signbit(-0.0), signbit(0.0), signbit(-1.0f), isnan((float)qnan));
    await printf("%d %d %d %d %d\n", fpclassify(0.0) == FP_ZERO, fpclassify(1e-310) == FP_SUBNORMAL,
                 fpclassify(1.0) == FP_NORMAL, fpclassify(inf) == FP_INFINITE,
                 fpclassify(qnan) == FP_NAN);
    int e;
    double ip;
    await printf("%a %a %a %a %a %a\n", sqrt(2.0), floor(-2.5), ceil(-2.5), fmod(7.5, 2.0),
                 trunc(-2.7), round(2.5));
    await printf("%a %d %a %a %a %a\n", frexp(48.0, &e), e, ldexp(0.75, 10), modf(-3.25, &ip), ip,
                 copysign(3.0, -0.0));
    await printf("%d %d\n", isnan(nan("")), signbit(nan("")) == 0);
}

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return x < y ? -1 : x > y;
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static coro(braam_call *) void sorting(void)
{
    int a[50];
    unsigned seed = 12345;
    for (int k = 0; k < 50; k++) {
        seed = seed * 1103515245 + 12345;
        a[k] = (int)(seed >> 16) % 100 - 50;
    }
    qsort(a, 50, sizeof *a, cmp_int);
    for (int k = 0; k < 50; k++)
        await printf("%d ", a[k]);
    await printf("\n");
    for (int key = -52; key <= 52; key += 13) {
        int *p = bsearch(&key, a, 50, sizeof *a, cmp_int);
        await printf("%d:%s ", key, p ? "found" : "-");
    }
    const char *w[] = { "pear", "apple", "fig", "banana", "cherry", "date", "apple" };
    qsort(w, 7, sizeof *w, cmp_str);
    for (int k = 0; k < 7; k++)
        await printf("%s ", w[k]);
    const char *key = "date";
    await printf("%d\n", bsearch(&key, w, 7, sizeof *w, cmp_str) != NULL);
    qsort(a, 0, sizeof *a, cmp_int);
    qsort(a, 1, sizeof *a, cmp_int);
}

static coro(braam_call *) void strings(void)
{
    char *d = strdup("hello"), *n = strndup("hello", 3), *n2 = strndup("hi", 10);
    await printf("%s %s %s %zu %zu\n", d, n, n2, strnlen("hello", 3), strnlen("hi", 10));
    free(d);
    free(n);
    free(n2);
    await printf("%zu %zu %zu %zu\n", strspn("abcabd", "abc"), strspn("", "a"),
                 strcspn("hello", "lo"), strcspn("abc", ""));
    await printf("%s %d\n", strpbrk("hello, world", " ,"), strpbrk("abc", "xyz") == NULL);
    char line[] = "  a,b,,c  ";
    char *save, *tok = strtok_r(line, " ,", &save);
    for (; tok; tok = strtok_r(NULL, " ,", &save))
        await printf("[%s]", tok);
    char buf[16];
    memset(buf, '.', sizeof buf);
    char *p = memccpy(buf, "abc:def", ':', 7);
    await printf(" %d %.6s %d\n", (int)(p - buf), buf, memccpy(buf, "abc", 'x', 3) == NULL);
}

static int vfmt(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

static coro(braam_call *) void formats(void)
{
    char buf[8];
    int n = snprintf(buf, sizeof buf, "%s-%d", "abcdef", 12345);
    await printf("%d %s ", n, buf);
    n = vfmt(buf, sizeof buf, "%05d|%-3x|", 42, 255);
    await printf("%d %s ", n, buf);
    n = vfmt(NULL, 0, "%s", "counted");
    await printf("%d ", n);
    char big[64];
    n = sprintf(big, "%lld %llu %hhd %hu %ld %zu %c %%", -9223372036854775807LL - 1,
                18446744073709551615ULL, 300, 70000, -5L, (size_t)7, 'x');
    await printf("%d %s\n", n, big);
}

static coro(braam_call *) void options(void)
{
    static const struct option longs[]  = { { "verbose", no_argument, 0, 'v' },
                                            { "target", required_argument, 0, 't' },
                                            { "color", optional_argument, 0, 'c' },
                                            { "max-errors", required_argument, 0, 'm' },
                                            { 0, 0, 0, 0 } };
    static const char *const cases[][8] = {
        { "p", "-v", "in", "--target=wasm", "out", 0 },
        { "p", "in", "-t", "x", "--verbose", "--", "-v", 0 },
        { "p", "--col", "--color=always", "-vt", "y", 0 },
        { "p", "--max", "3", "--tar", "z", "a", "b", 0 },
        { "p", "-q", "--nope", "--target", 0 },
        { "p", "-", "--verbose=1", "--ma=7", 0 },
    };
    opterr = 0;
    for (size_t k = 0; k < sizeof cases / sizeof *cases; k++) {
        char *argv[8];
        int argc = 0;
        while (cases[k][argc]) {
            argv[argc] = (char *)cases[k][argc];
            argc++;
        }
        argv[argc] = NULL;
        optind     = 0; /* a reset, to BSD's getopt and GNU's */
        int c, idx = -1;
        await printf("case %zu:", k);
        while ((c = getopt_long(argc, argv, "vt:", longs, &idx)) != -1)
            await printf(" %c(%s,%d,%d)", c, optarg ? optarg : "-", idx, c == '?' ? optopt : 0);
        await printf(" |");
        for (int i = optind; i < argc; i++)
            await printf(" %s", argv[i]);
        await printf("\n");
    }
}

static coro(braam_call *) void calendar(void)
{
    static const time_t when[] = { 0,          951782400,  951868799,  -1,        1700000000,
                                   2147483647, 1104537600, 1230681600, 1609459199 };
    for (size_t k = 0; k < sizeof when / sizeof *when; k++) {
        const struct tm *tm = gmtime(&when[k]);
        char buf[256];
        size_t n = strftime(buf, sizeof buf,
                            "%a %A %b %B %c|%C %d %D %e %F %g %G %h %H %I %j %m %M %n%p %r %R %S %t"
                            "%T %u %U %V %w %W %x %X %y %Y %%",
                            tm);
        await printf("%zu %s yday %d\n", n, buf, tm->tm_yday);
    }
    char small[4];
    await printf("%zu\n", strftime(small, sizeof small, "%Y", gmtime(&when[0])));
}

coro(braam_call *) int main(int argc, char **argv)
{
    (void)argc, (void)argv;
    await ctypes();
    await integers();
    await floats();
    await sorting();
    await strings();
    await formats();
    await options();
    await calendar();
    return 0;
}
