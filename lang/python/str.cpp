// str: immutable UTF-8, indexed and counted in codepoints.
#include "format.h"
#include "gc.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "kernel/hash.h"
#include "kernel/text.h"
#include "ops.h"

namespace {

// Bytes to chars, or false on a malformed sequence. utf8_decode yields U+FFFD
// for bad input rather than saying so, so the check is here.
bool utf8_count(Str s, u32 &chars, bool &ascii)
{
    usize n = 0;
    ascii   = true;
    for (usize i = 0; i < s.size();) {
        u8 c = u8(s[i]);
        usize width;
        u32 lo;
        if (c < 0x80) {
            width = 1;
            lo    = 0;
        } else if ((c & 0xe0) == 0xc0) {
            width = 2;
            lo    = 0x80;
        } else if ((c & 0xf0) == 0xe0) {
            width = 3;
            lo    = 0x800;
        } else if ((c & 0xf8) == 0xf0) {
            width = 4;
            lo    = 0x10000;
        } else {
            return false;
        }
        if (i + width > s.size())
            return false;
        u32 cp = width == 1 ? c : (c & (0xff >> (width + 1)));
        for (usize k = 1; k < width; k++) {
            u8 cc = u8(s[i + k]);
            if ((cc & 0xc0) != 0x80)
                return false;
            cp = (cp << 6) | (cc & 0x3f);
        }
        if (cp < lo || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            return false;
        if (cp >= 0x80)
            ascii = false;
        i += width;
        n++;
    }
    chars = u32(n);
    return true;
}

R str_len(Value v, usize &out)
{
    out = str_of(v)->chars;
    return R::Ok;
}

R str_hash(Value v, u32 &out)
{
    out = str_of(v)->hash;
    return R::Ok;
}

R str_eq(Value a, Value b, bool &out)
{
    if (!is_str(b))
        return R::NotImpl;
    StrObj *x = str_of(a), *y = str_of(b);
    out = x == y || (x->hash == y->hash && x->str() == y->str());
    return R::Ok;
}

R str_order(Value a, Value b, Cmp op, bool &out)
{
    if (!is_str(b))
        return R::NotImpl;
    Str x = str_of(a)->str(), y = str_of(b)->str();
    usize n = x.size() < y.size() ? x.size() : y.size();
    int c   = 0;
    for (usize i = 0; i < n && c == 0; i++)
        c = u8(x[i]) < u8(y[i]) ? -1 : u8(x[i]) > u8(y[i]) ? 1 : 0;
    if (c == 0)
        c = x.size() < y.size() ? -1 : x.size() > y.size() ? 1 : 0;
    out = op == Cmp::Lt ? c < 0 : op == Cmp::Le ? c <= 0 : op == Cmp::Gt ? c > 0 : c >= 0;
    return R::Ok;
}

R str_getitem(Value v, Value key, Value &out)
{
    StrObj *s = str_of(v);
    if (is_slice(key)) {
        i64 start = 0, stop = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, s->chars, start, stop, step, count))
            return R::Err;
        String buf;
        for (usize k = 0; k < count; k++) {
            usize c  = usize(start + i64(k) * step);
            usize at = str_offset_of(s, c);
            usize to = str_offset_of(s, c + 1);
            if (!buf.append(Str(s->bytes() + at, to - at)))
                return err_set("MemoryError", "out of memory");
        }
        out = obj_value(str_raw(buf.str()));
        return out.is_nil() ? err_set("MemoryError", "out of memory") : R::Ok;
    }
    usize i = 0;
    if (index_of(key, s->chars, i) != R::Ok)
        return R::Err;
    usize at = str_offset_of(s, i);
    usize to = str_offset_of(s, i + 1);
    out      = obj_value(str_raw(Str(s->bytes() + at, to - at)));
    return out.is_nil() ? err_set("MemoryError", "out of memory") : R::Ok;
}

R str_contains(Value v, Value item, bool &out)
{
    if (!is_str(item))
        return err_set2("TypeError", "'in <string>' requires string as left operand",
                        type_name(item));
    out = str_of(v)->str().find(str_of(item)->str()) != Str::npos;
    return R::Ok;
}

R str_binop(Value a, Value b, Op op, Value &out)
{
    if (op == Op::Add && is_str(a) && is_str(b)) {
        StrObj *x = str_of(a), *y = str_of(b);
        String joined;
        if (!joined.append(x->str()) || !joined.append(y->str()))
            return err_set("MemoryError", "out of memory");
        out = obj_value(str_raw(joined.str()));
        return out.is_nil() ? err_set("MemoryError", "out of memory") : R::Ok;
    }
    // `%` may need Python -- a value's own __str__ or __format__ -- so what
    // comes back can be a ContObj, which the VM lands like any other.
    if (op == Op::Mod && is_str(a)) {
        out = str_mod(a, b);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (op == Op::Mul) {
        Value s = is_str(a) ? a : b;
        Value n = is_str(a) ? b : a;
        i64 count;
        if (!is_str(s) || !as_index(n, count))
            return R::NotImpl;
        if (count < 0)
            count = 0;
        String joined;
        Str one = str_of(s)->str();
        for (i64 i = 0; i < count; i++)
            if (!joined.append(one))
                return err_set("MemoryError", "out of memory");
        out = obj_value(str_raw(joined.str()));
        return out.is_nil() ? err_set("MemoryError", "out of memory") : R::Ok;
    }
    return R::NotImpl;
}

R str_str(Value v, String &out)
{
    return out.append(str_of(v)->str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

} // namespace

// repr lives in repr.cpp, with the other quoting.
R str_repr(Value v, String &out);

constexpr Type str_type{ .name     = "str",
                         .hash     = str_hash,
                         .eq       = str_eq,
                         .order    = str_order,
                         .repr     = str_repr,
                         .str      = str_str,
                         .len      = str_len,
                         .getitem  = str_getitem,
                         .contains = str_contains,
                         .binop    = str_binop,
                         .iter     = seq_iter };

StrObj *str_raw(Str s)
{
    StrObj *o = static_cast<StrObj *>(obj_alloc(&str_type, sizeof(StrObj) + s.size()));
    if (!o)
        return nullptr;
    o->len  = u32(s.size());
    o->hash = hash_key(s);
    for (usize i = 0; i < s.size(); i++)
        o->bytes()[i] = s[i];

    u32 chars  = 0;
    bool ascii = true;
    utf8_count(s, chars, ascii);
    o->chars = chars;
    if (ascii)
        o->flags |= OBJ_ASCII;
    return o;
}

Value str_new(Str s)
{
    u32 chars  = 0;
    bool ascii = true;
    if (!utf8_count(s, chars, ascii))
        return err_set("ValueError", "invalid UTF-8"), Value();
    StrObj *o = str_raw(s);
    if (!o)
        return err_set("MemoryError", "out of memory"), Value();
    return obj_value(o);
}

usize str_offset_of(const StrObj *s, usize i)
{
    if (s->flags & OBJ_ASCII)
        return i < s->len ? i : s->len;
    usize at = 0;
    for (usize n = 0; n < i && at < s->len; n++) {
        u8 c = u8(s->bytes()[at]);
        at += c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
    }
    return at < s->len ? at : s->len;
}

u32 str_char_at(const StrObj *s, usize i)
{
    char32_t cp = 0;
    utf8_decode(s->str(), str_offset_of(s, i), cp);
    return u32(cp);
}
