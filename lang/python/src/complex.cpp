// complex: two f64 in one object, and the arithmetic over them.
//
// The whole type is here -- the slots, the methods and the repr -- because it
// is small and because nothing else in the tower has to know about it: a
// complex meets an int or a float only through binop, and never through a
// comparison other than equality.
#include "complex.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "math/ftoa.h"
#include "math/math.h"
#include "method.h"
#include "ops.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

bool complex_truth(Value v)
{
    ComplexObj *c = complex_of(v);
    return c->re != 0 || c->im != 0;
}

R complex_hash(Value v, u32 &out)
{
    // hash(complex(x, 0)) is hash(x), which is what makes {1: 'a'}[1+0j] hit.
    ComplexObj *c = complex_of(v);
    u32 a = 0, b = 0;
    Root re{ float_new(c->re) }, im{ float_new(c->im) };
    if (re.v.is_nil() || im.v.is_nil())
        return R::Err;
    if (py_hash(re.v, a) != R::Ok || py_hash(im.v, b) != R::Ok)
        return R::Err;
    out = a + 1000003u * b;
    if (out == u32(-1))
        out = u32(-2);
    return R::Ok;
}

// One part, as CPython writes it: the shortest round-trip digits, and no
// trailing `.0` -- a complex says `(1+0j)` and not `(1.0+0.0j)`.
bool put_part(String &out, f64 x)
{
    char tmp[48];
    Str s = float_text(tmp, sizeof tmp, x);
    if (s.size() > 2 && s[s.size() - 2] == '.' && s[s.size() - 1] == '0')
        s = s.substr(0, s.size() - 2);
    return out.append(s);
}

R complex_repr(Value v, String &out)
{
    ComplexObj *c = complex_of(v);
    // A real part of positive zero is not written at all, which is why the
    // parentheses come and go: `1j`, but `(-0+1j)`.
    if (c->re == 0 && !signbit(c->re)) {
        if (!put_part(out, c->im) || !out.push('j'))
            return oom();
        return R::Ok;
    }
    if (!out.push('(') || !put_part(out, c->re))
        return oom();
    if (!out.push(signbit(c->im) ? '-' : '+'))
        return oom();
    if (!put_part(out, fabs(c->im)) || !out.append("j)"))
        return oom();
    return R::Ok;
}

// A complex equals an int or a float when its imaginary part is zero and the
// real part equals that number; nothing else compares at all.
R complex_eq(Value a, Value b, bool &out)
{
    ComplexObj *x = complex_of(a);
    if (is_complex(b)) {
        ComplexObj *y = complex_of(b);
        out           = x->re == y->re && x->im == y->im;
        return R::Ok;
    }
    f64 other = 0;
    if (!as_number(b, other))
        return R::NotImpl;
    out = x->im == 0 && x->re == other;
    return R::Ok;
}

R complex_order(Value a, Value b, Cmp op, bool &out)
{
    (void)a;
    (void)b;
    (void)out;
    Buf<96> m;
    m.put("'").put(cmp_symbol(op)).put("' not supported between instances of 'complex' and ");
    m.put("'complex'");
    return err_set("TypeError", m.str());
}

// The two operands as complex pairs. False when one of them is not a number.
bool pair(Value a, Value b, f64 &ar, f64 &ai, f64 &br, f64 &bi)
{
    if (is_complex(a)) {
        ar = complex_of(a)->re;
        ai = complex_of(a)->im;
    } else if (as_number(a, ar)) {
        ai = 0;
    } else {
        return false;
    }
    if (is_complex(b)) {
        br = complex_of(b)->re;
        bi = complex_of(b)->im;
    } else if (as_number(b, br)) {
        bi = 0;
    } else {
        return false;
    }
    return true;
}

// Smith's division, which keeps the intermediate products in range where the
// textbook formula would overflow.
void div_parts(f64 ar, f64 ai, f64 br, f64 bi, f64 &qr, f64 &qi)
{
    if (fabs(br) >= fabs(bi)) {
        f64 r   = bi / br;
        f64 den = br + bi * r;
        qr      = (ar + ai * r) / den;
        qi      = (ai - ar * r) / den;
    } else {
        f64 r   = br / bi;
        f64 den = br * r + bi;
        qr      = (ar * r + ai) / den;
        qi      = (ai * r - ar) / den;
    }
}

// z ** w. A small whole exponent is repeated multiplication, which is exact
// where the polar form would not be; anything else goes through exp and log.
void pow_parts(f64 ar, f64 ai, f64 br, f64 bi, f64 &pr, f64 &pi)
{
    if (bi == 0 && br == floor(br) && fabs(br) <= 100 && !(ar == 0 && ai == 0)) {
        bool inv = br < 0;
        i32 n    = i32(fabs(br));
        f64 xr = 1, xi = 0, yr = ar, yi = ai;
        for (; n; n >>= 1) {
            if (n & 1) {
                f64 t = xr * yr - xi * yi;
                xi    = xr * yi + xi * yr;
                xr    = t;
            }
            f64 t = yr * yr - yi * yi;
            yi    = 2 * yr * yi;
            yr    = t;
        }
        if (inv)
            div_parts(1, 0, xr, xi, xr, xi);
        pr = xr;
        pi = xi;
        return;
    }
    if (ar == 0 && ai == 0) {
        pr = br == 0 && bi == 0 ? 1 : 0;
        pi = 0;
        return;
    }
    // The polar form, written the way CPython writes it: pow() of the modulus
    // rather than exp(log()), so `(2+0j) ** 0.5` is exactly sqrt(2).
    f64 mod = hypot(ar, ai), at = atan2(ai, ar);
    f64 len   = pow(mod, br);
    f64 phase = at * br;
    if (bi != 0) {
        len /= exp(at * bi);
        phase += bi * log(mod);
    }
    pr = len * cos(phase);
    pi = len * sin(phase);
}

R complex_binop(Value a, Value b, Op op, Value &out)
{
    f64 ar = 0, ai = 0, br = 0, bi = 0;
    if (!pair(a, b, ar, ai, br, bi))
        return R::NotImpl;
    if (int_too_wide(a, ar) || int_too_wide(b, br))
        return R::Err;

    f64 r = 0, i = 0;
    switch (op) {
    case Op::Add:
        r = ar + br;
        i = ai + bi;
        break;
    case Op::Sub:
        r = ar - br;
        i = ai - bi;
        break;
    case Op::Mul:
        // Each product is rounded before the sum: the target is built with
        // -ffp-contract=off, or an fma here would answer differently from the
        // same formula written in Python.
        r = ar * br - ai * bi;
        i = ar * bi + ai * br;
        break;
    case Op::Div:
        if (br == 0 && bi == 0)
            return err_set("ZeroDivisionError", "complex division by zero");
        div_parts(ar, ai, br, bi, r, i);
        break;
    case Op::Pow:
        pow_parts(ar, ai, br, bi, r, i);
        break;
    case Op::FloorDiv:
    case Op::Mod:
        return err_set("TypeError", "can't take floor or mod of complex number");
    default:
        return R::NotImpl;
    }
    out = complex_new(r, i);
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------- the methods

ComplexObj *self_complex(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_complex(s)) {
        Buf<96> b;
        b.put(who).put("() requires a complex");
        return err_set2("TypeError", b.str(), type_name(s)), nullptr;
    }
    return complex_of(s);
}

R m_conjugate(const CallArgs &a, Value &out)
{
    ComplexObj *c = self_complex(a, "conjugate");
    if (!c || !meth_args(a, "conjugate", 0, 0))
        return R::Err;
    out = complex_new(c->re, -c->im);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method COMPLEX[] = {
    { "conjugate", m_conjugate },
};

R complex_getattr(Value v, StrObj *name, Value &out)
{
    ComplexObj *c = complex_of(v);
    Str n         = name->str();
    if (n == "real" || n == "imag") {
        out = float_new(n == "real" ? c->re : c->im);
        return out.is_nil() ? R::Err : R::Ok;
    }
    return R::NotImpl;
}

} // namespace

constexpr Type complex_type{ .name    = "complex",
                             .truth   = complex_truth,
                             .hash    = complex_hash,
                             .eq      = complex_eq,
                             .order   = complex_order,
                             .repr    = complex_repr,
                             .binop   = complex_binop,
                             .getattr = complex_getattr };

Value complex_new(f64 re, f64 im)
{
    ComplexObj *o = static_cast<ComplexObj *>(obj_alloc(&complex_type, sizeof(ComplexObj)));
    if (!o)
        return oom(), Value();
    o->re = re;
    o->im = im;
    return obj_value(o);
}

R complex_negate(Value v, Value &out)
{
    ComplexObj *c = complex_of(v);
    out           = complex_new(-c->re, -c->im);
    return out.is_nil() ? R::Err : R::Ok;
}

R complex_abs(Value v, Value &out)
{
    ComplexObj *c = complex_of(v);
    out           = float_new(hypot(c->re, c->im));
    return out.is_nil() ? R::Err : R::Ok;
}

// One signed float, and how far it reached. `j` is not part of it.
bool one_part(Str s, usize &at, f64 &out)
{
    usize used    = 0;
    Option<f64> v = scan_f64(s.substr(at), used);
    if (!v.has_value() || !used)
        return false;
    at += used;
    out = v.value();
    return true;
}

bool complex_parse(Str s, f64 &re, f64 &im)
{
    usize i = 0, j = s.size();
    while (i < j && (s[i] == ' ' || s[i] == '\t'))
        i++;
    while (j > i && (s[j - 1] == ' ' || s[j - 1] == '\t'))
        j--;
    Str body = s.substr(i, j - i);
    if (body.size() && body[0] == '(' && body[body.size() - 1] == ')')
        body = body.substr(1, body.size() - 2);
    if (!body.size())
        return false;

    re        = 0;
    im        = 0;
    usize at  = 0;
    f64 first = 0;
    // A bare `j` or `+j` is one, which scan_f64 will not read.
    bool bare = body[at] == 'j' || body[at] == 'J' ||
                ((body[at] == '+' || body[at] == '-') && at + 1 < body.size() &&
                 (body[at + 1] == 'j' || body[at + 1] == 'J'));
    if (bare) {
        first = body[at] == '-' ? -1.0 : 1.0;
        at += body[at] == 'j' || body[at] == 'J' ? 0 : 1;
    } else if (!one_part(body, at, first)) {
        return false;
    }

    if (at < body.size() && (body[at] == 'j' || body[at] == 'J')) {
        at++;
        if (at != body.size())
            return false;
        im = first;
        return true;
    }
    if (bare)
        return false;
    if (at == body.size()) {
        re = first;
        return true;
    }

    // A real part, then an imaginary one that must carry its own sign.
    if (body[at] != '+' && body[at] != '-')
        return false;
    re         = first;
    f64 second = 0;
    if (at + 1 < body.size() && (body[at + 1] == 'j' || body[at + 1] == 'J')) {
        second = body[at] == '-' ? -1.0 : 1.0;
        at += 2;
    } else if (!one_part(body, at, second)) {
        return false;
    } else {
        if (at >= body.size() || (body[at] != 'j' && body[at] != 'J'))
            return false;
        at++;
    }
    if (at != body.size())
        return false;
    im = second;
    return true;
}

bool complex_methods()
{
    return method_install(&complex_type, COMPLEX);
}
