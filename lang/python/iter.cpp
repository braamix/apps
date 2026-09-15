// Slice, range, and the three iterators every container here is walked with.
#include "iter.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "ops.h"

namespace {

// ------------------------------------------------------------------- slice

void slice_trace(Obj *o)
{
    SliceObj *s = static_cast<SliceObj *>(o);
    gc_mark(s->start);
    gc_mark(s->stop);
    gc_mark(s->step);
}

R slice_repr(Value v, String &out)
{
    SliceObj *s = static_cast<SliceObj *>(v.obj());
    if (!out.append("slice("))
        return err_set("MemoryError", "out of memory");
    Value parts[3] = { s->start, s->stop, s->step };
    for (u32 k = 0; k < 3; k++) {
        if (k && !out.append(", "))
            return err_set("MemoryError", "out of memory");
        if (py_repr(parts[k], out) != R::Ok)
            return R::Err;
    }
    return out.push(')') ? R::Ok : err_set("MemoryError", "out of memory");
}

// ------------------------------------------------------------------- range

R range_len(Value v, usize &out)
{
    RangeObj *r = static_cast<RangeObj *>(v.obj());
    i64 span    = r->step > 0 ? r->stop - r->start : r->start - r->stop;
    i64 step    = r->step > 0 ? r->step : -r->step;
    out         = span <= 0 ? 0 : usize((span + step - 1) / step);
    return R::Ok;
}

R range_getitem(Value v, Value key, Value &out)
{
    RangeObj *r = static_cast<RangeObj *>(v.obj());
    usize n     = 0;
    range_len(v, n);
    if (is_slice(key)) {
        i64 start = 0, stop = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, n, start, stop, step, count))
            return R::Err;
        // A slice of a range is a range: no items are made, and the bounds are
        // the slice's own indices mapped back through this range's step.
        out = range_new(r->start + start * r->step, r->start + stop * r->step, step * r->step);
        return out.is_nil() ? R::Err : R::Ok;
    }
    usize i = 0;
    if (index_of(key, n, i) != R::Ok)
        return R::Err;
    out = int_from_i64(r->start + i64(i) * r->step);
    return out.is_nil() ? R::Err : R::Ok;
}

R range_repr(Value v, String &out)
{
    RangeObj *r = static_cast<RangeObj *>(v.obj());
    char tmp[24];
    Buf<96> b;
    b.put("range(").put(int_text(tmp, sizeof tmp, r->start));
    b.put(", ").put(int_text(tmp, sizeof tmp, r->stop));
    if (r->step != 1)
        b.put(", ").put(int_text(tmp, sizeof tmp, r->step));
    b.put(')');
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

// --------------------------------------------------------------- iterators

// One shape for all three: `at` is the next position, `owner` what is walked.
struct IterObj : Obj {
    Value owner;
    usize at;
};

extern const Type seq_iter_type;
extern const Type table_iter_type;
extern const Type range_iter_type;

void iter_trace(Obj *o)
{
    gc_mark(static_cast<IterObj *>(o)->owner);
}

Value iter_self(Value v)
{
    return v;
}

IterObj *iter_new(const Type *t, Value owner)
{
    Root r{ owner };
    IterObj *o = static_cast<IterObj *>(obj_alloc(t, sizeof(IterObj)));
    if (!o)
        return err_set("MemoryError", "out of memory"), nullptr;
    o->owner = r.v;
    o->at    = 0;
    return o;
}

R seq_iter_next(Value v, Value &out)
{
    IterObj *it = static_cast<IterObj *>(v.obj());
    usize n     = 0;
    if (py_len(it->owner, n) != R::Ok)
        return R::Err;
    if (it->at >= n)
        return R::NotImpl;
    Value key = Value::of_int(i32(it->at));
    it->at++;
    return py_getitem(it->owner, key, out);
}

R table_iter_next(Value v, Value &out)
{
    IterObj *it    = static_cast<IterObj *>(v.obj());
    const Table &t = is_dict(it->owner) ? static_cast<DictObj *>(it->owner.obj())->t
                                        : static_cast<SetObj *>(it->owner.obj())->t;
    Value key, val;
    return table_next(t, it->at, key, val) ? (out = key, R::Ok) : R::NotImpl;
}

R range_iter_next(Value v, Value &out)
{
    IterObj *it = static_cast<IterObj *>(v.obj());
    RangeObj *r = static_cast<RangeObj *>(it->owner.obj());
    i64 at      = r->start + i64(it->at) * r->step;
    if (r->step > 0 ? at >= r->stop : at <= r->stop)
        return R::NotImpl;
    it->at++;
    out = int_from_i64(at);
    return out.is_nil() ? R::Err : R::Ok;
}

R iter_repr(Value v, String &out)
{
    Buf<64> b;
    b.put("<").put(type_name(v)).put(" object>");
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

constexpr Type seq_iter_type{ .name  = "iterator",
                              .trace = iter_trace,
                              .repr  = iter_repr,
                              .iter  = iter_self,
                              .next  = seq_iter_next };

constexpr Type table_iter_type{ .name  = "iterator",
                                .trace = iter_trace,
                                .repr  = iter_repr,
                                .iter  = iter_self,
                                .next  = table_iter_next };

constexpr Type range_iter_type{ .name  = "range_iterator",
                                .trace = iter_trace,
                                .repr  = iter_repr,
                                .iter  = iter_self,
                                .next  = range_iter_next };

Value range_iter(Value v)
{
    return obj_value(iter_new(&range_iter_type, v));
}

} // namespace

constexpr Type slice_type{ .name = "slice", .trace = slice_trace, .repr = slice_repr };

constexpr Type range_type{ .name    = "range",
                           .repr    = range_repr,
                           .len     = range_len,
                           .getitem = range_getitem,
                           .iter    = range_iter };

Value slice_new(Value start, Value stop, Value step)
{
    Root a{ start }, b{ stop }, c{ step };
    SliceObj *s = static_cast<SliceObj *>(obj_alloc(&slice_type, sizeof(SliceObj)));
    if (!s)
        return err_set("MemoryError", "out of memory"), Value();
    s->start = a.v;
    s->stop  = b.v;
    s->step  = c.v;
    return obj_value(s);
}

bool slice_resolve(Value v, usize len, i64 &start, i64 &stop, i64 &step, usize &count)
{
    SliceObj *s = static_cast<SliceObj *>(v.obj());
    step        = 1;
    if (!is_none(s->step) && !as_index(s->step, step))
        return err_set("TypeError", "slice indices must be integers or None"), false;
    if (step == 0)
        return err_set("ValueError", "slice step cannot be zero"), false;

    // Going backwards, the ends swap and one past each is -1 and n - 1.
    i64 n     = i64(len);
    i64 lo    = step > 0 ? 0 : -1;
    i64 hi    = step > 0 ? n : n - 1;
    i64 first = step > 0 ? 0 : n - 1;
    i64 last  = step > 0 ? n : -1;
    if (!is_none(s->start)) {
        if (!as_index(s->start, first))
            return err_set("TypeError", "slice indices must be integers or None"), false;
        if (first < 0)
            first += n;
        first = first < lo ? lo : first > hi ? hi : first;
    }
    if (!is_none(s->stop)) {
        if (!as_index(s->stop, last))
            return err_set("TypeError", "slice indices must be integers or None"), false;
        if (last < 0)
            last += n;
        last = last < lo ? lo : last > hi ? hi : last;
    }

    i64 span = step > 0 ? last - first : first - last;
    i64 mag  = step > 0 ? step : -step;
    start    = first;
    stop     = last;
    count    = span <= 0 ? 0 : usize((span + mag - 1) / mag);
    return true;
}

Value range_new(i64 start, i64 stop, i64 step)
{
    if (step == 0)
        return err_set("ValueError", "range() arg 3 must not be zero"), Value();
    RangeObj *r = static_cast<RangeObj *>(obj_alloc(&range_type, sizeof(RangeObj)));
    if (!r)
        return err_set("MemoryError", "out of memory"), Value();
    r->start = start;
    r->stop  = stop;
    r->step  = step;
    return obj_value(r);
}

Value seq_iter(Value seq)
{
    return obj_value(iter_new(&seq_iter_type, seq));
}

Value table_iter(Value owner)
{
    return obj_value(iter_new(&table_iter_type, owner));
}
