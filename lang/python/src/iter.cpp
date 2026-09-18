// Slice, and the iterators every container here is walked with; range is
// range.cpp.
#include "iter.h"

#include "builtin.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "reduce.h"
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

// ------------------------------------------------------------------ pickle

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The builtin `name`, which a reduce value names as its callable.
Value builtin_named(Str name)
{
    StrObj *n = str_intern(name);
    Value v;
    if (!n || dict_get(builtins_dict(), obj_value(n), v) != R::Ok)
        return err_pending() ? Value() : (err_set("SystemError", "no such builtin"), Value());
    return v;
}

IterObj *self_iter_of(const CallArgs &a, const Type *t, Str who)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != t) {
        Buf<96> b;
        b.put(who).put("() requires a ").put(t->name);
        return err_set("TypeError", b.str()), nullptr;
    }
    return static_cast<IterObj *>(a.args[0].obj());
}

// (fn, args[, state]) with the builtin `fn`.
R answer(Str fn, Value args, Value state, Value &out)
{
    Root ra{ args }, rs{ state };
    Root f{ builtin_named(fn) };
    if (f.v.is_nil() || ra.v.is_nil())
        return R::Err;
    out = rs.v.is_nil() ? tuple_of(f.v, ra.v) : tuple_of(f.v, ra.v, rs.v);
    return out.is_nil() ? R::Err : R::Ok;
}

// A sequence iterator: iter(seq), then the position.
R seq_reduce(const CallArgs &a, Value &out)
{
    IterObj *it = self_iter_of(a, a.nargs ? type_of(a.args[0]) : nullptr, "__reduce__");
    if (!it || !meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    Root self{ a.args[0] };
    Root at{ int_from_i64(i64(it->at)) };
    return answer("iter", tuple_of(it->owner), at.v, out);
}

// __setstate__(index): where a sequence or reversed iterator resumes.
R seq_setstate(const CallArgs &a, Value &out)
{
    IterObj *it = self_iter_of(a, a.nargs ? type_of(a.args[0]) : nullptr, "__setstate__");
    if (!it || !meth_args(a, "__setstate__", 1, 1))
        return R::Err;
    i64 i = 0;
    if (!as_index(a.args[1], i))
        return err_pending() ? R::Err : err_set("TypeError", "an integer is required");
    usize n = 0;
    if (py_len(it->owner, n) != R::Ok)
        return R::Err;
    if (i < 0)
        i = 0;
    if (u64(i) > n)
        i = i64(n);
    it = static_cast<IterObj *>(a.args[0].obj());
    if (a.args[0].obj()->type == &rev_iter_type)
        it->at = i >= i64(n) ? 0 : n - 1 - usize(i); // the index of the next item, from the front
    else
        it->at = usize(i);
    out = value_none();
    return R::Ok;
}

// A dict's or set's keys: what is left of them, as a list.
R table_reduce(const CallArgs &a, Value &out)
{
    IterObj *it = self_iter_of(a, &table_iter_type, "__reduce__");
    if (!it || !meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    Root self{ a.args[0] };
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    usize at = static_cast<IterObj *>(self.v.obj())->at;
    Value k, v;
    for (;;) {
        IterObj *now   = static_cast<IterObj *>(self.v.obj());
        const Table &t = is_dict(now->owner) ? static_cast<DictObj *>(now->owner.obj())->t
                                             : static_cast<SetObj *>(now->owner.obj())->t;
        if (!table_next(t, at, k, v))
            break;
        if (!list_push(list_of(rl.v), k))
            return oom();
    }
    return answer("iter", tuple_of(rl.v), Value(), out);
}

R enum_reduce(const CallArgs &a, Value &out)
{
    IterObj *it = self_iter_of(a, &enum_iter_type, "__reduce__");
    if (!it || !meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    Root self{ a.args[0] };
    Root n{ int_from_i64(i64(it->at)) };
    if (n.v.is_nil())
        return R::Err;
    return answer("enumerate", tuple_of(static_cast<IterObj *>(self.v.obj())->owner, n.v), Value(),
                  out);
}

// reversed: the sequence and the index of the next item from the front.
R rev_reduce(const CallArgs &a, Value &out)
{
    IterObj *it = self_iter_of(a, &rev_iter_type, "__reduce__");
    if (!it || !meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    Root self{ a.args[0] };
    usize n = 0;
    if (py_len(it->owner, n) != R::Ok)
        return R::Err;
    it = static_cast<IterObj *>(self.v.obj());
    if (it->at >= n)
        return answer("reversed", tuple_of(obj_value(tuple_new(0))), Value(), out);
    Root at{ int_from_i64(i64(n - 1 - it->at)) };
    return answer("reversed", tuple_of(it->owner), at.v, out);
}

R zip_reduce(const CallArgs &a, Value &out)
{
    IterObj *it = self_iter_of(a, &zip_iter_type, "__reduce__");
    if (!it || !meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    return answer("zip", it->owner, it->at ? value_bool(true) : Value(), out);
}

R zip_setstate(const CallArgs &a, Value &out)
{
    IterObj *it = self_iter_of(a, &zip_iter_type, "__setstate__");
    if (!it || !meth_args(a, "__setstate__", 1, 1))
        return R::Err;
    it->at = py_truth(a.args[1]) ? 1 : 0;
    out    = value_none();
    return R::Ok;
}

// map and filter have made their list already: what is left of it.
R made_reduce(const CallArgs &a, Value &out)
{
    const Type *t = a.nargs ? type_of(a.args[0]) : nullptr;
    IterObj *it   = self_iter_of(a, t == &map_type ? &map_type : &filter_type, "__reduce__");
    if (!it || !meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    Root self{ a.args[0] };
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    ListObj *src = list_of(static_cast<IterObj *>(self.v.obj())->owner);
    for (usize i = static_cast<IterObj *>(self.v.obj())->at; i < src->items.size(); i++)
        if (!list_push(list_of(rl.v), src->items[i]))
            return oom();
    return answer("iter", tuple_of(rl.v), Value(), out);
}

constexpr Method SEQ_PICKLE[] = { { "__reduce__", seq_reduce }, { "__setstate__", seq_setstate } };
constexpr Method TABLE_PICKLE[] = { { "__reduce__", table_reduce } };
constexpr Method ENUM_PICKLE[]  = { { "__reduce__", enum_reduce } };
constexpr Method REV_PICKLE[]  = { { "__reduce__", rev_reduce }, { "__setstate__", seq_setstate } };
constexpr Method ZIP_PICKLE[]  = { { "__reduce__", zip_reduce }, { "__setstate__", zip_setstate } };
constexpr Method MADE_PICKLE[] = { { "__reduce__", made_reduce } };

} // namespace

bool iter_pickle_methods()
{
    return method_install(&seq_iter_type, SEQ_PICKLE) &&
           method_install(&table_iter_type, TABLE_PICKLE) &&
           method_install(&enum_iter_type, ENUM_PICKLE) &&
           method_install(&rev_iter_type, REV_PICKLE) &&
           method_install(&zip_iter_type, ZIP_PICKLE) && method_install(&map_type, MADE_PICKLE) &&
           method_install(&filter_type, MADE_PICKLE);
}

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

// A slice compares, orders and hashes as (start, stop, step) does.
R slice_hash(Value v, u32 &out)
{
    SliceObj *s    = static_cast<SliceObj *>(v.obj());
    Value parts[3] = { s->start, s->stop, s->step };
    u32 h          = 2166136261u;
    for (Value p : parts) {
        u32 e = 0;
        if (py_hash(p, e) != R::Ok)
            return R::Err;
        h = (h ^ e) * 16777619u;
    }
    out = h;
    return R::Ok;
}

R slice_eq(Value a, Value b, bool &out)
{
    if (!is_slice(b))
        return R::NotImpl;
    SliceObj *x = static_cast<SliceObj *>(a.obj());
    SliceObj *y = static_cast<SliceObj *>(b.obj());
    Value xs[3] = { x->start, x->stop, x->step };
    Value ys[3] = { y->start, y->stop, y->step };
    return seq_eq(xs, 3, ys, 3, out);
}

R slice_order(Value a, Value b, Cmp op, bool &out)
{
    if (!is_slice(b))
        return R::NotImpl;
    SliceObj *x = static_cast<SliceObj *>(a.obj());
    SliceObj *y = static_cast<SliceObj *>(b.obj());
    Value xs[3] = { x->start, x->stop, x->step };
    Value ys[3] = { y->start, y->stop, y->step };
    return seq_order(xs, 3, ys, 3, op, out);
}

constexpr Type slice_type{ .name    = "slice",
                           .trace   = slice_trace,
                           .hash    = slice_hash,
                           .eq      = slice_eq,
                           .order   = slice_order,
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
