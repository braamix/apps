// Slice, and the iterators every container here is walked with; range is
// range.cpp.
#include "iter.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"

namespace {

// ------------------------------------------------------------------- slice

void slice_trace(Obj *o)
{
    SliceObj *s = static_cast<SliceObj *>(o);
    gc_mark(s->start);
    gc_mark(s->stop);
    gc_mark(s->step);
}

// start, stop and step.
R slice_getattr(Value v, StrObj *name, Value &out)
{
    SliceObj *s = static_cast<SliceObj *>(v.obj());
    Str n       = name->str();
    if (n == "start")
        out = s->start;
    else if (n == "stop")
        out = s->stop;
    else if (n == "step")
        out = s->step;
    else
        return R::NotImpl;
    return R::Ok;
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

// --------------------------------------------------------------- iterators

// One shape for all three: `at` is the next position, `owner` what is walked.
struct IterObj : Obj {
    Value owner;
    usize at;
};

extern const Type seq_iter_type;
extern const Type table_iter_type;

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

// `at` counts, `owner` is the iterator being walked.
R enum_iter_next(Value v, Value &out)
{
    IterObj *it = static_cast<IterObj *>(v.obj());
    Root got;
    R r = py_next(it->owner, got.v);
    if (r != R::Ok)
        return r;
    TupleObj *t = tuple_new(2);
    if (!t)
        return err_set("MemoryError", "out of memory");
    t->items()[0] = Value::of_int(i32(it->at++));
    t->items()[1] = got.v;
    out           = obj_value(t);
    return R::Ok;
}

// reversed(): `at` counts from the end, so the length is read each time.
R rev_iter_next(Value v, Value &out)
{
    IterObj *it = static_cast<IterObj *>(v.obj());
    usize n     = 0;
    if (py_len(it->owner, n) != R::Ok)
        return R::Err;
    if (it->at >= n)
        return R::NotImpl;
    Value key = Value::of_int(i32(n - 1 - it->at));
    it->at++;
    return py_getitem(it->owner, key, out);
}

// zip(): one item from each, until one runs out.
// zip(strict=True), which `at` holds: the iterables must end together.
R zip_strict(IterObj *it, usize len, usize ended)
{
    TupleObj *s = static_cast<TupleObj *>(it->owner.obj());
    Buf<96> b;
    if (ended) {
        b.put("zip() argument ").put(u64(ended + 1)).put(" is shorter than argument");
        if (ended > 1)
            b.put("s 1-").put(u64(ended));
        else
            b.put(" 1");
        return err_set("ValueError", b.str());
    }
    // The first ran out: the others must have too.
    for (usize j = 1; j < len; j++) {
        Value got;
        R r = py_next(s->items()[j], got);
        if (r == R::Err)
            return r;
        if (r == R::Ok) {
            b.put("zip() argument ").put(u64(j + 1)).put(" is longer than argument 1");
            return err_set("ValueError", b.str());
        }
    }
    return R::NotImpl;
}

R zip_iter_next(Value v, Value &out)
{
    IterObj *it = static_cast<IterObj *>(v.obj());
    TupleObj *s = static_cast<TupleObj *>(it->owner.obj());
    if (!s->len) // zip() with no arguments yields nothing
        return R::NotImpl;
    bool strict = it->at != 0;
    usize len   = s->len;
    Root made{ obj_value(tuple_new(len)) };
    if (made.v.is_nil())
        return err_set("MemoryError", "out of memory");
    for (usize i = 0; i < len; i++) {
        Value got;
        R r = py_next(static_cast<TupleObj *>(it->owner.obj())->items()[i], got);
        if (r == R::NotImpl && strict)
            return zip_strict(it, len, i);
        if (r != R::Ok)
            return r;
        static_cast<TupleObj *>(made.v.obj())->items()[i] = got;
    }
    out = made.v;
    return R::Ok;
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

constexpr Type enum_iter_type{ .name  = "enumerate",
                               .trace = iter_trace,
                               .repr  = iter_repr,
                               .iter  = iter_self,
                               .next  = enum_iter_next };

constexpr Type rev_iter_type{ .name  = "reversed",
                              .trace = iter_trace,
                              .repr  = iter_repr,
                              .iter  = iter_self,
                              .next  = rev_iter_next };

constexpr Type zip_iter_type{ .name  = "zip",
                              .trace = iter_trace,
                              .repr  = iter_repr,
                              .iter  = iter_self,
                              .next  = zip_iter_next };

} // namespace

// map() and filter() build their list first, so both are a sequence iterator
// under another name. README says why.
constexpr Type map_type{ .name  = "map",
                         .trace = iter_trace,
                         .repr  = iter_repr,
                         .iter  = iter_self,
                         .next  = seq_iter_next };

constexpr Type filter_type{ .name  = "filter",
                            .trace = iter_trace,
                            .repr  = iter_repr,
                            .iter  = iter_self,
                            .next  = seq_iter_next };

Value made_iter(Value list, const Type *t)
{
    return obj_value(iter_new(t, list));
}

Value reversed_new(Value seq)
{
    if (seq.is_obj() && seq.obj()->type == &range_type)
        return range_reversed(seq);
    if (is_mappingproxy(seq))
        return reversed_new(mappingproxy_inner(seq));
    // A dict is reversed as its keys, newest first, and a view as its items.
    if (is_anydict(seq) || (seq.is_obj() && seq.obj()->type == &view_type)) {
        ListObj *keys = py_list_of(seq);
        if (!keys)
            return Value();
        Vec<Value> &xs = keys->items;
        for (usize i = 0, j = xs.size(); i + 1 < j; i++, j--) {
            Value t   = xs[i];
            xs[i]     = xs[j - 1];
            xs[j - 1] = t;
        }
        return py_iter(obj_value(keys));
    }
    const Type *t = type_of(seq);
    if (!t || !t->len || !t->getitem) {
        Buf<128> m;
        m.put('\'').put(type_name(seq)).put("' object is not reversible");
        return err_set("TypeError", m.str()), Value();
    }
    return obj_value(iter_new(&rev_iter_type, seq));
}

Value zip_new(Value iters, bool strict)
{
    Value v = obj_value(iter_new(&zip_iter_type, iters));
    if (!v.is_nil() && strict)
        static_cast<IterObj *>(v.obj())->at = 1;
    return v;
}

constexpr Type slice_type{ .name    = "slice",
                           .trace   = slice_trace,
                           .repr    = slice_repr,
                           .getattr = slice_getattr };

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

Value seq_iter(Value seq)
{
    return obj_value(iter_new(&seq_iter_type, seq));
}

Value table_iter(Value owner)
{
    return obj_value(iter_new(&table_iter_type, owner));
}

Value enum_iter(Value seq, i64 start)
{
    Root it{ py_iter(seq) };
    if (it.v.is_nil())
        return Value();
    IterObj *o = iter_new(&enum_iter_type, it.v);
    if (!o)
        return Value();
    o->at = usize(start);
    return obj_value(o);
}
