// dict and set: their methods, frozenset, and the three views a dict answers.
//
// A view is its own iterable type, not a list. That makes `d.keys() & other` a
// set operation, and `for k in d.keys()` copies nothing.
#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "frame.h"
#include "gc.h"
#include "gen.h"
#include "intern.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"

namespace {

DictObj *dict_at(Value v)
{
    return static_cast<DictObj *>(v.obj());
}

// The table behind a dict, a set or a frozenset. Null for anything else,
// a view included: those are walked with the iterator protocol.
const Table *table_of(Value v);

bool view_is(Value v);

// ------------------------------------------------------------------- views

struct ViewObj : Obj {
    Value owner; // the dict
    u32 kind;
};

void view_trace(Obj *o)
{
    gc_mark(static_cast<ViewObj *>(o)->owner);
}

R view_len(Value v, usize &out)
{
    out = dict_len(dict_at(static_cast<ViewObj *>(v.obj())->owner));
    return R::Ok;
}

// One (key, value) pair, for an items view.
Value pair_of(Value k, Value val)
{
    Root rk{ k }, rv{ val };
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom_err(), Value();
    t->items()[0] = rk.v;
    t->items()[1] = rv.v;
    return obj_value(t);
}

struct ViewIter : Obj {
    Value view;
    usize at;
};

extern const Type view_iter_type;

void view_iter_trace(Obj *o)
{
    gc_mark(static_cast<ViewIter *>(o)->view);
}

Value iter_self(Value v)
{
    return v;
}

R view_iter_next(Value v, Value &out)
{
    ViewIter *it = static_cast<ViewIter *>(v.obj());
    ViewObj *vw  = static_cast<ViewObj *>(it->view.obj());
    Value k, val;
    if (!table_next(dict_at(vw->owner)->t, it->at, k, val))
        return R::NotImpl;
    if (vw->kind == VIEW_KEYS)
        out = k;
    else if (vw->kind == VIEW_VALUES)
        out = val;
    else if ((out = pair_of(k, val)).is_nil())
        return R::Err;
    return R::Ok;
}

Value view_iter(Value v)
{
    Root rv{ v };
    ViewIter *it = static_cast<ViewIter *>(obj_alloc(&view_iter_type, sizeof(ViewIter)));
    if (!it)
        return oom_err(), Value();
    it->view = rv.v;
    it->at   = 0;
    return obj_value(it);
}

R view_repr(Value v, String &out)
{
    ViewObj *vw = static_cast<ViewObj *>(v.obj());
    Str name    = vw->kind == VIEW_KEYS     ? Str("dict_keys")
                  : vw->kind == VIEW_VALUES ? Str("dict_values")
                                            : Str("dict_items");
    if (!out.append(name) || !out.push('('))
        return oom_err();
    bool first = true;
    usize at   = 0;
    Value k, val;
    if (!out.push('['))
        return oom_err();
    while (table_next(dict_at(vw->owner)->t, at, k, val)) {
        if (!first && !out.append(", "))
            return oom_err();
        first   = false;
        Value x = vw->kind == VIEW_KEYS ? k : vw->kind == VIEW_VALUES ? val : pair_of(k, val);
        if (x.is_nil() || py_repr(x, out) != R::Ok)
            return R::Err;
    }
    return out.append("])") ? R::Ok : oom_err();
}

// A keys or items view is a set: it compares by value and cannot hash. A
// values view is not, and hashes by identity like any object.
R view_hash(Value v, u32 &out)
{
    if (static_cast<ViewObj *>(v.obj())->kind != VIEW_VALUES)
        return err_unhashable(v);
    out = u32(usize(v.obj())) >> 4;
    return R::Ok;
}

R view_contains(Value v, Value item, bool &out)
{
    ViewObj *vw = static_cast<ViewObj *>(v.obj());
    if (vw->kind == VIEW_KEYS) {
        Value ignored;
        R r = dict_get(dict_at(vw->owner), item, ignored);
        if (r == R::Err)
            return R::Err;
        out = r == R::Ok;
        return R::Ok;
    }
    usize at = 0;
    Value k, val;
    while (table_next(dict_at(vw->owner)->t, at, k, val)) {
        Value x = vw->kind == VIEW_VALUES ? val : pair_of(k, val);
        if (x.is_nil())
            return R::Err;
        bool same = false;
        if (py_eq(x, item, same) != R::Ok)
            return R::Err;
        if (same) {
            out = true;
            return R::Ok;
        }
    }
    out = false;
    return R::Ok;
}

// ------------------------------------------------------------- set helpers

// Every member of `v`, as a fresh set. Null with the error pending.
SetObj *set_from(Value v, bool frozen)
{
    Root rv{ v };
    SetObj *s = frozen ? frozenset_new() : set_new();
    if (!s)
        return oom_err(), nullptr;
    Root rs{ obj_value(s) };
    // A dict yields its keys, a set its members. Both are the table's keys.
    const Table *t = table_of(rv.v);
    if (t) {
        usize at = 0;
        Value k, val;
        while (table_next(*t, at, k, val))
            if (set_add(set_at(rs.v), k) != R::Ok)
                return nullptr;
        return set_at(rs.v);
    }
    Root it{ py_iter(rv.v) };
    if (it.v.is_nil())
        return nullptr;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return nullptr;
        if (r == R::NotImpl)
            break;
        if (set_add(set_at(rs.v), got.v) != R::Ok)
            return nullptr;
    }
    return set_at(rs.v);
}

const Table *table_of(Value v)
{
    if (is_anydict(v))
        return &dict_at(v)->t;
    if (is_anyset(v))
        return &set_at(v)->t;
    return nullptr;
}

enum : u32 { OP_OR, OP_AND, OP_SUB, OP_XOR };

// The four set operations, over `a` and every member of `b`.
R set_combine(Value a, Value b, u32 op, bool frozen, Value &out)
{
    Root ra{ a }, rb{ b };
    SetObj *left = set_from(ra.v, false);
    if (!left)
        return R::Err;
    Root rl{ obj_value(left) };
    SetObj *right = set_from(rb.v, false);
    if (!right)
        return R::Err;
    Root rr{ obj_value(right) };

    SetObj *made = frozen ? frozenset_new() : set_new();
    if (!made)
        return oom_err();
    Root rm{ obj_value(made) };

    usize at = 0;
    Value k, val;
    while (table_next(set_at(rl.v)->t, at, k, val)) {
        bool has = false;
        if (set_has(set_at(rr.v), k, has) != R::Ok)
            return R::Err;
        bool keep = op == OP_OR ? true : op == OP_AND ? has : !has;
        if (keep && set_add(set_at(rm.v), k) != R::Ok)
            return R::Err;
    }
    if (op == OP_OR || op == OP_XOR) {
        at = 0;
        while (table_next(set_at(rr.v)->t, at, k, val)) {
            bool has = false;
            if (set_has(set_at(rl.v), k, has) != R::Ok)
                return R::Err;
            if ((op == OP_OR || !has) && set_add(set_at(rm.v), k) != R::Ok)
                return R::Err;
        }
    }
    out = rm.v;
    return R::Ok;
}

// The members of two sets compared: equal, subset, superset, disjoint.
R set_relation(Value a, Value b, Cmp op, bool &out);

} // namespace

R anyset_binop(Value a, Value b, Op op, Value &out)
{
    u32 which = op == Op::Or    ? OP_OR
                : op == Op::And ? OP_AND
                : op == Op::Sub ? OP_SUB
                : op == Op::Xor ? OP_XOR
                                : ~0u;
    if (which == ~0u)
        return R::NotImpl;
    bool ok_a = is_anyset(a) || view_is(a);
    bool ok_b = is_anyset(b) || view_is(b);
    if (!ok_a || !ok_b)
        return R::NotImpl;
    // The left operand decides the result's type, as in CPython:
    // frozenset | set is a frozenset, set | frozenset is a set.
    return set_combine(a, b, which, is_frozenset(a), out);
}

namespace {

R set_relation(Value a, Value b, Cmp op, bool &out)
{
    Root ra{ a }, rb{ b };
    SetObj *x = set_from(ra.v, false);
    if (!x)
        return R::Err;
    Root rx{ obj_value(x) };
    SetObj *y = set_from(rb.v, false);
    if (!y)
        return R::Err;
    Root ry{ obj_value(y) };

    bool sub = true, sup = true;
    usize at = 0;
    Value k, val;
    while (table_next(set_at(rx.v)->t, at, k, val)) {
        bool has = false;
        if (set_has(set_at(ry.v), k, has) != R::Ok)
            return R::Err;
        sub = sub && has;
    }
    at = 0;
    while (table_next(set_at(ry.v)->t, at, k, val)) {
        bool has = false;
        if (set_has(set_at(rx.v), k, has) != R::Ok)
            return R::Err;
        sup = sup && has;
    }
    usize nx = set_len(set_at(rx.v)), ny = set_len(set_at(ry.v));
    switch (op) {
    case Cmp::Le:
        out = sub;
        break;
    case Cmp::Lt:
        out = sub && nx < ny;
        break;
    case Cmp::Ge:
        out = sup;
        break;
    default:
        out = sup && ny < nx;
        break;
    }
    return R::Ok;
}

} // namespace

R anyset_order(Value a, Value b, Cmp op, bool &out)
{
    if (!(is_anyset(a) || view_is(a)) || !(is_anyset(b) || view_is(b)))
        return R::NotImpl;
    return set_relation(a, b, op, out);
}

R anyset_eq(Value a, Value b, bool &out)
{
    if (!(is_anyset(a) || view_is(a)) || !(is_anyset(b) || view_is(b)))
        return R::NotImpl;
    const Table *x = table_of(a);
    const Table *y = table_of(b);
    if (x && y && x->live != y->live) {
        out = false;
        return R::Ok;
    }
    bool le = false, ge = false;
    if (set_relation(a, b, Cmp::Le, le) != R::Ok || set_relation(a, b, Cmp::Ge, ge) != R::Ok)
        return R::Err;
    out = le && ge;
    return R::Ok;
}

// A frozenset hashes by its members, in any order. Exclusive-or, as CPython's
// own hash is built from.
R frozenset_hash(Value v, u32 &out)
{
    u32 h    = 0;
    usize at = 0;
    Value k, val;
    while (table_next(set_at(v)->t, at, k, val)) {
        u32 one = 0;
        if (py_hash(k, one) != R::Ok)
            return R::Err;
        h ^= one * 0x9e3779b9u;
    }
    out = h ^ u32(set_len(set_at(v)));
    return R::Ok;
}

namespace {

// ---------------------------------------------------------------- dict work

R m_keys(const CallArgs &a, Value &out)
{
    DictObj *d = self_anydict(a, "keys");
    if (!d || !meth_args(a, "keys", 0, 0))
        return R::Err;
    out = dict_view(method_self(a.args[0]), VIEW_KEYS);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_values(const CallArgs &a, Value &out)
{
    DictObj *d = self_anydict(a, "values");
    if (!d || !meth_args(a, "values", 0, 0))
        return R::Err;
    out = dict_view(method_self(a.args[0]), VIEW_VALUES);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_items(const CallArgs &a, Value &out)
{
    DictObj *d = self_anydict(a, "items");
    if (!d || !meth_args(a, "items", 0, 0))
        return R::Err;
    out = dict_view(method_self(a.args[0]), VIEW_ITEMS);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_get(const CallArgs &a, Value &out)
{
    DictObj *d = self_anydict(a, "get");
    if (!d || !meth_args(a, "get", 1, 2))
        return R::Err;
    R r = dict_get(d, a.args[1], out);
    if (r == R::Err)
        return R::Err;
    if (r == R::NotImpl)
        out = a.nargs > 2 ? a.args[2] : value_none();
    return R::Ok;
}

R m_setdefault(const CallArgs &a, Value &out)
{
    DictObj *d = self_dict(a, "setdefault");
    if (!d || !meth_args(a, "setdefault", 1, 2))
        return R::Err;
    R r = dict_get(d, a.args[1], out);
    if (r == R::Err)
        return R::Err;
    if (r == R::Ok)
        return R::Ok;
    Root rd{ method_self(a.args[0]) }, rk{ a.args[1] };
    out = a.nargs > 2 ? a.args[2] : value_none();
    return dict_set(dict_at(rd.v), rk.v, out);
}

R m_dict_pop(const CallArgs &a, Value &out)
{
    DictObj *d = self_dict(a, "pop");
    if (!d || !meth_args(a, "pop", 1, 2))
        return R::Err;
    R r = dict_get(d, a.args[1], out);
    if (r == R::Err)
        return R::Err;
    if (r == R::NotImpl) {
        if (a.nargs > 2) {
            out = a.args[2];
            return R::Ok;
        }
        return key_error(a.args[1]);
    }
    return dict_del(d, a.args[1]) == R::Err ? R::Err : R::Ok;
}

R m_popitem(const CallArgs &a, Value &out)
{
    DictObj *d = self_dict(a, "popitem");
    if (!d || !meth_args(a, "popitem", 0, 0))
        return R::Err;
    // The last inserted, as CPython pops.
    usize at = 0;
    Value k, val, lastk, lastv;
    bool any = false;
    while (table_next(d->t, at, k, val)) {
        lastk = k;
        lastv = val;
        any   = true;
    }
    if (!any)
        return err_set("KeyError", "popitem(): dictionary is empty");
    Root rd{ method_self(a.args[0]) }, rk{ lastk }, rv{ lastv };
    out = pair_of(rk.v, rv.v);
    if (out.is_nil())
        return R::Err;
    return dict_del(dict_at(rd.v), rk.v) == R::Err ? R::Err : R::Ok;
}

R m_update(const CallArgs &a, Value &out)
{
    DictObj *d = self_dict(a, "update");
    if (!d || a.nargs > 2)
        return d ? err_set("TypeError", "update() takes at most one positional argument") : R::Err;
    if (a.nargs == 2 && iter_needs_vm(a.args[1]))
        return iter_park(a, 1, m_update, out);
    Root rd{ method_self(a.args[0]) };
    if (a.nargs == 2) {
        Root src{ frame_locals_dict(a.args[1]) };
        if (src.v.is_nil())
            return R::Err;
        while (is_mappingproxy(src.v))
            src = mappingproxy_inner(src.v);
        if (is_inst(src.v) && type_has_py_special(src.v, "keys")) {
            if (a.nkw)
                return err_set("TypeError", "update() with a mapping of one's own and keywords");
            return dict_fill_keys(rd.v, src.v, out);
        }
        src            = method_self(src.v);
        const Table *t = table_of(src.v);
        if (t && is_anydict(src.v)) {
            usize at = 0;
            Value k, val;
            while (table_next(dict_at(src.v)->t, at, k, val))
                if (dict_set(dict_at(rd.v), k, val) != R::Ok)
                    return R::Err;
        } else {
            Root it{ py_iter(src.v) };
            if (it.v.is_nil())
                return R::Err;
            for (usize at = 0;; at++) {
                Root got;
                R r = py_next(it.v, got.v);
                if (r == R::Err)
                    return R::Err;
                if (r == R::NotImpl)
                    break;
                usize n = 0;
                if (py_len(got.v, n) != R::Ok)
                    return R::Err;
                if (n != 2) {
                    char c[24];
                    Buf<128> m;
                    m.put("dictionary update sequence element #")
                        .put(int_text(c, sizeof c, i64(at)));
                    m.put(" has length ").put(int_text(c, sizeof c, i64(n))).put("; 2 is required");
                    return err_set("ValueError", m.str());
                }
                // Pin the key: taking the value allocates.
                Root k, val;
                if (py_getitem(got.v, Value::of_int(0), k.v) != R::Ok ||
                    py_getitem(got.v, Value::of_int(1), val.v) != R::Ok)
                    return R::Err;
                if (dict_set(dict_at(rd.v), k.v, val.v) != R::Ok)
                    return R::Err;
            }
        }
    }
    for (u32 i = 0; i < a.nkw; i++)
        if (dict_set(dict_at(rd.v), a.kwnames[i], a.kwvals[i]) != R::Ok)
            return R::Err;
    out = value_none();
    return R::Ok;
}

R m_dict_clear(const CallArgs &a, Value &out)
{
    DictObj *d = self_dict(a, "clear");
    if (!d || !meth_args(a, "clear", 0, 0))
        return R::Err;
    d->t.entries.clear();
    d->t.index.clear();
    d->t.live = 0;
    out       = value_none();
    return R::Ok;
}

R m_dict_copy(const CallArgs &a, Value &out)
{
    DictObj *d = self_dict(a, "copy");
    if (!d || !meth_args(a, "copy", 0, 0))
        return R::Err;
    Root rd{ method_self(a.args[0]) };
    DictObj *c = dict_new();
    if (!c)
        return oom_err();
    Root rc{ obj_value(c) };
    usize at = 0;
    Value k, val;
    while (table_next(dict_at(rd.v)->t, at, k, val))
        if (dict_set(dict_at(rc.v), k, val) != R::Ok)
            return R::Err;
    out = rc.v;
    return R::Ok;
}

R m_fromkeys(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs < 1 || a.nargs > 2)
        return err_set("TypeError", "fromkeys() takes from 1 to 2 arguments");
    if (iter_needs_vm(a.args[0]))
        return iter_park(a, 0, m_fromkeys, out);
    Root fill{ a.nargs > 1 ? a.args[1] : value_none() };
    Root it{ py_iter(a.args[0]) };
    if (it.v.is_nil())
        return R::Err;
    DictObj *d = dict_new();
    if (!d)
        return oom_err();
    Root rd{ obj_value(d) };
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            break;
        if (dict_set(dict_at(rd.v), got.v, fill.v) != R::Ok)
            return R::Err;
    }
    out = rd.v;
    return R::Ok;
}

// -------------------------------------------------------------- frozendict

// A fresh dict becomes a frozendict: the layout is the same.
Value frozen(Value d)
{
    d.obj()->type = &frozendict_type;
    return d;
}

R frozen_step(ContObj *k, Value in)
{
    return cont_done(k, is_dict(in) ? frozen(in) : in);
}

// A dict made, possibly by a continuation, handed back as a frozendict.
R freeze(Value made, Value &out)
{
    if (!is_cont(made)) {
        out = frozen(made);
        return R::Ok;
    }
    Root rm{ made };
    Root kv{ cont_new(frozen_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(rm.v)->next = kv.v;
    out                 = rm.v;
    return R::Ok;
}

// A frozendict is its own copy; a subclass's copy is a frozendict.
R m_fd_copy(const CallArgs &a, Value &out)
{
    Value self = a.nargs ? a.args[0] : Value();
    DictObj *d = self_anydict(a, "copy");
    if (!d || !meth_args(a, "copy", 0, 0))
        return R::Err;
    if (is_frozendict(self)) {
        out = self;
        return R::Ok;
    }
    Root rs{ method_self(self) };
    DictObj *c = dict_new();
    if (!c)
        return oom_err();
    Root rc{ obj_value(c) };
    usize at = 0;
    Value k, val;
    while (table_next(dict_at(rs.v)->t, at, k, val))
        if (dict_set(dict_at(rc.v), k, val) != R::Ok)
            return R::Err;
    out = frozen(rc.v);
    return R::Ok;
}

// What pickle rebuilds one from: a dict of the same pairs.
R m_fd_getnewargs(const CallArgs &a, Value &out)
{
    DictObj *d = self_anydict(a, "__getnewargs__");
    if (!d || !meth_args(a, "__getnewargs__", 0, 0))
        return R::Err;
    Root rs{ method_self(a.args[0]) };
    DictObj *c = dict_new();
    if (!c)
        return oom_err();
    Root rc{ obj_value(c) };
    usize at = 0;
    Value k, val;
    while (table_next(dict_at(rs.v)->t, at, k, val))
        if (dict_set(dict_at(rc.v), k, val) != R::Ok)
            return R::Err;
    TupleObj *t = tuple_new(1);
    if (!t)
        return oom_err();
    t->items()[0] = rc.v;
    out           = obj_value(t);
    return R::Ok;
}

// dict.fromkeys(iterable, value) on a class of the program's own: cls(), then
// each key stored as the class stores it. s[0] the class, s[1] the keys, s[2]
// the value, s[3] what cls() made, s[4] its __setitem__ where that is Python.
R fromkeys_step(ContObj *k, Value in)
{
    if (k->i == 0) {
        k->i = 1;
        return cont_call(k, k->s[0], Value(), 0);
    }
    if (k->i == 1) {
        k->i    = 2;
        k->s[3] = in;
        if (type_has_py_special(in, "__setitem__")) {
            k->s[4] = type_special(in, "__setitem__");
            if (k->s[4].is_nil())
                return R::Err;
        }
    }
    ListObj *keys = list_of(k->s[1]);
    while (k->j < keys->items.size()) {
        Value key = keys->items[k->j++];
        if (!k->s[4].is_nil())
            return cont_call(k, k->s[4], key, 2, k->s[2]);
        if (py_setitem(k->s[3], key, k->s[2]) != R::Ok)
            return R::Err;
        keys = list_of(k->s[1]);
    }
    return cont_done(k, k->s[3]);
}

R m_dict_fromkeys(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs < 2 || a.nargs > 3)
        return err_set("TypeError", "fromkeys expected at least 1 argument");
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, m_dict_fromkeys, out);
    Value cls = a.args[0];
    CallArgs rest;
    rest.args  = a.args + 1;
    rest.nargs = a.nargs - 1;
    if (!is_type(cls) || type_obj(cls)->desc == &dict_type)
        return m_fromkeys(rest, out);
    if (type_obj(cls)->desc == &frozendict_type) {
        Root made;
        if (m_fromkeys(rest, made.v) != R::Ok)
            return R::Err;
        return freeze(made.v, out);
    }
    Root rc{ cls }, fill{ a.nargs > 2 ? a.args[2] : value_none() };
    ListObj *keys = py_list_of(a.args[1]);
    if (!keys)
        return R::Err;
    Root rk{ obj_value(keys) };
    Root kv{ cont_new(fromkeys_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rc.v;
    k->s[1]    = rk.v;
    k->s[2]    = fill.v;
    out        = kv.v;
    return R::Ok;
}

// ----------------------------------------------------------------- set work

SetObj *mutable_self(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_set(s)) {
        Buf<96> b;
        b.put(who).put("() requires a set");
        return err_set2("TypeError", b.str(), type_name(s)), nullptr;
    }
    return set_at(s);
}

R m_add(const CallArgs &a, Value &out)
{
    SetObj *s = mutable_self(a, "add");
    if (!s || !meth_args(a, "add", 1, 1))
        return R::Err;
    if (set_add(s, a.args[1]) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R m_discard(const CallArgs &a, Value &out)
{
    SetObj *s = mutable_self(a, "discard");
    if (!s || !meth_args(a, "discard", 1, 1))
        return R::Err;
    bool had = false;
    if (set_discard(s, a.args[1], had) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R m_set_remove(const CallArgs &a, Value &out)
{
    SetObj *s = mutable_self(a, "remove");
    if (!s || !meth_args(a, "remove", 1, 1))
        return R::Err;
    bool had = false;
    if (set_discard(s, a.args[1], had) != R::Ok)
        return R::Err;
    if (!had) {
        return key_error(a.args[1]);
    }
    out = value_none();
    return R::Ok;
}

R m_set_pop(const CallArgs &a, Value &out)
{
    SetObj *s = mutable_self(a, "pop");
    if (!s || !meth_args(a, "pop", 0, 0))
        return R::Err;
    usize at = 0;
    Value k, val;
    if (!table_next(s->t, at, k, val))
        return err_set("KeyError", "pop from an empty set");
    Root rs{ method_self(a.args[0]) }, rk{ k };
    bool had = false;
    if (set_discard(set_at(rs.v), rk.v, had) != R::Ok)
        return R::Err;
    out = rk.v;
    return R::Ok;
}

R m_set_clear(const CallArgs &a, Value &out)
{
    SetObj *s = mutable_self(a, "clear");
    if (!s || !meth_args(a, "clear", 0, 0))
        return R::Err;
    s->t.entries.clear();
    s->t.index.clear();
    s->t.live = 0;
    out       = value_none();
    return R::Ok;
}

R m_set_copy(const CallArgs &a, Value &out)
{
    SetObj *s = self_set(a, "copy");
    if (!s)
        return R::Err;
    Value self = method_self(a.args[0]);
    if (!meth_args(a, "copy", 0, 0))
        return R::Err;
    SetObj *c = set_from(self, is_frozenset(self));
    if (!c)
        return R::Err;
    out = obj_value(c);
    return R::Ok;
}

// union, intersection, difference and symmetric_difference. Each takes any
// iterable, not only a set.
R set_method(const CallArgs &a, Str who, u32 op, Value &out)
{
    SetObj *s = self_set(a, who);
    if (!s)
        return R::Err;
    if (a.nkw)
        return err_set2("TypeError", "takes no keyword arguments", who);
    Root acc{ method_self(a.args[0]) };
    bool frozen = is_frozenset(acc.v);
    // No argument at all: a copy, as CPython answers.
    SetObj *c = set_from(acc.v, frozen);
    if (!c)
        return R::Err;
    acc = obj_value(c);
    for (u32 i = 1; i < a.nargs; i++) {
        Value made;
        if (set_combine(acc.v, a.args[i], op, frozen, made) != R::Ok)
            return R::Err;
        acc = made;
    }
    out = acc.v;
    return R::Ok;
}

R m_union(const CallArgs &a, Value &out)
{
    return set_method(a, "union", OP_OR, out);
}

R m_intersection(const CallArgs &a, Value &out)
{
    return set_method(a, "intersection", OP_AND, out);
}

R m_difference(const CallArgs &a, Value &out)
{
    return set_method(a, "difference", OP_SUB, out);
}

R m_symdiff(const CallArgs &a, Value &out)
{
    return set_method(a, "symmetric_difference", OP_XOR, out);
}

// The in-place four: the same work, then the members moved into self.
R set_update(const CallArgs &a, Str who, u32 op, Value &out)
{
    SetObj *s = mutable_self(a, who);
    if (!s || a.nkw)
        return s ? err_set2("TypeError", "takes no keyword arguments", who) : R::Err;
    Root rs{ method_self(a.args[0]) };
    for (u32 i = 1; i < a.nargs; i++) {
        Value made;
        if (set_combine(rs.v, a.args[i], op, false, made) != R::Ok)
            return R::Err;
        Root rm{ made };
        SetObj *self = set_at(rs.v);
        self->t.entries.clear();
        self->t.index.clear();
        self->t.live = 0;
        usize at     = 0;
        Value k, val;
        while (table_next(set_at(rm.v)->t, at, k, val))
            if (set_add(set_at(rs.v), k) != R::Ok)
                return R::Err;
    }
    out = value_none();
    return R::Ok;
}

R m_set_update(const CallArgs &a, Value &out)
{
    return set_update(a, "update", OP_OR, out);
}

R m_inter_update(const CallArgs &a, Value &out)
{
    return set_update(a, "intersection_update", OP_AND, out);
}

R m_diff_update(const CallArgs &a, Value &out)
{
    return set_update(a, "difference_update", OP_SUB, out);
}

R m_symdiff_update(const CallArgs &a, Value &out)
{
    return set_update(a, "symmetric_difference_update", OP_XOR, out);
}

R set_test(const CallArgs &a, Str who, Cmp op, Value &out)
{
    SetObj *s = self_set(a, who);
    if (!s || !meth_args(a, who, 1, 1))
        return R::Err;
    Root rs{ method_self(a.args[0]) }, ro{ a.args[1] };
    SetObj *other = set_from(ro.v, false);
    if (!other)
        return R::Err;
    bool yes = false;
    if (set_relation(rs.v, obj_value(other), op, yes) != R::Ok)
        return R::Err;
    out = value_bool(yes);
    return R::Ok;
}

R m_issubset(const CallArgs &a, Value &out)
{
    return set_test(a, "issubset", Cmp::Le, out);
}

R m_issuperset(const CallArgs &a, Value &out)
{
    return set_test(a, "issuperset", Cmp::Ge, out);
}

// A view answers this too, so self_set is too narrow a check.
R m_isdisjoint(const CallArgs &a, Value &out)
{
    Root rs{ a.nargs ? method_self(a.args[0]) : Value() };
    if (!is_anyset(rs.v) && !view_is(rs.v))
        return err_set2("TypeError", "isdisjoint() requires a set", type_name(rs.v));
    if (!meth_args(a, "isdisjoint", 1, 1))
        return R::Err;
    Value made;
    if (set_combine(rs.v, a.args[1], OP_AND, false, made) != R::Ok)
        return R::Err;
    out = value_bool(set_len(set_at(made)) == 0);
    return R::Ok;
}

// ------------------------------------------------------------------ tables

constexpr Method DICT[] = {
    { "keys", m_keys },       { "values", m_values },         { "items", m_items },
    { "get", m_get },         { "setdefault", m_setdefault }, { "pop", m_dict_pop },
    { "popitem", m_popitem }, { "update", m_update },         { "clear", m_dict_clear },
    { "copy", m_dict_copy },
};

constexpr Method FROZENDICT[] = {
    { "keys", m_keys }, { "values", m_values }, { "items", m_items },
    { "get", m_get },   { "copy", m_fd_copy },  { "__getnewargs__", m_fd_getnewargs },
};

// The half a frozenset has as well.
constexpr Method SET_CONST[] = {
    { "copy", m_set_copy },
    { "union", m_union },
    { "intersection", m_intersection },
    { "difference", m_difference },
    { "symmetric_difference", m_symdiff },
    { "issubset", m_issubset },
    { "issuperset", m_issuperset },
    { "isdisjoint", m_isdisjoint },
};

constexpr Method SET_MUT[] = {
    { "add", m_add },
    { "discard", m_discard },
    { "remove", m_set_remove },
    { "pop", m_set_pop },
    { "clear", m_set_clear },
    { "update", m_set_update },
    { "intersection_update", m_inter_update },
    { "difference_update", m_diff_update },
    { "symmetric_difference_update", m_symdiff_update },
};

constexpr Method VIEW[] = { { "isdisjoint", m_isdisjoint } };

constexpr Type view_iter_type{ .name  = "dict_iterator",
                               .trace = view_iter_trace,
                               .iter  = iter_self,
                               .next  = view_iter_next };

bool view_is(Value v)
{
    return v.is_obj() && v.obj()->type == &view_type;
}

} // namespace

R b_frozendict(const CallArgs &a, Value &out)
{
    if (a.nargs > 1) {
        char n[24];
        Buf<96> b;
        b.put("frozendict expected at most 1 argument, got ")
            .put(int_text(n, sizeof n, i64(a.nargs)));
        return err_set("TypeError", b.str());
    }
    if (a.nargs == 1 && !a.nkw && is_frozendict(a.args[0])) {
        out = a.args[0];
        return R::Ok;
    }
    Root made;
    if (py_dict_of(a, made.v) != R::Ok)
        return R::Err;
    return freeze(made.v, out);
}

constexpr Type view_type{ .name     = "dict_view",
                          .trace    = view_trace,
                          .hash     = view_hash,
                          .eq       = anyset_eq,
                          .order    = anyset_order,
                          .repr     = view_repr,
                          .len      = view_len,
                          .contains = view_contains,
                          .binop    = anyset_binop,
                          .iter     = view_iter };

Value dict_view(Value d, u32 kind)
{
    Root rd{ d };
    ViewObj *v = static_cast<ViewObj *>(obj_alloc(&view_type, sizeof(ViewObj)));
    if (!v)
        return oom_err(), Value();
    v->owner = rd.v;
    v->kind  = kind;
    return obj_value(v);
}

bool map_methods()
{
    // fromkeys is a classmethod: a subclass's call makes the subclass.
    Root dt{ type_wrap(&dict_type) };
    Root fk{ native_new("fromkeys", m_dict_fromkeys) };
    if (dt.v.is_nil() || fk.v.is_nil())
        return false;
    Root cm{ classmethod_new(fk.v) };
    Root ft{ type_wrap(&frozendict_type) };
    StrObj *fkn = str_intern("fromkeys");
    if (cm.v.is_nil() || ft.v.is_nil() || !fkn ||
        dict_set(static_cast<DictObj *>(type_obj(dt.v)->dict.obj()), obj_value(fkn), cm.v) !=
            R::Ok ||
        dict_set(static_cast<DictObj *>(type_obj(ft.v)->dict.obj()), obj_value(fkn), cm.v) != R::Ok)
        return false;
    return method_install(&dict_type, DICT) && method_install(&frozendict_type, FROZENDICT) &&
           method_install(&set_type, SET_CONST) && method_install(&set_type, SET_MUT) &&
           method_install(&frozenset_type, SET_CONST) && method_install(&view_type, VIEW);
}
