// `_collections`: the floor CPython's collections/__init__.py stands on.
//
// `deque` is native and is a real ring buffer -- a Vec with a head index --
// because a deque whose appendleft is O(n) is not a deque. `defaultdict` and
// `OrderedDict` are not: they are heap classes over `dict`, built here the way
// a `class D(dict)` body would build them, so an instance is an InstObj with a
// real dict inside it and every dict method already works on one. The dict
// this interpreter has keeps insertion order, so OrderedDict is dict plus
// move_to_end, a popitem that takes an end, and an equality that minds the
// order.
#include "call.h"
#include "compare.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// ------------------------------------------------------------------- deque

struct DequeObj : Obj {
    Vec<Value> buf; // the ring; `head` is where item 0 sits
    usize head;
    usize count;
    i64 maxlen; // -1 for no bound
};

extern const Type deque_type;

bool is_deque(Value v)
{
    return v.is_obj() && v.obj()->type == &deque_type;
}

DequeObj *deque_of(Value v)
{
    return static_cast<DequeObj *>(v.obj());
}

void deque_trace(Obj *o)
{
    DequeObj *d = static_cast<DequeObj *>(o);
    for (usize i = 0; i < d->count; i++)
        gc_mark(d->buf[(d->head + i) % d->buf.size()]);
}

void deque_fini(Obj *o)
{
    static_cast<DequeObj *>(o)->buf.~Vec();
}

Value &slot_at(DequeObj *d, usize i)
{
    return d->buf[(d->head + i) % d->buf.size()];
}

// Room for one more. The ring is copied out in order and back in at zero, so
// the growth also normalises it.
bool deque_grow(DequeObj *d)
{
    if (d->count < d->buf.size())
        return true;
    usize want = d->buf.size() ? d->buf.size() * 2 : 8;
    Vec<Value> fresh;
    if (!fresh.reserve(want))
        return oom() == R::Ok;
    for (usize i = 0; i < d->count; i++)
        fresh.push(slot_at(d, i));
    while (fresh.size() < want)
        fresh.push(Value());
    d->buf  = static_cast<Vec<Value> &&>(fresh);
    d->head = 0;
    return true;
}

bool deque_push_back(DequeObj *d, Value v)
{
    Root rv{ v };
    if (!deque_grow(d))
        return false;
    slot_at(d, d->count) = rv.v;
    d->count++;
    // maxlen: the far end goes, which is what makes a deque a sliding window.
    if (d->maxlen >= 0 && i64(d->count) > d->maxlen) {
        d->head = (d->head + 1) % d->buf.size();
        d->count--;
    }
    return true;
}

bool deque_push_front(DequeObj *d, Value v)
{
    Root rv{ v };
    if (!deque_grow(d))
        return false;
    d->head = (d->head + d->buf.size() - 1) % d->buf.size();
    d->count++;
    slot_at(d, 0) = rv.v;
    if (d->maxlen >= 0 && i64(d->count) > d->maxlen)
        d->count--;
    return true;
}

Value deque_new(i64 maxlen)
{
    DequeObj *d = static_cast<DequeObj *>(obj_alloc(&deque_type, sizeof(DequeObj)));
    if (!d)
        return oom(), Value();
    new (&d->buf) Vec<Value>();
    d->head   = 0;
    d->count  = 0;
    d->maxlen = maxlen;
    return obj_value(d);
}

R deque_len(Value v, usize &out)
{
    out = deque_of(v)->count;
    return R::Ok;
}

R deque_getitem(Value v, Value key, Value &out)
{
    DequeObj *d = deque_of(v);
    if (is_slice(key))
        return err_set("TypeError", "sequence index must be an integer");
    usize i = 0;
    if (index_of(key, d->count, i) != R::Ok)
        return R::Err;
    out = slot_at(d, i);
    return R::Ok;
}

R deque_setitem(Value v, Value key, Value item)
{
    DequeObj *d = deque_of(v);
    usize i     = 0;
    if (index_of(key, d->count, i) != R::Ok)
        return R::Err;
    slot_at(d, i) = item;
    return R::Ok;
}

// The one that is not a ring operation: a hole in the middle is closed by
// shifting the shorter side.
void deque_erase(DequeObj *d, usize at)
{
    for (usize i = at; i + 1 < d->count; i++)
        slot_at(d, i) = slot_at(d, i + 1);
    d->count--;
}

R deque_delitem(Value v, Value key)
{
    DequeObj *d = deque_of(v);
    usize i     = 0;
    if (index_of(key, d->count, i) != R::Ok)
        return R::Err;
    deque_erase(d, i);
    return R::Ok;
}

bool deque_truth(Value v)
{
    return deque_of(v)->count != 0;
}

R deque_contains(Value v, Value item, bool &out)
{
    DequeObj *d = deque_of(v);
    for (usize i = 0; i < d->count; i++) {
        bool same = false;
        if (py_eq(slot_at(d, i), item, same) != R::Ok)
            return R::Err;
        if (same) {
            out = true;
            return R::Ok;
        }
    }
    out = false;
    return R::Ok;
}

R deque_repr(Value v, String &out)
{
    Root rv{ v };
    if (!out.append("deque(["))
        return oom();
    for (usize i = 0; i < deque_of(rv.v)->count; i++) {
        if (i && !out.append(", "))
            return oom();
        if (py_repr(slot_at(deque_of(rv.v), i), out) != R::Ok)
            return R::Err;
    }
    if (!out.append("])"))
        return oom();
    if (deque_of(rv.v)->maxlen >= 0) {
        char tmp[24];
        String tail;
        if (!tail.append(", maxlen=") ||
            !tail.append(int_text(tmp, sizeof tmp, deque_of(rv.v)->maxlen)))
            return oom();
        // Back up over the ")" and put the keyword inside it.
        out.pop();
        if (!out.append(tail.str()) || !out.push(')'))
            return oom();
    }
    return R::Ok;
}

// The items, as a plain list: comparisons and the eager methods go through it.
ListObj *deque_items(Value v)
{
    Root rv{ v };
    ListObj *l = list_new();
    if (!l)
        return oom(), nullptr;
    Root rl{ obj_value(l) };
    for (usize i = 0; i < deque_of(rv.v)->count; i++)
        if (!list_push(list_of(rl.v), slot_at(deque_of(rv.v), i)))
            return oom(), nullptr;
    return list_of(rl.v);
}

R deque_eq(Value a, Value b, bool &out)
{
    if (!is_deque(b))
        return R::NotImpl;
    if (deque_of(a)->count != deque_of(b)->count) {
        out = false;
        return R::Ok;
    }
    Root ra{ a }, rb{ b };
    for (usize i = 0; i < deque_of(ra.v)->count; i++) {
        bool same = false;
        if (py_eq(slot_at(deque_of(ra.v), i), slot_at(deque_of(rb.v), i), same) != R::Ok)
            return R::Err;
        if (!same) {
            out = false;
            return R::Ok;
        }
    }
    out = true;
    return R::Ok;
}

R deque_order(Value a, Value b, Cmp op, bool &out)
{
    if (!is_deque(b))
        return R::NotImpl;
    Root ra{ a }, rb{ b };
    ListObj *x = deque_items(ra.v);
    if (!x)
        return R::Err;
    Root rx{ obj_value(x) };
    ListObj *y = deque_items(rb.v);
    if (!y)
        return R::Err;
    Root ry{ obj_value(y) };
    return seq_order(list_of(rx.v)->items.data(), list_of(rx.v)->items.size(),
                     list_of(ry.v)->items.data(), list_of(ry.v)->items.size(), op, out);
}

Value deque_iter(Value v)
{
    // Over a snapshot: a deque may be appended to while it is walked, and the
    // ring would then hand back an item twice.
    ListObj *l = deque_items(v);
    return l ? seq_iter(obj_value(l)) : Value();
}

DequeObj *self_deque(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_deque(s)) {
        Buf<96> b;
        b.put(who).put("() requires a deque");
        return err_set2("TypeError", b.str(), type_name(s)), nullptr;
    }
    return deque_of(s);
}

R d_append(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "append");
    if (!d || !meth_args(a, "append", 1, 1))
        return R::Err;
    if (!deque_push_back(d, a.args[1]))
        return R::Err;
    out = value_none();
    return R::Ok;
}

R d_appendleft(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "appendleft");
    if (!d || !meth_args(a, "appendleft", 1, 1))
        return R::Err;
    if (!deque_push_front(d, a.args[1]))
        return R::Err;
    out = value_none();
    return R::Ok;
}

R d_pop(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "pop");
    if (!d || !meth_args(a, "pop", 0, 0))
        return R::Err;
    if (!d->count)
        return err_set("IndexError", "pop from an empty deque");
    out = slot_at(d, d->count - 1);
    d->count--;
    return R::Ok;
}

R d_popleft(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "popleft");
    if (!d || !meth_args(a, "popleft", 0, 0))
        return R::Err;
    if (!d->count)
        return err_set("IndexError", "pop from an empty deque");
    out     = slot_at(d, 0);
    d->head = (d->head + 1) % d->buf.size();
    d->count--;
    return R::Ok;
}

R extend_from(Value self, Value src, bool left)
{
    Root rs{ self }, rv{ src };
    ListObj *items = py_list_of(rv.v);
    if (!items)
        return R::Err;
    Root ri{ obj_value(items) };
    for (usize i = 0; i < list_of(ri.v)->items.size(); i++) {
        Value one = list_of(ri.v)->items[i];
        bool ok =
            left ? deque_push_front(deque_of(rs.v), one) : deque_push_back(deque_of(rs.v), one);
        if (!ok)
            return R::Err;
    }
    return R::Ok;
}

R d_extend(const CallArgs &a, Value &out)
{
    if (!self_deque(a, "extend") || !meth_args(a, "extend", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, d_extend, out);
    if (extend_from(method_self(a.args[0]), a.args[1], false) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R d_extendleft(const CallArgs &a, Value &out)
{
    if (!self_deque(a, "extendleft") || !meth_args(a, "extendleft", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, d_extendleft, out);
    if (extend_from(method_self(a.args[0]), a.args[1], true) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R d_rotate(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "rotate");
    if (!d || !meth_args(a, "rotate", 0, 1))
        return R::Err;
    i64 n = 1;
    if (a.nargs > 1 && !as_index(a.args[1], n))
        return err_set("TypeError", "rotate() wants an integer");
    // An item at a time off one end and onto the other. The head cannot just
    // be moved: the slots outside the window are not part of the deque.
    if (d->count > 1) {
        i64 m = i64(d->count);
        i64 k = ((n % m) + m) % m;
        // Whichever way round is shorter: a rotation right by m - 1 is a
        // rotation left by one.
        if (k * 2 <= m) {
            for (i64 i = 0; i < k; i++) {
                Value last    = slot_at(d, d->count - 1);
                d->head       = (d->head + d->buf.size() - 1) % d->buf.size();
                slot_at(d, 0) = last;
            }
        } else {
            for (i64 i = k; i < m; i++) {
                Value first              = slot_at(d, 0);
                d->head                  = (d->head + 1) % d->buf.size();
                slot_at(d, d->count - 1) = first;
            }
        }
    }
    out = value_none();
    return R::Ok;
}

R d_clear(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "clear");
    if (!d || !meth_args(a, "clear", 0, 0))
        return R::Err;
    d->head  = 0;
    d->count = 0;
    out      = value_none();
    return R::Ok;
}

R d_reverse(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "reverse");
    if (!d || !meth_args(a, "reverse", 0, 0))
        return R::Err;
    for (usize i = 0; i + 1 < d->count - i; i++) {
        Value t                      = slot_at(d, i);
        slot_at(d, i)                = slot_at(d, d->count - 1 - i);
        slot_at(d, d->count - 1 - i) = t;
    }
    out = value_none();
    return R::Ok;
}

R d_copy(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "copy");
    if (!d || !meth_args(a, "copy", 0, 0))
        return R::Err;
    Root self{ method_self(a.args[0]) };
    Root made{ deque_new(deque_of(self.v)->maxlen) };
    if (made.v.is_nil())
        return R::Err;
    for (usize i = 0; i < deque_of(self.v)->count; i++)
        if (!deque_push_back(deque_of(made.v), slot_at(deque_of(self.v), i)))
            return R::Err;
    out = made.v;
    return R::Ok;
}

// count, index and remove all compare, and an __eq__ written in Python is a
// call, so each hands the list to compare.cpp's searcher.
R search(const CallArgs &a, Value &out, u32 what, Str who)
{
    DequeObj *d = self_deque(a, who);
    if (!d)
        return R::Err;
    Root self{ method_self(a.args[0]) };
    ListObj *items = deque_items(self.v);
    if (!items)
        return R::Err;
    Root ri{ obj_value(items) };
    out = cmp_find(ri.v, a.args[1], what, 0, list_of(ri.v)->items.size());
    return out.is_nil() ? R::Err : R::Ok;
}

R d_count(const CallArgs &a, Value &out)
{
    return meth_args(a, "count", 1, 1) ? search(a, out, CMP_COUNT, "count") : R::Err;
}

R d_index(const CallArgs &a, Value &out)
{
    return meth_args(a, "index", 1, 1) ? search(a, out, CMP_INDEX, "index") : R::Err;
}

R d_remove(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "remove");
    if (!d || !meth_args(a, "remove", 1, 1))
        return R::Err;
    for (usize i = 0; i < d->count; i++) {
        bool same = false;
        if (py_eq(slot_at(d, i), a.args[1], same) != R::Ok)
            return R::Err;
        if (same) {
            deque_erase(d, i);
            out = value_none();
            return R::Ok;
        }
    }
    return err_set("ValueError", "deque.remove(x): x not in deque");
}

R d_insert(const CallArgs &a, Value &out)
{
    DequeObj *d = self_deque(a, "insert");
    if (!d || !meth_args(a, "insert", 2, 2))
        return R::Err;
    i64 at = 0;
    if (!as_index(a.args[1], at))
        return err_set("TypeError", "insert() wants an integer index");
    if (d->maxlen >= 0 && i64(d->count) >= d->maxlen)
        return err_set("IndexError", "deque already at its maximum size");
    if (at < 0)
        at += i64(d->count);
    if (at < 0)
        at = 0;
    if (at > i64(d->count))
        at = i64(d->count);
    if (!deque_push_back(d, value_none()))
        return R::Err;
    for (usize i = d->count - 1; i > usize(at); i--)
        slot_at(d, i) = slot_at(d, i - 1);
    slot_at(d, usize(at)) = a.args[2];
    out                   = value_none();
    return R::Ok;
}

R d_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() != "maxlen")
        return R::NotImpl;
    i64 n = deque_of(v)->maxlen;
    out   = n < 0 ? value_none() : int_from_i64(n);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method DEQUE_METHODS[] = {
    { "append", d_append },   { "appendleft", d_appendleft }, { "pop", d_pop },
    { "popleft", d_popleft }, { "extend", d_extend },         { "extendleft", d_extendleft },
    { "rotate", d_rotate },   { "clear", d_clear },           { "reverse", d_reverse },
    { "copy", d_copy },       { "__copy__", d_copy },         { "count", d_count },
    { "index", d_index },     { "remove", d_remove },         { "insert", d_insert },
};

constexpr Type deque_type{ .name     = "deque",
                           .trace    = deque_trace,
                           .fini     = deque_fini,
                           .truth    = deque_truth,
                           .eq       = deque_eq,
                           .order    = deque_order,
                           .repr     = deque_repr,
                           .len      = deque_len,
                           .getitem  = deque_getitem,
                           .setitem  = deque_setitem,
                           .delitem  = deque_delitem,
                           .contains = deque_contains,
                           .iter     = deque_iter,
                           .getattr  = d_getattr };

R b_deque(const CallArgs &a, Value &out)
{
    Str names[2] = { "iterable", "maxlen" };
    Value got[2] = { Value(), Value() };
    if (a.nargs > 2)
        return err_set("TypeError", "deque() takes at most 2 arguments");
    for (u32 i = 0; i < a.nargs; i++)
        got[i] = a.args[i];
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        u32 i = n == names[0] ? 0 : n == names[1] ? 1 : 2;
        if (i > 1)
            return err_set2("TypeError", "deque() got an unexpected keyword argument", n);
        got[i] = a.kwvals[k];
    }
    if (!got[0].is_nil() && iter_needs_vm(got[0]))
        return iter_park(a, 0, b_deque, out);
    i64 most = -1;
    if (!got[1].is_nil() && !is_none(got[1])) {
        if (!as_index(got[1], most) || most < 0)
            return err_set("ValueError", "maxlen must be non-negative");
    }
    Root made{ deque_new(most) };
    if (made.v.is_nil())
        return R::Err;
    if (!got[0].is_nil() && !is_none(got[0]) && extend_from(made.v, got[0], false) != R::Ok)
        return R::Err;
    out = made.v;
    return R::Ok;
}

// --------------------------------------------------------- the two dict kinds

// A namespace for a class body written in C++: the natives, by name.
bool put_methods(DictObj *into, const Method *tab, usize n)
{
    Root rd{ obj_value(into) };
    for (usize i = 0; i < n; i++) {
        Root fn{ native_new(tab[i].name, tab[i].fn) };
        if (fn.v.is_nil())
            return false;
        StrObj *k = str_intern(tab[i].name);
        if (!k)
            return oom() == R::Ok;
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(k), fn.v) != R::Ok)
            return false;
    }
    return true;
}

template <usize N>
inline bool put_methods(DictObj *into, const Method (&tab)[N])
{
    return put_methods(into, tab, N);
}

// `class <name>(dict): ...`, with the body already filled in.
Value dict_subclass(Str name, DictObj *body)
{
    Root rb{ obj_value(body) };
    Root nm{ str_new(name) };
    if (nm.v.is_nil())
        return Value();
    Root base{ type_wrap(&dict_type) };
    if (base.v.is_nil())
        return Value();
    TupleObj *bases = tuple_new(1);
    if (!bases)
        return oom(), Value();
    bases->items()[0] = base.v;
    Root rt{ obj_value(bases) };
    return type_new(nm.v, rt.v, rb.v);
}

// The attribute a defaultdict keeps its factory in. It is an ordinary
// instance attribute, where CPython's is a member of the C struct; the
// difference shows only in vars(d).
Value factory_of(Value self)
{
    StrObj *n = str_intern("default_factory");
    Value got;
    if (!n || !is_inst(self) || inst_of(self)->dict.is_nil())
        return value_none();
    DictObj *d = static_cast<DictObj *>(inst_of(self)->dict.obj());
    return dict_get(d, obj_value(n), got) == R::Ok ? got : value_none();
}

R dd_init(const CallArgs &a, Value &out)
{
    if (a.nargs < 1)
        return err_set("TypeError", "__init__() needs self");
    Root self{ a.args[0] };
    Root fn{ a.nargs > 1 ? a.args[1] : value_none() };
    StrObj *n = str_intern("default_factory");
    if (!n)
        return oom();
    if (inst_setattr(self.v, n, fn.v) != R::Ok)
        return R::Err;
    // Anything after the factory is what dict() would have taken.
    Value inner = method_self(self.v);
    for (u32 i = 2; i < a.nargs; i++) {
        ListObj *items = py_list_of(a.args[i]);
        if (!items)
            return R::Err;
        Root ri{ obj_value(items) };
        for (usize j = 0; j < list_of(ri.v)->items.size(); j++) {
            Value pair = list_of(ri.v)->items[j];
            Value k, v;
            if (py_getitem(pair, Value::of_int(0), k) != R::Ok ||
                py_getitem(pair, Value::of_int(1), v) != R::Ok)
                return R::Err;
            if (py_setitem(inner, k, v) != R::Ok)
                return R::Err;
        }
    }
    for (u32 k = 0; k < a.nkw; k++)
        if (py_setitem(inner, a.kwnames[k], a.kwvals[k]) != R::Ok)
            return R::Err;
    out = value_none();
    return R::Ok;
}

// The factory has answered: the value goes in under the key, and is the
// subscript's answer.
R dd_missing_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    Root got{ in };
    if (py_setitem(k->s[1], k->s[2], got.v) != R::Ok)
        return R::Err;
    return cont_done(k, got.v);
}

R dd_missing(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__missing__", 1, 1))
        return R::Err;
    Root self{ a.args[0] }, key{ a.args[1] };
    Root fn{ factory_of(self.v) };
    if (is_none(fn.v)) {
        return key_error(key.v);
    }
    Root kv{ cont_new(dd_missing_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = fn.v;
    cont_of(kv.v)->s[1] = method_self(self.v);
    cont_of(kv.v)->s[2] = key.v;
    out                 = kv.v;
    return R::Ok;
}

R dd_repr(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__repr__", 0, 0))
        return R::Err;
    Root self{ a.args[0] };
    String text;
    if (!text.append("defaultdict("))
        return oom();
    if (py_repr(factory_of(self.v), text) != R::Ok)
        return R::Err;
    if (!text.append(", "))
        return oom();
    if (py_repr(method_self(self.v), text) != R::Ok)
        return R::Err;
    if (!text.push(')'))
        return oom();
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R dd_copy(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "copy", 0, 0))
        return R::Err;
    Root self{ a.args[0] };
    Root cls{ type_of_value(self.v) };
    if (cls.v.is_nil())
        return R::Err;
    Root args{ obj_value(tuple_new(2)) };
    if (args.v.is_nil())
        return oom();
    static_cast<TupleObj *>(args.v.obj())->items()[0] = factory_of(self.v);
    Root items{ dict_view(method_self(self.v), VIEW_ITEMS) };
    if (items.v.is_nil())
        return R::Err;
    static_cast<TupleObj *>(args.v.obj())->items()[1] = items.v;
    out                                               = attr_invoke(cls.v, args.v);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method DEFAULTDICT[] = {
    { "__init__", dd_init }, { "__missing__", dd_missing }, { "__repr__", dd_repr },
    { "copy", dd_copy },     { "__copy__", dd_copy },
};

// OrderedDict. The dict under it is already insertion-ordered, so what is
// added is the two ends being nameable and an equality that minds the order.
// The pairs of a dict, in order, as a flat list of key, value, key, value.
ListObj *pairs_of(Value d)
{
    Root rd{ d };
    ListObj *l = list_new();
    if (!l)
        return oom(), nullptr;
    Root rl{ obj_value(l) };
    usize at = 0;
    Value k, v;
    while (table_next(static_cast<DictObj *>(rd.v.obj())->t, at, k, v))
        if (!list_push(list_of(rl.v), k) || !list_push(list_of(rl.v), v))
            return oom(), nullptr;
    return list_of(rl.v);
}

// `last` is what the keyword says; both the positional and the keyword form
// are accepted, because the library writes it both ways.
bool end_wanted(const CallArgs &a, u32 at, bool &last)
{
    if (a.nargs > at)
        last = py_truth(a.args[at]);
    for (u32 k = 0; k < a.nkw; k++) {
        if (!is_str(a.kwnames[k]) || str_of(a.kwnames[k])->str() != "last")
            return err_set("TypeError", "unexpected keyword argument"), false;
        last = py_truth(a.kwvals[k]);
    }
    return true;
}

R od_move_to_end(const CallArgs &a, Value &out)
{
    if (a.nargs < 2 || a.nargs > 3)
        return err_set("TypeError", "move_to_end() takes a key and an optional end");
    bool last = true;
    if (!end_wanted(a, 2, last))
        return R::Err;

    Root self{ method_self(a.args[0]) }, key{ a.args[1] };
    DictObj *d = static_cast<DictObj *>(self.v.obj());
    Value had;
    if (dict_get(d, key.v, had) != R::Ok)
        return err_pending() ? R::Err : err_set("KeyError", "not in the dictionary");
    Root rh{ had };
    if (last) {
        d = static_cast<DictObj *>(self.v.obj());
        if (dict_del(d, key.v) != R::Ok || dict_set(d, key.v, rh.v) != R::Ok)
            return R::Err;
        out = value_none();
        return R::Ok;
    }
    // To the front: the order is insertion order, so everything comes out and
    // goes back in with this key first.
    ListObj *old = pairs_of(self.v);
    if (!old)
        return R::Err;
    Root ro{ obj_value(old) };
    for (usize i = 0; i < list_of(ro.v)->items.size(); i += 2)
        if (dict_del(static_cast<DictObj *>(self.v.obj()), list_of(ro.v)->items[i]) != R::Ok)
            return R::Err;
    if (dict_set(static_cast<DictObj *>(self.v.obj()), key.v, rh.v) != R::Ok)
        return R::Err;
    for (usize i = 0; i < list_of(ro.v)->items.size(); i += 2) {
        bool same = false;
        if (py_eq(list_of(ro.v)->items[i], key.v, same) != R::Ok)
            return R::Err;
        if (same)
            continue;
        if (dict_set(static_cast<DictObj *>(self.v.obj()), list_of(ro.v)->items[i],
                     list_of(ro.v)->items[i + 1]) != R::Ok)
            return R::Err;
    }
    out = value_none();
    return R::Ok;
}

R od_popitem(const CallArgs &a, Value &out)
{
    if (a.nargs > 2)
        return err_set("TypeError", "popitem() takes at most one argument");
    bool last = true;
    if (!end_wanted(a, 1, last))
        return R::Err;
    Root self{ method_self(a.args[0]) };
    DictObj *d = static_cast<DictObj *>(self.v.obj());
    if (!dict_len(d))
        return err_set("KeyError", "dictionary is empty");
    usize at = 0;
    Value key, val, k2, v2;
    while (table_next(d->t, at, k2, v2)) {
        key = k2;
        val = v2;
        if (!last)
            break;
    }
    Root rk{ key }, rv{ val };
    TupleObj *pair = tuple_new(2);
    if (!pair)
        return oom();
    pair->items()[0] = rk.v;
    pair->items()[1] = rv.v;
    Root rp{ obj_value(pair) };
    if (dict_del(static_cast<DictObj *>(self.v.obj()), rk.v) != R::Ok)
        return R::Err;
    out = rp.v;
    return R::Ok;
}

// Two OrderedDicts are equal only in the same order; against a plain dict the
// order does not count, which is what CPython says.
R od_eq(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__eq__", 1, 1))
        return R::Err;
    Root self{ method_self(a.args[0]) }, other{ a.args[1] };
    Root inner{ method_self(other.v) };
    if (!is_dict(inner.v)) {
        out = value_notimpl();
        return R::Ok;
    }
    bool same = false;
    if (py_eq(self.v, inner.v, same) != R::Ok)
        return R::Err;
    // Only when the other side is an OrderedDict too does the order count.
    if (!same || !is_inst(other.v) || type_of_value(other.v) != type_of_value(a.args[0])) {
        out = value_bool(same);
        return R::Ok;
    }
    usize ax = 0, ay = 0;
    Value kx, vx, ky, vy;
    while (table_next(static_cast<DictObj *>(self.v.obj())->t, ax, kx, vx) &&
           table_next(static_cast<DictObj *>(inner.v.obj())->t, ay, ky, vy)) {
        bool eq = false;
        if (py_eq(kx, ky, eq) != R::Ok)
            return R::Err;
        if (!eq) {
            out = value_bool(false);
            return R::Ok;
        }
    }
    out = value_bool(true);
    return R::Ok;
}

R od_repr(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__repr__", 0, 0))
        return R::Err;
    Root self{ a.args[0] };
    String text;
    Value cls = type_of_value(self.v);
    if (cls.is_nil())
        return R::Err;
    if (!text.append(str_of(type_obj(cls)->name)->str()) || !text.push('('))
        return oom();
    // The 3.9 form, `OrderedDict([(k, v), ...])`: that is the CPython every
    // golden in this suite is measured against.
    DictObj *d = static_cast<DictObj *>(method_self(self.v).obj());
    if (dict_len(d)) {
        if (!text.push('['))
            return oom();
        usize at = 0;
        Value k, x;
        bool first = true;
        while (table_next(static_cast<DictObj *>(method_self(self.v).obj())->t, at, k, x)) {
            if (!first && !text.append(", "))
                return oom();
            first = false;
            if (!text.push('('))
                return oom();
            if (py_repr(k, text) != R::Ok)
                return R::Err;
            if (!text.append(", "))
                return oom();
            if (py_repr(x, text) != R::Ok)
                return R::Err;
            if (!text.push(')'))
                return oom();
        }
        if (!text.push(']'))
            return oom();
    }
    if (!text.push(')'))
        return oom();
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method ORDEREDDICT[] = {
    { "move_to_end", od_move_to_end },
    { "popitem", od_popitem },
    { "__eq__", od_eq },
    { "__repr__", od_repr },
};

// _count_elements(mapping, iterable): what Counter.update reaches for.
R b_count_elements(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_count_elements", 2, 2))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, b_count_elements, out);
    Root m{ a.args[0] };
    ListObj *items = py_list_of(a.args[1]);
    if (!items)
        return R::Err;
    Root ri{ obj_value(items) };
    Value inner = method_self(m.v);
    for (usize i = 0; i < list_of(ri.v)->items.size(); i++) {
        Value key = list_of(ri.v)->items[i];
        Value had;
        R r = is_dict(inner) ? dict_get(static_cast<DictObj *>(inner.obj()), key, had)
                             : py_getitem(inner, key, had);
        if (r == R::Err) {
            if (err_kind() != "KeyError")
                return R::Err;
            err_clear();
            r = R::NotImpl;
        }
        Value next;
        if (r == R::Ok) {
            if (py_binop(had, Value::of_int(1), Op::Add, next) != R::Ok)
                return R::Err;
        } else {
            next = Value::of_int(1);
        }
        if (py_setitem(inner, key, next) != R::Ok)
            return R::Err;
    }
    out = value_none();
    return R::Ok;
}

constexpr ModDef DEFS[] = { { "_count_elements", b_count_elements } };

} // namespace

bool coll_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&deque_type, DEQUE_METHODS))
        return false;
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_type(d, &deque_type, b_deque) || !mod_defs(d, DEFS))
        return false;

    struct Sub {
        Str name;
        const Method *tab;
        usize n;
    };
    const Sub SUBS[] = {
        { "defaultdict", DEFAULTDICT, sizeof DEFAULTDICT / sizeof DEFAULTDICT[0] },
        { "OrderedDict", ORDEREDDICT, sizeof ORDEREDDICT / sizeof ORDEREDDICT[0] },
    };
    for (const Sub &s : SUBS) {
        DictObj *body = dict_new();
        if (!body)
            return oom() == R::Ok;
        Root rb{ obj_value(body) };
        if (!put_methods(static_cast<DictObj *>(rb.v.obj()), s.tab, s.n))
            return false;
        Root cls{ dict_subclass(s.name, static_cast<DictObj *>(rb.v.obj())) };
        if (cls.v.is_nil() || !mod_put(static_cast<DictObj *>(rd.v.obj()), s.name, cls.v))
            return false;
    }
    return true;
}
