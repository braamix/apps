// int's and float's methods. There is no bignum until phase 14, so these work
// in 64 bits and raise OverflowError past that, as the number tower does.
#include "gc.h"
#include "kernel/fmt.h"
#include "math/ftoa.h"
#include "math/math.h"
#include "method.h"
#include "ops.h"

namespace {

// self as an integer. A bool is one, here as everywhere.
bool self_int(const CallArgs &a, Str who, i64 &out)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!as_index(s, out)) {
        Buf<96> b;
        b.put(who).put("() requires an int");
        return err_set2("TypeError", b.str(), type_name(s)), false;
    }
    return true;
}

bool self_float(const CallArgs &a, Str who, f64 &out)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_float(s)) {
        Buf<96> b;
        b.put(who).put("() requires a float");
        return err_set2("TypeError", b.str(), type_name(s)), false;
    }
    out = float_of(s);
    return true;
}

// -------------------------------------------------------------------- int

R m_bit_length(const CallArgs &a, Value &out)
{
    i64 n = 0;
    if (!self_int(a, "bit_length", n) || !meth_args(a, "bit_length", 0, 0))
        return R::Err;
    u64 m  = n < 0 ? u64(-(n + 1)) + 1 : u64(n);
    i32 at = 0;
    while (m) {
        m >>= 1;
        at++;
    }
    out = Value::of_int(at);
    return R::Ok;
}

R m_bit_count(const CallArgs &a, Value &out)
{
    i64 n = 0;
    if (!self_int(a, "bit_count", n) || !meth_args(a, "bit_count", 0, 0))
        return R::Err;
    u64 m  = n < 0 ? u64(-(n + 1)) + 1 : u64(n);
    i32 at = 0;
    for (; m; m >>= 1)
        at += i32(m & 1);
    out = Value::of_int(at);
    return R::Ok;
}

// The byte order of to_bytes and from_bytes.
R order_of(Value v, Str who, bool &little)
{
    if (!is_str(v))
        return err_set2("TypeError", "byteorder must be a str", type_name(v));
    Str s = str_of(v)->str();
    if (s == "little")
        little = true;
    else if (s == "big")
        little = false;
    else
        return err_set2("ValueError", "byteorder must be 'little' or 'big'", who);
    return R::Ok;
}

bool signed_kw(const CallArgs &a, Str who, bool &out)
{
    for (u32 i = 0; i < a.nkw; i++) {
        Str n = is_str(a.kwnames[i]) ? str_of(a.kwnames[i])->str() : Str();
        if (n == "signed") {
            out = py_truth(a.kwvals[i]);
        } else if (n != "length" && n != "byteorder" && n != "bytes") {
            Buf<96> b;
            b.put(who).put("() got an unexpected keyword argument '").put(n).put("'");
            return err_set("TypeError", b.str()), false;
        }
    }
    return true;
}

R m_to_bytes(const CallArgs &a, Value &out)
{
    i64 n = 0;
    if (!self_int(a, "to_bytes", n))
        return R::Err;
    if (a.nargs < 2 || a.nargs > 3)
        return err_set("TypeError", "to_bytes() takes from 1 to 2 arguments");
    i64 len = 0;
    if (!as_index(a.args[1], len))
        return err_set2("TypeError", "length must be an integer", type_name(a.args[1]));
    if (len < 0)
        return err_set("ValueError", "length argument must be non-negative");
    bool little = false;
    if (a.nargs > 2 && order_of(a.args[2], "to_bytes", little) != R::Ok)
        return R::Err;
    bool sgn = false;
    if (!signed_kw(a, "to_bytes", sgn))
        return R::Err;
    if (n < 0 && !sgn)
        return err_set("OverflowError", "can't convert a negative int to unsigned");

    // Past the eighth octet there is only the sign. A shift of 64 is not a
    // shift at all on this target.
    u64 m    = u64(n);
    u8 above = n < 0 ? 0xff : 0;
    String b;
    for (i64 i = 0; i < len; i++)
        if (!b.push(char(i < 8 ? u8(m >> (8 * u64(i))) : above)))
            return oom_err();
    // What did not fit has to be the sign extension and nothing else.
    u64 rest = len >= 8 ? 0 : m >> (8 * u64(len));
    u64 want = (n < 0 && len < 8) ? (~u64(0) >> (8 * u64(len))) : 0;
    if (rest != want)
        return err_set("OverflowError", "int too big to convert");
    if (!little)
        for (i64 i = 0; i < len / 2; i++) {
            char t                = b[usize(i)];
            b[usize(i)]           = b[usize(len - 1 - i)];
            b[usize(len - 1 - i)] = t;
        }
    out = bytes_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R m_from_bytes(const CallArgs &a, Value &out)
{
    if (a.nargs < 1 || a.nargs > 2)
        return err_set("TypeError", "from_bytes() takes from 1 to 2 arguments");
    Str s;
    String owned;
    if (!bytes_like(a.args[0], s)) {
        // Any iterable of octets, which is what CPython accepts too.
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
            i64 o = 0;
            if (!as_index(got.v, o) || o < 0 || o > 255)
                return err_set("ValueError", "byte must be in range(0, 256)");
            if (!owned.push(char(o)))
                return oom_err();
        }
        s = owned.str();
    }
    bool little = false;
    if (a.nargs > 1 && order_of(a.args[1], "from_bytes", little) != R::Ok)
        return R::Err;
    bool sgn = false;
    if (!signed_kw(a, "from_bytes", sgn))
        return R::Err;
    // Padding beyond eight octets is sign extension, not an overflow. For an
    // unsigned value it is zero.
    u8 pad  = 0;
    usize n = s.size();
    if (sgn && n) {
        u8 top = u8(s[little ? n - 1 : 0]);
        pad    = (top & 0x80) ? 0xff : 0;
    }
    while (n > 8 && u8(s[little ? n - 1 : s.size() - n]) == pad)
        n--;
    if (n > 8)
        return err_set("OverflowError", "int too large (no bignum yet)");
    Str run = little ? s.substr(0, n) : s.substr(s.size() - n);

    u64 m = 0;
    for (usize i = 0; i < run.size(); i++) {
        usize at = little ? run.size() - 1 - i : i;
        m        = (m << 8) | u8(run[at]);
    }
    i64 v = i64(m);
    if (sgn && run.size() && run.size() < 8) {
        u64 sign = u64(1) << (8 * run.size() - 1);
        if (m & sign)
            v = i64(m | (~u64(0) << (8 * run.size())));
    }
    out = int_from_i64(v);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_conjugate(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "conjugate", 0, 0))
        return R::Err;
    out = method_self(a.args[0]);
    return R::Ok;
}

R m_int_ratio(const CallArgs &a, Value &out)
{
    i64 n = 0;
    if (!self_int(a, "as_integer_ratio", n) || !meth_args(a, "as_integer_ratio", 0, 0))
        return R::Err;
    Value num = int_from_i64(n);
    if (num.is_nil())
        return R::Err;
    Root rn{ num };
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom_err();
    t->items()[0] = rn.v;
    t->items()[1] = Value::of_int(1);
    out           = obj_value(t);
    return R::Ok;
}

// ------------------------------------------------------------------ float

R m_is_integer(const CallArgs &a, Value &out)
{
    f64 v = 0;
    if (!self_float(a, "is_integer", v) || !meth_args(a, "is_integer", 0, 0))
        return R::Err;
    out = value_bool(!isnan(v) && !isinf(v) && floor(v) == v);
    return R::Ok;
}

R m_hex(const CallArgs &a, Value &out)
{
    f64 v = 0;
    if (!self_float(a, "hex", v) || !meth_args(a, "hex", 0, 0))
        return R::Err;
    char tmp[64];
    Str text;
    if (v == 0) {
        // %a of zero has no digits after the point. CPython writes one.
        text = (1 / v) < 0 ? Str("-0x0.0p+0") : Str("0x0.0p+0");
    } else if (isnan(v) || isinf(v)) {
        text = fmt_f64(tmp, sizeof tmp, v, -1, 'f');
    } else {
        text = fmt_f64(tmp, sizeof tmp, v, 13, 'a');
    }
    out = str_new(text);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_fromhex(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs != 1)
        return err_set("TypeError", "fromhex() takes exactly one argument");
    if (!is_str(a.args[0]))
        return err_set2("TypeError", "fromhex() argument must be a str", type_name(a.args[0]));
    Option<f64> v = parse_f64(str_of(a.args[0])->str());
    if (!v.has_value())
        return err_set("ValueError", "invalid hexadecimal floating-point string");
    out = float_new(v.value());
    return out.is_nil() ? R::Err : R::Ok;
}

R m_float_ratio(const CallArgs &a, Value &out)
{
    f64 v = 0;
    if (!self_float(a, "as_integer_ratio", v) || !meth_args(a, "as_integer_ratio", 0, 0))
        return R::Err;
    if (isnan(v))
        return err_set("ValueError", "cannot convert NaN to an integer ratio");
    if (isinf(v))
        return err_set("OverflowError", "cannot convert Infinity to an integer ratio");

    // Double until the value is whole. Without a bignum the numerator
    // overflows long before the 1074 steps that could need.
    f64 num   = v;
    i64 den   = 1;
    int steps = 0;
    while (floor(num) != num && steps < 63) {
        num *= 2;
        den *= 2;
        steps++;
    }
    if (floor(num) != num || num > 9.2e18 || num < -9.2e18)
        return err_set("OverflowError", "int too large (no bignum yet)");
    Root rn{ int_from_i64(i64(num)) };
    if (rn.v.is_nil())
        return R::Err;
    Root rd{ int_from_i64(den) };
    if (rd.v.is_nil())
        return R::Err;
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom_err();
    t->items()[0] = rn.v;
    t->items()[1] = rd.v;
    out           = obj_value(t);
    return R::Ok;
}

constexpr Method INT[] = {
    { "bit_length", m_bit_length }, { "bit_count", m_bit_count },
    { "to_bytes", m_to_bytes },     { "from_bytes", m_from_bytes, true },
    { "conjugate", m_conjugate },   { "as_integer_ratio", m_int_ratio },
};

constexpr Method FLOAT[] = {
    { "is_integer", m_is_integer },        { "hex", m_hex },
    { "fromhex", m_fromhex, true },        { "conjugate", m_conjugate },
    { "as_integer_ratio", m_float_ratio },
};

} // namespace

bool num_methods()
{
    // bool reaches int's methods through its MRO, so it needs no table.
    return method_install(&int_type, INT) && method_install(&float_type, FLOAT);
}
