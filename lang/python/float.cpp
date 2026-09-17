// float: a boxed f64, printed the way CPython prints one.
#include "math/ftoa.h"
#include "math/math.h"
#include "ops.h"

namespace {

bool float_truth(Value v)
{
    return float_of(v) != 0;
}

R float_hash(Value v, u32 &out)
{
    f64 x = float_of(v);
    // An integral float must hash as the equal int, or {1: 'a'}[1.0] misses.
    if (x == floor(x) && x >= -9.2e18 && x <= 9.2e18) {
        out = u32(i64(x));
        return R::Ok;
    }
    u64 bits;
    __builtin_memcpy(&bits, &x, sizeof bits);
    out = u32(bits) ^ u32(bits >> 32);
    return R::Ok;
}

R float_repr(Value v, String &out)
{
    char tmp[48];
    Str s = float_text(tmp, sizeof tmp, float_of(v));
    return out.append(s) ? R::Ok : err_set("MemoryError", "out of memory");
}

} // namespace

// A float is its own real part.
R float_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() == "real")
        out = v;
    else if (name->str() == "imag")
        out = float_new(0.0);
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Type float_type{ .name    = "float",
                           .truth   = float_truth,
                           .hash    = float_hash,
                           .repr    = float_repr,
                           .getattr = float_getattr,
                           .patma   = PATMA_SELF };

Value float_new(f64 x)
{
    FloatObj *o = static_cast<FloatObj *>(obj_alloc(&float_type, sizeof(FloatObj)));
    if (!o) {
        err_set("MemoryError", "out of memory");
        return Value();
    }
    o->v = x;
    return obj_value(o);
}

// The shortest round-trip digits of `s`, and where the decimal point goes:
// the value is 0.<digits> times ten to the decpt.
void split(Str s, bool &neg, char *digits, usize &ndig, i32 &decpt)
{
    usize i = 0;
    neg     = i < s.size() && s[i] == '-';
    if (neg || (i < s.size() && s[i] == '+'))
        i++;

    ndig       = 0;
    i32 before = 0;
    bool point = false;
    for (; i < s.size(); i++) {
        if (s[i] == '.') {
            point = true;
            continue;
        }
        if (s[i] < '0' || s[i] > '9')
            break;
        if (!point)
            before++;
        if (ndig < 24)
            digits[ndig++] = s[i];
    }

    i32 exp = 0;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        bool eneg = i < s.size() && s[i] == '-';
        if (eneg || (i < s.size() && s[i] == '+'))
            i++;
        for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; i++)
            exp = exp * 10 + (s[i] - '0');
        if (eneg)
            exp = -exp;
    }

    decpt    = before + exp;
    usize at = 0;
    while (at < ndig && digits[at] == '0') {
        at++;
        decpt--;
    }
    for (usize k = 0; k + at < ndig; k++)
        digits[k] = digits[k + at];
    ndig -= at;
    while (ndig > 1 && digits[ndig - 1] == '0')
        ndig--;
    if (ndig == 0 || digits[0] == '0') {
        ndig      = 1;
        digits[0] = '0';
        decpt     = 1;
    }
}

// CPython's repr: shortest digits, exponent form when decpt <= -4 or > 16,
// always a `.0` or an exponent, and the exponent at least two digits.
Str float_text(char *out, usize cap, f64 x)
{
    if (isnan(x))
        return "nan";
    if (isinf(x))
        return x < 0 ? "-inf" : "inf";

    char tmp[48];
    char digits[26];
    usize ndig = 0;
    i32 decpt  = 0;
    bool neg   = false;
    split(fmt_f64_shortest(tmp, sizeof tmp, x), neg, digits, ndig, decpt);

    Buf<48> b;
    if (neg)
        b.put('-');
    if (decpt <= -4 || decpt > 16) {
        b.put(digits[0]);
        if (ndig > 1) {
            b.put('.');
            b.put(Str(digits + 1, ndig - 1));
        }
        i32 e = decpt - 1;
        b.put('e').put(e < 0 ? '-' : '+');
        u32 m = u32(e < 0 ? -e : e);
        if (m < 10)
            b.put('0');
        b.put(u64(m));
    } else if (decpt <= 0) {
        b.put("0.");
        for (i32 k = 0; k < -decpt; k++)
            b.put('0');
        b.put(Str(digits, ndig));
    } else if (usize(decpt) >= ndig) {
        b.put(Str(digits, ndig));
        for (usize k = ndig; k < usize(decpt); k++)
            b.put('0');
        b.put(".0");
    } else {
        b.put(Str(digits, usize(decpt)));
        b.put('.');
        b.put(Str(digits + decpt, ndig - usize(decpt)));
    }

    usize n = b.str().size() < cap ? b.str().size() : cap;
    for (usize k = 0; k < n; k++)
        out[k] = b.str()[k];
    return Str(out, n);
}
