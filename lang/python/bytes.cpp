// bytes: immutable octets. Indexing one yields an int, as in Python 3.
#include "iter.h"
#include "kernel/hash.h"
#include "ops.h"

namespace {

R bytes_len(Value v, usize &out)
{
    out = static_cast<BytesObj *>(v.obj())->len;
    return R::Ok;
}

R bytes_hash(Value v, u32 &out)
{
    out = static_cast<BytesObj *>(v.obj())->hash;
    return R::Ok;
}

R bytes_eq(Value a, Value b, bool &out)
{
    if (!is_bytes(b))
        return R::NotImpl;
    out = static_cast<BytesObj *>(a.obj())->str() == static_cast<BytesObj *>(b.obj())->str();
    return R::Ok;
}

R bytes_order(Value a, Value b, Cmp op, bool &out)
{
    if (!is_bytes(b))
        return R::NotImpl;
    Str x   = static_cast<BytesObj *>(a.obj())->str();
    Str y   = static_cast<BytesObj *>(b.obj())->str();
    usize n = x.size() < y.size() ? x.size() : y.size();
    int c   = 0;
    for (usize i = 0; i < n && c == 0; i++)
        c = u8(x[i]) < u8(y[i]) ? -1 : u8(x[i]) > u8(y[i]) ? 1 : 0;
    if (c == 0)
        c = x.size() < y.size() ? -1 : x.size() > y.size() ? 1 : 0;
    out = op == Cmp::Lt ? c < 0 : op == Cmp::Le ? c <= 0 : op == Cmp::Gt ? c > 0 : c >= 0;
    return R::Ok;
}

R bytes_getitem(Value v, Value key, Value &out)
{
    BytesObj *b = static_cast<BytesObj *>(v.obj());
    if (is_slice(key)) {
        i64 start = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, b->len, start, step, count))
            return R::Err;
        String buf;
        for (usize k = 0; k < count; k++)
            if (!buf.push(char(b->data()[usize(start + i64(k) * step)])))
                return err_set("MemoryError", "out of memory");
        out = bytes_new(buf.str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    usize i = 0;
    if (index_of(key, b->len, i) != R::Ok)
        return R::Err;
    out = Value::of_int(b->data()[i]);
    return R::Ok;
}

R bytes_contains(Value v, Value item, bool &out)
{
    BytesObj *b = static_cast<BytesObj *>(v.obj());
    i64 n       = 0;
    if (as_index(item, n)) {
        if (n < 0 || n > 255)
            return err_set("ValueError", "byte must be in range(0, 256)");
        out = false;
        for (usize i = 0; i < b->len && !out; i++)
            out = b->data()[i] == u8(n);
        return R::Ok;
    }
    if (!is_bytes(item))
        return err_set2("TypeError", "a bytes-like object is required", type_name(item));
    out = b->str().find(static_cast<BytesObj *>(item.obj())->str()) != Str::npos;
    return R::Ok;
}

R bytes_binop(Value a, Value b, Op op, Value &out)
{
    if (op == Op::Add && is_bytes(a) && is_bytes(b)) {
        String joined;
        if (!joined.append(static_cast<BytesObj *>(a.obj())->str()) ||
            !joined.append(static_cast<BytesObj *>(b.obj())->str()))
            return err_set("MemoryError", "out of memory");
        out = bytes_new(joined.str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (op == Op::Mul) {
        Value s = is_bytes(a) ? a : b;
        Value n = is_bytes(a) ? b : a;
        i64 count;
        if (!is_bytes(s) || !as_index(n, count))
            return R::NotImpl;
        String joined;
        Str one = static_cast<BytesObj *>(s.obj())->str();
        for (i64 i = 0; i < count; i++)
            if (!joined.append(one))
                return err_set("MemoryError", "out of memory");
        out = bytes_new(joined.str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    return R::NotImpl;
}

} // namespace

R bytes_repr(Value v, String &out);

constexpr Type bytes_type{ .name     = "bytes",
                           .hash     = bytes_hash,
                           .eq       = bytes_eq,
                           .order    = bytes_order,
                           .repr     = bytes_repr,
                           .len      = bytes_len,
                           .getitem  = bytes_getitem,
                           .contains = bytes_contains,
                           .binop    = bytes_binop,
                           .iter     = seq_iter };

Value bytes_new(Str s)
{
    BytesObj *o = static_cast<BytesObj *>(obj_alloc(&bytes_type, sizeof(BytesObj) + s.size()));
    if (!o)
        return err_set("MemoryError", "out of memory"), Value();
    o->len  = u32(s.size());
    o->hash = hash_key(s);
    for (usize i = 0; i < s.size(); i++)
        o->data()[i] = u8(s[i]);
    return obj_value(o);
}
