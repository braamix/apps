//
// IEEE-754 binary128 in software: see float128.h.  The part above F128_RUNTIME is
// also compiled by our own compiler into the RISC-V runtime, so it keeps to the C we
// compile (no shadowed names) and to <stdint.h>.
//
#include "float128.h"

typedef uint64_t u64;

// A 128-bit unsigned integer.
typedef struct {
    u64 hi, lo;
} U128;

enum { F_ZERO, F_FINITE, F_INF, F_NAN };

// An unpacked number.  A finite one is normalized: the significand's leading bit is
// bit 112, and its value is m * 2^(exp - 16383 - 112); a subnormal gets exp <= 0.
typedef struct {
    int cls;
    int sign;
    int exp;
    U128 m;
    Float128 bits;
} Num;

#define SIGN  0x8000000000000000ULL
#define FRAC  0x0000ffffffffffffULL // the fraction bits of the high doubleword
#define EXPS  0x7fff000000000000ULL
#define QUIET 0x0000800000000000ULL
#define BIAS  16383

static int top64(u64 x)
{
    int n = 0;
    if (x >> 32) {
        n += 32;
        x >>= 32;
    }
    if (x >> 16) {
        n += 16;
        x >>= 16;
    }
    if (x >> 8) {
        n += 8;
        x >>= 8;
    }
    if (x >> 4) {
        n += 4;
        x >>= 4;
    }
    if (x >> 2) {
        n += 2;
        x >>= 2;
    }
    if (x >> 1)
        n += 1;
    return n;
}

// The position of the leading one bit, or -1.
static int u128_top(U128 a)
{
    if (a.hi)
        return 64 + top64(a.hi);
    return a.lo ? top64(a.lo) : -1;
}

static U128 u128_shl(U128 a, int n)
{
    U128 r;
    if (n == 0)
        return a;
    if (n >= 128) {
        r.hi = 0;
        r.lo = 0;
    } else if (n >= 64) {
        r.hi = a.lo << (n - 64);
        r.lo = 0;
    } else {
        r.hi = a.hi << n | a.lo >> (64 - n);
        r.lo = a.lo << n;
    }
    return r;
}

static U128 u128_shr(U128 a, int n)
{
    U128 r;
    if (n == 0)
        return a;
    if (n >= 128) {
        r.hi = 0;
        r.lo = 0;
    } else if (n >= 64) {
        r.hi = 0;
        r.lo = a.hi >> (n - 64);
    } else {
        r.hi = a.hi >> n;
        r.lo = a.lo >> n | a.hi << (64 - n);
    }
    return r;
}

// Shift right, or-ing the bits shifted out into bit 0.
static U128 u128_shr_sticky(U128 a, int n)
{
    U128 r = u128_shr(a, n);
    U128 back;
    if (n <= 0)
        return a;
    back = u128_shl(r, n);
    if (n >= 128 || back.hi != a.hi || back.lo != a.lo)
        r.lo |= 1;
    return r;
}

static U128 u128_add(U128 a, U128 b)
{
    U128 r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo);
    return r;
}

static U128 u128_sub(U128 a, U128 b)
{
    U128 r;
    r.lo = a.lo - b.lo;
    r.hi = a.hi - b.hi - (a.lo < b.lo);
    return r;
}

static int u128_cmp(U128 a, U128 b)
{
    if (a.hi != b.hi)
        return a.hi < b.hi ? -1 : 1;
    if (a.lo != b.lo)
        return a.lo < b.lo ? -1 : 1;
    return 0;
}

// The 128-bit product of two doublewords.
static U128 mul64(u64 a, u64 b)
{
    u64 a0 = a & 0xffffffff, a1 = a >> 32, b0 = b & 0xffffffff, b1 = b >> 32;
    u64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    u64 mid = (p00 >> 32) + (p01 & 0xffffffff) + (p10 & 0xffffffff);
    U128 r;
    r.lo = mid << 32 | (p00 & 0xffffffff);
    r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    return r;
}

static Float128 make(u64 hi, u64 lo)
{
    Float128 f;
    f.hi = hi;
    f.lo = lo;
    return f;
}

static Num unpack(Float128 f)
{
    Num n;
    n.bits = f;
    n.sign = f.hi >> 63;
    n.exp  = (f.hi >> 48) & 0x7fff;
    n.m.hi = f.hi & FRAC;
    n.m.lo = f.lo;
    if (n.exp == 0x7fff) {
        n.cls = (n.m.hi | n.m.lo) ? F_NAN : F_INF;
    } else if (n.exp) {
        n.cls = F_FINITE;
        n.m.hi |= FRAC + 1;
    } else if (n.m.hi | n.m.lo) {
        int shift = 112 - u128_top(n.m);
        n.cls     = F_FINITE;
        n.m       = u128_shl(n.m, shift);
        n.exp     = 1 - shift;
    } else {
        n.cls = F_ZERO;
    }
    return n;
}

static Float128 quiet(Num a)
{
    return make(a.bits.hi | QUIET, a.bits.lo);
}

static Float128 default_nan(void)
{
    return make(EXPS | QUIET, 0);
}

static Float128 infinity(int sign)
{
    return make((u64)sign << 63 | EXPS, 0);
}

static Float128 signed_zero(int sign)
{
    return make((u64)sign << 63, 0);
}

// Round m * 2^(exp - 16383 - 125) and pack it.
static Float128 round_pack(int sign, int exp, U128 m)
{
    u64 s    = (u64)sign << 63;
    int lead = u128_top(m);
    u64 rest;
    if (lead < 0)
        return make(s, 0);
    if (lead > 125) {
        m = u128_shr_sticky(m, lead - 125);
        exp += lead - 125;
    } else if (lead < 125) {
        m = u128_shl(m, 125 - lead);
        exp -= 125 - lead;
    }
    if (exp <= 0) {
        // Subnormal: scaled as exponent 1, without the leading bit.
        m   = u128_shr_sticky(m, exp < -200 ? 201 : 1 - exp);
        exp = 1;
    }
    rest = m.lo & 0x1fff;
    m    = u128_shr(m, 13);
    if (rest > 0x1000 || (rest == 0x1000 && (m.lo & 1))) {
        U128 one;
        one.hi = 0;
        one.lo = 1;
        m      = u128_add(m, one);
    }
    if (m.hi >> 49) {
        m = u128_shr(m, 1);
        exp++;
    }
    if (!(m.hi >> 48))
        exp = 0;
    if (exp >= 0x7fff)
        return infinity(sign);
    return make(s | (u64)exp << 48 | (m.hi & FRAC), m.lo);
}

// A finite unpacked number, packed again.
static Float128 repack(Num a)
{
    return round_pack(a.sign, a.exp, u128_shl(a.m, 13));
}

static Float128 add_signed(Float128 x, Float128 y, int negate)
{
    Num a = unpack(x), b = unpack(y);
    U128 ma, mb, m;
    b.sign ^= negate;
    if (a.cls == F_NAN)
        return quiet(a);
    if (b.cls == F_NAN)
        return quiet(b);
    if (a.cls == F_INF)
        return b.cls == F_INF && a.sign != b.sign ? default_nan() : infinity(a.sign);
    if (b.cls == F_INF)
        return infinity(b.sign);
    if (a.cls == F_ZERO && b.cls == F_ZERO)
        return signed_zero(a.sign & b.sign);
    if (a.cls == F_ZERO)
        return repack(b);
    if (b.cls == F_ZERO)
        return repack(a);
    if (a.exp < b.exp || (a.exp == b.exp && u128_cmp(a.m, b.m) < 0)) {
        Num t = a;
        a     = b;
        b     = t;
    }
    ma = u128_shl(a.m, 13);
    mb = u128_shr_sticky(u128_shl(b.m, 13), a.exp - b.exp);
    if (a.sign == b.sign) {
        m = u128_add(ma, mb);
    } else {
        m = u128_sub(ma, mb);
        if (!(m.hi | m.lo))
            return signed_zero(0);
    }
    return round_pack(a.sign, a.exp, m);
}

F128_API Float128 f128_add(Float128 x, Float128 y)
{
    return add_signed(x, y, 0);
}

F128_API Float128 f128_sub(Float128 x, Float128 y)
{
    return add_signed(x, y, 1);
}

F128_API Float128 f128_mul(Float128 x, Float128 y)
{
    Num a = unpack(x), b = unpack(y);
    int sign = a.sign ^ b.sign;
    U128 ll, lh, hl, hh, m;
    u64 p1, p2, p3, c1, c2;
    if (a.cls == F_NAN)
        return quiet(a);
    if (b.cls == F_NAN)
        return quiet(b);
    if (a.cls == F_INF || b.cls == F_INF)
        return a.cls == F_ZERO || b.cls == F_ZERO ? default_nan() : infinity(sign);
    if (a.cls == F_ZERO || b.cls == F_ZERO)
        return signed_zero(sign);

    // The 226-bit product, in doublewords p3..p0.
    ll = mul64(a.m.lo, b.m.lo);
    lh = mul64(a.m.lo, b.m.hi);
    hl = mul64(a.m.hi, b.m.lo);
    hh = mul64(a.m.hi, b.m.hi);
    p1 = ll.hi + lh.lo;
    c1 = p1 < ll.hi;
    p1 += hl.lo;
    c1 += p1 < hl.lo;
    p2 = lh.hi + hl.hi;
    c2 = p2 < lh.hi;
    p2 += hh.lo;
    c2 += p2 < hh.lo;
    p2 += c1;
    c2 += p2 < c1;
    p3 = hh.hi + c2;

    // Bits 227..100, the rest sticky.
    m.hi = p2 >> 36 | p3 << 28;
    m.lo = p1 >> 36 | p2 << 28;
    if (ll.lo || (p1 & 0xfffffffff))
        m.lo |= 1;
    return round_pack(sign, a.exp + b.exp - (BIAS - 1), m);
}

F128_API Float128 f128_div(Float128 x, Float128 y)
{
    Num a = unpack(x), b = unpack(y);
    int sign = a.sign ^ b.sign;
    U128 q, r;
    int i;
    if (a.cls == F_NAN)
        return quiet(a);
    if (b.cls == F_NAN)
        return quiet(b);
    if (a.cls == F_INF)
        return b.cls == F_INF ? default_nan() : infinity(sign);
    if (b.cls == F_INF)
        return signed_zero(sign);
    if (b.cls == F_ZERO)
        return a.cls == F_ZERO ? default_nan() : infinity(sign);
    if (a.cls == F_ZERO)
        return signed_zero(sign);

    // q = a.m * 2^125 / b.m, one bit at a time; a remainder is sticky.
    q.hi = 0;
    q.lo = 0;
    r    = a.m;
    for (i = 0; i < 126; i++) {
        q = u128_shl(q, 1);
        if (u128_cmp(r, b.m) >= 0) {
            r = u128_sub(r, b.m);
            q.lo |= 1;
        }
        r = u128_shl(r, 1);
    }
    if (r.hi | r.lo)
        q.lo |= 1;
    return round_pack(sign, a.exp - b.exp + BIAS, q);
}

F128_API Float128 f128_neg(Float128 x)
{
    x.hi ^= SIGN;
    return x;
}

F128_API int f128_is_nan(Float128 x)
{
    u64 h = x.hi & ~SIGN;
    return h > EXPS || (h == EXPS && x.lo);
}

F128_API int f128_is_zero(Float128 x)
{
    return !((x.hi & ~SIGN) | x.lo);
}

F128_API int f128_cmp(Float128 x, Float128 y)
{
    u64 xh = x.hi & ~SIGN, yh = y.hi & ~SIGN;
    int c;
    if (f128_is_nan(x) || f128_is_nan(y))
        return 2;
    if (!(xh | x.lo | yh | y.lo))
        return 0; // +0 == -0
    if ((x.hi ^ y.hi) & SIGN)
        return (x.hi & SIGN) ? -1 : 1;
    if (xh != yh)
        c = xh < yh ? -1 : 1;
    else if (x.lo != y.lo)
        c = x.lo < y.lo ? -1 : 1;
    else
        c = 0;
    return (x.hi & SIGN) ? -c : c;
}

// The integer part of |x| when below 2^bits, and *big = 0; else *big = 1.
static u64 int_part(Num a, int bits, int *big)
{
    int e = a.exp - BIAS;
    *big  = 0;
    if (a.cls == F_ZERO || (a.cls == F_FINITE && e < 0))
        return 0;
    if (a.cls != F_FINITE || e >= bits) {
        *big = 1;
        return 0;
    }
    return u128_shr(a.m, 112 - e).lo;
}

F128_API int64_t f128_to_i64(Float128 x, int bits)
{
    Num a = unpack(x);
    int big;
    u64 v = int_part(a, bits - 1, &big);
    if (big) {
        u64 max = ~(u64)0 >> (65 - bits);
        return a.sign ? -(int64_t)max - 1 : (int64_t)max;
    }
    return a.sign ? -(int64_t)v : (int64_t)v;
}

F128_API uint64_t f128_to_u64(Float128 x, int bits)
{
    Num a = unpack(x);
    int big;
    u64 v = int_part(a, bits, &big);
    if (a.sign)
        return 0;
    return big ? ~(u64)0 >> (64 - bits) : v;
}

static Float128 from_magnitude(int sign, u64 v)
{
    int lead;
    U128 m;
    if (!v)
        return signed_zero(sign);
    lead = top64(v);
    m.hi = 0;
    m.lo = v;
    m    = u128_shl(m, 112 - lead);
    return make((u64)sign << 63 | (u64)(BIAS + lead) << 48 | (m.hi & FRAC), m.lo);
}

F128_API Float128 f128_from_i64(int64_t v)
{
    return v < 0 ? from_magnitude(1, -(u64)v) : from_magnitude(0, (u64)v);
}

F128_API Float128 f128_from_u64(uint64_t v)
{
    return from_magnitude(0, v);
}

// From a binary format of `fbits` fraction and `ebits` exponent bits: exact.
static Float128 widen(u64 bits, int fbits, int ebits)
{
    int emax = (1 << ebits) - 1, bias = emax >> 1;
    u64 s = bits >> (fbits + ebits) << 63;
    int e = (int)(bits >> fbits) & emax;
    U128 m;
    m.hi = 0;
    m.lo = bits & (((u64)1 << fbits) - 1);
    if (e == emax) {
        m = u128_shl(m, 112 - fbits);
        return make(s | EXPS | m.hi | ((m.hi | m.lo) ? QUIET : 0), m.lo);
    }
    if (e == 0) {
        int shift;
        if (!m.lo)
            return make(s, 0);
        shift = fbits - top64(m.lo);
        m.lo <<= shift;
        e = 1 - shift;
    }
    m = u128_shl(m, 112 - fbits);
    return make(s | (u64)(e - bias + BIAS) << 48 | (m.hi & FRAC), m.lo);
}

// To a binary format of `fbits` fraction and `ebits` exponent bits, rounded.
static u64 narrow(Float128 x, int fbits, int ebits)
{
    Num a    = unpack(x);
    int emax = (1 << ebits) - 1, bias = emax >> 1;
    u64 s    = (u64)a.sign << (fbits + ebits);
    u64 mask = ((u64)1 << fbits) - 1;
    int e, shift, c;
    U128 q, rem, half;
    u64 r;
    if (a.cls == F_NAN)
        return s | (u64)emax << fbits | (u64)1 << (fbits - 1) |
               (u128_shr(a.m, 112 - fbits).lo & mask);
    if (a.cls == F_INF)
        return s | (u64)emax << fbits;
    if (a.cls == F_ZERO)
        return s;
    e     = a.exp - BIAS + bias;
    shift = 112 - fbits;
    if (e <= 0) {
        // Subnormal: scaled as exponent 1, without the leading bit.
        shift += 1 - e;
        e = 1;
    }
    if (shift > 113)
        return s; // below half the least subnormal
    q       = u128_shr(a.m, shift);
    rem     = u128_sub(a.m, u128_shl(q, shift));
    half.hi = 0;
    half.lo = 1;
    half    = u128_shl(half, shift - 1);
    r       = q.lo;
    c       = u128_cmp(rem, half);
    if (c > 0 || (c == 0 && (r & 1)))
        r++;
    if (r >> (fbits + 1)) {
        r >>= 1;
        e++;
    }
    if (!(r >> fbits))
        e = 0;
    if (e >= emax)
        return s | (u64)emax << fbits;
    return s | (u64)e << fbits | (r & mask);
}

F128_API Float128 f128_from_double_bits(uint64_t bits)
{
    return widen(bits, 52, 11);
}

F128_API uint64_t f128_to_double_bits(Float128 x)
{
    return narrow(x, 52, 11);
}

F128_API Float128 f128_from_float_bits(uint32_t bits)
{
    return widen(bits, 23, 8);
}

F128_API uint32_t f128_to_float_bits(Float128 x)
{
    return (uint32_t)narrow(x, 23, 8);
}

#ifndef F128_RUNTIME
//
// Host only: double conversions, parsing and formatting.
//
#include <stdio.h>
#include <string.h>

#include "xalloc.h"

Float128 f128_from_double(double d)
{
    uint64_t bits;
    memcpy(&bits, &d, sizeof(bits));
    return f128_from_double_bits(bits);
}

double f128_to_double(Float128 x)
{
    uint64_t bits = f128_to_double_bits(x);
    double d;
    memcpy(&d, &bits, sizeof(d));
    return d;
}

float f128_to_float(Float128 x)
{
    uint32_t bits = f128_to_float_bits(x);
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

// An unsigned big integer, 32-bit limbs, least significant first.
typedef struct {
    uint32_t *d;
    int n, cap;
} Big;

static void big_reserve(Big *b, int n)
{
    if (n <= b->cap)
        return;
    int cap     = n * 2 + 4;
    uint32_t *d = xalloc(cap * sizeof(uint32_t), __func__, __FILE__, __LINE__);
    if (b->n)
        memcpy(d, b->d, b->n * sizeof(uint32_t));
    xfree(b->d);
    b->d   = d;
    b->cap = cap;
}

static void big_trim(Big *b)
{
    while (b->n > 0 && b->d[b->n - 1] == 0)
        b->n--;
}

// b = b * mul + add.
static void big_mul_add(Big *b, uint32_t mul, uint32_t add)
{
    uint64_t carry = add;
    for (int i = 0; i < b->n; i++) {
        uint64_t v = (uint64_t)b->d[i] * mul + carry;
        b->d[i]    = (uint32_t)v;
        carry      = v >> 32;
    }
    if (carry) {
        big_reserve(b, b->n + 1);
        b->d[b->n++] = (uint32_t)carry;
    }
}

static int big_bits(const Big *b)
{
    return b->n ? 32 * (b->n - 1) + top64(b->d[b->n - 1]) + 1 : 0;
}

static void big_shl(Big *b, int n)
{
    int words = n / 32, bits = n % 32;
    if (!b->n || n == 0)
        return;
    big_reserve(b, b->n + words + 1);
    b->d[b->n] = 0;
    for (int i = b->n; i >= 0; i--) {
        uint32_t v = b->d[i] << bits;
        if (bits && i > 0)
            v |= b->d[i - 1] >> (32 - bits);
        b->d[i + words] = v;
    }
    for (int i = 0; i < words; i++)
        b->d[i] = 0;
    b->n += words + 1;
    big_trim(b);
}

static void big_shr1(Big *b)
{
    for (int i = 0; i < b->n; i++)
        b->d[i] = b->d[i] >> 1 | (i + 1 < b->n ? b->d[i + 1] << 31 : 0);
    big_trim(b);
}

static int big_cmp(const Big *a, const Big *b)
{
    if (a->n != b->n)
        return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i])
            return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

// a -= b, where a >= b.
static void big_sub(Big *a, const Big *b)
{
    int64_t borrow = 0;
    for (int i = 0; i < a->n; i++) {
        int64_t v = (int64_t)a->d[i] - (i < b->n ? b->d[i] : 0) - borrow;
        borrow    = v < 0;
        a->d[i]   = (uint32_t)(v + (borrow << 32));
    }
    big_trim(a);
}

// a / b * 2^p, rounded; a and b are consumed.
static Float128 from_ratio(int sign, Big *a, Big *b, long p)
{
    // q = a * 2^k / b, of 126 or 127 bits; a remainder is sticky.
    long k = 126 + big_bits(b) - big_bits(a);
    U128 q = { 0, 0 };
    if (k >= 0)
        big_shl(a, (int)k);
    else
        big_shl(b, (int)-k);
    big_shl(b, 126);
    for (int i = 126; i >= 0; i--) {
        if (big_cmp(a, b) >= 0) {
            big_sub(a, b);
            if (i >= 64)
                q.hi |= (u64)1 << (i - 64);
            else
                q.lo |= (u64)1 << i;
        }
        big_shr1(b);
    }
    if (a->n)
        q.lo |= 1;
    xfree(a->d);
    xfree(b->d);
    long exp = BIAS + 125 + p - k;
    if (exp > 100000)
        exp = 100000;
    if (exp < -100000)
        exp = -100000;
    return round_pack(sign, (int)exp, q);
}

static int digit_value(char c, int base)
{
    int v = c >= '0' && c <= '9'   ? c - '0'
            : c >= 'a' && c <= 'f' ? c - 'a' + 10
            : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                   : 99;
    return v < base ? v : -1;
}

Float128 f128_from_string(const char *s, const char **end)
{
    const char *p = s;
    int sign      = 0;
    if (*p == '+' || *p == '-')
        sign = *p++ == '-';
    int hex = p[0] == '0' && (p[1] == 'x' || p[1] == 'X');
    if (hex)
        p += 2;
    int base = hex ? 16 : 10;

    // The digits as an integer; `scale` digits of them follow the point.
    Big a       = { 0 };
    long scale  = 0;
    int ndigits = 0, point = 0;
    for (;; p++) {
        if (*p == '.' && !point) {
            point = 1;
            continue;
        }
        int v = digit_value(*p, base);
        if (v < 0)
            break;
        if (a.n || v) {
            big_mul_add(&a, base, v);
            ndigits++;
        }
        if (point)
            scale++;
    }
    long exp = 0;
    if ((hex && (*p == 'p' || *p == 'P')) || (!hex && (*p == 'e' || *p == 'E'))) {
        const char *q = p + 1;
        int esign     = 1;
        if (*q == '+' || *q == '-')
            esign = *q++ == '-' ? -1 : 1;
        if (*q >= '0' && *q <= '9') {
            for (; *q >= '0' && *q <= '9'; q++)
                if (exp < 1000000)
                    exp = exp * 10 + (*q - '0');
            exp *= esign;
            p = q;
        }
    }
    if (end)
        *end = p;
    if (!a.n) {
        xfree(a.d);
        return signed_zero(sign);
    }

    Big b = { 0 };
    big_mul_add(&b, 1, 1);
    if (hex)
        return from_ratio(sign, &a, &b, exp - 4 * scale);
    long e10 = exp - scale;
    if (e10 + ndigits > 5000) {
        xfree(a.d);
        xfree(b.d);
        return infinity(sign);
    }
    if (e10 + ndigits < -5000) {
        xfree(a.d);
        xfree(b.d);
        return signed_zero(sign);
    }
    for (; e10 > 0; e10--)
        big_mul_add(&a, 10, 0);
    for (; e10 < 0; e10++)
        big_mul_add(&b, 10, 0);
    return from_ratio(sign, &a, &b, 0);
}

char *f128_format(Float128 x, char *buf)
{
    static const char hexdig[] = "0123456789abcdef";
    const char *sign           = x.hi & SIGN ? "-" : "";
    int e                      = (x.hi >> 48) & 0x7fff;
    if (f128_is_nan(x)) {
        strcpy(buf, "nan");
        return buf;
    }
    if (e == 0x7fff) {
        strcpy(buf, x.hi & SIGN ? "-inf" : "inf");
        return buf;
    }
    if (f128_is_zero(x)) {
        strcpy(buf, x.hi & SIGN ? "-0x0p+0" : "0x0p+0");
        return buf;
    }
    // 28 fraction digits: 12 of the high doubleword, 16 of the low; zeros trimmed.
    char frac[29];
    for (int i = 0; i < 12; i++)
        frac[i] = hexdig[(x.hi >> (44 - 4 * i)) & 15];
    for (int i = 0; i < 16; i++)
        frac[12 + i] = hexdig[(x.lo >> (60 - 4 * i)) & 15];
    int len = 28;
    while (len > 0 && frac[len - 1] == '0')
        len--;
    frac[len]    = 0;
    int unbiased = e ? e - BIAS : 1 - BIAS;
    sprintf(buf, "%s0x%d%s%sp%+d", sign, e ? 1 : 0, len ? "." : "", frac, unbiased);
    return buf;
}

Float128 f128_round(Float128 x, int mant_dig)
{
    Num a = unpack(x);
    int shift, c;
    U128 q, rem, half;
    if (a.cls != F_FINITE || mant_dig >= 113)
        return x;
    shift = 113 - mant_dig;
    if (a.exp < 1)
        shift += 1 - a.exp; // a subnormal keeps fewer bits, on the format's own grid
    if (shift > 113)
        return signed_zero(a.sign); // below half the least subnormal
    q       = u128_shr(a.m, shift);
    rem     = u128_sub(a.m, u128_shl(q, shift));
    half.hi = 0;
    half.lo = 1;
    half    = u128_shl(half, shift - 1);
    c       = u128_cmp(rem, half);
    if (c > 0 || (c == 0 && (q.lo & 1))) {
        U128 one;
        one.hi = 0;
        one.lo = 1;
        q      = u128_add(q, one);
    }
    // Exact from here: round_pack only renormalizes, or overflows to infinity.
    return round_pack(a.sign, a.exp, u128_shl(q, shift + 13));
}

void f128_to_x87(Float128 x, uint8_t out[10])
{
    Num a = unpack(f128_round(x, 64));
    u64 sig;
    int e;
    if (a.cls == F_NAN) {
        e   = 0x7fff;
        sig = SIGN | 0x4000000000000000ULL | u128_shr(a.m, 49).lo;
    } else if (a.cls == F_INF) {
        e   = 0x7fff;
        sig = SIGN;
    } else if (a.cls == F_ZERO) {
        e   = 0;
        sig = 0;
    } else if (a.exp >= 1) {
        e   = a.exp;
        sig = u128_shr(a.m, 49).lo;
    } else {
        // Denormal: exponent field 0, scaled as exponent 1 without the integer bit.
        e   = 0;
        sig = u128_shr(a.m, 49 + 1 - a.exp).lo;
    }
    for (int i = 0; i < 8; i++)
        out[i] = (uint8_t)(sig >> (8 * i));
    out[8] = (uint8_t)e;
    out[9] = (uint8_t)(e >> 8 | a.sign << 7);
}

Float128 f128_from_x87(const uint8_t in[10])
{
    u64 sig = 0;
    int e   = (in[9] & 0x7f) << 8 | in[8];
    int s   = in[9] >> 7;
    U128 m;
    for (int i = 0; i < 8; i++)
        sig |= (u64)in[i] << (8 * i);
    if (e == 0x7fff) {
        if (!(sig & SIGN))
            return default_nan(); // pseudo-infinity or pseudo-NaN
        if (!(sig << 1))
            return infinity(s);
        m = u128_shl((U128){ 0, sig & ~SIGN }, 49);
        return make((u64)s << 63 | EXPS | m.hi, m.lo);
    }
    if (e && !(sig & SIGN))
        return default_nan(); // unnormal
    // sig * 2^(e - 16383 - 63), a (pseudo-)denormal taken as exponent 1.
    m.hi = sig >> 2;
    m.lo = sig << 62;
    return round_pack(s, e ? e : 1, m);
}
#endif
