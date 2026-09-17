// The table behind dict and set: entries in insertion order, with an open
// addressing index over them.
#include "exc.h"
#include "gc.h"
#include "iter.h"
#include "method.h"
#include "ops.h"

namespace {

constexpr i32 EMPTY = -1;
constexpr i32 DEAD  = -2;

void table_trace(Table &t)
{
    for (usize i = 0; i < t.entries.size(); i++) {
        gc_mark(t.entries[i].key);
        gc_mark(t.entries[i].val);
    }
}

// Where `key` is, or where it would go. `entry` is the entry number or EMPTY.
R table_lookup(Table &t, Value key, u32 h, usize &slot, i32 &entry)
{
    entry = EMPTY;
    if (t.index.size() == 0) {
        slot = 0;
        return R::Ok;
    }
    usize mask = t.index.size() - 1;
    usize i    = h & mask;
    usize dead = t.index.size();
    for (usize step = 0; step <= mask; step++) {
        i32 e = t.index[i];
        if (e == EMPTY) {
            slot = dead < t.index.size() ? dead : i;
            return R::Ok;
        }
        if (e == DEAD) {
            if (dead == t.index.size())
                dead = i;
        } else {
            Entry &en = t.entries[usize(e)];
            if (en.hash == h) {
                bool same = false;
                if (py_eq(en.key, key, same) != R::Ok)
                    return R::Err;
                if (same) {
                    slot  = i;
                    entry = e;
                    return R::Ok;
                }
            }
        }
        i = (i + 1) & mask;
    }
    slot = dead < t.index.size() ? dead : 0;
    return R::Ok;
}

// Rebuild the index, dropping deleted entries from the order as it goes.
bool table_rehash(Table &t, usize want)
{
    usize cap = 8;
    while (cap < want * 2)
        cap *= 2;

    Vec<Entry> kept;
    if (!kept.reserve(t.live))
        return false;
    for (usize i = 0; i < t.entries.size(); i++)
        if (!t.entries[i].key.is_nil() && !kept.push(t.entries[i]))
            return false;

    Vec<i32> index;
    if (!index.resize(cap))
        return false;
    for (usize i = 0; i < cap; i++)
        index[i] = EMPTY;

    usize mask = cap - 1;
    for (usize e = 0; e < kept.size(); e++) {
        usize i = kept[e].hash & mask;
        while (index[i] != EMPTY)
            i = (i + 1) & mask;
        index[i] = i32(e);
    }

    t.entries = static_cast<Vec<Entry> &&>(kept);
    t.index   = static_cast<Vec<i32> &&>(index);
    return true;
}

R table_put(Table &t, Value key, Value val)
{
    u32 h = 0;
    if (py_hash(key, h) != R::Ok)
        return R::Err;

    usize slot = 0;
    i32 entry  = EMPTY;
    if (table_lookup(t, key, h, slot, entry) != R::Ok)
        return R::Err;
    if (entry >= 0) {
        t.entries[usize(entry)].val = val;
        return R::Ok;
    }

    // Two thirds full, or half the order deleted: rebuild first.
    if ((t.entries.size() + 1) * 3 >= t.index.size() * 2 || t.entries.size() > t.live * 2 + 8) {
        if (!table_rehash(t, t.live + 1))
            return err_set("MemoryError", "out of memory");
        if (table_lookup(t, key, h, slot, entry) != R::Ok)
            return R::Err;
    }

    if (!t.entries.push(Entry{ key, val, h }))
        return err_set("MemoryError", "out of memory");
    t.index[slot] = i32(t.entries.size() - 1);
    t.live++;
    return R::Ok;
}

// NotImpl when the key is absent.
R table_take(Table &t, Value key, Value &out, bool remove)
{
    u32 h = 0;
    if (py_hash(key, h) != R::Ok)
        return R::Err;
    usize slot = 0;
    i32 entry  = EMPTY;
    if (table_lookup(t, key, h, slot, entry) != R::Ok)
        return R::Err;
    if (entry < 0)
        return R::NotImpl;
    out = t.entries[usize(entry)].val;
    if (remove) {
        t.entries[usize(entry)].key = Value();
        t.entries[usize(entry)].val = Value();
        t.index[slot]               = DEAD;
        t.live--;
    }
    return R::Ok;
}

void dict_trace(Obj *o)
{
    table_trace(static_cast<DictObj *>(o)->t);
}

void dict_fini(Obj *o)
{
    static_cast<DictObj *>(o)->t.~Table();
}

R dict_len_slot(Value v, usize &out)
{
    out = static_cast<DictObj *>(v.obj())->t.live;
    return R::Ok;
}

R dict_getitem(Value v, Value key, Value &out)
{
    R r = dict_get(static_cast<DictObj *>(v.obj()), key, out);
    if (r != R::NotImpl)
        return r;
    return key_error(key);
}

R dict_setitem(Value v, Value key, Value item)
{
    return dict_set(static_cast<DictObj *>(v.obj()), key, item);
}

R dict_delitem(Value v, Value key)
{
    R r = dict_del(static_cast<DictObj *>(v.obj()), key);
    if (r != R::NotImpl)
        return r;
    return key_error(key);
}

R dict_contains(Value v, Value item, bool &out)
{
    Value ignored;
    R r = table_take(static_cast<DictObj *>(v.obj())->t, item, ignored, false);
    if (r == R::Err)
        return R::Err;
    out = r == R::Ok;
    return R::Ok;
}

R dict_eq(Value a, Value b, bool &out)
{
    if (!b.is_obj() || b.obj()->type != &dict_type)
        return R::NotImpl;
    DictObj *x = static_cast<DictObj *>(a.obj());
    DictObj *y = static_cast<DictObj *>(b.obj());
    if (x->t.live != y->t.live) {
        out = false;
        return R::Ok;
    }
    usize at = 0;
    Value k, v;
    while (table_next(x->t, at, k, v)) {
        Value other;
        R r = table_take(y->t, k, other, false);
        if (r == R::Err)
            return R::Err;
        // Identity first, as CPython's comparison does.
        bool same = r == R::Ok && v == other;
        if (r == R::NotImpl || (!same && (py_eq(v, other, same) != R::Ok || !same))) {
            if (err_pending())
                return R::Err;
            out = false;
            return R::Ok;
        }
    }
    out = true;
    return R::Ok;
}

void set_trace(Obj *o)
{
    table_trace(static_cast<SetObj *>(o)->t);
}

void set_fini(Obj *o)
{
    static_cast<SetObj *>(o)->t.~Table();
}

R set_len_slot(Value v, usize &out)
{
    out = static_cast<SetObj *>(v.obj())->t.live;
    return R::Ok;
}

R set_contains(Value v, Value item, bool &out)
{
    return set_has(static_cast<SetObj *>(v.obj()), item, out);
}

} // namespace

R dict_repr(Value v, String &out);
R set_repr(Value v, String &out);
R frozenset_repr(Value v, String &out);

// `a | b` of two dicts is a new one, b's values winning (PEP 584).
R dict_binop(Value a, Value b, Op op, Value &out)
{
    if (op != Op::Or || !is_dict(a) || !is_dict(b))
        return R::NotImpl;
    Root ra{ a }, rb{ b };
    DictObj *d = dict_new();
    if (!d)
        return err_set("MemoryError", "out of memory");
    Root rd{ obj_value(d) };
    Value both[2] = { ra.v, rb.v };
    for (Value from : both) {
        usize at = 0;
        Value k, v;
        while (table_next(static_cast<DictObj *>(from.obj())->t, at, k, v))
            if (dict_set(static_cast<DictObj *>(rd.v.obj()), k, v) != R::Ok)
                return R::Err;
    }
    out = rd.v;
    return R::Ok;
}

constexpr Type dict_type{ .name     = "dict",
                          .trace    = dict_trace,
                          .fini     = dict_fini,
                          .eq       = dict_eq,
                          .repr     = dict_repr,
                          .len      = dict_len_slot,
                          .getitem  = dict_getitem,
                          .setitem  = dict_setitem,
                          .delitem  = dict_delitem,
                          .contains = dict_contains,
                          .binop    = dict_binop,
                          .iter     = table_iter,
                          .patma    = PATMA_MAP | PATMA_SELF };

// The set protocol is in mapmeth.cpp: a dict view answers it too.
constexpr Type set_type{ .name     = "set",
                         .trace    = set_trace,
                         .fini     = set_fini,
                         .eq       = anyset_eq,
                         .order    = anyset_order,
                         .repr     = set_repr,
                         .len      = set_len_slot,
                         .contains = set_contains,
                         .binop    = anyset_binop,
                         .iter     = table_iter,
                         .patma    = PATMA_SELF };

constexpr Type frozenset_type{ .name     = "frozenset",
                               .trace    = set_trace,
                               .fini     = set_fini,
                               .hash     = frozenset_hash,
                               .eq       = anyset_eq,
                               .order    = anyset_order,
                               .repr     = frozenset_repr,
                               .len      = set_len_slot,
                               .contains = set_contains,
                               .binop    = anyset_binop,
                               .iter     = table_iter,
                               .patma    = PATMA_SELF };

DictObj *dict_new()
{
    DictObj *o = static_cast<DictObj *>(obj_alloc(&dict_type, sizeof(DictObj)));
    if (!o)
        return nullptr;
    new (&o->t) Table();
    return o;
}

R dict_get(DictObj *d, Value key, Value &out)
{
    return table_take(d->t, key, out, false);
}

R dict_set(DictObj *d, Value key, Value val)
{
    return table_put(d->t, key, val);
}

R dict_del(DictObj *d, Value key)
{
    Value gone;
    return table_take(d->t, key, gone, true);
}

SetObj *set_new()
{
    SetObj *o = static_cast<SetObj *>(obj_alloc(&set_type, sizeof(SetObj)));
    if (!o)
        return nullptr;
    new (&o->t) Table();
    return o;
}

SetObj *frozenset_new()
{
    SetObj *o = static_cast<SetObj *>(obj_alloc(&frozenset_type, sizeof(SetObj)));
    if (!o)
        return nullptr;
    new (&o->t) Table();
    return o;
}

R set_add(SetObj *s, Value v)
{
    return table_put(s->t, v, Value());
}

R set_has(SetObj *s, Value v, bool &out)
{
    Value ignored;
    R r = table_take(s->t, v, ignored, false);
    if (r == R::Err)
        return R::Err;
    out = r == R::Ok;
    return R::Ok;
}

R set_discard(SetObj *s, Value v, bool &out)
{
    Value gone;
    R r = table_take(s->t, v, gone, true);
    if (r == R::Err)
        return R::Err;
    out = r == R::Ok;
    return R::Ok;
}

bool table_next(const Table &t, usize &at, Value &key, Value &val)
{
    while (at < t.entries.size()) {
        const Entry &e = t.entries[at++];
        if (!e.key.is_nil()) {
            key = e.key;
            val = e.val;
            return true;
        }
    }
    return false;
}
