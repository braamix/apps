// The item codec. Native byte order throughout: this is a machine buffer, and
// `_struct` is where the wire formats live.
#include "binfmt.h"

#include "bigint.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "ops.h"

namespace {

// `l` and `L` are four octets here, as they are in every 32-bit ABI; `u` is a
// codepoint, which CPython also stores four wide.
constexpr ItemKind KINDS[] = {
    { 'b', 1, IT_INT },   { 'B', 1, IT_UINT }, { 'u', 4, IT_CHAR }, { 'h', 2, IT_INT },
    { 'H', 2, IT_UINT },  { 'i', 4, IT_INT },  { 'I', 4, IT_UINT }, { 'l', 4, IT_INT },
    { 'L', 4, IT_UINT },  { 'q', 8, IT_INT },  { 'Q', 8, IT_UINT }, { 'f', 4, IT_FLOAT },
    { 'd', 8, IT_FLOAT }, { '?', 1, IT_BOOL }, { 'c', 1, IT_UINT },
};

u64 take(Str octets, usize at, usize width)
{
    u64 v = 0;
    for (usize i = 0; i < width; i++)
        v |= u64(u8(octets[at + i])) << (8 * i);
    return v;
}

void give(char *octets, usize at, usize width, u64 v)
{
    for (usize i = 0; i < width; i++)
        octets[at + i] = char(u8(v >> (8 * i)));
}

// Eight octets unsigned reaches past i64, so the magnitude comes out of the
// bignum rather than through as_index.
bool as_u64(Value v, u64 &out)
{
    i64 n = 0;
    if (as_index(v, n)) {
        if (n < 0)
            return false;
        out = u64(n);
        return true;
    }
    if (!is_big(v) || big_of(v)->neg || big_of(v)->len > 2)
        return false;
    out = big_of(v)->limbs()[0];
    if (big_of(v)->len > 1)
        out |= u64(big_of(v)->limbs()[1]) << 32;
    return true;
}

bool fits(const ItemKind *k, i64 v)
{
    if (k->kind == IT_UINT)
        return v >= 0 && (k->width >= 8 || u64(v) < (u64(1) << (8 * k->width)));
    if (k->width >= 8)
        return true;
    i64 top = i64(1) << (8 * k->width - 1);
    return v >= -top && v < top;
}

} // namespace

const ItemKind *item_kind(char code)
{
    for (const ItemKind &one : KINDS)
        if (one.code == code)
            return &one;
    return nullptr;
}

Value item_get(Str octets, usize at, const ItemKind *k)
{
    u64 raw = take(octets, at, k->width);
    switch (k->kind) {
    case IT_BOOL:
        return value_bool(raw != 0);
    case IT_FLOAT: {
        if (k->width == 4) {
            u32 bits = u32(raw);
            f32 x    = 0;
            __builtin_memcpy(&x, &bits, 4);
            return float_new(f64(x));
        }
        f64 x = 0;
        __builtin_memcpy(&x, &raw, 8);
        return float_new(x);
    }
    case IT_CHAR: {
        char buf[4];
        usize n = utf8_encode(char32_t(raw), buf);
        if (!n)
            return err_set("ValueError", "not a codepoint"), Value();
        return str_new(Str(buf, n));
    }
    case IT_UINT:
        if (k->width >= 8 && (raw >> 63)) {
            u32 limbs[2] = { u32(raw), u32(raw >> 32) };
            return big_make(limbs, 2, false);
        }
        return int_from_i64(i64(raw));
    default: {
        i64 v = 0;
        if (k->width >= 8) {
            v = i64(raw);
        } else {
            u64 sign = u64(1) << (8 * k->width - 1);
            v        = i64(raw & (sign - 1)) - i64(raw & sign);
        }
        return int_from_i64(v);
    }
    }
}

R item_put(char *octets, usize at, const ItemKind *k, Value v)
{
    switch (k->kind) {
    case IT_BOOL:
        give(octets, at, 1, py_truth(v) ? 1 : 0);
        return R::Ok;
    case IT_FLOAT: {
        f64 x = 0;
        if (!as_number(v, x))
            return err_set2("TypeError", "a float is required", type_name(v));
        if (k->width == 4) {
            f32 y    = f32(x);
            u32 bits = 0;
            __builtin_memcpy(&bits, &y, 4);
            give(octets, at, 4, bits);
        } else {
            u64 bits = 0;
            __builtin_memcpy(&bits, &x, 8);
            give(octets, at, 8, bits);
        }
        return R::Ok;
    }
    case IT_CHAR: {
        if (!is_str(v) || str_of(v)->chars != 1)
            return err_set("TypeError", "array item must be a single character");
        give(octets, at, 4, str_char_at(str_of(v), 0));
        return R::Ok;
    }
    default: {
        if (is_float(v))
            return err_set("TypeError", "an integer is required");
        if (k->kind == IT_UINT && k->width >= 8) {
            u64 u = 0;
            if (!as_u64(v, u))
                return err_set("OverflowError", "value out of range");
            give(octets, at, 8, u);
            return R::Ok;
        }
        i64 n = 0;
        if (!as_index(v, n))
            return is_big(v) ? err_set("OverflowError", "value out of range")
                             : err_set2("TypeError", "an integer is required", type_name(v));
        if (!fits(k, n))
            return err_set("OverflowError", "value out of range");
        give(octets, at, k->width, u64(n));
        return R::Ok;
    }
    }
}
