// Comparisons that have to call Python, driven from C++.
//
// The fix the plan names is a continuation that owns the loop itself: the sort,
// the fold and the search are state machines over a CmpObj, and each comparison
// needing Python is one request the VM answers. The algorithms are the same as
// the plain ones beside them -- the merge is bottom-up, iterative and stable in
// both -- so a list of instances and a list of integers come out alike.
#include "compare.h"

#include "call.h"
#include "gc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The dunder each comparison asks for, and the one the other side answers with.
struct Pair {
    Str name, refl;
};

Pair cmp_names(Cmp op)
{
    switch (op) {
    case Cmp::Eq:
        return { "__eq__", "__eq__" };
    case Cmp::Ne:
        return { "__ne__", "__ne__" };
    case Cmp::Lt:
        return { "__lt__", "__gt__" };
    case Cmp::Le:
        return { "__le__", "__ge__" };
    case Cmp::Gt:
        return { "__gt__", "__lt__" };
    default:
        return { "__ge__", "__le__" };
    }
}

enum : u8 { T_SORT, T_FOLD, T_FIND, T_SEQ };

struct CmpObj : Obj {
    Value keys;      // what is compared
    Value vals;      // what is answered with, or Nil when the keys are it
    Value target;    // what a search looks for
    Value best;      // the best a fold has seen
    Value lhs, rhs;  // the operands of the comparison now out for an answer
    Value fn, arg;   // the call that will answer it
    Value fn2, arg2; // the reflected call, where the first says NotImplemented
    Vec<u32> idx, tmp;
    usize w, lo, i, j, o, mid, hi; // the merge's own loop
    usize at, end, count;          // the fold's and the search's
    Cmp op;
    u8 task;
    u32 what;
    bool rev, least, inplace, started;
};

void cmp_trace(Obj *o)
{
    CmpObj *c = static_cast<CmpObj *>(o);
    gc_mark(c->keys);
    gc_mark(c->vals);
    gc_mark(c->target);
    gc_mark(c->best);
    gc_mark(c->lhs);
    gc_mark(c->rhs);
    gc_mark(c->fn);
    gc_mark(c->arg);
    gc_mark(c->fn2);
    gc_mark(c->arg2);
}

void cmp_fini(Obj *o)
{
    static_cast<CmpObj *>(o)->~CmpObj();
}

R cmp_repr(Value v, String &out)
{
    (void)v;
    return out.append("<comparison>") ? R::Ok : oom();
}

constexpr Type cmp_type{ .name  = "comparison",
                         .trace = cmp_trace,
                         .fini  = cmp_fini,
                         .repr  = cmp_repr };

CmpObj *cmp_of(Value v)
{
    return static_cast<CmpObj *>(v.obj());
}

Value cmp_new(u8 task)
{
    CmpObj *c = static_cast<CmpObj *>(obj_alloc(&cmp_type, sizeof(CmpObj)));
    if (!c)
        return oom(), Value();
    // Placement-new: the Vecs have constructors, and obj_alloc hands back raw
    // storage the way it does for a list.
    new (static_cast<void *>(&c->idx)) Vec<u32>();
    new (static_cast<void *>(&c->tmp)) Vec<u32>();
    c->keys = c->vals = c->target = c->best = Value();
    c->lhs = c->rhs = c->fn = c->arg = c->fn2 = c->arg2 = Value();
    c->w = c->lo = c->i = c->j = c->o = c->mid = c->hi = 0;
    c->at = c->end = c->count = 0;
    c->op                     = Cmp::Lt;
    c->task                   = task;
    c->what                   = CMP_INDEX;
    c->rev = c->least = c->inplace = c->started = false;
    return obj_value(c);
}

Vec<Value> &items_of(Value v)
{
    return list_of(v)->items;
}

// A comparison between two sequences is a comparison of their items, so a
// list of instances needs the driver as much as the instances do. Bounded,
// because a list may hold itself.
bool seq_is_python(Value v, bool order, u32 depth)
{
    Value xs = cmp_items(v);
    if (xs.is_nil() || depth > 3)
        return false;
    for (usize i = 0; i < items_of(xs).size(); i++) {
        Value one = items_of(xs)[i];
        if (is_inst(one) || is_meta_inst(one) ? cmp_is_python(one, order)
                                              : seq_is_python(one, order, depth + 1))
            return true;
    }
    return false;
}

// Ask for `a op b`. R::Ok with `made` false means `out` is the answer already;
// R::Ok with `made` true means a call has been parked and the step will be
// re-entered with what it returned.
R ask(ContObj *k, CmpObj *c, Value a, Value b, Cmp op, bool &out, bool &made)
{
    made   = false;
    c->lhs = a;
    c->rhs = b;
    c->op  = op;
    c->fn = c->arg = c->fn2 = c->arg2 = Value();

    // Two sequences compare by their items, which is a driver of its own.
    if ((op == Cmp::Eq || op == Cmp::Ne) && cmp_same_kind(a, b) &&
        (seq_is_python(a, false, 0) || seq_is_python(b, false, 0))) {
        Value sub = cmp_seq(a, b, op == Cmp::Ne);
        if (sub.is_nil())
            return R::Err;
        cont_of(sub)->next = Value::of_obj(k);
        made               = true;
        return cont_done(k, sub);
    }

    Pair d = cmp_names(op);
    // Both are pinned: binding the second allocates, and a bound method with
    // nothing pointing at it is exactly what a collection there would take.
    Root left{ type_special(a, d.name) };
    // A comparison tries the other side's reflected method even where both
    // are the same class -- do_richcompare does, and it is what lets a class
    // with only a __lt__ answer `>` as well.
    Root right{ type_special(b, d.refl) };
    if (left.v.is_nil() && right.v.is_nil())
        return py_cmp(a, b, op, out);

    if (left.v.is_nil()) {
        c->fn  = right.v;
        c->arg = a;
    } else {
        c->fn   = left.v;
        c->arg  = b;
        c->fn2  = right.v;
        c->arg2 = a;
    }
    made = true;
    return cont_call(k, c->fn, c->arg);
}

// Neither side answered: identity for equality, an error for an ordering.
R nobody_answered(CmpObj *c, bool &out)
{
    if (c->op == Cmp::Eq || c->op == Cmp::Ne)
        return out = (c->lhs == c->rhs) == (c->op == Cmp::Eq), R::Ok;
    Buf<96> m;
    m.put("'").put(cmp_symbol(c->op)).put("' not supported between instances of '");
    m.put(type_name(c->lhs)).put("' and '").put(type_name(c->rhs)).put("'");
    return err_set("TypeError", m.str());
}

// ------------------------------------------------------------------- tasks

// One merge pass at a time, stopping wherever a comparison needs Python.
R sort_run(ContObj *k, CmpObj *c, bool have, bool answer)
{
    usize n = c->idx.size();
    if (have)
        c->tmp[c->o++] = answer ? c->idx[c->j++] : c->idx[c->i++];

    Vec<Value> &keys = items_of(c->keys);
    while (c->w < n) {
        while (c->lo < n) {
            if (!c->started) {
                c->mid     = c->lo + c->w < n ? c->lo + c->w : n;
                c->hi      = c->lo + 2 * c->w < n ? c->lo + 2 * c->w : n;
                c->i       = c->lo;
                c->j       = c->mid;
                c->o       = c->lo;
                c->started = true;
            }
            while (c->i < c->mid && c->j < c->hi) {
                // The left run wins a tie, which is what makes it stable. Only
                // __lt__ is ever asked for, as CPython's sort promises: a
                // reverse sort swaps the operands rather than the operator.
                bool take = false, made = false;
                usize l = c->idx[c->i], r = c->idx[c->j];
                if (ask(k, c, keys[c->rev ? l : r], keys[c->rev ? r : l], Cmp::Lt, take, made) !=
                    R::Ok)
                    return R::Err;
                if (made)
                    return R::Ok;
                c->tmp[c->o++] = take ? c->idx[c->j++] : c->idx[c->i++];
            }
            while (c->i < c->mid)
                c->tmp[c->o++] = c->idx[c->i++];
            while (c->j < c->hi)
                c->tmp[c->o++] = c->idx[c->j++];
            c->lo += 2 * c->w;
            c->started = false;
        }
        for (usize x = 0; x < n; x++)
            c->idx[x] = c->tmp[x];
        c->w *= 2;
        c->lo = 0;
    }

    // Into a copy first: a key function may have changed the list.
    Value src = c->vals.is_nil() ? c->keys : c->vals;
    Vec<Value> sorted;
    for (usize x = 0; x < c->idx.size(); x++)
        if (c->idx[x] < items_of(src).size() && !sorted.push(items_of(src)[c->idx[x]]))
            return oom();
    if (c->inplace) {
        for (usize x = 0; x < sorted.size() && x < items_of(src).size(); x++)
            items_of(src)[x] = sorted[x];
        return cont_done(k, value_none());
    }
    ListObj *out = list_new();
    if (!out)
        return oom();
    Root ro{ obj_value(out) };
    for (usize x = 0; x < sorted.size(); x++)
        if (!list_push(list_of(ro.v), sorted[x]))
            return oom();
    return cont_done(k, ro.v);
}

R fold_run(ContObj *k, CmpObj *c, bool have, bool answer)
{
    Vec<Value> &keys = items_of(c->keys);
    if (have && answer)
        c->count = c->at - 1;
    while (c->at < keys.size()) {
        usize x = c->at++;
        if (!x) {
            c->count = 0;
            continue;
        }
        bool better = false, made = false;
        if (ask(k, c, keys[x], keys[c->count], c->least ? Cmp::Lt : Cmp::Gt, better, made) != R::Ok)
            return R::Err;
        if (made)
            return R::Ok;
        if (better)
            c->count = x;
    }
    Vec<Value> &vals = items_of(c->vals.is_nil() ? c->keys : c->vals);
    return cont_done(k, c->count < vals.size() ? vals[c->count] : value_none());
}

R find_run(ContObj *k, CmpObj *c, bool have, bool answer)
{
    Vec<Value> &xs = items_of(c->keys);
    if (have && answer) {
        if (c->what == CMP_COUNT)
            c->count++;
        else if (c->what == CMP_INDEX)
            return cont_done(k, Value::of_int(i32(c->at - 1)));
        else
            return cont_done(k, value_bool(c->what == CMP_IN));
    }
    while (c->at < c->end && c->at < xs.size()) {
        usize x   = c->at++;
        bool same = false, made = false;
        if (ask(k, c, xs[x], c->target, Cmp::Eq, same, made) != R::Ok)
            return R::Err;
        if (made)
            return R::Ok;
        if (!same)
            continue;
        if (c->what == CMP_COUNT) {
            c->count++;
            continue;
        }
        if (c->what == CMP_INDEX)
            return cont_done(k, Value::of_int(i32(x)));
        return cont_done(k, value_bool(c->what == CMP_IN));
    }
    if (c->what == CMP_COUNT)
        return cont_done(k, Value::of_int(i32(c->count)));
    if (c->what != CMP_INDEX)
        return cont_done(k, value_bool(c->what == CMP_NOTIN));
    return err_set("ValueError", "value is not in the sequence");
}

// Two sequences, item by item: equal only where they are the same length and
// every pair agrees.
R seq_run(ContObj *k, CmpObj *c, bool have, bool answer)
{
    Vec<Value> &xs = items_of(c->keys);
    Vec<Value> &ys = items_of(c->vals);
    if (have && !answer)
        return cont_done(k, value_bool(c->what == CMP_NOTIN));
    if (xs.size() != ys.size())
        return cont_done(k, value_bool(c->what == CMP_NOTIN));
    while (c->at < xs.size()) {
        usize x   = c->at++;
        bool same = false, made = false;
        if (ask(k, c, xs[x], ys[x], Cmp::Eq, same, made) != R::Ok)
            return R::Err;
        if (made)
            return R::Ok;
        if (!same)
            return cont_done(k, value_bool(c->what == CMP_NOTIN));
    }
    return cont_done(k, value_bool(c->what != CMP_NOTIN));
}

// The one step every task shares: the answer to the last comparison comes back
// here, the reflected call is tried where the first said NotImplemented, and
// the task then runs on until it needs another.
R step(ContObj *k, Value in)
{
    CmpObj *c   = cmp_of(k->s[0]);
    bool have   = k->i++ > 0;
    bool answer = false;
    if (have) {
        if (is_notimpl(in) && !c->fn2.is_nil()) {
            Value fn = c->fn2, arg = c->arg2;
            c->fn2 = c->arg2 = Value();
            k->i--; // the same comparison, from the other side
            return cont_call(k, fn, arg);
        }
        if (is_notimpl(in)) {
            if (nobody_answered(c, answer) != R::Ok)
                return R::Err;
        } else {
            answer = py_truth(in);
        }
    }
    switch (c->task) {
    case T_SORT:
        return sort_run(k, c, have, answer);
    case T_FOLD:
        return fold_run(k, c, have, answer);
    case T_SEQ:
        return seq_run(k, c, have, answer);
    default:
        return find_run(k, c, have, answer);
    }
}

Value driver(Value cv)
{
    Root rc{ cv };
    Value kv = cont_new(step);
    if (kv.is_nil())
        return Value();
    cont_of(kv)->s[0] = rc.v;
    return kv;
}

} // namespace

bool cmp_same_kind(Value a, Value b)
{
    return (is_list(a) && is_list(b)) || (is_tuple(a) && is_tuple(b)) || (is_dict(a) && is_dict(b));
}

Value cmp_items(Value v)
{
    if (is_list(v))
        return v;
    if (is_dict(v)) {
        // A dict's values: what a comparison of two of them compares.
        Root rv{ v };
        ListObj *l = list_new();
        if (!l)
            return oom(), Value();
        Root rl{ obj_value(l) };
        usize at = 0;
        Value k, x;
        while (table_next(static_cast<DictObj *>(rv.v.obj())->t, at, k, x))
            if (!list_push(list_of(rl.v), x))
                return oom(), Value();
        return rl.v;
    }
    if (!is_tuple(v))
        return Value();
    Root rv{ v };
    ListObj *l = list_new();
    if (!l)
        return oom(), Value();
    Root rl{ obj_value(l) };
    TupleObj *t = static_cast<TupleObj *>(rv.v.obj());
    for (usize i = 0; i < t->len; i++)
        if (!list_push(list_of(rl.v), static_cast<TupleObj *>(rv.v.obj())->items()[i]))
            return oom(), Value();
    return rl.v;
}

// Two dicts as two lists of values: a's in its order, and b's under the same
// keys. When the keys differ the lists differ in length, which is unequal.
bool dict_pair(Value a, Value b, Value &la, Value &lb)
{
    Root ra{ a }, rb{ b };
    Root x{ obj_value(list_new()) }, y{ obj_value(list_new()) };
    if (x.v.is_nil() || y.v.is_nil())
        return oom(), false;
    la = x.v;
    lb = y.v;
    if (dict_len(static_cast<DictObj *>(ra.v.obj())) !=
        dict_len(static_cast<DictObj *>(rb.v.obj())))
        return list_push(list_of(x.v), value_none()) || (oom(), false);
    usize at = 0;
    Value k, v;
    while (table_next(static_cast<DictObj *>(ra.v.obj())->t, at, k, v)) {
        Root rk{ k }, rvv{ v };
        Value other;
        R r = dict_get(static_cast<DictObj *>(rb.v.obj()), rk.v, other);
        if (r == R::Err)
            return false;
        if (r != R::Ok) {
            list_of(x.v)->items.clear();
            list_of(y.v)->items.clear();
            return list_push(list_of(x.v), value_none()) || (oom(), false);
        }
        if (!list_push(list_of(x.v), rvv.v) || !list_push(list_of(y.v), other))
            return oom(), false;
    }
    return true;
}

Value cmp_seq(Value a, Value b, bool ne)
{
    Root ra, rb;
    if (is_dict(a) && is_dict(b)) {
        if (!dict_pair(a, b, ra.v, rb.v))
            return Value();
    } else {
        ra = cmp_items(a);
        rb = cmp_items(b);
    }
    if (ra.v.is_nil() || rb.v.is_nil())
        return Value();
    Root rc{ cmp_new(T_SEQ) };
    if (rc.v.is_nil())
        return Value();
    CmpObj *c = cmp_of(rc.v);
    c->keys   = ra.v;
    c->vals   = rb.v;
    c->what   = ne ? CMP_NOTIN : CMP_IN;
    return driver(rc.v);
}

bool cmp_is_python(Value v, bool order)
{
    // A class whose metaclass writes the comparison is asked as an instance is.
    if (!is_inst(v) && !is_meta_inst(v))
        return seq_is_python(v, order, 0);
    if (!order)
        return type_has_py_special(v, "__eq__") || type_has_py_special(v, "__ne__");
    return type_has_py_special(v, "__lt__") || type_has_py_special(v, "__gt__") ||
           type_has_py_special(v, "__le__") || type_has_py_special(v, "__ge__");
}

bool cmp_any_python(const Vec<Value> &xs, bool order)
{
    for (usize i = 0; i < xs.size(); i++)
        if (cmp_is_python(xs[i], order))
            return true;
    return false;
}

Value cmp_sort(Value vals, Value keys, bool rev, bool inplace)
{
    Root rv{ vals }, rk{ keys };
    Root rc{ cmp_new(T_SORT) };
    if (rc.v.is_nil())
        return Value();
    CmpObj *c  = cmp_of(rc.v);
    c->keys    = rk.v.is_nil() ? rv.v : rk.v;
    c->vals    = rk.v.is_nil() ? Value() : rv.v;
    c->rev     = rev;
    c->inplace = inplace;
    c->w       = 1;
    usize n    = items_of(c->keys).size();
    for (usize i = 0; i < n; i++)
        if (!c->idx.push(u32(i)) || !c->tmp.push(0))
            return oom(), Value();
    return driver(rc.v);
}

Value cmp_fold(Value vals, Value keys, bool least)
{
    Root rv{ vals }, rk{ keys };
    Root rc{ cmp_new(T_FOLD) };
    if (rc.v.is_nil())
        return Value();
    CmpObj *c = cmp_of(rc.v);
    c->keys   = rk.v.is_nil() ? rv.v : rk.v;
    c->vals   = rk.v.is_nil() ? Value() : rv.v;
    c->least  = least;
    return driver(rc.v);
}

Value cmp_find(Value items, Value target, u32 what, usize from, usize end)
{
    Root ri{ items }, rt{ target };
    Root rc{ cmp_new(T_FIND) };
    if (rc.v.is_nil())
        return Value();
    CmpObj *c = cmp_of(rc.v);
    c->keys   = ri.v;
    c->target = rt.v;
    c->at     = from;
    c->end    = end;
    c->what   = what;
    return driver(rc.v);
}
