#include "number.h"

namespace {

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

// A digit's value in base 10 or 16, or -1.
i32 digit(char c, bool hex)
{
    if (is_digit(c))
        return c - '0';
    char l = char(c | 0x20);
    if (hex && l >= 'a' && l <= 'f')
        return l - 'a' + 10;
    return -1;
}

// The magnitude of an integer: digits with single underscores between
// them, 0x for hex. False when not one, or above `max`.
bool magnitude(Str s, u64 max, u64 &v)
{
    bool hex = s.starts_with("0x");
    usize i  = hex ? 2 : 0;
    u64 base = hex ? 16 : 10;
    if (i == s.size())
        return false;
    v          = 0;
    bool under = true; // at the start, as after an underscore
    for (; i < s.size(); i++) {
        if (s[i] == '_') {
            if (under)
                return false;
            under = true;
            continue;
        }
        i32 d = digit(s[i], hex);
        if (d < 0 || v > (max - u64(d)) / base)
            return false;
        v     = v * base + u64(d);
        under = false;
    }
    return !under;
}

// ------------------------------------------------------------ big integers

// Unsigned and of fixed size, enough for any float literal once it is cut
// to size (see Digits). A plain aggregate, so the ones below are zeroed
// statics rather than stack. Running out of limbs is sticky.
struct Big {
    static constexpr u32 LIMBS = 180; // 5760 bits
    u32 d[LIMBS];
    u32 n; // limbs in use; d[n-1] != 0
    bool over;

    void set(u32 v)
    {
        d[0] = v;
        n    = v ? 1 : 0;
        over = false;
    }

    void trim()
    {
        while (n && !d[n - 1])
            n--;
    }

    // this = this * m + a
    void mul_add(u32 m, u32 a)
    {
        u64 c = a;
        for (u32 k = 0; k < n; k++) {
            c += u64(d[k]) * m;
            d[k] = u32(c);
            c >>= 32;
        }
        if (!c)
            return;
        if (n == LIMBS)
            over = true;
        else
            d[n++] = u32(c);
    }

    u32 bits() const { return n ? 32 * n - u32(__builtin_clz(d[n - 1])) : 0; }

    // this = x << s
    void shl(const Big &x, u32 s)
    {
        u32 w = s / 32, b = s % 32;
        over = x.over || x.n + w + 1 > LIMBS;
        n    = 0;
        if (over)
            return;
        for (u32 k = 0; k < x.n + w + 1; k++)
            d[k] = 0;
        for (u32 k = 0; k < x.n; k++) {
            d[k + w] |= x.d[k] << b;
            if (b)
                d[k + w + 1] |= x.d[k] >> (32 - b);
        }
        n = x.n + w + 1;
        trim();
    }

    void shr1()
    {
        for (u32 k = 0; k < n; k++)
            d[k] = (d[k] >> 1) | (k + 1 < n ? d[k + 1] << 31 : 0);
        trim();
    }

    // this -= x, where x <= this
    void sub(const Big &x)
    {
        i64 c = 0;
        for (u32 k = 0; k < n; k++) {
            c += i64(d[k]) - (k < x.n ? i64(x.d[k]) : 0);
            d[k] = u32(c);
            c >>= 32;
        }
        trim();
    }
};

i32 cmp(const Big &a, const Big &b)
{
    if (a.n != b.n)
        return a.n < b.n ? -1 : 1;
    for (u32 k = a.n; k-- > 0;)
        if (a.d[k] != b.d[k])
            return a.d[k] < b.d[k] ? -1 : 1;
    return 0;
}

Big g_num, g_den, g_a, g_b, g_t;

// ------------------------------------------------------------ floats

struct Format {
    u32 width;
    u32 p;    // precision, the hidden bit included
    i32 emin; // the least normal exponent
    i32 emax; // the greatest, and the bias
};

constexpr Format F32{ 32, 24, -126, 127 };
constexpr Format F64{ 64, 53, -1022, 1023 };

enum class Got { Value, Zero, Range, Bad };

// The significant digits of a literal and the power of the base they are
// scaled by. Past KEEP digits the rest becomes one nonzero digit if it is
// not all zeros. That keeps the value on the same side of every value and
// halfway point of f32 and f64, none of which has more than 767 decimal or
// 14 hex significant digits, so it rounds the same.
struct Digits {
    static constexpr u32 KEEP_DEC = 800;
    static constexpr u32 KEEP_HEX = 32;
    u8 s[KEEP_DEC + 1];
    u32 n       = 0;
    u32 keep    = 0;
    i64 scale   = 0; // in digits
    bool sticky = false;

    void add(u32 d, bool frac)
    {
        if (frac)
            scale--;
        if (n == 0 && d == 0)
            return; // a leading zero
        if (n < keep) {
            s[n++] = u8(d);
            return;
        }
        scale++;
        sticky |= d != 0;
    }
};

Digits g_digits;

// digit ('_'? digit)* from s[i], each digit into `g`; false for none. An
// underscore not followed by a digit is left for the caller to refuse.
bool run(Str s, usize &i, bool hex, Digits &g, bool frac)
{
    if (i >= s.size() || digit(s[i], hex) < 0)
        return false;
    for (;;) {
        g.add(u32(digit(s[i], hex)), frac);
        usize k = i + 1;
        if (k < s.size() && s[k] == '_')
            k++;
        if (k >= s.size() || digit(s[k], hex) < 0) {
            i++;
            return true;
        }
        i = k;
    }
}

// A finite literal without its sign: its digits, and in `g.scale` the
// power of 10 for a decimal one and of 2 for a hex one.
Got split(Str s, bool &hex, Digits &g)
{
    hex     = s.starts_with("0x");
    g.keep  = hex ? Digits::KEEP_HEX : Digits::KEEP_DEC;
    usize i = hex ? 2 : 0;
    if (!run(s, i, hex, g, false))
        return Got::Bad;
    if (i < s.size() && s[i] == '.') {
        i++;
        run(s, i, hex, g, true);
    }
    i64 exp = 0;
    if (i < s.size() && (s[i] | 0x20) == (hex ? 'p' : 'e')) {
        i++;
        bool neg = i < s.size() && s[i] == '-';
        if (i < s.size() && (s[i] == '-' || s[i] == '+'))
            i++;
        Digits e; // only its count matters: saturate rather than keep
        usize k = i;
        if (!run(s, i, false, e, false))
            return Got::Bad;
        for (; k < i; k++)
            if (s[k] != '_' && exp < 1'000'000'000)
                exp = exp * 10 + (s[k] - '0');
        if (neg)
            exp = -exp;
    }
    if (i != s.size())
        return Got::Bad;
    if (g.sticky) {
        g.s[g.n++] = 1;
        g.scale--;
    }
    if (!g.n)
        return Got::Zero;
    g.scale = hex ? g.scale * 4 + exp : g.scale + exp;
    return Got::Value;
}

// num * 2^b2 / den, which is not zero, rounded into `f`: its bits, the
// sign aside. The temporaries stay within max(num, den) + p + 3 bits.
Got round(const Big &num, const Big &den, i64 b2, const Format &f, u64 &out)
{
    // e, the exponent of the leading bit, exactly.
    u32 a = num.bits(), dl = den.bits();
    i64 lg;
    if (a >= dl) {
        g_t.shl(den, a - dl);
        lg = cmp(num, g_t) >= 0 ? i64(a) - dl : i64(a) - dl - 1;
    } else {
        g_t.shl(num, dl - a);
        lg = cmp(g_t, den) >= 0 ? i64(a) - dl : i64(a) - dl - 1;
    }
    i64 e = lg + b2;
    if (e > f.emax)
        return Got::Range;
    if (e < f.emin - i64(f.p) - 1)
        return Got::Zero; // below half the least subnormal
    // q, the value in units of the last place, and what is left over.
    i64 u = (e > f.emin ? e : f.emin) - (f.p - 1);
    i64 s = b2 - u;
    g_a.shl(num, u32(s > 0 ? s : 0));
    g_b.shl(den, u32(s < 0 ? -s : 0));
    g_t.shl(g_b, f.p);
    if (g_a.over || g_t.over)
        return Got::Range;
    u64 q = 0;
    for (u32 i = f.p; i-- > 0;) {
        g_t.shr1();
        if (cmp(g_a, g_t) >= 0) {
            g_a.sub(g_t);
            q |= u64(1) << i;
        }
    }
    g_t.shl(g_a, 1);
    i32 c = cmp(g_t, g_b);
    if (c > 0 || (c == 0 && (q & 1)))
        q++;
    if (q == u64(1) << f.p) {
        q >>= 1;
        u++;
    }
    if (!q)
        return Got::Zero;
    if (q < u64(1) << (f.p - 1)) {
        out = q; // subnormal
        return Got::Value;
    }
    i64 exp = u + f.p - 1;
    if (exp > f.emax)
        return Got::Range;
    out = u64(exp + f.emax) << (f.p - 1) | (q - (u64(1) << (f.p - 1)));
    return Got::Value;
}

bool parse_float(Str s, const Format &f, u64 &bits)
{
    bool neg = s.starts_with("-");
    if (neg || s.starts_with("+"))
        s = s.substr(1);
    u64 sign = neg ? u64(1) << (f.width - 1) : 0;
    u64 inf  = u64(2 * f.emax + 1) << (f.p - 1);
    u64 frac = (u64(1) << (f.p - 1)) - 1;
    if (s == "inf") {
        bits = sign | inf;
        return true;
    }
    if (s == "nan") {
        bits = sign | inf | u64(1) << (f.p - 2);
        return true;
    }
    if (s.starts_with("nan:")) {
        u64 payload;
        if (!s.substr(4).starts_with("0x") || !magnitude(s.substr(4), frac, payload) || !payload)
            return false;
        bits = sign | inf | payload;
        return true;
    }
    Digits &g = g_digits;
    g         = Digits();
    bool hex;
    Got got = split(s, hex, g);
    u64 mag = 0;
    if (got == Got::Value) {
        g_num.set(0);
        g_den.set(1);
        for (u32 k = 0; k < g.n; k++)
            g_num.mul_add(hex ? 16 : 10, g.s[k]);
        i64 b2 = 0;
        if (hex) {
            b2    = g.scale;
            i64 a = g_num.bits();
            if (a + b2 - 1 > f.emax)
                got = Got::Range;
            else if (a + b2 < -1200)
                got = Got::Zero;
        } else if (i64(g.n) + g.scale - 1 >= 310) {
            got = Got::Range; // at least 10^309
        } else if (i64(g.n) + g.scale <= -330) {
            got = Got::Zero; // below 10^-330
        } else {
            Big &by = g.scale >= 0 ? g_num : g_den;
            for (i64 k = g.scale >= 0 ? g.scale : -g.scale; k > 0; k--)
                by.mul_add(10, 0);
        }
        if (got == Got::Value)
            got = g_num.over || g_den.over ? Got::Range : round(g_num, g_den, b2, f, mag);
    }
    if (got == Got::Range || got == Got::Bad)
        return false;
    bits = sign | (got == Got::Zero ? 0 : mag);
    return true;
}

} // namespace

bool parse_uint(Str s, u32 bits, u64 &v)
{
    u64 max = bits == 64 ? ~u64(0) : (u64(1) << bits) - 1;
    return magnitude(s, max, v);
}

bool parse_int(Str s, u32 bits, u64 &v)
{
    bool neg = s.starts_with("-");
    if (neg || s.starts_with("+"))
        s = s.substr(1);
    u64 mask = bits == 64 ? ~u64(0) : (u64(1) << bits) - 1;
    u64 max  = neg ? u64(1) << (bits - 1) : mask;
    if (!magnitude(s, max, v))
        return false;
    if (neg)
        v = (~v + 1) & mask;
    return true;
}

bool parse_f32(Str s, u32 &bits)
{
    u64 b;
    if (!parse_float(s, F32, b))
        return false;
    bits = u32(b);
    return true;
}

bool parse_f64(Str s, u64 &bits)
{
    return parse_float(s, F64, bits);
}
