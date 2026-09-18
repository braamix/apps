// bytes and bytearray: octets, immutable and not. Indexing either yields an
// int, as in Python 3. Every read-only slot is shared.
#include "bigint.h"
#include "format.h"
#include "gc.h"
#include "iter.h"
#include "kernel/hash.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The octets of either.
Str octets(Value v)
{
    Str s;
    bytes_like(v, s);
    return s;
}

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

// -b: the program asked to be warned, -bb to be stopped.
R bytes_warning(Str message)
{
    u32 level = py_config().bytes_warning;
    if (level >= 2)
        return err_set("BytesWarning", message);
    if (level)
        vm_warn_later("BytesWarning", message);
    return R::Ok;
}

R any_eq(Value a, Value b, bool &out)
{
    Str y;
    if (!bytes_like(b, y)) {
        if (py_config().bytes_warning) {
            bool array = a.obj()->type != &bytes_type;
            Str m      = is_str(b) ? array ? Str("Comparison between bytearray and string")
                                           : Str("Comparison between bytes and string")
                         : !array && is_intval(b) ? Str("Comparison between bytes and int")
                                                  : Str();
            if (!m.empty() && bytes_warning(m) != R::Ok)
                return R::Err;
        }
        return R::NotImpl;
    }
    out = octets(a) == y;
    return R::Ok;
}

R any_order(Value a, Value b, Cmp op, bool &out)
{
    Str y;
    if (!bytes_like(b, y))
        return R::NotImpl;
    Str x   = octets(a);
    usize n = x.size() < y.size() ? x.size() : y.size();
    int c   = 0;
    for (usize i = 0; i < n && c == 0; i++)
        c = u8(x[i]) < u8(y[i]) ? -1 : u8(x[i]) > u8(y[i]) ? 1 : 0;
    if (c == 0)
        c = x.size() < y.size() ? -1 : x.size() > y.size() ? 1 : 0;
    out = op == Cmp::Lt ? c < 0 : op == Cmp::Le ? c <= 0 : op == Cmp::Gt ? c > 0 : c >= 0;
    return R::Ok;
}

// A slice keeps self's type.
R any_getitem(Value v, Value key, Value &out)
{
    Str s = octets(v);
    if (is_slice(key)) {
        i64 start = 0, stop = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, s.size(), start, stop, step, count))
            return R::Err;
        String buf;
        for (usize k = 0; k < count; k++)
            if (!buf.push(s[usize(start + i64(k) * step)]))
                return oom();
        out = is_bytearray(v) ? bytearray_new(buf.str()) : bytes_new(buf.str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    usize i = 0;
    if (index_of(key, s.size(), i) != R::Ok)
        return R::Err;
    out = Value::of_int(u8(s[i]));
    return R::Ok;
}

R any_contains(Value v, Value item, bool &out)
{
    Str s = octets(v);
    i64 n = 0;
    if (as_index(item, n)) {
        if (n < 0 || n > 255)
            return err_set("ValueError", "byte must be in range(0, 256)");
        out = false;
        for (usize i = 0; i < s.size() && !out; i++)
            out = u8(s[i]) == u8(n);
        return R::Ok;
    }
    Str sub;
    if (!bytes_like(item, sub))
        return err_set2("TypeError", "a bytes-like object is required", type_name(item));
    out = s.find(sub) != Str::npos;
    return R::Ok;
}

// The result takes the type of the side that is the sequence.
R any_binop(Value a, Value b, Op op, Value &out)
{
    Str x, y;
    // Anything with a buffer may follow bytes or a bytearray.
    if (op == Op::Add && (is_bytes(a) || is_bytearray(a)) && bytes_like(a, x) &&
        buffer_like(b, y)) {
        String joined;
        if (!joined.append(x) || !joined.append(y))
            return oom();
        out = is_bytearray(a) ? bytearray_new(joined.str()) : bytes_new(joined.str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (op == Op::Mod && bytes_like(a, x)) {
        out = bytes_mod(a, b);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (op == Op::Mul) {
        Value s = bytes_like(a, x) ? a : b;
        Value n = bytes_like(a, x) ? b : a;
        i64 count;
        if (!bytes_like(s, x) || !as_index(n, count))
            return R::NotImpl;
        String joined;
        if (count > 0 && i64(x.size()) > (i64(1) << 31) / count)
            return err_set("OverflowError", "repeated bytes are too long");
        if (count > 0 && !gc_room(joined, usize(count) * x.size()))
            return oom();
        for (i64 i = 0; i < count; i++)
            if (!joined.append(x))
                return oom();
        out = is_bytearray(s) ? bytearray_new(joined.str()) : bytes_new(joined.str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    return R::NotImpl;
}

// ------------------------------------------------------------- bytearray

void array_trace(Obj *)
{
    // No values inside.
}

void array_fini(Obj *o)
{
    static_cast<ArrayObj *>(o)->data.~Vec();
}

R array_len(Value v, usize &out)
{
    out = array_of(v)->data.size();
    return R::Ok;
}

// One octet, from an int.
R octet_of(Value v, u8 &out)
{
    i64 n = 0;
    if (!as_index(v, n))
        return err_set2("TypeError", "an integer is required", type_name(v));
    if (n < 0 || n > 255)
        return err_set("ValueError", "byte must be in range(0, 256)");
    out = u8(n);
    return R::Ok;
}

R array_setitem(Value v, Value key, Value item)
{
    ArrayObj *b = array_of(v);
    if (is_slice(key)) {
        i64 start = 0, stop = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, b->data.size(), start, stop, step, count))
            return R::Err;
        Root rv{ v };
        Str from;
        String owned;
        if (bytes_like(item, from)) {
            // Copy first: the source may be this bytearray.
            if (!owned.append(from))
                return oom();
            from = owned.str();
        } else {
            // Any iterable of octets, gathered before anything moves.
            Root it{ py_iter(item) };
            if (it.v.is_nil())
                return R::Err;
            for (;;) {
                Root got;
                R r = py_next(it.v, got.v);
                if (r == R::Err)
                    return R::Err;
                if (r == R::NotImpl)
                    break;
                u8 o = 0;
                if (octet_of(got.v, o) != R::Ok || !owned.push(char(o)))
                    return err_pending() ? R::Err : oom();
            }
            from = owned.str();
        }
        b = array_of(rv.v);
        if (step != 1) {
            if (from.size() != count)
                return err_set("ValueError",
                               "attempt to assign to an extended slice of a different size");
            for (usize k = 0; k < count; k++)
                b->data[usize(start + i64(k) * step)] = u8(from[k]);
            return R::Ok;
        }
        b->data.erase(usize(start), count);
        for (usize k = 0; k < from.size(); k++)
            if (!b->data.insert(usize(start) + k, u8(from[k])))
                return oom();
        return R::Ok;
    }
    usize i = 0;
    if (index_of(key, b->data.size(), i) != R::Ok)
        return R::Err;
    u8 o = 0;
    if (octet_of(item, o) != R::Ok)
        return R::Err;
    b->data[i] = o;
    return R::Ok;
}

R array_delitem(Value v, Value key)
{
    ArrayObj *b = array_of(v);
    if (is_slice(key)) {
        i64 start = 0, stop = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, b->data.size(), start, stop, step, count))
            return R::Err;
        for (usize k = 0; k < count; k++)
            b->data.erase(usize(start + i64(step > 0 ? count - 1 - k : k) * step), 1);
        return R::Ok;
    }
    usize i = 0;
    if (index_of(key, b->data.size(), i) != R::Ok)
        return R::Err;
    b->data.erase(i, 1);
    return R::Ok;
}

} // namespace

R bytes_repr(Value v, String &out);
R array_repr(Value v, String &out);

namespace {

R bytes_str(Value v, String &out)
{
    if (py_config().bytes_warning && bytes_warning("str() on a bytes instance") != R::Ok)
        return R::Err;
    return bytes_repr(v, out);
}

R array_str(Value v, String &out)
{
    if (py_config().bytes_warning && bytes_warning("str() on a bytearray instance") != R::Ok)
        return R::Err;
    return array_repr(v, out);
}

} // namespace

constexpr Type bytes_type{ .name     = "bytes",
                           .hash     = bytes_hash,
                           .eq       = any_eq,
                           .order    = any_order,
                           .repr     = bytes_repr,
                           .str      = bytes_str,
                           .len      = bytes_len,
                           .getitem  = any_getitem,
                           .contains = any_contains,
                           .binop    = any_binop,
                           .iter     = seq_iter,
                           .patma    = PATMA_SELF };

constexpr Type bytearray_type{ .name     = "bytearray",
                               .trace    = array_trace,
                               .fini     = array_fini,
                               .eq       = any_eq,
                               .order    = any_order,
                               .repr     = array_repr,
                               .str      = array_str,
                               .len      = array_len,
                               .getitem  = any_getitem,
                               .setitem  = array_setitem,
                               .delitem  = array_delitem,
                               .contains = any_contains,
                               .binop    = any_binop,
                               .iter     = seq_iter,
                               .patma    = PATMA_SELF };

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

Value bytearray_new(Str s)
{
    ArrayObj *o = static_cast<ArrayObj *>(obj_alloc(&bytearray_type, sizeof(ArrayObj)));
    if (!o)
        return err_set("MemoryError", "out of memory"), Value();
    new (&o->data) Vec<u8>();
    for (usize i = 0; i < s.size(); i++)
        if (!o->data.push(u8(s[i])))
            return err_set("MemoryError", "out of memory"), Value();
    return obj_value(o);
}
