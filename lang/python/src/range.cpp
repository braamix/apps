// range, over ints of any width, and its two iterators: range_iterator where
// everything fits a machine word, longrange_iterator where it does not. The
// rule for which is CPython's, with its 64-bit long.
#include "bigint.h"
#include "gc.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

RangeObj *range_of(Value v)
{
    return static_cast<RangeObj *>(v.obj());
}

// bool is an int to a range, but a range holds ints.
Value plain_int(Value v)
{
    return is_bool(v) ? Value::of_int(is_true(v) ? 1 : 0) : v;
}

bool arith(Value a, Value b, Op op, Value &out)
{
    return int_arith(a, b, op, out) == R::Ok;
}

bool less(Value a, Value b)
{
    bool r = false;
    int_compare(a, b, Cmp::Lt, r);
    return r;
}

bool same(Value a, Value b)
{
    bool r = false;
    int_compare(a, b, Cmp::Eq, r);
    return r;
}

bool negative(Value v)
{
    return int_is_neg(v);
}

bool zero(Value v)
{
    return v.is_int() && v.as_int() == 0;
}

// CPython's compute_range_length.
bool range_length(Value lo, Value hi, Value step, Value &out)
{
    Root rl{ lo }, rh{ hi }, rs{ step };
    bool up = !negative(rs.v);
    if (up ? !less(rl.v, rh.v) : !less(rh.v, rl.v)) {
        out = Value::of_int(0);
        return true;
    }
    Root diff, mag{ rs.v };
    if (!arith(up ? rh.v : rl.v, up ? rl.v : rh.v, Op::Sub, diff.v) ||
        !arith(diff.v, Value::of_int(1), Op::Sub, diff.v))
        return false;
    if (!up && !arith(Value::of_int(0), rs.v, Op::Sub, mag.v))
        return false;
    return arith(diff.v, mag.v, Op::FloorDiv, diff.v) &&
           arith(diff.v, Value::of_int(1), Op::Add, out);
}

R range_len(Value v, usize &out)
{
    Value n = range_of(v)->len;
    i64 x   = 0;
    if (!int_to_i64(n, x) || x > i64(usize(-1) >> 1))
        return err_set("OverflowError", "Python int too large to convert to C ssize_t");
    out = usize(x);
    return R::Ok;
}

bool range_truth(Value v)
{
    return !zero(range_of(v)->len);
}

// start + i * step.
bool item_at(Value r, Value i, Value &out)
{
    Root rr{ r }, ri{ i }, t;
    return arith(ri.v, range_of(rr.v)->step, Op::Mul, t.v) &&
           arith(range_of(rr.v)->start, t.v, Op::Add, out);
}

// _PySlice_GetLongIndices over the range's length, and the range it selects.
R range_slice(Value rv, Value key, Value &out)
{
    Root r{ rv };
    SliceObj *sl = static_cast<SliceObj *>(key.obj());
    Root k{ key };
    Root step{ is_none(sl->step) ? Value::of_int(1) : plain_int(sl->step) };
    Root start{ plain_int(sl->start) }, stop{ plain_int(sl->stop) };
    Value parts[3] = { step.v, start.v, stop.v };
    for (Value p : parts)
        if (!is_none(p) && !is_intval(p))
            return err_set("TypeError",
                           "slice indices must be integers or None or have an __index__ method");
    if (zero(step.v))
        return err_set("ValueError", "slice step cannot be zero");
    bool back = negative(step.v);
    Root len{ range_of(r.v)->len };
    Root lower{ back ? Value::of_int(-1) : Value::of_int(0) };
    Root upper{ len.v };
    if (back && !arith(len.v, Value::of_int(-1), Op::Add, upper.v))
        return R::Err;
    // One end, clamped the way CPython clamps it.
    auto end = [&](Value given, Value dflt, Value &got) -> bool {
        if (is_none(given)) {
            got = dflt;
            return true;
        }
        Root x{ given };
        if (negative(x.v)) {
            if (!arith(x.v, len.v, Op::Add, x.v))
                return false;
            got = less(x.v, lower.v) ? lower.v : x.v;
        } else {
            got = less(upper.v, x.v) ? upper.v : x.v;
        }
        return true;
    };
    Root first, last;
    if (!end(start.v, back ? upper.v : lower.v, first.v) ||
        !end(stop.v, back ? lower.v : upper.v, last.v))
        return R::Err;
    Root a, b, c;
    if (!item_at(r.v, first.v, a.v) || !item_at(r.v, last.v, b.v) ||
        !arith(step.v, range_of(r.v)->step, Op::Mul, c.v))
        return R::Err;
    out = range_new_ints(a.v, b.v, c.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R range_getitem(Value v, Value key, Value &out)
{
    if (is_slice(key))
        return range_slice(v, key, out);
    if (!is_intval(key))
        return err_not("range indices must be integers or slices", key);
    Root r{ v }, i{ plain_int(key) };
    if (negative(i.v) && !arith(i.v, range_of(r.v)->len, Op::Add, i.v))
        return R::Err;
    if (negative(i.v) || !less(i.v, range_of(r.v)->len))
        return err_set("IndexError", "range object index out of range");
    return item_at(r.v, i.v, out) ? R::Ok : R::Err;
}

R range_repr(Value v, String &out)
{
    Root r{ v };
    if (!out.append("range(") || py_repr(range_of(r.v)->start, out) != R::Ok || !out.append(", ") ||
        py_repr(range_of(r.v)->stop, out) != R::Ok)
        return err_pending() ? R::Err : oom();
    Value step = range_of(r.v)->step;
    if (!(step.is_int() && step.as_int() == 1) &&
        (!out.append(", ") || py_repr(step, out) != R::Ok))
        return err_pending() ? R::Err : oom();
    return out.push(')') ? R::Ok : oom();
}

R range_getattr(Value v, StrObj *name, Value &out)
{
    RangeObj *r = range_of(v);
    Str n       = name->str();
    if (n == "start")
        out = r->start;
    else if (n == "stop")
        out = r->stop;
    else if (n == "step")
        out = r->step;
    else
        return R::NotImpl;
    return R::Ok;
}

void range_trace(Obj *o)
{
    RangeObj *r = static_cast<RangeObj *>(o);
    gc_mark(r->start);
    gc_mark(r->stop);
    gc_mark(r->step);
    gc_mark(r->len);
}

// Two ranges are equal when they yield the same items.
R range_eq(Value a, Value b, bool &out)
{
    if (!b.is_obj() || b.obj()->type != &range_type)
        return R::NotImpl;
    RangeObj *x = range_of(a), *y = range_of(b);
    out =
        x == y ||
        (same(x->len, y->len) &&
         (zero(x->len) || (same(x->start, y->start) &&
                           ((x->len.is_int() && x->len.as_int() == 1) || same(x->step, y->step)))));
    return R::Ok;
}

// As CPython's: the hash of (len, start, step), with what does not matter None.
R range_hash(Value v, u32 &out)
{
    Root r{ v };
    TupleObj *t = tuple_new(3);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    RangeObj *x   = range_of(r.v);
    bool one      = x->len.is_int() && x->len.as_int() == 1;
    t->items()[0] = x->len;
    t->items()[1] = zero(x->len) ? value_none() : x->start;
    t->items()[2] = zero(x->len) || one ? value_none() : x->step;
    return py_hash(rt.v, out);
}

// Where an int would be in the range, or false.
bool int_position(Value rv, Value x, Value &at)
{
    Root r{ rv }, rx{ plain_int(x) };
    RangeObj *g = range_of(r.v);
    if (zero(g->len))
        return false;
    Root last;
    if (!item_at(r.v, g->len, last.v))
        return false;
    g = range_of(r.v);
    if (negative(g->step) ? (!less(last.v, rx.v) || less(g->start, rx.v))
                          : (less(rx.v, g->start) || !less(rx.v, last.v)))
        return false;
    Root off, rem;
    if (!arith(rx.v, range_of(r.v)->start, Op::Sub, off.v) ||
        !arith(off.v, range_of(r.v)->step, Op::Mod, rem.v) || !zero(rem.v))
        return false;
    return arith(off.v, range_of(r.v)->step, Op::FloorDiv, at);
}

// Anything else is looked for item by item.
R scan(Value rv, Value x, u32 &count, Value &first)
{
    Root r{ rv }, rx{ x };
    Root it{ py_iter(r.v) };
    if (it.v.is_nil())
        return R::Err;
    count = 0;
    for (i64 i = 0;; i++) {
        Root got;
        R n = py_next(it.v, got.v);
        if (n == R::Err)
            return R::Err;
        if (n == R::NotImpl)
            return R::Ok;
        bool eq = false;
        if (py_eq(got.v, rx.v, eq) != R::Ok)
            return R::Err;
        if (eq && !count++)
            first = int_from_i64(i);
    }
}

R range_contains(Value v, Value item, bool &out)
{
    if (is_intval(item)) {
        Value at;
        out = int_position(v, item, at);
        return err_pending() ? R::Err : R::Ok;
    }
    u32 n = 0;
    Value first;
    if (scan(v, item, n, first) != R::Ok)
        return R::Err;
    out = n != 0;
    return R::Ok;
}

// ------------------------------------------------------------ the iterators

constexpr i64 I64_TOP = i64((u64(1) << 63) - 1);

struct RangeIterObj : Obj {
    i64 next, step;
    u64 left;
};

struct LongIterObj : Obj {
    Value start, step, len, at;
};

extern const Type range_iter_type;
extern const Type longrange_iter_type;

Value self_iter(Value v)
{
    return v;
}

R range_iter_next(Value v, Value &out)
{
    RangeIterObj *it = static_cast<RangeIterObj *>(v.obj());
    if (!it->left)
        return R::NotImpl;
    i64 x = it->next;
    it->left--;
    // Wraps only past the last item, which is never read.
    it->next = i64(u64(x) + u64(it->step));
    out      = int_from_i64(x);
    return out.is_nil() ? R::Err : R::Ok;
}

void long_trace(Obj *o)
{
    LongIterObj *it = static_cast<LongIterObj *>(o);
    gc_mark(it->start);
    gc_mark(it->step);
    gc_mark(it->len);
    gc_mark(it->at);
}

R long_next(Value v, Value &out)
{
    Root rv{ v };
    LongIterObj *it = static_cast<LongIterObj *>(rv.v.obj());
    if (!less(it->at, it->len))
        return R::NotImpl;
    Root t, x, n;
    if (!arith(it->at, it->step, Op::Mul, t.v) ||
        !arith(static_cast<LongIterObj *>(rv.v.obj())->start, t.v, Op::Add, x.v) ||
        !arith(static_cast<LongIterObj *>(rv.v.obj())->at, Value::of_int(1), Op::Add, n.v))
        return R::Err;
    static_cast<LongIterObj *>(rv.v.obj())->at = n.v;
    out                                        = x.v;
    return R::Ok;
}

R iter_repr(Value v, String &out)
{
    Buf<64> b;
    b.put("<").put(type_name(v)).put(" object>");
    return out.append(b.str()) ? R::Ok : oom();
}

constexpr Type range_iter_type{ .name = "range_iterator",
                                .repr = iter_repr,
                                .iter = self_iter,
                                .next = range_iter_next };

constexpr Type longrange_iter_type{ .name  = "longrange_iterator",
                                    .trace = long_trace,
                                    .repr  = iter_repr,
                                    .iter  = self_iter,
                                    .next  = long_next };

// CPython's range_iter: the fast one where start, stop, step and the length
// fit a long, and the last step cannot overflow one.
Value iter_over(Value start, Value stop, Value step, Value len)
{
    i64 lo = 0, hi = 0, st = 0, n = 0;
    bool fast =
        int_to_i64(start, lo) && int_to_i64(stop, hi) && int_to_i64(step, st) && int_to_i64(len, n);
    if (fast && n)
        fast = st > 0 ? hi <= I64_TOP - (st - 1) : hi >= -I64_TOP - 1 + (-1 - st);
    if (fast) {
        RangeIterObj *it =
            static_cast<RangeIterObj *>(obj_alloc(&range_iter_type, sizeof(RangeIterObj)));
        if (!it)
            return oom(), Value();
        it->next = lo;
        it->step = st;
        it->left = u64(n);
        return obj_value(it);
    }
    Root a{ start }, b{ step }, c{ len };
    LongIterObj *it =
        static_cast<LongIterObj *>(obj_alloc(&longrange_iter_type, sizeof(LongIterObj)));
    if (!it)
        return oom(), Value();
    it->start = a.v;
    it->step  = b.v;
    it->len   = c.v;
    it->at    = Value::of_int(0);
    return obj_value(it);
}

Value range_iter(Value v)
{
    RangeObj *r = range_of(v);
    return iter_over(r->start, r->stop, r->step, r->len);
}

// ------------------------------------------------------------- the methods

RangeObj *self_range(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!s.is_obj() || s.obj()->type != &range_type) {
        Buf<96> b;
        b.put("descriptor '").put(who).put("' requires a 'range' object");
        return err_set("TypeError", b.str()), nullptr;
    }
    return range_of(s);
}

R m_count(const CallArgs &a, Value &out)
{
    RangeObj *r = self_range(a, "count");
    if (!r || !meth_args(a, "count", 1, 1))
        return R::Err;
    if (is_intval(a.args[1])) {
        Value at;
        bool in = int_position(obj_value(r), a.args[1], at);
        if (err_pending())
            return R::Err;
        out = Value::of_int(in ? 1 : 0);
        return R::Ok;
    }
    u32 n = 0;
    Value first;
    if (scan(obj_value(r), a.args[1], n, first) != R::Ok)
        return R::Err;
    out = Value::of_int(i32(n));
    return R::Ok;
}

R m_index(const CallArgs &a, Value &out)
{
    RangeObj *r = self_range(a, "index");
    if (!r || !meth_args(a, "index", 1, 1))
        return R::Err;
    Root rr{ obj_value(r) }, x{ a.args[1] };
    if (is_intval(x.v)) {
        if (int_position(rr.v, x.v, out))
            return R::Ok;
        if (err_pending())
            return R::Err;
        return err_set("ValueError", "range.index(x): x not in range");
    }
    u32 n = 0;
    Root first;
    if (scan(rr.v, x.v, n, first.v) != R::Ok)
        return R::Err;
    if (!n)
        return err_set("ValueError", "sequence.index(x): x not in sequence");
    out = first.v;
    return R::Ok;
}

// The same items backwards: start at the last, and step the other way.
R m_reversed(const CallArgs &a, Value &out)
{
    RangeObj *r = self_range(a, "__reversed__");
    if (!r || !meth_args(a, "__reversed__", 0, 0))
        return R::Err;
    out = range_reversed(obj_value(r));
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method RANGE[] = {
    { "count", m_count },
    { "index", m_index },
    { "__reversed__", m_reversed },
};

} // namespace

Value range_reversed(Value rv)
{
    Root rr{ rv }, last, back, stop, n;
    if (!arith(range_of(rr.v)->len, Value::of_int(-1), Op::Add, n.v) ||
        !item_at(rr.v, n.v, last.v) ||
        !arith(Value::of_int(0), range_of(rr.v)->step, Op::Sub, back.v) ||
        !arith(range_of(rr.v)->start, range_of(rr.v)->step, Op::Sub, stop.v))
        return Value();
    return iter_over(last.v, stop.v, back.v, range_of(rr.v)->len);
}

constexpr Type range_type{ .name     = "range",
                           .trace    = range_trace,
                           .truth    = range_truth,
                           .hash     = range_hash,
                           .eq       = range_eq,
                           .repr     = range_repr,
                           .len      = range_len,
                           .getitem  = range_getitem,
                           .contains = range_contains,
                           .iter     = range_iter,
                           .getattr  = range_getattr,
                           .patma    = PATMA_SEQ,
                           .final    = true };

Value range_new_ints(Value start, Value stop, Value step)
{
    Root a{ plain_int(start) }, b{ plain_int(stop) }, c{ plain_int(step) };
    if (zero(c.v))
        return err_set("ValueError", "range() arg 3 must not be zero"), Value();
    Root n;
    if (!range_length(a.v, b.v, c.v, n.v))
        return Value();
    RangeObj *r = static_cast<RangeObj *>(obj_alloc(&range_type, sizeof(RangeObj)));
    if (!r)
        return oom(), Value();
    r->start = a.v;
    r->stop  = b.v;
    r->step  = c.v;
    r->len   = n.v;
    return obj_value(r);
}

Value range_new(i64 start, i64 stop, i64 step)
{
    Root a{ int_from_i64(start) }, b{ int_from_i64(stop) }, c{ int_from_i64(step) };
    if (a.v.is_nil() || b.v.is_nil() || c.v.is_nil())
        return Value();
    return range_new_ints(a.v, b.v, c.v);
}

bool range_methods()
{
    return method_install(&range_type, RANGE);
}
