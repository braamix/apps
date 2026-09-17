// list's and tuple's methods.
//
// `list.sort(key=)` is the one that calls back into Python, so it is a
// continuation, as `sorted` is. Both use the same merge underneath.
#include "call.h"
#include "compare.h"
#include "gc.h"
#include "gen.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"

namespace {

// Where in `xs` an item equal to `v` sits, within [from, to). npos for none.
R find_at(const Vec<Value> &xs, Value v, usize from, usize to, usize &out)
{
    for (usize i = from; i < to && i < xs.size(); i++) {
        bool same = false;
        if (py_eq(xs[i], v, same) != R::Ok)
            return R::Err;
        if (same) {
            out = i;
            return R::Ok;
        }
    }
    out = Str::npos;
    return R::Ok;
}

// The start/end pair list.index takes, clamped as a slice is.
bool span_of(Value lo, Value hi, usize len, usize &from, usize &to)
{
    i64 n = i64(len), i = 0, j = n;
    if (!lo.is_nil() && !is_none(lo) && !as_index(lo, i))
        return err_set("TypeError", "slice indices must be integers"), false;
    if (!hi.is_nil() && !is_none(hi) && !as_index(hi, j))
        return err_set("TypeError", "slice indices must be integers"), false;
    if (i < 0)
        i += n;
    if (j < 0)
        j += n;
    i    = i < 0 ? 0 : i > n ? n : i;
    j    = j < 0 ? 0 : j > n ? n : j;
    from = usize(i);
    to   = usize(j > i ? j : i);
    return true;
}

// ------------------------------------------------------------------- list

R m_append(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "append");
    if (!l || !meth_args(a, "append", 1, 1))
        return R::Err;
    if (!list_push(l, a.args[1]))
        return oom_err();
    out = value_none();
    return R::Ok;
}

R m_extend(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "extend");
    if (!l || !meth_args(a, "extend", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, m_extend, out);
    Root rl{ method_self(a.args[0]) };
    // Gather first: the argument may be self.
    ListObj *more = py_list_of(a.args[1]);
    if (!more)
        return R::Err;
    Root rm{ obj_value(more) };
    Vec<Value> &xs = list_of(rm.v)->items;
    for (usize i = 0; i < xs.size(); i++)
        if (!list_push(list_of(rl.v), xs[i]))
            return oom_err();
    out = value_none();
    return R::Ok;
}

R m_insert(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "insert");
    if (!l || !meth_args(a, "insert", 2, 2))
        return R::Err;
    i64 at = 0;
    if (!as_index(a.args[1], at))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    i64 n = i64(l->items.size());
    if (at < 0)
        at += n;
    at = at < 0 ? 0 : at > n ? n : at;
    if (!l->items.insert(usize(at), a.args[2]))
        return oom_err();
    out = value_none();
    return R::Ok;
}

R m_pop(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "pop");
    if (!l || !meth_args(a, "pop", 0, 1))
        return R::Err;
    if (l->items.empty())
        return err_set("IndexError", "pop from empty list");
    i64 at = i64(l->items.size()) - 1;
    if (a.nargs > 1 && !as_index(a.args[1], at))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (at < 0)
        at += i64(l->items.size());
    if (at < 0 || at >= i64(l->items.size()))
        return err_set("IndexError", "pop index out of range");
    out = l->items[usize(at)];
    l->items.erase(usize(at), 1);
    return R::Ok;
}

// The index the search found, turned into the erase remove() promised. It is
// the search's `next`, so it is entered with that index rather than calling.
R remove_step(ContObj *k, Value in)
{
    i64 at = 0;
    if (!as_index(in, at))
        return err_set("SystemError", "a search answered with no index");
    list_of(k->s[0])->items.erase(usize(at), 1);
    return cont_done(k, value_none());
}

R m_remove(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "remove");
    if (!l || !meth_args(a, "remove", 1, 1))
        return R::Err;
    Root rl{ method_self(a.args[0]) };
    // An __eq__ written in Python is a call, so the search suspends and the
    // erase is what the answer is for; remove_step finishes it.
    if (cmp_is_python(a.args[1], false) || cmp_any_python(l->items, false)) {
        Root rf{ cmp_find(rl.v, a.args[1], CMP_INDEX, 0, l->items.size()) };
        if (rf.v.is_nil())
            return R::Err;
        Root kv{ cont_new(remove_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = rl.v;
        cont_of(rf.v)->next = kv.v;
        out                 = rf.v;
        return R::Ok;
    }
    usize at = 0;
    if (find_at(l->items, a.args[1], 0, l->items.size(), at) != R::Ok)
        return R::Err;
    if (at == Str::npos)
        return err_set("ValueError", "list.remove(x): x not in list");
    list_of(rl.v)->items.erase(at, 1);
    out = value_none();
    return R::Ok;
}

// list.__init__(iterable=()): emptied, then extended.
R m_list_init(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "__init__");
    if (!l)
        return R::Err;
    if (a.nkw)
        return err_set("TypeError", "list() takes no keyword arguments");
    if (a.nargs > 2) {
        char tmp[24];
        Buf<96> b;
        b.put("list expected at most 1 argument, got ");
        b.put(int_text(tmp, sizeof tmp, i64(a.nargs - 1)));
        return err_set("TypeError", b.str());
    }
    if (a.nargs == 2 && iter_needs_vm(a.args[1]))
        return iter_park(a, 1, m_list_init, out);
    Root rl{ method_self(a.args[0]) };
    Root more;
    if (a.nargs == 2) {
        ListObj *m = py_list_of(a.args[1]);
        if (!m)
            return R::Err;
        more = obj_value(m);
    }
    list_of(rl.v)->items.clear();
    if (!more.v.is_nil())
        for (Value v : list_of(more.v)->items)
            if (!list_push(list_of(rl.v), v))
                return oom_err();
    out = value_none();
    return R::Ok;
}

R m_clear(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "clear");
    if (!l || !meth_args(a, "clear", 0, 0))
        return R::Err;
    l->items.clear();
    out = value_none();
    return R::Ok;
}

R m_copy(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "copy");
    if (!l || !meth_args(a, "copy", 0, 0))
        return R::Err;
    Root rl{ method_self(a.args[0]) };
    ListObj *c = list_new();
    if (!c)
        return oom_err();
    Root rc{ obj_value(c) };
    Vec<Value> &xs = list_of(rl.v)->items;
    for (usize i = 0; i < xs.size(); i++)
        if (!list_push(list_of(rc.v), xs[i]))
            return oom_err();
    out = rc.v;
    return R::Ok;
}

R m_reverse(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "reverse");
    if (!l || !meth_args(a, "reverse", 0, 0))
        return R::Err;
    Vec<Value> &xs = l->items;
    for (usize i = 0; i < xs.size() / 2; i++) {
        Value t               = xs[i];
        xs[i]                 = xs[xs.size() - 1 - i];
        xs[xs.size() - 1 - i] = t;
    }
    out = value_none();
    return R::Ok;
}

// index and count, shared with tuple.
R seq_index(const Vec<Value> &xs, Value seq, const CallArgs &a, Str who, Value &out)
{
    static const Str NAMES[] = { "value", "start", "stop" };
    Value got[3];
    if (!meth_take(a, who, NAMES, 1, got))
        return R::Err;
    usize from = 0, to = 0;
    if (!span_of(got[1], got[2], xs.size(), from, to))
        return R::Err;
    if (cmp_is_python(got[0], false) || cmp_any_python(xs, false)) {
        out = cmp_find(seq, got[0], CMP_INDEX, from, to);
        return out.is_nil() ? R::Err : R::Ok;
    }
    usize at = 0;
    if (find_at(xs, got[0], from, to, at) != R::Ok)
        return R::Err;
    if (at == Str::npos)
        return err_set("ValueError", "value is not in the sequence");
    out = Value::of_int(i32(at));
    return R::Ok;
}

R seq_count(const Vec<Value> &xs, Value seq, const CallArgs &a, Str who, Value &out)
{
    if (!meth_args(a, who, 1, 1))
        return R::Err;
    if (cmp_is_python(a.args[1], false) || cmp_any_python(xs, false)) {
        out = cmp_find(seq, a.args[1], CMP_COUNT, 0, xs.size());
        return out.is_nil() ? R::Err : R::Ok;
    }
    i64 n = 0;
    for (usize i = 0; i < xs.size(); i++) {
        bool same = false;
        if (py_eq(xs[i], a.args[1], same) != R::Ok)
            return R::Err;
        n += same;
    }
    out = Value::of_int(i32(n));
    return R::Ok;
}

R m_list_index(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "index");
    return l ? seq_index(l->items, method_self(a.args[0]), a, "index", out) : R::Err;
}

R m_list_count(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "count");
    return l ? seq_count(l->items, method_self(a.args[0]), a, "count", out) : R::Err;
}

// ------------------------------------------------------------------- sort

// s[0] the list, s[1] the key function, s[2] the keys, s[3] reverse.
R sort_finish(ContObj *k)
{
    ListObj *l    = list_of(k->s[0]);
    ListObj *keys = list_of(k->s[2]);
    // A key that compares in Python: the merge itself has to suspend, so it
    // becomes a continuation of its own and this one is done.
    if (cmp_any_python(keys->items, true)) {
        Value c = cmp_sort(k->s[0], k->s[2], is_true(k->s[3]), true);
        return c.is_nil() ? R::Err : cont_done(k, c);
    }
    Vec<u32> idx;
    for (usize i = 0; i < l->items.size(); i++)
        if (!idx.push(u32(i)))
            return oom_err();
    if (sort_idx(keys->items, idx, is_true(k->s[3])) != R::Ok)
        return R::Err;

    // Into a copy first: a key function may have changed the list.
    Vec<Value> sorted;
    for (usize i = 0; i < idx.size(); i++)
        if (idx[i] < l->items.size() && !sorted.push(l->items[idx[i]]))
            return oom_err();
    for (usize i = 0; i < sorted.size() && i < l->items.size(); i++)
        l->items[i] = sorted[i];
    return cont_done(k, value_none());
}

R sort_step(ContObj *k, Value in)
{
    ListObj *l = list_of(k->s[0]);
    if (k->i > 0 && !list_push(list_of(k->s[2]), in))
        return oom_err();
    if (k->i < l->items.size())
        return cont_call(k, k->s[1], l->items[k->i++]);
    return sort_finish(k);
}

R m_sort(const CallArgs &a, Value &out)
{
    ListObj *l = self_list(a, "sort");
    if (!l)
        return R::Err;
    if (a.nargs > 1)
        return err_set("TypeError", "sort() takes no positional arguments");
    Root key;
    bool rev = false;
    for (u32 i = 0; i < a.nkw; i++) {
        Str n = is_str(a.kwnames[i]) ? str_of(a.kwnames[i])->str() : Str();
        if (n == "key") {
            if (!is_none(a.kwvals[i]))
                key = a.kwvals[i];
        } else if (n == "reverse") {
            rev = py_truth(a.kwvals[i]);
        } else {
            return err_set2("TypeError", "sort() got an unexpected keyword argument", n);
        }
    }

    Root rl{ method_self(a.args[0]) };
    if (key.v.is_nil() && cmp_any_python(l->items, true)) {
        out = cmp_sort(rl.v, Value(), rev, true);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (key.v.is_nil()) {
        Vec<u32> idx;
        for (usize i = 0; i < l->items.size(); i++)
            if (!idx.push(u32(i)))
                return oom_err();
        if (sort_idx(l->items, idx, rev) != R::Ok)
            return R::Err;
        Vec<Value> sorted;
        for (usize i = 0; i < idx.size(); i++)
            if (!sorted.push(list_of(rl.v)->items[idx[i]]))
                return oom_err();
        for (usize i = 0; i < sorted.size(); i++)
            list_of(rl.v)->items[i] = sorted[i];
        out = value_none();
        return R::Ok;
    }

    Root kv{ cont_new(sort_step) };
    if (kv.v.is_nil())
        return R::Err;
    ListObj *keys = list_new();
    if (!keys)
        return oom_err();
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rl.v;
    k->s[1]    = key.v;
    k->s[2]    = obj_value(keys);
    k->s[3]    = value_bool(rev);
    out        = kv.v;
    return R::Ok;
}

// ------------------------------------------------------------------ tuple

// A list of the items, because a search that suspends needs an object to hold
// them across the call.
Value tuple_items(TupleObj *t)
{
    Root rt{ obj_value(t) };
    ListObj *l = list_new();
    if (!l)
        return oom_err(), Value();
    Root rl{ obj_value(l) };
    for (usize i = 0; i < static_cast<TupleObj *>(rt.v.obj())->len; i++)
        if (!list_push(list_of(rl.v), static_cast<TupleObj *>(rt.v.obj())->items()[i]))
            return oom_err(), Value();
    return rl.v;
}

R m_tuple_index(const CallArgs &a, Value &out)
{
    TupleObj *t = self_tuple(a, "index");
    if (!t)
        return R::Err;
    Root xs{ tuple_items(t) };
    if (xs.v.is_nil())
        return R::Err;
    return seq_index(list_of(xs.v)->items, xs.v, a, "index", out);
}

R m_tuple_count(const CallArgs &a, Value &out)
{
    TupleObj *t = self_tuple(a, "count");
    if (!t)
        return R::Err;
    Root xs{ tuple_items(t) };
    if (xs.v.is_nil())
        return R::Err;
    return seq_count(list_of(xs.v)->items, xs.v, a, "count", out);
}

// ------------------------------------------------------------------ slice

// slice.indices(len): start, stop and step against that length.
R m_indices(const CallArgs &a, Value &out)
{
    Value self = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_slice(self))
        return err_set2("TypeError", "indices() requires a slice", type_name(self));
    if (!meth_args(a, "indices", 1, 1))
        return R::Err;
    i64 len = 0;
    if (!as_index(a.args[1], len))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (len < 0)
        return err_set("ValueError", "length should not be negative");

    i64 start = 0, stop = 0, step = 1;
    usize count = 0;
    Root rs{ self };
    if (!slice_resolve(rs.v, usize(len), start, stop, step, count))
        return R::Err;
    TupleObj *t = tuple_new(3);
    if (!t)
        return oom_err();
    i64 n[3] = { start, stop, step };
    for (usize i = 0; i < 3; i++) {
        Value v = int_from_i64(n[i]);
        if (v.is_nil())
            return R::Err;
        t->items()[i] = v;
    }
    out = obj_value(t);
    return R::Ok;
}

constexpr Method LIST[] = {
    { "append", m_append },    { "extend", m_extend },   { "insert", m_insert },
    { "pop", m_pop },          { "remove", m_remove },   { "clear", m_clear },
    { "copy", m_copy },        { "reverse", m_reverse }, { "index", m_list_index },
    { "count", m_list_count }, { "sort", m_sort },       { "__init__", m_list_init },
};

constexpr Method TUPLE[] = { { "index", m_tuple_index }, { "count", m_tuple_count } };

constexpr Method SLICE[] = { { "indices", m_indices } };

} // namespace

bool seq_methods()
{
    return method_install(&list_type, LIST) && method_install(&tuple_type, TUPLE) &&
           method_install(&slice_type, SLICE);
}
