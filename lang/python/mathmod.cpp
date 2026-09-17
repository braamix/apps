// `math` and `cmath` over braam::math, which is C99 §7.12 answered by musl.
//
// The floating half is a table: one name, one libm call, one error rule --
// a domain error is ValueError and a range error OverflowError, worked out
// from the answer rather than from errno, which does not exist here. The
// integer half is the interesting part: factorial, comb, gcd and isqrt are
// exact, so they run over bigint.h and never touch a double.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "complex.h"
#include "gc.h"
#include "import.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "math/math.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr i64 I64_MAX = 0x7fffffffffffffffll;
constexpr i64 I64_MIN = -I64_MAX - 1;

constexpr f64 PI  = 3.141592653589793;
constexpr f64 E   = 2.718281828459045;
constexpr f64 TAU = 6.283185307179586;

bool is_nan(f64 x)
{
    return x != x;
}

bool is_inf(f64 x)
{
    return x == __builtin_inf() || x == -__builtin_inf();
}

// A float argument: PyFloat_AsDouble. An integer past the float range is an
// OverflowError, as CPython's conversion says.
bool to_f64(Value v, f64 &out)
{
    if (is_big(v)) {
        out = int_to_f64(v);
        if (is_inf(out))
            return err_set("OverflowError", "int too large to convert to float") == R::Ok;
        return true;
    }
    if (as_number(v, out))
        return true;
    Buf<96> b;
    b.put("must be real number, not ").put(type_name(v));
    return err_set("TypeError", b.str()) == R::Ok;
}

bool arg_f64(const CallArgs &a, Str, u32 at, f64 &out)
{
    return to_f64(a.args[at], out);
}

// Arguments whose class writes __float__ in Python: each is converted by a
// call, and `again` entered with the answers. False when none needs it.
bool float_args(const CallArgs &a, u32 n, R (*again)(const CallArgs &, Value &out), Value &out,
                R &r)
{
    for (u32 i = 0; i < n && i < a.nargs; i++)
        if (!is_float(a.args[i]) && !is_intval(a.args[i]) &&
            (redo_converted(a, i, "__float__", again, out, r) ||
             redo_converted(a, i, "__index__", again, out, r)))
            return true;
    return false;
}

// The float repr a domain error message quotes.
Str repr_of(char *buf, usize cap, f64 x)
{
    return float_text(buf, cap, x);
}

// The C library's errno, which only the functions ported from CPython set.
enum : u8 { E_NONE, E_DOM, E_RANGE };

u8 math_errno;

// is_error: an errno turned into the exception, or not.
bool is_error(f64 x, bool raise_edom)
{
    if (math_errno == E_DOM) {
        if (raise_edom)
            err_set("ValueError", "math domain error");
        return true;
    }
    if (math_errno == E_RANGE) {
        if (fabs(x) < 1.5)
            return false;
        err_set("OverflowError", "math range error");
        return true;
    }
    return false;
}

// ---------------------------------------------- what CPython computes itself

constexpr f64 LOGPI = 1.144729885849400174143427351353058711647;

f64 m_sinpi(f64 x)
{
    if (!isfinite(x))
        return sin(x);
    f64 y = fmod(fabs(x), 2.0);
    i32 n = i32(round(2.0 * y));
    f64 r = 0;
    switch (n) {
    case 0:
        r = sin(PI * y);
        break;
    case 1:
        r = cos(PI * (y - 0.5));
        break;
    case 2:
        r = sin(PI * (1.0 - y));
        break;
    case 3:
        r = -cos(PI * (y - 1.5));
        break;
    default:
        r = sin(PI * (y - 2.0));
        break;
    }
    return copysign(1.0, x) * r;
}

f64 m_cospi(f64 x)
{
    if (!isfinite(x))
        return cos(x);
    x = fabs(x - 2.0 * round(0.5 * x));
    if (x <= 0.25)
        return cos(PI * x);
    if (x == 0.5)
        return 0.0;
    if (x <= 0.75)
        return sin(PI * (0.5 - x));
    return -cos(PI * (1.0 - x));
}

f64 m_tanpi(f64 x)
{
    if (!isfinite(x))
        return tan(x);
    f64 y    = x - 2.0 * round(0.5 * x);
    f64 absy = fabs(y);
    if (absy == 0.0)
        return copysign(0.0, x);
    if (absy == 1.0)
        return copysign(0.0, -x);
    if (absy == 0.5) {
        math_errno = E_RANGE;
        return 1.0 / copysign(0.0, y);
    }
    if (absy > 0.5) {
        y -= copysign(1.0, y);
        absy = fabs(y);
    }
    if (absy <= 0.25)
        return tan(PI * y);
    return copysign(1.0 / tan(PI * (0.5 - absy)), y);
}

f64 m_acospi(f64 x)
{
    if (x >= 0.5)
        return 2.0 * asin(sqrt((1.0 - x) / 2.0)) / PI;
    f64 r = acos(x) / PI;
    return r > 1.0 ? 1.0 : r;
}

f64 m_asinpi(f64 x)
{
    f64 r = asin(x) / PI;
    return fabs(r) > 0.5 ? copysign(0.5, r) : r;
}

f64 m_atanpi(f64 x)
{
    f64 r = atan(x) / PI;
    return fabs(r) > 0.5 ? copysign(0.5, r) : r;
}

f64 m_atan2pi(f64 y, f64 x)
{
    f64 r = atan2(y, x) / PI;
    return fabs(r) > 1.0 ? copysign(1.0, r) : r;
}

f64 m_log1p(f64 x)
{
    return x == 0.0 ? x : log1p(x);
}

// Lanczos, with Boost's parameters: CPython's own gamma, not the platform's.
constexpr i32 LANCZOS_N            = 13;
constexpr f64 LANCZOS_G            = 6.024680040776729583740234375;
constexpr f64 LANCZOS_G_MINUS_HALF = 5.524680040776729583740234375;

constexpr f64 LANCZOS_NUM[LANCZOS_N] = {
    23531376880.410759688572007674451636754734846804940,
    42919803642.649098768957899047001988850926355848959,
    35711959237.355668049440185451547166705960488635843,
    17921034426.037209699919755754458931112671403265390,
    6039542586.3520280050642916443072979210699388420708,
    1439720407.3117216736632230727949123939715485786772,
    248874557.86205415651146038641322942321632125127801,
    31426415.585400194380614231628318205362874684987640,
    2876370.6289353724412254090516208496135991145378768,
    186056.26539522349504029498971604569928220784236328,
    8071.6720023658162106380029022722506138218516325024,
    210.82427775157934587250973392071336271166969580291,
    2.5066282746310002701649081771338373386264310793408,
};

constexpr f64 LANCZOS_DEN[LANCZOS_N] = { 0.0,         39916800.0, 120543840.0, 150917976.0,
                                         105258076.0, 45995730.0, 13339535.0,  2637558.0,
                                         357423.0,    32670.0,    1925.0,      66.0,
                                         1.0 };

constexpr i32 NGAMMA_INTEGRAL = 23;

constexpr f64 GAMMA_INTEGRAL[NGAMMA_INTEGRAL] = {
    1.0,
    1.0,
    2.0,
    6.0,
    24.0,
    120.0,
    720.0,
    5040.0,
    40320.0,
    362880.0,
    3628800.0,
    39916800.0,
    479001600.0,
    6227020800.0,
    87178291200.0,
    1307674368000.0,
    20922789888000.0,
    355687428096000.0,
    6402373705728000.0,
    121645100408832000.0,
    2432902008176640000.0,
    51090942171709440000.0,
    1124000727777607680000.0,
};

f64 lanczos_sum(f64 x)
{
    f64 num = 0.0, den = 0.0;
    if (x < 5.0) {
        for (i32 i = LANCZOS_N; --i >= 0;) {
            num = num * x + LANCZOS_NUM[i];
            den = den * x + LANCZOS_DEN[i];
        }
    } else {
        for (i32 i = 0; i < LANCZOS_N; i++) {
            num = num / x + LANCZOS_NUM[i];
            den = den / x + LANCZOS_DEN[i];
        }
    }
    return num / den;
}

f64 m_tgamma(f64 x)
{
    if (!isfinite(x)) {
        if (is_nan(x) || x > 0.0)
            return x;
        math_errno = E_DOM;
        return __builtin_nan("");
    }
    if (x == 0.0) {
        math_errno = E_DOM;
        return copysign(__builtin_inf(), x);
    }
    if (x == floor(x)) {
        if (x < 0.0) {
            math_errno = E_DOM;
            return __builtin_nan("");
        }
        if (x <= NGAMMA_INTEGRAL)
            return GAMMA_INTEGRAL[i32(x) - 1];
    }
    f64 absx = fabs(x);
    if (absx < 1e-20) {
        f64 r = 1.0 / x;
        if (is_inf(r))
            math_errno = E_RANGE;
        return r;
    }
    if (absx > 200.0) {
        if (x < 0.0)
            return 0.0 / m_sinpi(x);
        math_errno = E_RANGE;
        return __builtin_inf();
    }
    f64 y = absx + LANCZOS_G_MINUS_HALF;
    f64 z;
    if (absx > LANCZOS_G_MINUS_HALF) {
        f64 q = y - absx;
        z     = q - LANCZOS_G_MINUS_HALF;
    } else {
        f64 q = y - LANCZOS_G_MINUS_HALF;
        z     = q - absx;
    }
    z     = z * LANCZOS_G / y;
    f64 r = 0;
    if (x < 0.0) {
        r = -PI / m_sinpi(absx) / absx * exp(y) / lanczos_sum(absx);
        r -= z * r;
        if (absx < 140.0) {
            r /= pow(y, absx - 0.5);
        } else {
            f64 sqrtpow = pow(y, absx / 2.0 - 0.25);
            r /= sqrtpow;
            r /= sqrtpow;
        }
    } else {
        r = lanczos_sum(absx) / exp(y);
        r += z * r;
        if (absx < 140.0) {
            r *= pow(y, absx - 0.5);
        } else {
            f64 sqrtpow = pow(y, absx / 2.0 - 0.25);
            r *= sqrtpow;
            r *= sqrtpow;
        }
    }
    if (is_inf(r))
        math_errno = E_RANGE;
    return r;
}

f64 m_lgamma(f64 x)
{
    if (!isfinite(x))
        return is_nan(x) ? x : __builtin_inf();
    if (x == floor(x) && x <= 2.0) {
        if (x <= 0.0) {
            math_errno = E_DOM;
            return __builtin_inf();
        }
        return 0.0;
    }
    f64 absx = fabs(x);
    if (absx < 1e-20)
        return -log(absx);
    f64 r = log(lanczos_sum(absx)) - LANCZOS_G;
    r += (absx - 0.5) * (log(absx + LANCZOS_G - 0.5) - 1);
    if (x < 0.0)
        r = LOGPI - log(fabs(m_sinpi(absx))) - log(absx) - r;
    if (is_inf(r))
        math_errno = E_RANGE;
    return r;
}

f64 m_remainder(f64 x, f64 y)
{
    if (isfinite(x) && isfinite(y)) {
        if (y == 0.0)
            return __builtin_nan("");
        f64 absx = fabs(x), absy = fabs(y);
        f64 m = fmod(absx, absy);
        f64 c = absy - m;
        f64 r;
        if (m < c) {
            r = m;
        } else if (m > c) {
            r = -c;
        } else {
            r = m - 2.0 * fmod(0.5 * (absx - m), absy);
        }
        return copysign(1.0, x) * r;
    }
    if (is_nan(x))
        return x;
    if (is_nan(y))
        return y;
    if (is_inf(x))
        return __builtin_nan("");
    return x;
}

// ------------------------------------------------------ extended precision

struct DoubleLength {
    f64 hi, lo;
};

DoubleLength dl_fast_sum(f64 a, f64 b)
{
    f64 x = a + b;
    f64 y = (a - x) + b;
    return { x, y };
}

DoubleLength dl_sum(f64 a, f64 b)
{
    f64 x = a + b;
    f64 z = x - a;
    f64 y = (a - (x - z)) + (b - z);
    return { x, y };
}

DoubleLength dl_mul(f64 x, f64 y)
{
    f64 z  = x * y;
    f64 zz = fma(x, y, -z);
    return { z, zz };
}

struct TripleLength {
    f64 hi, lo, tiny;
};

TripleLength tl_fma(f64 x, f64 y, TripleLength total)
{
    DoubleLength pr = dl_mul(x, y);
    DoubleLength sm = dl_sum(total.hi, pr.hi);
    DoubleLength r1 = dl_sum(total.lo, pr.lo);
    DoubleLength r2 = dl_sum(r1.hi, sm.lo);
    return { sm.hi, r2.hi, total.tiny + r1.lo + r2.lo };
}

f64 tl_to_d(TripleLength total)
{
    DoubleLength last = dl_sum(total.lo, total.hi);
    return total.tiny + last.lo + last.hi;
}

// The Euclidean norm, as CPython's vector_norm: lossless scaling, lossless
// squaring, and a differential correction at the end.
f64 vector_norm(usize n, f64 *vec, f64 max, bool found_nan)
{
    if (is_inf(max))
        return max;
    if (found_nan)
        return __builtin_nan("");
    if (max == 0.0 || n <= 1)
        return max;
    int max_e = 0;
    frexp(max, &max_e);
    if (max_e < -1023) {
        constexpr f64 DBL_MIN = 2.2250738585072014e-308;
        for (usize i = 0; i < n; i++)
            vec[i] /= DBL_MIN;
        return DBL_MIN * vector_norm(n, vec, max / DBL_MIN, found_nan);
    }
    f64 scale = ldexp(1.0, -max_e);
    f64 csum = 1.0, frac1 = 0.0, frac2 = 0.0;
    for (usize i = 0; i < n; i++) {
        f64 x           = vec[i] * scale;
        DoubleLength pr = dl_mul(x, x);
        DoubleLength sm = dl_fast_sum(csum, pr.hi);
        csum            = sm.hi;
        frac1 += pr.lo;
        frac2 += sm.lo;
    }
    f64 h           = sqrt(csum - 1.0 + (frac1 + frac2));
    DoubleLength pr = dl_mul(-h, h);
    DoubleLength sm = dl_fast_sum(csum, pr.hi);
    csum            = sm.hi;
    frac1 += pr.lo;
    frac2 += sm.lo;
    f64 x = csum - 1.0 + (frac1 + frac2);
    h += x / (2.0 * h);
    return h / scale;
}

// ------------------------------------------------- the one-argument table

using Fn1 = double (*)(double);

// FUNC1, FUNC1D, FUNC1A and FUNC1AD: what an answer out of range means.
enum : u8 {
    K_PLAIN, // a NaN or an infinity from finite input is a domain error
    K_OVER,  // ... but an infinity is an overflow
    K_ERRNO, // the function says, through math_errno
};

struct Unary {
    Str name;
    Fn1 fn;
    u8 kind;
    Str msg; // the domain error's message, with the argument; or none
};

constexpr Str MSG_UNIT  = "expected a number in range from -1 up to 1, got ";
constexpr Str MSG_FIN   = "expected a finite input, got ";
constexpr Str MSG_GAMMA = "expected a noninteger or positive integer, got ";

constexpr Unary UNARY[] = {
    { "acos", acos, K_PLAIN, MSG_UNIT },
    { "acosh", acosh, K_PLAIN, "expected argument value not less than 1, got " },
    { "acospi", m_acospi, K_PLAIN, MSG_UNIT },
    { "asin", asin, K_PLAIN, MSG_UNIT },
    { "asinh", asinh, K_PLAIN, "" },
    { "asinpi", m_asinpi, K_PLAIN, MSG_UNIT },
    { "atan", atan, K_PLAIN, "" },
    { "atanh", atanh, K_PLAIN, "expected a number between -1 and 1, got " },
    { "atanpi", m_atanpi, K_PLAIN, MSG_UNIT },
    { "cbrt", cbrt, K_PLAIN, "" },
    { "cos", cos, K_PLAIN, MSG_FIN },
    { "cosh", cosh, K_OVER, "" },
    { "cospi", m_cospi, K_PLAIN, MSG_FIN },
    { "erf", erf, K_ERRNO, "" },
    { "erfc", erfc, K_ERRNO, "" },
    { "exp", exp, K_OVER, "" },
    { "exp2", exp2, K_OVER, "" },
    { "expm1", expm1, K_OVER, "" },
    { "fabs", fabs, K_PLAIN, "" },
    { "gamma", m_tgamma, K_ERRNO, MSG_GAMMA },
    { "lgamma", m_lgamma, K_ERRNO, MSG_GAMMA },
    { "log1p", m_log1p, K_PLAIN, "expected argument value > -1, got " },
    { "sin", sin, K_PLAIN, MSG_FIN },
    { "sinh", sinh, K_OVER, "" },
    { "sinpi", m_sinpi, K_PLAIN, MSG_FIN },
    { "sqrt", sqrt, K_PLAIN, "expected a nonnegative input, got " },
    { "tan", tan, K_PLAIN, MSG_FIN },
    { "tanh", tanh, K_PLAIN, "" },
    { "tanpi", m_tanpi, K_PLAIN, "expected a finite input not equal to a half-integer, got " },
};

R domain_err(Str msg, f64 x)
{
    if (msg.empty())
        return err_set("ValueError", "math domain error");
    char tmp[32];
    Buf<128> b;
    b.put(msg).put(repr_of(tmp, sizeof tmp, x));
    return err_set("ValueError", b.str());
}

// math_1 and math_1a.
R math_1(f64 x, const Unary &u, Value &out)
{
    math_errno = E_NONE;
    f64 r      = u.fn(x);
    if (u.kind == K_ERRNO) {
        if (math_errno) {
            if (!u.msg.empty() && math_errno == E_DOM)
                return domain_err(u.msg, x);
            if (is_error(r, u.msg.empty()))
                return R::Err;
        }
    } else {
        if (is_nan(r) && !is_nan(x))
            return domain_err(u.msg, x);
        if (is_inf(r) && isfinite(x)) {
            if (u.kind == K_OVER)
                return err_set("OverflowError", "math range error");
            return domain_err(u.msg, x);
        }
    }
    out = float_new(r);
    return out.is_nil() ? R::Err : R::Ok;
}

template <usize I>
R unary_at(const CallArgs &a, Value &out)
{
    const Unary &u = UNARY[I];
    if (!args_only(a, u.name, 1, 1))
        return R::Err;
    R r = R::Ok;
    if (float_args(a, 1, unary_at<I>, out, r))
        return r;
    f64 x = 0;
    if (!to_f64(a.args[0], x))
        return R::Err;
    return math_1(x, u, out);
}

// ------------------------------------------------- two arguments and more

// math_2: the error rules for a function of two doubles.
R math_2(f64 x, f64 y, f64 r, Value &out)
{
    math_errno = E_NONE;
    if (is_nan(r)) {
        if (!is_nan(x) && !is_nan(y))
            math_errno = E_DOM;
    } else if (is_inf(r)) {
        if (isfinite(x) && isfinite(y))
            math_errno = E_RANGE;
    }
    if (math_errno && is_error(r, true))
        return R::Err;
    out = float_new(r);
    return out.is_nil() ? R::Err : R::Ok;
}

using Fn2 = double (*)(double, double);

// Two doubles in, one out; `again` is the native itself, for __float__.
R two(const CallArgs &a, Str who, Fn2 fn, R (*again)(const CallArgs &, Value &out), Value &out)
{
    if (!args_only(a, who, 2, 2))
        return R::Err;
    R r = R::Ok;
    if (float_args(a, 2, again, out, r))
        return r;
    f64 x = 0, y = 0;
    if (!to_f64(a.args[0], x) || !to_f64(a.args[1], y))
        return R::Err;
    return math_2(x, y, fn(x, y), out);
}

R m_atan2(const CallArgs &a, Value &out)
{
    return two(a, "atan2", atan2, m_atan2, out);
}

R m_atan2pi(const CallArgs &a, Value &out)
{
    return two(a, "atan2pi", m_atan2pi, m_atan2pi, out);
}

R m_copysign(const CallArgs &a, Value &out)
{
    return two(a, "copysign", copysign, m_copysign, out);
}

R m_remainder(const CallArgs &a, Value &out)
{
    return two(a, "remainder", m_remainder, m_remainder, out);
}

// Two doubles in and one out, with no error rule: fmax, fmin.
R plain2(const CallArgs &a, Str who, Fn2 fn, R (*again)(const CallArgs &, Value &out), Value &out)
{
    if (!args_only(a, who, 2, 2))
        return R::Err;
    R r = R::Ok;
    if (float_args(a, 2, again, out, r))
        return r;
    f64 x = 0, y = 0;
    if (!to_f64(a.args[0], x) || !to_f64(a.args[1], y))
        return R::Err;
    out = float_new(fn(x, y));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_fmax(const CallArgs &a, Value &out)
{
    return plain2(a, "fmax", fmax, m_fmax, out);
}

R m_fmin(const CallArgs &a, Value &out)
{
    return plain2(a, "fmin", fmin, m_fmin, out);
}

R m_fmod(const CallArgs &a, Value &out)
{
    if (!args_only(a, "fmod", 2, 2))
        return R::Err;
    R r = R::Ok;
    if (float_args(a, 2, m_fmod, out, r))
        return r;
    f64 x = 0, y = 0;
    if (!to_f64(a.args[0], x) || !to_f64(a.args[1], y))
        return R::Err;
    if (is_inf(y) && isfinite(x)) {
        out = float_new(x);
        return out.is_nil() ? R::Err : R::Ok;
    }
    f64 got    = fmod(x, y);
    math_errno = is_nan(got) && !is_nan(x) && !is_nan(y) ? E_DOM : E_NONE;
    if (math_errno && is_error(got, true))
        return R::Err;
    out = float_new(got);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_fma(const CallArgs &a, Value &out)
{
    if (!args_only(a, "fma", 3, 3))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, 3, m_fma, out, rr))
        return rr;
    f64 x = 0, y = 0, z = 0;
    if (!to_f64(a.args[0], x) || !to_f64(a.args[1], y) || !to_f64(a.args[2], z))
        return R::Err;
    f64 r = fma(x, y, z);
    if (!isfinite(r)) {
        if (is_nan(r)) {
            if (!is_nan(x) && !is_nan(y) && !is_nan(z))
                return err_set("ValueError", "invalid operation in fma");
        } else if (isfinite(x) && isfinite(y) && isfinite(z)) {
            return err_set("OverflowError", "overflow in fma");
        }
    }
    out = float_new(r);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_pow(const CallArgs &a, Value &out)
{
    if (!args_only(a, "pow", 2, 2))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, 2, m_pow, out, rr))
        return rr;
    f64 x = 0, y = 0;
    if (!to_f64(a.args[0], x) || !to_f64(a.args[1], y))
        return R::Err;
    f64 r      = 0;
    math_errno = E_NONE;
    if (!isfinite(x) || !isfinite(y)) {
        if (is_nan(x)) {
            r = y == 0. ? 1. : x;
        } else if (is_nan(y)) {
            r = x == 1. ? 1. : y;
        } else if (is_inf(x)) {
            bool odd_y = isfinite(y) && fmod(fabs(y), 2.0) == 1.0;
            if (y > 0.)
                r = odd_y ? x : fabs(x);
            else if (y == 0.)
                r = 1.;
            else
                r = odd_y ? copysign(0., x) : 0.;
        } else {
            if (fabs(x) == 1.0)
                r = 1.;
            else if (y > 0. && fabs(x) > 1.0)
                r = y;
            else if (y < 0. && fabs(x) < 1.0)
                r = -y;
            else
                r = 0.;
        }
    } else {
        r = pow(x, y);
        if (!isfinite(r)) {
            if (is_nan(r))
                math_errno = E_DOM;
            else
                math_errno = x == 0. ? E_DOM : E_RANGE;
        }
    }
    if (math_errno && is_error(r, true))
        return R::Err;
    out = float_new(r);
    return out.is_nil() ? R::Err : R::Ok;
}

// Every argument as a double, for hypot and dist: `max` and whether a NaN
// was among them come back beside.
bool vector_of(const Value *items, usize n, Vec<f64> &out, f64 &max, bool &nan)
{
    max = 0;
    nan = false;
    for (usize i = 0; i < n; i++) {
        f64 x = 0;
        if (!to_f64(items[i], x))
            return false;
        x = fabs(x);
        if (!out.push(x))
            return oom() == R::Ok;
        nan |= is_nan(x);
        if (x > max)
            max = x;
    }
    return true;
}

R m_hypot(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "hypot() takes no keyword arguments");
    R r = R::Ok;
    if (float_args(a, a.nargs, m_hypot, out, r))
        return r;
    Vec<f64> v;
    f64 max  = 0;
    bool nan = false;
    if (!vector_of(a.args, a.nargs, v, max, nan))
        return R::Err;
    out = float_new(vector_norm(v.size(), v.data(), max, nan));
    return out.is_nil() ? R::Err : R::Ok;
}

// A sequence as a tuple, which is what dist takes each point as.
Value as_tuple(Value v)
{
    if (is_tuple(v))
        return v;
    ListObj *l = py_list_of(v);
    if (!l)
        return Value();
    Root rl{ obj_value(l) };
    TupleObj *t = tuple_new(l->items.size());
    if (!t)
        return oom(), Value();
    for (usize i = 0; i < list_of(rl.v)->items.size(); i++)
        t->items()[i] = list_of(rl.v)->items[i];
    return obj_value(t);
}

R m_dist(const CallArgs &a, Value &out)
{
    if (!args_only(a, "dist", 2, 2))
        return R::Err;
    if (iter_needs_vm(a.args[0]))
        return iter_park(a, 0, m_dist, out);
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, m_dist, out);
    Root p{ as_tuple(a.args[0]) };
    if (p.v.is_nil())
        return R::Err;
    Root q{ as_tuple(a.args[1]) };
    if (q.v.is_nil())
        return R::Err;
    TupleObj *tp = static_cast<TupleObj *>(p.v.obj());
    TupleObj *tq = static_cast<TupleObj *>(q.v.obj());
    if (tp->len != tq->len)
        return err_set("ValueError", "both points must have the same number of dimensions");
    Vec<f64> diffs;
    f64 max  = 0;
    bool nan = false;
    for (u32 i = 0; i < tp->len; i++) {
        f64 px = 0, qx = 0;
        if (!to_f64(tp->items()[i], px) || !to_f64(tq->items()[i], qx))
            return R::Err;
        f64 x = fabs(px - qx);
        if (!diffs.push(x))
            return oom();
        nan |= is_nan(x);
        if (x > max)
            max = x;
    }
    out = float_new(vector_norm(diffs.size(), diffs.data(), max, nan));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_nextafter(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "x", "y", "steps" };
    Value v[3];
    if (a.nargs > 2)
        return err_set("TypeError", "nextafter() takes exactly 2 positional arguments");
    for (u32 k = 0; k < a.nkw; k++)
        if (!is_str(a.kwnames[k]) || str_of(a.kwnames[k])->str() != "steps")
            return err_set2("TypeError", "nextafter() got an unexpected keyword argument",
                            is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str());
    if (!fn_take(a, "nextafter", NAMES, 2, v))
        return R::Err;
    f64 x = 0, y = 0;
    if (!to_f64(v[0], x) || !to_f64(v[1], y))
        return R::Err;
    if (v[2].is_nil() || is_none(v[2])) {
        out = float_new(nextafter(x, y));
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (!is_intval(v[2]))
        return err_not_index(v[2]);
    if (int_is_neg(v[2]))
        return err_set("ValueError", "steps must be a non-negative integer");
    u64 usteps = ~u64(0);
    i64 n      = 0;
    if (int_to_i64(v[2], n))
        usteps = u64(n);
    else if (is_big(v[2]) && big_of(v[2])->len <= 2)
        usteps = u64(big_of(v[2])->limbs()[0]) |
                 (big_of(v[2])->len > 1 ? u64(big_of(v[2])->limbs()[1]) << 32 : 0);
    f64 r = x;
    if (usteps == 0 || is_nan(x)) {
        r = x;
    } else if (is_nan(y)) {
        r = y;
    } else {
        u64 ux = 0, uy = 0;
        __builtin_memcpy(&ux, &x, 8);
        __builtin_memcpy(&uy, &y, 8);
        constexpr u64 SIGN = u64(1) << 63;
        u64 ax = ux & ~SIGN, ay = uy & ~SIGN;
        u64 res = 0;
        if (ux == uy) {
            res = ux;
        } else if ((ux ^ uy) & SIGN) {
            if (ax + ay <= usteps)
                res = uy;
            else if (ax < usteps)
                res = (uy & SIGN) | (usteps - ax);
            else
                res = ux - usteps;
        } else if (ax > ay) {
            res = ax - ay >= usteps ? ux - usteps : uy;
        } else {
            res = ay - ax >= usteps ? ux + usteps : uy;
        }
        __builtin_memcpy(&r, &res, 8);
    }
    out = float_new(r);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_ulp(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ulp", 1, 1))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, 1, m_ulp, out, rr))
        return rr;
    f64 x = 0;
    if (!to_f64(a.args[0], x))
        return R::Err;
    f64 r = x;
    if (!is_nan(x)) {
        x = fabs(x);
        if (is_inf(x)) {
            r = x;
        } else {
            f64 x2 = nextafter(x, __builtin_inf());
            r      = is_inf(x2) ? x - nextafter(x, -__builtin_inf()) : x2 - x;
        }
    }
    out = float_new(r);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_ldexp(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ldexp", 2, 2))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, 1, m_ldexp, out, rr))
        return rr;
    f64 x = 0;
    if (!to_f64(a.args[0], x))
        return R::Err;
    if (!is_intval(a.args[1]))
        return err_set("TypeError", "Expected an int as second argument to ldexp.");
    i64 e = 0;
    if (!int_to_i64(a.args[1], e))
        e = int_is_neg(a.args[1]) ? I64_MIN : I64_MAX;
    f64 r      = 0;
    math_errno = E_NONE;
    if (x == 0. || !isfinite(x)) {
        r = x;
    } else if (e > 2147483647) {
        r          = copysign(__builtin_inf(), x);
        math_errno = E_RANGE;
    } else if (e < -2147483647 - 1) {
        r = copysign(0., x);
    } else {
        r = ldexp(x, int(e));
        if (is_inf(r))
            math_errno = E_RANGE;
    }
    if (math_errno && is_error(r, true))
        return R::Err;
    out = float_new(r);
    return out.is_nil() ? R::Err : R::Ok;
}

// Two floats in a tuple, the first pinned while the second is made.
R pair_out(f64 x, Value y, Value &out)
{
    Root ry{ y };
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    Value f = float_new(x);
    if (f.is_nil())
        return R::Err;
    static_cast<TupleObj *>(rt.v.obj())->items()[0] = f;
    static_cast<TupleObj *>(rt.v.obj())->items()[1] = ry.v;
    out                                             = rt.v;
    return R::Ok;
}

R m_frexp(const CallArgs &a, Value &out)
{
    if (!args_only(a, "frexp", 1, 1))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, 1, m_frexp, out, rr))
        return rr;
    f64 x = 0;
    if (!to_f64(a.args[0], x))
        return R::Err;
    int e = 0;
    f64 m = x;
    if (!is_nan(x) && !is_inf(x) && x != 0)
        m = frexp(x, &e);
    return pair_out(m, Value::of_int(e), out);
}

R m_modf(const CallArgs &a, Value &out)
{
    if (!args_only(a, "modf", 1, 1))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, 1, m_modf, out, rr))
        return rr;
    f64 x = 0;
    if (!to_f64(a.args[0], x))
        return R::Err;
    f64 ip = 0, fp = 0;
    if (is_inf(x)) {
        ip = x;
        fp = copysign(0., x);
    } else {
        fp = modf(x, &ip);
    }
    Root ri{ float_new(ip) };
    if (ri.v.is_nil())
        return R::Err;
    return pair_out(fp, ri.v, out);
}

// One double in and one out, with no error: degrees and radians.
template <f64 (*F)(f64), const char *N>
R plain1(const CallArgs &a, Value &out)
{
    if (!args_only(a, N, 1, 1))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, 1, plain1<F, N>, out, rr))
        return rr;
    f64 x = 0;
    if (!to_f64(a.args[0], x))
        return R::Err;
    out = float_new(F(x));
    return out.is_nil() ? R::Err : R::Ok;
}

f64 to_degrees(f64 x)
{
    return x * (180.0 / PI);
}

f64 to_radians(f64 x)
{
    return x * (PI / 180.0);
}

constexpr char N_DEGREES[] = "degrees";
constexpr char N_RADIANS[] = "radians";

// One double in, a bool out.
template <bool (*F)(f64), const char *N>
R test1(const CallArgs &a, Value &out)
{
    if (!args_only(a, N, 1, 1))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, 1, test1<F, N>, out, rr))
        return rr;
    f64 x = 0;
    if (!to_f64(a.args[0], x))
        return R::Err;
    out = value_bool(F(x));
    return R::Ok;
}

bool t_isnan(f64 x)
{
    return is_nan(x);
}

bool t_isinf(f64 x)
{
    return is_inf(x);
}

bool t_isfinite(f64 x)
{
    return isfinite(x);
}

bool t_isnormal(f64 x)
{
    return isnormal(x);
}

bool t_issubnormal(f64 x)
{
    return isfinite(x) && x != 0 && !isnormal(x);
}

bool t_signbit(f64 x)
{
    return signbit(x);
}

constexpr char N_ISNAN[]       = "isnan";
constexpr char N_ISINF[]       = "isinf";
constexpr char N_ISFINITE[]    = "isfinite";
constexpr char N_ISNORMAL[]    = "isnormal";
constexpr char N_ISSUBNORMAL[] = "issubnormal";
constexpr char N_SIGNBIT[]     = "signbit";

R m_isclose(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "a", "b", "rel_tol", "abs_tol" };
    Value v[4];
    if (a.nargs > 2)
        return err_set("TypeError", "isclose() takes exactly 2 positional arguments");
    if (!fn_take(a, "isclose", NAMES, 2, v))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, a.nargs, m_isclose, out, rr))
        return rr;
    f64 x = 0, y = 0, rel = 1e-9, abs_tol = 0.0;
    if (!to_f64(v[0], x) || !to_f64(v[1], y) || (!v[2].is_nil() && !to_f64(v[2], rel)) ||
        (!v[3].is_nil() && !to_f64(v[3], abs_tol)))
        return R::Err;
    if (rel < 0.0 || abs_tol < 0.0)
        return err_set("ValueError", "tolerances must be non-negative");
    bool close;
    if (x == y) {
        close = true;
    } else if (is_inf(x) || is_inf(y)) {
        close = false;
    } else {
        f64 diff = fabs(y - x);
        close    = diff <= fabs(rel * y) || diff <= fabs(rel * x) || diff <= abs_tol;
    }
    out = value_bool(close);
    return R::Ok;
}

// Shewchuk's msum with CPython's partials and half-even fix-up: exactly
// rounded, which a compensated sum is not.
R m_fsum(const CallArgs &a, Value &out)
{
    if (!args_only(a, "fsum", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[0]))
        return iter_park(a, 0, m_fsum, out);
    Root it{ py_iter(a.args[0]) };
    if (it.v.is_nil())
        return R::Err;
    Vec<f64> p;
    usize n         = 0;
    f64 special_sum = 0.0, inf_sum = 0.0;
    f64 hi = 0, yr = 0, lo = 0.0;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            break;
        f64 x = 0;
        if (!to_f64(got.v, x))
            return R::Err;
        f64 xsave = x;
        usize i   = 0;
        for (usize j = 0; j < n; j++) {
            f64 y = p[j];
            if (fabs(x) < fabs(y)) {
                f64 t = x;
                x     = y;
                y     = t;
            }
            hi = x + y;
            yr = hi - x;
            lo = y - yr;
            if (lo != 0.0)
                p[i++] = lo;
            x = hi;
        }
        n = i;
        if (x != 0.0) {
            if (!isfinite(x)) {
                if (isfinite(xsave))
                    return err_set("OverflowError", "intermediate overflow in fsum");
                if (is_inf(xsave))
                    inf_sum += xsave;
                special_sum += xsave;
                n = 0;
            } else {
                if (n == p.size() && !p.push(0))
                    return oom();
                p[n++] = x;
            }
        }
    }
    if (special_sum != 0.0) {
        if (is_nan(inf_sum))
            return err_set("ValueError", "-inf + inf in fsum");
        out = float_new(special_sum);
        return out.is_nil() ? R::Err : R::Ok;
    }
    hi = 0.0;
    if (n > 0) {
        hi = p[--n];
        while (n > 0) {
            f64 x = hi;
            f64 y = p[--n];
            hi    = x + y;
            yr    = hi - x;
            lo    = y - yr;
            if (lo != 0.0)
                break;
        }
        if (n > 0 && ((lo < 0.0 && p[n - 1] < 0.0) || (lo > 0.0 && p[n - 1] > 0.0))) {
            f64 y = lo * 2.0;
            f64 x = hi + y;
            yr    = x - hi;
            if (y == yr)
                hi = x;
        }
    }
    out = float_new(hi);
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------------- log

// frexp of an integer too wide for a double: x * 2**e with x in [0.5, 1).
f64 int_frexp(Value v, i64 &e)
{
    usize bits = int_bits(v);
    Value top;
    // The top 60 bits are plenty: the log only needs 53 of them.
    i64 shift = i64(bits) - 60;
    if (shift < 0)
        shift = 0;
    if (int_arith(v, Value::of_int(i32(shift)), Op::Rsh, top) != R::Ok)
        return 0;
    int ee = 0;
    f64 x  = frexp(int_to_f64(top), &ee);
    e      = ee + shift;
    return x;
}

// loghelper: an integer is taken exactly, however wide.
R loghelper(Value arg, Fn1 func, f64 &out)
{
    if (is_intval(arg)) {
        if (int_is_neg(arg) || !int_truth_of(arg))
            return err_set("ValueError", "expected a positive input");
        f64 x = int_to_f64(arg);
        if (is_inf(x)) {
            i64 e = 0;
            x     = int_frexp(arg, e);
            if (err_pending())
                return R::Err;
            out = fma(func(2.0), f64(e), func(x));
        } else {
            out = func(x);
        }
        return R::Ok;
    }
    f64 x = 0;
    if (!to_f64(arg, x))
        return R::Err;
    f64 r = func(x);
    if ((is_nan(r) && !is_nan(x)) || (is_inf(r) && isfinite(x)))
        return domain_err("expected a positive input, got ", x);
    out = r;
    return R::Ok;
}

R log_of(const CallArgs &a, Str who, Fn1 func, R (*again)(const CallArgs &, Value &out), Value &out)
{
    if (!args_only(a, who, 1, 1))
        return R::Err;
    R rr = R::Ok;
    if (float_args(a, 1, again, out, rr))
        return rr;
    f64 r = 0;
    if (loghelper(a.args[0], func, r) != R::Ok)
        return R::Err;
    out = float_new(r);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_log2(const CallArgs &a, Value &out)
{
    return log_of(a, "log2", log2, m_log2, out);
}

R m_log10(const CallArgs &a, Value &out)
{
    return log_of(a, "log10", log10, m_log10, out);
}

R m_log(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "log() takes no keyword arguments");
    if (a.nargs < 1 || a.nargs > 2) {
        char tmp[24];
        Buf<96> b;
        b.put(a.nargs < 1 ? "log expected at least 1 argument, got "
                          : "log expected at most 2 arguments, got ");
        b.put(int_text(tmp, sizeof tmp, a.nargs));
        return err_set("TypeError", b.str());
    }
    R rr = R::Ok;
    if (float_args(a, a.nargs, m_log, out, rr))
        return rr;
    f64 num = 0, den = 0;
    if (loghelper(a.args[0], log, num) != R::Ok)
        return R::Err;
    if (a.nargs == 1) {
        out = float_new(num);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (loghelper(a.args[1], log, den) != R::Ok)
        return R::Err;
    if (den == 0.0)
        return err_set("ZeroDivisionError", "division by zero");
    out = float_new(num / den);
    return out.is_nil() ? R::Err : R::Ok;
}

// --------------------------------------------------------------- sumprod

// CPython's sumprod: an exact integer total while the pairs are ints, a
// triple-length float total while they are floats, and the generic
// operations for everything else -- which may be Python, so the loop is a
// continuation. s[0] and s[1] the two lists, s[2] the total, s[3] and s[4]
// the pair, s[5] the product; `i` the index, `j` the state; x[0] the integer
// total and x[1..3] the float one, as bits.
enum : u32 {
    SP_INT_OK   = 1u << 0,
    SP_INT_USED = 1u << 1,
    SP_FLT_OK   = 1u << 2,
    SP_FLT_USED = 1u << 3,
};

enum : u32 { SP_LOOP, SP_MUL, SP_ADD, SP_FLUSH_INT, SP_FLUSH_FLT, SP_DONE };

inline bool long_add_overflows(i64 a, i64 b)
{
    return a > 0 ? b > I64_MAX - a : b < I64_MIN - a;
}

// Python 2's check: the wrapped product against the double one.
inline bool long_mul_overflows(i64 a, i64 b)
{
    i64 longprod = i64(u64(a) * u64(b));
    f64 dprod    = f64(a) * f64(b);
    f64 dlong    = f64(longprod);
    if (dlong == dprod)
        return false;
    f64 absdiff = fabs(dlong - dprod);
    return !(32.0 * absdiff <= fabs(dprod));
}

TripleLength sp_float(ContObj *k)
{
    TripleLength t;
    __builtin_memcpy(&t.hi, &k->x[1], 8);
    __builtin_memcpy(&t.lo, &k->x[2], 8);
    __builtin_memcpy(&t.tiny, &k->x[3], 8);
    return t;
}

void sp_set_float(ContObj *k, TripleLength t)
{
    __builtin_memcpy(&k->x[1], &t.hi, 8);
    __builtin_memcpy(&k->x[2], &t.lo, 8);
    __builtin_memcpy(&k->x[3], &t.tiny, 8);
}

// A small integer as a Python int: i64 past the value word promotes.
bool exact_i64(Value v, i64 &out)
{
    return (v.is_int() || is_big(v)) && int_to_i64(v, out);
}

// total + term, which may be a call; `then` is the state after it. True when
// the step has to return `r`: an error, or a call to wait for.
bool sp_add(ContObj *k, Value term, u32 then, R &r)
{
    Root rt{ term };
    Value got;
    if (binop_call(k->s[2], rt.v, Op::Add, got) != R::Ok) {
        r = R::Err;
        return true;
    }
    k->j = then;
    if (is_cont(got)) {
        k->i |= 1u << 31;
        r = cont_await(k, got);
        return true;
    }
    k->s[2] = got;
    return false;
}

R sumprod_step(ContObj *k, Value in)
{
    // Coming back from an awaited step: its answer is the new total, or the
    // product in SP_MUL's case.
    if (k->i & (1u << 31)) {
        k->i &= ~(1u << 31);
        if (k->j == SP_ADD)
            k->s[5] = in;
        else
            k->s[2] = in;
    }
    ListObj *p = list_of(k->s[0]);
    ListObj *q = list_of(k->s[1]);
    for (;;) {
        switch (k->j) {
        case SP_ADD: {
            // The product is in: add it, then the next pair.
            k->i++;
            Root term{ k->s[5] };
            k->s[5] = Value();
            R r     = R::Ok;
            if (sp_add(k, term.v, SP_LOOP, r))
                return r;
            continue;
        }
        case SP_FLUSH_INT:
        case SP_FLUSH_FLT:
            k->j = SP_LOOP;
            break;
        case SP_DONE:
            return cont_done(k, k->s[2]);
        default:
            break;
        }
        usize idx     = k->i;
        bool finished = idx >= p->items.size();
        u32 flags     = u32(k->s[6].as_int());
        if (flags & SP_INT_OK) {
            i64 ip = 0, iq = 0;
            if (!finished && exact_i64(p->items[idx], ip) && exact_i64(q->items[idx], iq) &&
                !long_mul_overflows(ip, iq) && !long_add_overflows(k->x[0], ip * iq)) {
                k->x[0] += ip * iq;
                k->s[6] = Value::of_int(i32(flags | SP_INT_USED));
                k->i++;
                continue;
            }
            flags &= ~SP_INT_OK;
            k->s[6] = Value::of_int(i32(flags & ~SP_INT_USED));
            if (flags & SP_INT_USED) {
                Value term = int_from_i64(k->x[0]);
                if (term.is_nil())
                    return R::Err;
                k->x[0] = 0;
                R r     = R::Ok;
                if (sp_add(k, term, SP_FLUSH_INT, r))
                    return r;
                continue;
            }
        }
        if (flags & SP_FLT_OK) {
            if (!finished) {
                Value vp = p->items[idx], vq = q->items[idx];
                f64 fp = 0, fq = 0;
                bool ok = true;
                if (is_float(vp) && is_float(vq)) {
                    fp = float_of(vp);
                    fq = float_of(vq);
                } else if (is_float(vp) && is_intval(vq)) {
                    fp = float_of(vp);
                    fq = int_to_f64(vq);
                    ok = !is_inf(fq);
                } else if (is_float(vq) && is_intval(vp)) {
                    fq = float_of(vq);
                    fp = int_to_f64(vp);
                    ok = !is_inf(fp);
                } else {
                    ok = false;
                }
                if (ok) {
                    TripleLength t = tl_fma(fp, fq, sp_float(k));
                    if (isfinite(t.hi)) {
                        sp_set_float(k, t);
                        k->s[6] = Value::of_int(i32(flags | SP_FLT_USED));
                        k->i++;
                        continue;
                    }
                }
            }
            flags &= ~SP_FLT_OK;
            k->s[6] = Value::of_int(i32(flags & ~SP_FLT_USED));
            if (flags & SP_FLT_USED) {
                Value term = float_new(tl_to_d(sp_float(k)));
                if (term.is_nil())
                    return R::Err;
                sp_set_float(k, { 0, 0, 0 });
                R r = R::Ok;
                if (sp_add(k, term, SP_FLUSH_FLT, r))
                    return r;
                continue;
            }
        }
        if (finished)
            return cont_done(k, k->s[2]);
        Value got;
        if (binop_call(p->items[idx], q->items[idx], Op::Mul, got) != R::Ok)
            return R::Err;
        k->j = SP_ADD;
        if (is_cont(got)) {
            k->i |= 1u << 31;
            return cont_await(k, got);
        }
        k->s[5] = got;
    }
}

R m_sumprod(const CallArgs &a, Value &out)
{
    if (!args_only(a, "sumprod", 2, 2))
        return R::Err;
    if (iter_needs_vm(a.args[0]))
        return iter_park(a, 0, m_sumprod, out);
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, m_sumprod, out);
    Root p{ obj_value(py_list_of(a.args[0])) };
    if (p.v.is_nil())
        return R::Err;
    Root q{ obj_value(py_list_of(a.args[1])) };
    if (q.v.is_nil())
        return R::Err;
    if (list_of(p.v)->items.size() != list_of(q.v)->items.size())
        return err_set("ValueError", "Inputs are not the same length");
    Root kv{ cont_new(sumprod_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = p.v;
    k->s[1]    = q.v;
    k->s[2]    = Value::of_int(0);
    k->x[0]    = 0;
    sp_set_float(k, { 0, 0, 0 });
    k->s[6] = Value::of_int(SP_INT_OK | SP_FLT_OK);
    k->j    = SP_LOOP;
    k->i    = 0;
    // Plain numbers never call: the answer is ready without parking.
    bool plain = true;
    for (Value v : list_of(p.v)->items)
        plain &= !is_inst(v);
    for (Value v : list_of(q.v)->items)
        plain &= !is_inst(v);
    if (!plain) {
        out = kv.v;
        return R::Ok;
    }
    R r = sumprod_step(k, Value());
    if (r != R::Ok)
        return r;
    out = cont_of(kv.v)->out;
    return R::Ok;
}

// ------------------------------------------------------------- the exact half

// An integer argument, refusing a float outright the way CPython's does.
bool arg_int(Value v, Str who, Value &out)
{
    if (is_intval(v)) {
        out = v;
        return true;
    }
    Buf<96> b;
    b.put('\'').put(type_name(v)).put("' object cannot be interpreted as an integer");
    (void)who;
    return err_set("TypeError", b.str()), false;
}

bool int_lt_zero(Value v)
{
    return int_is_neg(v);
}

// Every arithmetic step of the exact half goes through here, so a result past
// the value word promotes rather than wrapping.
bool arith(Value a, Value b, Op op, Root &out)
{
    Value got;
    if (int_arith(a, b, op, got) != R::Ok)
        return false;
    out = got;
    return true;
}

bool int_is_zero(Value v)
{
    return v.is_int() && v.as_int() == 0;
}

R m_ceil(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ceil", 1, 1))
        return R::Err;
    R r = R::Ok;
    if (answer_special(a.args[0], "__ceil__", Value(), out, r))
        return r;
    if (float_args(a, 1, m_ceil, out, r))
        return r;
    if (is_intval(a.args[0])) {
        out = a.args[0];
        return R::Ok;
    }
    f64 x = 0;
    if (!arg_f64(a, "ceil", 0, x))
        return R::Err;
    if (is_nan(x) || is_inf(x))
        return err_set("OverflowError", "cannot convert float infinity to integer");
    out = int_from_f64(ceil(x));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_floor(const CallArgs &a, Value &out)
{
    if (!args_only(a, "floor", 1, 1))
        return R::Err;
    R r = R::Ok;
    if (answer_special(a.args[0], "__floor__", Value(), out, r))
        return r;
    if (float_args(a, 1, m_floor, out, r))
        return r;
    if (is_intval(a.args[0])) {
        out = a.args[0];
        return R::Ok;
    }
    f64 x = 0;
    if (!arg_f64(a, "floor", 0, x))
        return R::Err;
    if (is_nan(x) || is_inf(x))
        return err_set("OverflowError", "cannot convert float infinity to integer");
    out = int_from_f64(floor(x));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_trunc(const CallArgs &a, Value &out)
{
    if (!args_only(a, "trunc", 1, 1))
        return R::Err;
    R r = R::Ok;
    if (answer_special(a.args[0], "__trunc__", Value(), out, r))
        return r;
    if (float_args(a, 1, m_trunc, out, r))
        return r;
    if (is_intval(a.args[0])) {
        out = a.args[0];
        return R::Ok;
    }
    f64 x = 0;
    if (!arg_f64(a, "trunc", 0, x))
        return R::Err;
    if (is_nan(x) || is_inf(x))
        return err_set("OverflowError", "cannot convert float infinity to integer");
    out = int_from_f64(trunc(x));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_factorial(const CallArgs &a, Value &out)
{
    if (!args_only(a, "factorial", 1, 1))
        return R::Err;
    Root n;
    if (!arg_int(a.args[0], "factorial", n.v))
        return R::Err;
    if (int_lt_zero(n.v))
        return err_set("ValueError", "factorial() not defined for negative values");
    i64 k = 0;
    if (!as_index(n.v, k) || k > 20000)
        return err_set("OverflowError", "factorial() argument is too large");
    Root acc{ Value::of_int(1) };
    for (i64 i = 2; i <= k; i++)
        if (!arith(acc.v, Value::of_int(i32(i)), Op::Mul, acc))
            return R::Err;
    out = acc.v;
    return R::Ok;
}

// The product of a descending run, which is what both comb and perm are made
// of: n * (n-1) * ... * (n-k+1).
bool falling(Value n, i64 k, Root &out)
{
    Root acc{ Value::of_int(1) }, cur{ n };
    for (i64 i = 0; i < k; i++) {
        if (!arith(acc.v, cur.v, Op::Mul, acc))
            return false;
        Value next;
        if (int_arith(cur.v, Value::of_int(1), Op::Sub, next) != R::Ok)
            return false;
        cur = next;
    }
    out = acc.v;
    return true;
}

R comb_or_perm(const CallArgs &a, Value &out, bool comb)
{
    Str who = comb ? Str("comb") : Str("perm");
    if (!args_only(a, who, 1, 2))
        return R::Err;
    Root n, r;
    if (!arg_int(a.args[0], who, n.v))
        return R::Err;
    if (a.nargs > 1 && !is_none(a.args[1])) {
        if (!arg_int(a.args[1], who, r.v))
            return R::Err;
    } else {
        r = n.v;
    }
    if (int_lt_zero(n.v) || int_lt_zero(r.v))
        return err_set("ValueError", "n and k must be non-negative");
    bool bigger = false;
    if (int_compare(r.v, n.v, Cmp::Gt, bigger) != R::Ok)
        return R::Err;
    if (bigger) {
        out = comb ? Value::of_int(0) : Value::of_int(0);
        return R::Ok;
    }
    i64 k = 0;
    if (!as_index(r.v, k) || k > 20000)
        return err_set("OverflowError", "argument is too large");
    Root top;
    if (!falling(n.v, k, top))
        return R::Err;
    if (!comb) {
        out = top.v;
        return R::Ok;
    }
    Root fact{ Value::of_int(1) };
    for (i64 i = 2; i <= k; i++)
        if (!arith(fact.v, Value::of_int(i32(i)), Op::Mul, fact))
            return R::Err;
    Value got;
    if (int_arith(top.v, fact.v, Op::FloorDiv, got) != R::Ok)
        return R::Err;
    out = got;
    return R::Ok;
}

R m_comb(const CallArgs &a, Value &out)
{
    return comb_or_perm(a, out, true);
}

R m_perm(const CallArgs &a, Value &out)
{
    return comb_or_perm(a, out, false);
}

// Euclid over whatever width the arguments are, so gcd of two bignums is
// exact. Both are made non-negative first: Python's gcd is.
bool gcd_pair(Value a, Value b, Root &out)
{
    Root x, y;
    if (int_absolute(a, x.v) != R::Ok || int_absolute(b, y.v) != R::Ok)
        return false;
    while (!int_is_zero(y.v)) {
        Value rem;
        if (int_arith(x.v, y.v, Op::Mod, rem) != R::Ok)
            return false;
        x = y.v;
        y = rem;
    }
    out = x.v;
    return true;
}

R m_gcd(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "gcd() takes no keyword arguments");
    for (u32 i = 0; i < a.nargs; i++) {
        R r = R::Ok;
        if (!is_intval(a.args[i]) && redo_converted(a, i, "__index__", m_gcd, out, r))
            return r;
    }
    Root acc{ Value::of_int(0) };
    for (u32 i = 0; i < a.nargs; i++) {
        Root one;
        if (!arg_int(a.args[i], "gcd", one.v))
            return R::Err;
        if (!gcd_pair(acc.v, one.v, acc))
            return R::Err;
    }
    out = acc.v;
    return R::Ok;
}

R m_lcm(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "lcm() takes no keyword arguments");
    for (u32 i = 0; i < a.nargs; i++) {
        R r = R::Ok;
        if (!is_intval(a.args[i]) && redo_converted(a, i, "__index__", m_lcm, out, r))
            return r;
    }
    Root acc{ Value::of_int(1) };
    for (u32 i = 0; i < a.nargs; i++) {
        Root one;
        if (!arg_int(a.args[i], "lcm", one.v))
            return R::Err;
        if (int_is_zero(one.v)) {
            out = Value::of_int(0);
            return R::Ok;
        }
        Root g;
        if (!gcd_pair(acc.v, one.v, g))
            return R::Err;
        Value q;
        if (int_arith(acc.v, g.v, Op::FloorDiv, q) != R::Ok)
            return R::Err;
        Root rq{ q };
        if (!arith(rq.v, one.v, Op::Mul, acc))
            return R::Err;
        if (int_absolute(acc.v, q) != R::Ok)
            return R::Err;
        acc = q;
    }
    out = acc.v;
    return R::Ok;
}

// Newton's method over integers: the iteration only ever decreases once it is
// past the root, which is what makes the stopping rule exact.
R m_isqrt(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isqrt", 1, 1))
        return R::Err;
    Root n;
    if (!arg_int(a.args[0], "isqrt", n.v))
        return R::Err;
    if (int_lt_zero(n.v))
        return err_set("ValueError", "isqrt() argument must be non-negative");
    if (int_is_zero(n.v)) {
        out = Value::of_int(0);
        return R::Ok;
    }
    // A first guess of 2^ceil(bits/2) is above the root and never below it.
    usize bits = int_bits(n.v);
    Value guess;
    if (int_arith(Value::of_int(1), Value::of_int(i32((bits + 2) / 2)), Op::Lsh, guess) != R::Ok)
        return R::Err;
    Root x{ guess };
    for (;;) {
        Value q;
        if (int_arith(n.v, x.v, Op::FloorDiv, q) != R::Ok)
            return R::Err;
        Root rq{ q };
        Root sum;
        if (!arith(x.v, rq.v, Op::Add, sum))
            return R::Err;
        Value next;
        if (int_arith(sum.v, Value::of_int(2), Op::FloorDiv, next) != R::Ok)
            return R::Err;
        bool smaller = false;
        if (int_compare(next, x.v, Cmp::Lt, smaller) != R::Ok)
            return R::Err;
        if (!smaller)
            break;
        x = next;
    }
    out = x.v;
    return R::Ok;
}

R m_prod(const CallArgs &a, Value &out)
{
    Root acc{ Value::of_int(1) };
    for (u32 k = 0; k < a.nkw; k++) {
        if (!is_str(a.kwnames[k]) || str_of(a.kwnames[k])->str() != "start")
            return err_set("TypeError", "prod() got an unexpected keyword argument");
        acc = a.kwvals[k];
    }
    if (a.nargs != 1) {
        char tmp[24];
        Buf<96> b;
        b.put("prod() takes exactly 1 positional argument (");
        b.put(int_text(tmp, sizeof tmp, a.nargs)).put(" given)");
        return err_set("TypeError", b.str());
    }
    if (iter_needs_vm(a.args[0]))
        return iter_park(a, 0, m_prod, out);
    Root it{ py_iter(a.args[0]) };
    if (it.v.is_nil())
        return R::Err;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            break;
        Value next;
        if (binop_call(acc.v, got.v, Op::Mul, next) != R::Ok)
            return R::Err;
        if (is_cont(next))
            return fold_rest(it.v, next, Op::Mul, out);
        acc = next;
    }
    out = acc.v;
    return R::Ok;
}

// ------------------------------------------------------------------- cmath

bool arg_complex(const CallArgs &a, Str who, u32 at, f64 &re, f64 &im)
{
    Value v = a.args[at];
    if (is_complex(v)) {
        re = complex_of(v)->re;
        im = complex_of(v)->im;
        return true;
    }
    if (as_number(v, re)) {
        im = 0;
        return true;
    }
    Buf<96> b;
    b.put(who).put("() argument must be a number");
    return err_set2("TypeError", b.str(), type_name(v)), false;
}

// Every cmath function of one complex is built from these two, the way the
// textbook identities are: the rest follow from exp, log and the arithmetic.
void c_exp(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 r = exp(re);
    ore   = r * cos(im);
    oim   = r * sin(im);
}

void c_log(f64 re, f64 im, f64 &ore, f64 &oim)
{
    ore = log(hypot(re, im));
    oim = atan2(im, re);
}

void c_mul(f64 ar, f64 ai, f64 br, f64 bi, f64 &ore, f64 &oim)
{
    ore = ar * br - ai * bi;
    oim = ar * bi + ai * br;
}

void c_div(f64 ar, f64 ai, f64 br, f64 bi, f64 &ore, f64 &oim)
{
    f64 d = br * br + bi * bi;
    ore   = (ar * br + ai * bi) / d;
    oim   = (ai * br - ar * bi) / d;
}

void c_sqrt(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 r = hypot(re, im);
    ore   = sqrt((r + re) / 2);
    oim   = sqrt((r - re) / 2);
    if (im < 0)
        oim = -oim;
}

// asin(z) = -i * log(i*z + sqrt(1 - z^2)), and the other two follow it.
void c_asin(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 sr = 0, si = 0;
    c_mul(re, im, re, im, sr, si);
    f64 rr = 0, ri = 0;
    c_sqrt(1 - sr, -si, rr, ri);
    f64 lr = 0, li = 0;
    c_log(rr - im, ri + re, lr, li);
    ore = li;
    oim = -lr;
}

// atan(z) = (i/2) * (log(1 - i*z) - log(1 + i*z)); 1 - i*z is (1 + im, -re).
void c_atan(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 nr = 0, ni = 0, dr = 0, di = 0;
    c_log(1 + im, -re, nr, ni);
    c_log(1 - im, re, dr, di);
    ore = -(ni - di) / 2;
    oim = (nr - dr) / 2;
}

using Cfn = void (*)(f64, f64, f64 &, f64 &);

struct CUnary {
    Str name;
    Cfn fn;
};

void c_cos(f64 re, f64 im, f64 &ore, f64 &oim)
{
    ore = cos(re) * cosh(im);
    oim = -sin(re) * sinh(im);
}

void c_sin(f64 re, f64 im, f64 &ore, f64 &oim)
{
    ore = sin(re) * cosh(im);
    oim = cos(re) * sinh(im);
}

void c_tan(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 sr = 0, si = 0, cr = 0, ci = 0;
    c_sin(re, im, sr, si);
    c_cos(re, im, cr, ci);
    c_div(sr, si, cr, ci, ore, oim);
}

void c_cosh(f64 re, f64 im, f64 &ore, f64 &oim)
{
    ore = cosh(re) * cos(im);
    oim = sinh(re) * sin(im);
}

void c_sinh(f64 re, f64 im, f64 &ore, f64 &oim)
{
    ore = sinh(re) * cos(im);
    oim = cosh(re) * sin(im);
}

void c_tanh(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 sr = 0, si = 0, cr = 0, ci = 0;
    c_sinh(re, im, sr, si);
    c_cosh(re, im, cr, ci);
    c_div(sr, si, cr, ci, ore, oim);
}

void c_acos(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 sr = 0, si = 0;
    c_asin(re, im, sr, si);
    ore = PI / 2 - sr;
    oim = -si;
}

// asinh(z) = i * asin(-i*z), which keeps one branch rule rather than two.
void c_asinh(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 sr = 0, si = 0;
    c_asin(im, -re, sr, si);
    ore = -si;
    oim = sr;
}

void c_acosh(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 sr = 0, si = 0;
    c_acos(re, im, sr, si);
    // acosh(z) = i*acos(z), with the sign chosen so the real part is >= 0.
    ore = -si;
    oim = sr;
    if (ore < 0) {
        ore = -ore;
        oim = -oim;
    }
}

void c_atanh(f64 re, f64 im, f64 &ore, f64 &oim)
{
    f64 sr = 0, si = 0;
    c_atan(-im, re, sr, si);
    ore = si;
    oim = -sr;
}

constexpr CUnary CUNARY[] = {
    { "exp", c_exp },     { "log", c_log },     { "sqrt", c_sqrt },   { "sin", c_sin },
    { "cos", c_cos },     { "tan", c_tan },     { "sinh", c_sinh },   { "cosh", c_cosh },
    { "tanh", c_tanh },   { "asin", c_asin },   { "acos", c_acos },   { "atan", c_atan },
    { "asinh", c_asinh }, { "acosh", c_acosh }, { "atanh", c_atanh },
};

R cunary_call(const CallArgs &a, Value &out, const CUnary &u)
{
    if (!args_only(a, u.name, 1, 1))
        return R::Err;
    f64 re = 0, im = 0;
    if (!arg_complex(a, u.name, 0, re, im))
        return R::Err;
    f64 or_ = 0, oi = 0;
    u.fn(re, im, or_, oi);
    out = complex_new(or_, oi);
    return out.is_nil() ? R::Err : R::Ok;
}

R c_phase(const CallArgs &a, Value &out)
{
    if (!args_only(a, "phase", 1, 1))
        return R::Err;
    f64 re = 0, im = 0;
    if (!arg_complex(a, "phase", 0, re, im))
        return R::Err;
    out = float_new(atan2(im, re));
    return out.is_nil() ? R::Err : R::Ok;
}

R c_polar(const CallArgs &a, Value &out)
{
    if (!args_only(a, "polar", 1, 1))
        return R::Err;
    f64 re = 0, im = 0;
    if (!arg_complex(a, "polar", 0, re, im))
        return R::Err;
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    Root r{ float_new(hypot(re, im)) };
    if (r.v.is_nil())
        return R::Err;
    Value p = float_new(atan2(im, re));
    if (p.is_nil())
        return R::Err;
    static_cast<TupleObj *>(rt.v.obj())->items()[0] = r.v;
    static_cast<TupleObj *>(rt.v.obj())->items()[1] = p;
    out                                             = rt.v;
    return R::Ok;
}

R c_rect(const CallArgs &a, Value &out)
{
    if (!args_only(a, "rect", 2, 2))
        return R::Err;
    f64 r = 0, p = 0;
    if (!arg_f64(a, "rect", 0, r) || !arg_f64(a, "rect", 1, p))
        return R::Err;
    out = complex_new(r * cos(p), r * sin(p));
    return out.is_nil() ? R::Err : R::Ok;
}

R c_isnan(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isnan", 1, 1))
        return R::Err;
    f64 re = 0, im = 0;
    if (!arg_complex(a, "isnan", 0, re, im))
        return R::Err;
    out = value_bool(is_nan(re) || is_nan(im));
    return R::Ok;
}

R c_isinf(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isinf", 1, 1))
        return R::Err;
    f64 re = 0, im = 0;
    if (!arg_complex(a, "isinf", 0, re, im))
        return R::Err;
    out = value_bool(is_inf(re) || is_inf(im));
    return R::Ok;
}

R c_isfinite(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isfinite", 1, 1))
        return R::Err;
    f64 re = 0, im = 0;
    if (!arg_complex(a, "isfinite", 0, re, im))
        return R::Err;
    out = value_bool(!is_nan(re) && !is_inf(re) && !is_nan(im) && !is_inf(im));
    return R::Ok;
}

R c_log10(const CallArgs &a, Value &out)
{
    if (!args_only(a, "log10", 1, 1))
        return R::Err;
    f64 re = 0, im = 0;
    if (!arg_complex(a, "log10", 0, re, im))
        return R::Err;
    f64 lr = 0, li = 0;
    c_log(re, im, lr, li);
    out = complex_new(lr / 2.302585092994046, li / 2.302585092994046);
    return out.is_nil() ? R::Err : R::Ok;
}

R c_isclose(const CallArgs &a, Value &out)
{
    // The real isclose, applied to the distance between the two points.
    if (a.nargs != 2)
        return err_set("TypeError", "isclose() takes exactly 2 positional arguments");
    f64 ar = 0, ai = 0, br = 0, bi = 0;
    if (!arg_complex(a, "isclose", 0, ar, ai) || !arg_complex(a, "isclose", 1, br, bi))
        return R::Err;
    f64 rel = 1e-9, abs_tol = 0;
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        f64 x = 0;
        if (!as_number(a.kwvals[k], x))
            return err_set("TypeError", "tolerance must be a number");
        if (n == "rel_tol")
            rel = x;
        else if (n == "abs_tol")
            abs_tol = x;
        else
            return err_set2("TypeError", "isclose() got an unexpected keyword argument", n);
    }
    if (ar == br && ai == bi) {
        out = value_bool(true);
        return R::Ok;
    }
    f64 d  = hypot(ar - br, ai - bi);
    f64 ma = hypot(ar, ai), mb = hypot(br, bi);
    out = value_bool(d <= rel * (ma > mb ? ma : mb) || d <= abs_tol);
    return R::Ok;
}

// One native per row of CUNARY, the way UNARY is done above.
template <usize I>
R cunary_at(const CallArgs &a, Value &out)
{
    return cunary_call(a, out, CUNARY[I]);
}

constexpr ModDef MATH_DEFS[] = {
    { "atan2", m_atan2 },
    { "atan2pi", m_atan2pi },
    { "hypot", m_hypot },
    { "dist", m_dist },
    { "fmod", m_fmod },
    { "fma", m_fma },
    { "fmax", m_fmax },
    { "fmin", m_fmin },
    { "remainder", m_remainder },
    { "copysign", m_copysign },
    { "nextafter", m_nextafter },
    { "ulp", m_ulp },
    { "ldexp", m_ldexp },
    { "frexp", m_frexp },
    { "modf", m_modf },
    { "degrees", plain1<to_degrees, N_DEGREES> },
    { "radians", plain1<to_radians, N_RADIANS> },
    { "isnan", test1<t_isnan, N_ISNAN> },
    { "isinf", test1<t_isinf, N_ISINF> },
    { "isfinite", test1<t_isfinite, N_ISFINITE> },
    { "isnormal", test1<t_isnormal, N_ISNORMAL> },
    { "issubnormal", test1<t_issubnormal, N_ISSUBNORMAL> },
    { "signbit", test1<t_signbit, N_SIGNBIT> },
    { "isclose", m_isclose },
    { "fsum", m_fsum },
    { "sumprod", m_sumprod },
    { "log", m_log },
    { "log2", m_log2 },
    { "log10", m_log10 },
    { "ceil", m_ceil },
    { "floor", m_floor },
    { "trunc", m_trunc },
    { "prod", m_prod },
    { "pow", m_pow },
};

// math.integer: the functions CPython moved there, shared with math.
constexpr ModDef INTEGER_DEFS[] = {
    { "comb", m_comb }, { "factorial", m_factorial }, { "gcd", m_gcd }, { "isqrt", m_isqrt },
    { "lcm", m_lcm },   { "perm", m_perm },
};

constexpr ModDef CMATH_DEFS[] = {
    { "phase", c_phase }, { "polar", c_polar },     { "rect", c_rect },
    { "isnan", c_isnan }, { "isinf", c_isinf },     { "isfinite", c_isfinite },
    { "log10", c_log10 }, { "isclose", c_isclose },
};

// The table's rows become natives here rather than in a loop, because each
// needs an address of its own and a lambda cannot have one without capturing.
#define UNARY_DEF(i) { UNARY[i].name, unary_at<i> }

constexpr ModDef UNARY_DEFS[] = {
    UNARY_DEF(0),  UNARY_DEF(1),  UNARY_DEF(2),  UNARY_DEF(3),  UNARY_DEF(4),  UNARY_DEF(5),
    UNARY_DEF(6),  UNARY_DEF(7),  UNARY_DEF(8),  UNARY_DEF(9),  UNARY_DEF(10), UNARY_DEF(11),
    UNARY_DEF(12), UNARY_DEF(13), UNARY_DEF(14), UNARY_DEF(15), UNARY_DEF(16), UNARY_DEF(17),
    UNARY_DEF(18), UNARY_DEF(19), UNARY_DEF(20), UNARY_DEF(21), UNARY_DEF(22), UNARY_DEF(23),
    UNARY_DEF(24), UNARY_DEF(25), UNARY_DEF(26), UNARY_DEF(27), UNARY_DEF(28),
};

#undef UNARY_DEF

#define CUNARY_DEF(i) { CUNARY[i].name, cunary_at<i> }

constexpr ModDef CUNARY_DEFS[] = {
    CUNARY_DEF(0),  CUNARY_DEF(1),  CUNARY_DEF(2),  CUNARY_DEF(3),  CUNARY_DEF(4),
    CUNARY_DEF(5),  CUNARY_DEF(6),  CUNARY_DEF(7),  CUNARY_DEF(8),  CUNARY_DEF(9),
    CUNARY_DEF(10), CUNARY_DEF(11), CUNARY_DEF(12), CUNARY_DEF(13), CUNARY_DEF(14),
};

#undef CUNARY_DEF

} // namespace

bool math_install(DictObj *into)
{
    static_assert(sizeof UNARY_DEFS / sizeof UNARY_DEFS[0] == sizeof UNARY / sizeof UNARY[0],
                  "every row of UNARY needs a native");
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, UNARY_DEFS) || !mod_defs(d, MATH_DEFS))
        return false;
    if (!mod_float(d, "pi", PI) || !mod_float(d, "e", E) || !mod_float(d, "tau", TAU) ||
        !mod_float(d, "inf", __builtin_inf()) || !mod_float(d, "nan", __builtin_nan("")))
        return false;
    // The integer functions are math.integer's, and math names the same ones.
    Root im{ builtin_module("_math_integer") };
    if (im.v.is_nil())
        return false;
    DictObj *id = module_dict(im.v);
    for (const ModDef &def : INTEGER_DEFS) {
        StrObj *k = str_intern(def.name);
        Value fn;
        if (!k || dict_get(id, obj_value(k), fn) != R::Ok ||
            !mod_put(static_cast<DictObj *>(rd.v.obj()), def.name, fn))
            return err_pending() ? false : oom() == R::Ok;
    }
    return module_register("math.integer", im.v) &&
           mod_put(static_cast<DictObj *>(rd.v.obj()), "integer", im.v);
}

bool math_integer_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    return mod_defs(static_cast<DictObj *>(rd.v.obj()), INTEGER_DEFS) &&
           mod_str(static_cast<DictObj *>(rd.v.obj()), "__name__", "math.integer");
}

bool cmath_install(DictObj *into)
{
    static_assert(sizeof CUNARY_DEFS / sizeof CUNARY_DEFS[0] == sizeof CUNARY / sizeof CUNARY[0],
                  "every row of CUNARY needs a native");
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, CUNARY_DEFS) || !mod_defs(d, CMATH_DEFS))
        return false;
    Root inf{ complex_new(__builtin_inf(), 0) }, nanv{ complex_new(__builtin_nan(""), 0) };
    Root infj{ complex_new(0, __builtin_inf()) }, nanj{ complex_new(0, __builtin_nan("")) };
    if (inf.v.is_nil() || nanv.v.is_nil() || infj.v.is_nil() || nanj.v.is_nil())
        return false;
    return mod_float(d, "pi", PI) && mod_float(d, "e", E) && mod_float(d, "tau", TAU) &&
           mod_float(d, "inf", __builtin_inf()) && mod_float(d, "nan", __builtin_nan("")) &&
           mod_put(d, "infj", infj.v) && mod_put(d, "nanj", nanj.v);
}
