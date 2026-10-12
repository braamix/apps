/*
 * long double (IEEE-754 binary128) arithmetic, comparisons and conversions: the
 * libgcc/compiler-rt routines that our code generator and clang call.  The work is
 * done by the compiler's own binary128 code, included here, so constants folded at
 * compile time and values computed at run time agree.  The `di` routines take and
 * return long long: 64 bits on every target that uses this file.
 */
#define F128_RUNTIME
#define F128_API static
#include "../../libutil/float128.c"

typedef union {
    long double f;
    Float128 q;
} Bits;

static Float128 q_of(long double f)
{
    Bits b;
    b.f = f;
    return b.q;
}

static long double ld_of(Float128 q)
{
    Bits b;
    b.q = q;
    return b.f;
}

long double __addtf3(long double x, long double y)
{
    return ld_of(f128_add(q_of(x), q_of(y)));
}

long double __subtf3(long double x, long double y)
{
    return ld_of(f128_sub(q_of(x), q_of(y)));
}

long double __multf3(long double x, long double y)
{
    return ld_of(f128_mul(q_of(x), q_of(y)));
}

long double __divtf3(long double x, long double y)
{
    return ld_of(f128_div(q_of(x), q_of(y)));
}

/* Unordered is `unordered`. */
static int compare(long double x, long double y, int unordered)
{
    int c = f128_cmp(q_of(x), q_of(y));
    return c == 2 ? unordered : c;
}

int __eqtf2(long double x, long double y)
{
    return compare(x, y, 1);
}

int __netf2(long double x, long double y)
{
    return compare(x, y, 1);
}

int __lttf2(long double x, long double y)
{
    return compare(x, y, 1);
}

int __letf2(long double x, long double y)
{
    return compare(x, y, 1);
}

int __cmptf2(long double x, long double y)
{
    return compare(x, y, 1);
}

int __gttf2(long double x, long double y)
{
    return compare(x, y, -1);
}

int __getf2(long double x, long double y)
{
    return compare(x, y, -1);
}

int __unordtf2(long double x, long double y)
{
    return f128_cmp(q_of(x), q_of(y)) == 2;
}

long long __fixtfdi(long double f)
{
    return f128_to_i64(q_of(f), 64);
}

int __fixtfsi(long double f)
{
    return (int)f128_to_i64(q_of(f), 32);
}

unsigned long long __fixunstfdi(long double f)
{
    return f128_to_u64(q_of(f), 64);
}

unsigned __fixunstfsi(long double f)
{
    return (unsigned)f128_to_u64(q_of(f), 32);
}

long double __floatditf(long long v)
{
    return ld_of(f128_from_i64(v));
}

long double __floatunditf(unsigned long long v)
{
    return ld_of(f128_from_u64(v));
}

long double __floatsitf(int v)
{
    return ld_of(f128_from_i64(v));
}

long double __floatunsitf(unsigned v)
{
    return ld_of(f128_from_u64(v));
}

long double __extenddftf2(double d)
{
    union {
        double d;
        uint64_t u;
    } x;
    x.d = d;
    return ld_of(f128_from_double_bits(x.u));
}

long double __extendsftf2(float f)
{
    union {
        float f;
        uint32_t u;
    } x;
    x.f = f;
    return ld_of(f128_from_float_bits(x.u));
}

double __trunctfdf2(long double f)
{
    union {
        double d;
        uint64_t u;
    } x;
    x.u = f128_to_double_bits(q_of(f));
    return x.d;
}

float __trunctfsf2(long double f)
{
    union {
        float f;
        uint32_t u;
    } x;
    x.u = f128_to_float_bits(q_of(f));
    return x.f;
}
