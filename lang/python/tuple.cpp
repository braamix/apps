// tuple: a fixed run of values stored inline after the header.
#include "gc.h"
#include "iter.h"
#include "ops.h"

namespace {

void tuple_trace(Obj *o)
{
    TupleObj *t = static_cast<TupleObj *>(o);
    for (usize i = 0; i < t->len; i++)
        gc_mark(t->items()[i]);
}

R tuple_len(Value v, usize &out)
{
    out = static_cast<TupleObj *>(v.obj())->len;
    return R::Ok;
}

// Python's: a tuple hashes from its items, so it can key a dict.
R tuple_hash(Value v, u32 &out)
{
    TupleObj *t = static_cast<TupleObj *>(v.obj());
    u32 h       = 2166136261u;
    for (usize i = 0; i < t->len; i++) {
        u32 e = 0;
        if (py_hash(t->items()[i], e) != R::Ok)
            return R::Err;
        h = (h ^ e) * 16777619u;
    }
    out = h;
    return R::Ok;
}

R tuple_eq(Value a, Value b, bool &out)
{
    if (!is_tuple(b))
        return R::NotImpl;
    TupleObj *x = static_cast<TupleObj *>(a.obj());
    TupleObj *y = static_cast<TupleObj *>(b.obj());
    return seq_eq(x->items(), x->len, y->items(), y->len, out);
}

R tuple_order(Value a, Value b, Cmp op, bool &out)
{
    if (!is_tuple(b))
        return R::NotImpl;
    TupleObj *x = static_cast<TupleObj *>(a.obj());
    TupleObj *y = static_cast<TupleObj *>(b.obj());
    return seq_order(x->items(), x->len, y->items(), y->len, op, out);
}

R tuple_getitem(Value v, Value key, Value &out)
{
    TupleObj *t = static_cast<TupleObj *>(v.obj());
    if (is_slice(key)) {
        i64 start = 0, stop = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, t->len, start, stop, step, count))
            return R::Err;
        Root rv{ v };
        TupleObj *r = tuple_new(count);
        if (!r)
            return err_set("MemoryError", "out of memory");
        t = static_cast<TupleObj *>(rv.v.obj());
        for (usize k = 0; k < count; k++)
            r->items()[k] = t->items()[usize(start + i64(k) * step)];
        out = obj_value(r);
        return R::Ok;
    }
    usize i = 0;
    if (index_of(key, t->len, i) != R::Ok)
        return R::Err;
    out = t->items()[i];
    return R::Ok;
}

R tuple_contains(Value v, Value item, bool &out)
{
    TupleObj *t = static_cast<TupleObj *>(v.obj());
    return seq_contains(t->items(), t->len, item, out);
}

R tuple_binop(Value a, Value b, Op op, Value &out)
{
    if (op == Op::Add && is_tuple(a) && is_tuple(b)) {
        TupleObj *x = static_cast<TupleObj *>(a.obj());
        TupleObj *y = static_cast<TupleObj *>(b.obj());
        Root ra{ a }, rb{ b };
        TupleObj *t = tuple_new(x->len + y->len);
        if (!t)
            return err_set("MemoryError", "out of memory");
        x = static_cast<TupleObj *>(ra.v.obj());
        y = static_cast<TupleObj *>(rb.v.obj());
        for (usize i = 0; i < x->len; i++)
            t->items()[i] = x->items()[i];
        for (usize i = 0; i < y->len; i++)
            t->items()[x->len + i] = y->items()[i];
        out = obj_value(t);
        return R::Ok;
    }
    if (op == Op::Mul) {
        Value s = is_tuple(a) ? a : b;
        Value n = is_tuple(a) ? b : a;
        i64 count;
        if (!is_tuple(s) || !as_index(n, count))
            return R::NotImpl;
        if (count < 0)
            count = 0;
        Root rs{ s };
        usize len   = static_cast<TupleObj *>(s.obj())->len;
        TupleObj *t = tuple_new(len * usize(count));
        if (!t)
            return err_set("MemoryError", "out of memory");
        TupleObj *x = static_cast<TupleObj *>(rs.v.obj());
        for (i64 k = 0; k < count; k++)
            for (usize i = 0; i < len; i++)
                t->items()[usize(k) * len + i] = x->items()[i];
        out = obj_value(t);
        return R::Ok;
    }
    return R::NotImpl;
}

} // namespace

R tuple_repr(Value v, String &out);

constexpr Type tuple_type{ .name     = "tuple",
                           .trace    = tuple_trace,
                           .hash     = tuple_hash,
                           .eq       = tuple_eq,
                           .order    = tuple_order,
                           .repr     = tuple_repr,
                           .len      = tuple_len,
                           .getitem  = tuple_getitem,
                           .contains = tuple_contains,
                           .binop    = tuple_binop,
                           .iter     = seq_iter,
                           .patma    = PATMA_SEQ | PATMA_SELF };

TupleObj *tuple_new(usize n)
{
    TupleObj *o =
        static_cast<TupleObj *>(obj_alloc(&tuple_type, sizeof(TupleObj) + n * sizeof(Value)));
    if (!o)
        return nullptr;
    o->len = u32(n);
    for (usize i = 0; i < n; i++)
        o->items()[i] = Value();
    return o;
}

// Item by item, as Python compares two sequences of the same kind.
R seq_eq(const Value *x, usize nx, const Value *y, usize ny, bool &out)
{
    if (nx != ny) {
        out = false;
        return R::Ok;
    }
    for (usize i = 0; i < nx; i++) {
        bool same = false;
        if (py_eq(x[i], y[i], same) != R::Ok)
            return R::Err;
        if (!same) {
            out = false;
            return R::Ok;
        }
    }
    out = true;
    return R::Ok;
}

// Lexicographic.
R seq_order(const Value *x, usize nx, const Value *y, usize ny, Cmp op, bool &out)
{
    usize n = nx < ny ? nx : ny;
    for (usize i = 0; i < n; i++) {
        bool same = false;
        if (py_eq(x[i], y[i], same) != R::Ok)
            return R::Err;
        if (same)
            continue;
        bool less = false;
        if (py_cmp(x[i], y[i], Cmp::Lt, less) != R::Ok)
            return R::Err;
        out = op == Cmp::Lt || op == Cmp::Le ? less : !less;
        return R::Ok;
    }
    int c = nx < ny ? -1 : nx > ny ? 1 : 0;
    out   = op == Cmp::Lt ? c < 0 : op == Cmp::Le ? c <= 0 : op == Cmp::Gt ? c > 0 : c >= 0;
    return R::Ok;
}

R seq_contains(const Value *x, usize n, Value item, bool &out)
{
    for (usize i = 0; i < n; i++) {
        bool same = false;
        if (py_eq(x[i], item, same) != R::Ok)
            return R::Err;
        if (same) {
            out = true;
            return R::Ok;
        }
    }
    out = false;
    return R::Ok;
}
