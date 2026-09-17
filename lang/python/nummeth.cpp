// int's and float's methods. There is no bignum until phase 14, so these work
// in 64 bits and raise OverflowError past that, as the number tower does.
#include "bigint.h"
#include "call.h"
#include "gc.h"
#include "gen.h"
#include "kernel/fmt.h"
#include "math/ftoa.h"
#include "math/math.h"
#include "method.h"
#include "ops.h"

namespace {

// self as an integer. A bool is one, here as everywhere.
// The int a method was reached through, at any width.
Value self_intval(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_intval(s)) {
        Buf<96> b;
        b.put(who).put("() requires an int");
        return err_set2("TypeError", b.str(), type_name(s)), Value();
    }
    return s;
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
    Value self = self_intval(a, "bit_length");
    if (self.is_nil() || !meth_args(a, "bit_length", 0, 0))
        return R::Err;
    out = Value::of_int(i32(int_bits(self)));
    return R::Ok;
}

R m_bit_count(const CallArgs &a, Value &out)
{
    Value self = self_intval(a, "bit_count");
    if (self.is_nil() || !meth_args(a, "bit_count", 0, 0))
        return R::Err;
    out = Value::of_int(i32(int_ones(self)));
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

// signed=, and length= and byteorder= where they were not given by position.
bool int_kws(const CallArgs &a, Str who, Value &length, Value &order, bool &out)
{
    for (u32 i = 0; i < a.nkw; i++) {
        Str n = is_str(a.kwnames[i]) ? str_of(a.kwnames[i])->str() : Str();
        if (n == "signed") {
            out = py_truth(a.kwvals[i]);
        } else if (n == "length" || n == "byteorder") {
            Value &slot = n == "length" ? length : order;
            if (!slot.is_nil()) {
                Buf<96> b;
                b.put("argument for ").put(who).put("() given by name ('").put(n);
                b.put("') and position");
                return err_set("TypeError", b.str()), false;
            }
            slot = a.kwvals[i];
        } else if (n != "bytes") {
            Buf<96> b;
            b.put(who).put("() got an unexpected keyword argument '").put(n).put("'");
            return err_set("TypeError", b.str()), false;
        }
    }
    return true;
}

R m_to_bytes(const CallArgs &a, Value &out)
{
    Value self = self_intval(a, "to_bytes");
    if (self.is_nil())
        return R::Err;
    if (a.nargs > 3)
        return err_set("TypeError", "to_bytes() takes at most 2 arguments");
    Value length = a.nargs > 1 ? a.args[1] : Value();
    Value order  = a.nargs > 2 ? a.args[2] : Value();
    bool sgn     = false;
    if (!int_kws(a, "to_bytes", length, order, sgn))
        return R::Err;
    i64 len = 1;
    if (!length.is_nil() && !as_index(length, len))
        return err_set2("TypeError", "length must be an integer", type_name(length));
    if (len < 0)
        return err_set("ValueError", "length argument must be non-negative");
    bool little = false;
    if (!order.is_nil() && order_of(order, "to_bytes", little) != R::Ok)
        return R::Err;

    String b;
    if (int_to_octets(self, usize(len), little, sgn, b) != R::Ok)
        return R::Err;
    out = bytes_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R m_from_bytes(const CallArgs &a, Value &out)
{
    if (a.nargs < 1 || a.nargs > 2)
        return err_set("TypeError", "from_bytes() takes from 1 to 2 arguments");
    if (iter_needs_vm(a.args[0]))
        return iter_park(a, 0, m_from_bytes, out);
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
    Value length;
    Value order = a.nargs > 1 ? a.args[1] : Value();
    bool sgn    = false;
    if (!int_kws(a, "from_bytes", length, order, sgn))
        return R::Err;
    bool little = false;
    if (!order.is_nil() && order_of(order, "from_bytes", little) != R::Ok)
        return R::Err;
    out = int_from_octets(s, little, sgn);
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
    Value self = self_intval(a, "as_integer_ratio");
    if (self.is_nil() || !meth_args(a, "as_integer_ratio", 0, 0))
        return R::Err;
    Root rn{ self };
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
        // From the bits, as CPython lays it out: a subnormal is 0x0.…p-1022.
        u64 bits = 0;
        for (usize i = 0; i < sizeof v; i++)
            bits |= u64(reinterpret_cast<const u8 *>(&v)[i]) << (8 * i);
        i32 field = i32((bits >> 52) & 0x7ff);
        u64 frac  = bits & ((u64(1) << 52) - 1);
        i32 e     = field ? field - 1023 : -1022;
        Buf<32> b;
        if (bits >> 63)
            b.put('-');
        b.put("0x").put(field ? '1' : '0').put('.');
        constexpr char DIGITS[] = "0123456789abcdef";
        for (i32 i = 12; i >= 0; i--)
            b.put(DIGITS[(frac >> (4 * i)) & 0xf]);
        b.put('p').put(e < 0 ? '-' : '+').put(u32(e < 0 ? -e : e));
        usize n = b.str().size() < sizeof tmp ? b.str().size() : sizeof tmp;
        for (usize i = 0; i < n; i++)
            tmp[i] = b.str()[i];
        text = Str(tmp, n);
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
    Root num, den;
    if (float_ratio(v, num.v, den.v) != R::Ok)
        return R::Err;
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom_err();
    t->items()[0] = num.v;
    t->items()[1] = den.v;
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
