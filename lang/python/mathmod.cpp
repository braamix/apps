// `math` and `cmath` over braam::math, which is C99 §7.12 answered by musl.
//
// The floating half is a table: one name, one libm call, one error rule --
// a domain error is ValueError and a range error OverflowError, worked out
// from the answer rather than from errno, which does not exist here. The
// integer half is the interesting part: factorial, comb, gcd and isqrt are
// exact, so they run over bigint.h and never touch a double.
#include "bigint.h"
#include "complex.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "math/math.h"
#include "method.h"
#include "module.h"
#include "ops.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

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

// One argument, as a double. A class instance with a __float__ is Python and
// cannot be called from here -- ground rule 2 -- so only the number tower is
// taken.
bool arg_f64(const CallArgs &a, Str who, u32 at, f64 &out)
{
    if (as_number(a.args[at], out))
        return true;
    Buf<96> b;
    b.put(who).put("() argument must be a number");
    return err_set2("TypeError", b.str(), type_name(a.args[at])), false;
}

// What a libm answer means when the arguments were finite. A NaN out of finite
// input is a domain error; an infinity is an overflow.
R check(Str who, f64 got, bool finite_in, Value &out)
{
    if (finite_in && is_nan(got))
        return err_set2("ValueError", "math domain error", who);
    if (finite_in && is_inf(got))
        return err_set2("OverflowError", "math range error", who);
    out = float_new(got);
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------- the one-argument table

using Fn1 = double (*)(double);

struct Unary {
    Str name;
    Fn1 fn;
    bool domain; // the answer is checked for a domain error
};

// Every one-argument function of a real. `domain` is false where an infinite
// or NaN answer is the right one -- exp overflowing is an OverflowError, but
// that is caught by the range check the wrapper always makes.
constexpr Unary UNARY[] = {
    { "acos", acos, true },    { "acosh", acosh, true },   { "asin", asin, true },
    { "asinh", asinh, false }, { "atan", atan, false },    { "atanh", atanh, true },
    { "cbrt", cbrt, false },   { "cos", cos, true },       { "cosh", cosh, false },
    { "erf", erf, false },     { "erfc", erfc, false },    { "exp", exp, false },
    { "exp2", exp2, false },   { "expm1", expm1, false },  { "fabs", fabs, false },
    { "gamma", tgamma, true }, { "lgamma", lgamma, true }, { "log1p", log1p, true },
    { "sin", sin, true },      { "sinh", sinh, false },    { "sqrt", sqrt, true },
    { "tan", tan, true },      { "tanh", tanh, false },
};

R unary_call(const CallArgs &a, Value &out, const Unary &u)
{
    if (!args_only(a, u.name, 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, u.name, 0, x))
        return R::Err;
    f64 got = u.fn(x);
    // An infinity or a NaN going in is answered as it stands: only a finite
    // argument can make a NaN mean "outside the domain".
    return check(u.name, got, !is_nan(x) && !is_inf(x), out);
}

// One native per row, built by a template so the table stays one line each.
template <usize I>
R unary_at(const CallArgs &a, Value &out)
{
    return unary_call(a, out, UNARY[I]);
}

// ------------------------------------------------------------ the rest of math

R m_log(const CallArgs &a, Value &out)
{
    if (!args_only(a, "log", 1, 2))
        return R::Err;
    f64 x = 0, base = 0;
    if (!arg_f64(a, "log", 0, x))
        return R::Err;
    if (x < 0 || (x == 0 && a.nargs < 2))
        return err_set("ValueError", "math domain error");
    if (a.nargs < 2)
        return check("log", log(x), !is_inf(x), out);
    if (!arg_f64(a, "log", 1, base))
        return R::Err;
    if (base <= 0 || base == 1)
        return err_set("ValueError", "math domain error");
    return check("log", log(x) / log(base), !is_inf(x), out);
}

R m_log2(const CallArgs &a, Value &out)
{
    if (!args_only(a, "log2", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "log2", 0, x))
        return R::Err;
    if (x <= 0)
        return err_set("ValueError", "math domain error");
    return check("log2", log2(x), !is_inf(x), out);
}

R m_log10(const CallArgs &a, Value &out)
{
    if (!args_only(a, "log10", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "log10", 0, x))
        return R::Err;
    if (x <= 0)
        return err_set("ValueError", "math domain error");
    return check("log10", log10(x), !is_inf(x), out);
}

R m_atan2(const CallArgs &a, Value &out)
{
    if (!args_only(a, "atan2", 2, 2))
        return R::Err;
    f64 y = 0, x = 0;
    if (!arg_f64(a, "atan2", 0, y) || !arg_f64(a, "atan2", 1, x))
        return R::Err;
    out = float_new(atan2(y, x));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_hypot(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "hypot() takes no keyword arguments");
    f64 sum = 0;
    for (u32 i = 0; i < a.nargs; i++) {
        f64 x = 0;
        if (!arg_f64(a, "hypot", i, x))
            return R::Err;
        sum += x * x;
    }
    out = float_new(sqrt(sum));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_dist(const CallArgs &a, Value &out)
{
    if (!args_only(a, "dist", 2, 2))
        return R::Err;
    usize n = 0, m = 0;
    if (py_len(a.args[0], n) != R::Ok || py_len(a.args[1], m) != R::Ok)
        return R::Err;
    if (n != m)
        return err_set("ValueError", "both points must have the same number of dimensions");
    f64 sum = 0;
    for (usize i = 0; i < n; i++) {
        Value p, q;
        if (py_getitem(a.args[0], Value::of_int(i32(i)), p) != R::Ok ||
            py_getitem(a.args[1], Value::of_int(i32(i)), q) != R::Ok)
            return R::Err;
        f64 x = 0, y = 0;
        if (!as_number(p, x) || !as_number(q, y))
            return err_set("TypeError", "dist() wants numbers");
        sum += (x - y) * (x - y);
    }
    out = float_new(sqrt(sum));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_fmod(const CallArgs &a, Value &out)
{
    if (!args_only(a, "fmod", 2, 2))
        return R::Err;
    f64 x = 0, y = 0;
    if (!arg_f64(a, "fmod", 0, x) || !arg_f64(a, "fmod", 1, y))
        return R::Err;
    if (y == 0 && !is_nan(x))
        return err_set("ValueError", "math domain error");
    out = float_new(fmod(x, y));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_remainder(const CallArgs &a, Value &out)
{
    if (!args_only(a, "remainder", 2, 2))
        return R::Err;
    f64 x = 0, y = 0;
    if (!arg_f64(a, "remainder", 0, x) || !arg_f64(a, "remainder", 1, y))
        return R::Err;
    if (y == 0 && !is_nan(x))
        return err_set("ValueError", "math domain error");
    out = float_new(remainder(x, y));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_copysign(const CallArgs &a, Value &out)
{
    if (!args_only(a, "copysign", 2, 2))
        return R::Err;
    f64 x = 0, y = 0;
    if (!arg_f64(a, "copysign", 0, x) || !arg_f64(a, "copysign", 1, y))
        return R::Err;
    out = float_new(copysign(x, y));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_nextafter(const CallArgs &a, Value &out)
{
    if (!args_only(a, "nextafter", 2, 2))
        return R::Err;
    f64 x = 0, y = 0;
    if (!arg_f64(a, "nextafter", 0, x) || !arg_f64(a, "nextafter", 1, y))
        return R::Err;
    out = float_new(nextafter(x, y));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_ulp(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ulp", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "ulp", 0, x))
        return R::Err;
    x = fabs(x);
    if (is_nan(x) || is_inf(x))
        out = float_new(x);
    else
        out = float_new(nextafter(x, __builtin_inf()) - x);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_ldexp(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ldexp", 2, 2))
        return R::Err;
    f64 x = 0;
    i64 n = 0;
    if (!arg_f64(a, "ldexp", 0, x))
        return R::Err;
    if (!as_index(a.args[1], n))
        return err_set("TypeError", "ldexp() wants an integer exponent");
    f64 got = ldexp(x, n > 100000 ? 100000 : n < -100000 ? -100000 : i32(n));
    if (is_inf(got) && !is_inf(x))
        return err_set("OverflowError", "math range error");
    out = float_new(got);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_frexp(const CallArgs &a, Value &out)
{
    if (!args_only(a, "frexp", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "frexp", 0, x))
        return R::Err;
    int e       = 0;
    f64 got     = frexp(x, &e);
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    Value m = float_new(got);
    if (m.is_nil())
        return R::Err;
    static_cast<TupleObj *>(rt.v.obj())->items()[0] = m;
    static_cast<TupleObj *>(rt.v.obj())->items()[1] = Value::of_int(e);
    out                                             = rt.v;
    return R::Ok;
}

R m_modf(const CallArgs &a, Value &out)
{
    if (!args_only(a, "modf", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "modf", 0, x))
        return R::Err;
    f64 ip      = 0;
    f64 fp      = modf(x, &ip);
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    // The first is pinned while the second is made: float_new allocates.
    Root a0{ float_new(fp) };
    if (a0.v.is_nil())
        return R::Err;
    Value a1 = float_new(ip);
    if (a1.is_nil())
        return R::Err;
    static_cast<TupleObj *>(rt.v.obj())->items()[0] = a0.v;
    static_cast<TupleObj *>(rt.v.obj())->items()[1] = a1;
    out                                             = rt.v;
    return R::Ok;
}

R m_degrees(const CallArgs &a, Value &out)
{
    if (!args_only(a, "degrees", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "degrees", 0, x))
        return R::Err;
    out = float_new(x * (180.0 / PI));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_radians(const CallArgs &a, Value &out)
{
    if (!args_only(a, "radians", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "radians", 0, x))
        return R::Err;
    out = float_new(x * (PI / 180.0));
    return out.is_nil() ? R::Err : R::Ok;
}

R m_isnan(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isnan", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "isnan", 0, x))
        return R::Err;
    out = value_bool(is_nan(x));
    return R::Ok;
}

R m_isinf(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isinf", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "isinf", 0, x))
        return R::Err;
    out = value_bool(is_inf(x));
    return R::Ok;
}

R m_isfinite(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isfinite", 1, 1))
        return R::Err;
    f64 x = 0;
    if (!arg_f64(a, "isfinite", 0, x))
        return R::Err;
    out = value_bool(!is_nan(x) && !is_inf(x));
    return R::Ok;
}

R m_isclose(const CallArgs &a, Value &out)
{
    Str names[2] = { "rel_tol", "abs_tol" };
    Value got[2] = { Value(), Value() };
    if (a.nargs != 2)
        return err_set("TypeError", "isclose() takes exactly 2 positional arguments");
    for (u32 k = 0; k < a.nkw; k++) {
        Str n   = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        bool ok = false;
        for (u32 i = 0; i < 2; i++)
            if (n == names[i]) {
                got[i] = a.kwvals[k];
                ok     = true;
            }
        if (!ok)
            return err_set2("TypeError", "isclose() got an unexpected keyword argument", n);
    }
    f64 x = 0, y = 0, rel = 1e-9, abs_tol = 0.0;
    if (!arg_f64(a, "isclose", 0, x) || !arg_f64(a, "isclose", 1, y))
        return R::Err;
    if (!got[0].is_nil() && !as_number(got[0], rel))
        return err_set("TypeError", "rel_tol must be a number");
    if (!got[1].is_nil() && !as_number(got[1], abs_tol))
        return err_set("TypeError", "abs_tol must be a number");
    if (rel < 0 || abs_tol < 0)
        return err_set("ValueError", "tolerances must be non-negative");
    if (x == y) {
        out = value_bool(true);
        return R::Ok;
    }
    if (is_inf(x) || is_inf(y) || is_nan(x) || is_nan(y)) {
        out = value_bool(false);
        return R::Ok;
    }
    f64 d  = fabs(y - x);
    f64 ax = fabs(x), ay = fabs(y);
    f64 top = rel * (ax > ay ? ax : ay);
    out     = value_bool(d <= top || d <= abs_tol);
    return R::Ok;
}

R m_fsum(const CallArgs &a, Value &out)
{
    if (!args_only(a, "fsum", 1, 1))
        return R::Err;
    // Neumaier's compensated sum: one running total and one correction, which
    // is what makes fsum exact where a plain loop is not.
    Root it{ py_iter(a.args[0]) };
    if (it.v.is_nil())
        return R::Err;
    f64 sum = 0, comp = 0;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            break;
        f64 x = 0;
        if (!as_number(got.v, x))
            return err_set2("TypeError", "fsum() wants numbers", type_name(got.v));
        f64 t = sum + x;
        if (fabs(sum) >= fabs(x))
            comp += (sum - t) + x;
        else
            comp += (x - t) + sum;
        sum = t;
    }
    out = float_new(sum + comp);
    return out.is_nil() ? R::Err : R::Ok;
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
    b.put(who).put("() wants an integer");
    return err_set2("TypeError", b.str(), type_name(v)), false;
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
    if (a.nargs != 1)
        return err_set("TypeError", "prod() takes exactly one positional argument");
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
        if (py_binop(acc.v, got.v, Op::Mul, next) != R::Ok)
            return R::Err;
        acc = next;
    }
    out = acc.v;
    return R::Ok;
}

R m_pow(const CallArgs &a, Value &out)
{
    if (!args_only(a, "pow", 2, 2))
        return R::Err;
    f64 x = 0, y = 0;
    if (!arg_f64(a, "pow", 0, x) || !arg_f64(a, "pow", 1, y))
        return R::Err;
    if (x < 0 && !is_inf(y) && y != trunc(y))
        return err_set("ValueError", "math domain error");
    f64 got = pow(x, y);
    if (is_inf(got) && !is_inf(x) && !is_inf(y))
        return err_set("OverflowError", "math range error");
    out = float_new(got);
    return out.is_nil() ? R::Err : R::Ok;
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
    { "hypot", m_hypot },
    { "dist", m_dist },
    { "fmod", m_fmod },
    { "remainder", m_remainder },
    { "copysign", m_copysign },
    { "nextafter", m_nextafter },
    { "ulp", m_ulp },
    { "ldexp", m_ldexp },
    { "frexp", m_frexp },
    { "modf", m_modf },
    { "degrees", m_degrees },
    { "radians", m_radians },
    { "isnan", m_isnan },
    { "isinf", m_isinf },
    { "isfinite", m_isfinite },
    { "isclose", m_isclose },
    { "fsum", m_fsum },
    { "log", m_log },
    { "log2", m_log2 },
    { "log10", m_log10 },
    { "ceil", m_ceil },
    { "floor", m_floor },
    { "trunc", m_trunc },
    { "factorial", m_factorial },
    { "comb", m_comb },
    { "perm", m_perm },
    { "gcd", m_gcd },
    { "lcm", m_lcm },
    { "isqrt", m_isqrt },
    { "prod", m_prod },
    { "pow", m_pow },
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
    UNARY_DEF(18), UNARY_DEF(19), UNARY_DEF(20), UNARY_DEF(21), UNARY_DEF(22),
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
    return mod_float(d, "pi", PI) && mod_float(d, "e", E) && mod_float(d, "tau", TAU) &&
           mod_float(d, "inf", __builtin_inf()) && mod_float(d, "nan", __builtin_nan(""));
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
