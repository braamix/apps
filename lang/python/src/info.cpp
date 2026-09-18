// The struct sequence's handlers. One set, shared by every descriptor
// INFO_TYPE declares.
#include "info.h"

#include "func.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

InfoObj *info_of(Value v)
{
    return static_cast<InfoObj *>(v.obj());
}

TupleObj *items_of(Value v)
{
    return static_cast<TupleObj *>(info_of(v)->items.obj());
}

TupleObj *names_of(Value v)
{
    return static_cast<TupleObj *>(info_of(v)->names.obj());
}

// The items of whatever is being compared against: a tuple, or another of
// these. Null for anything else, which is what makes the comparison NotImpl.
const TupleObj *side(Value v, u32 &n)
{
    const TupleObj *t = nullptr;
    if (is_tuple(v))
        t = static_cast<const TupleObj *>(v.obj());
    else if (is_info(v))
        t = items_of(v);
    if (t)
        n = t->len;
    return t;
}

} // namespace

void info_trace(Obj *o)
{
    gc_mark(static_cast<InfoObj *>(o)->items);
    gc_mark(static_cast<InfoObj *>(o)->names);
    gc_mark(static_cast<InfoObj *>(o)->hidden);
}

R info_len(Value v, usize &out)
{
    out = items_of(v)->len;
    return R::Ok;
}

R info_getitem(Value v, Value key, Value &out)
{
    return py_getitem(info_of(v)->items, key, out);
}

R info_getattr(Value v, StrObj *name, Value &out)
{
    // The hidden fields first: os.stat_result shows st_atime as an int at
    // index 7 and answers it by name as a float.
    TupleObj *n = names_of(v);
    u32 shown   = items_of(v)->len;
    for (u32 pass = 0; pass < 2; pass++)
        for (u32 i = pass ? 0 : shown; i < (pass ? shown : n->len); i++)
            if (str_of(n->items()[i])->str() == name->str()) {
                out = i < shown
                          ? items_of(v)->items()[i]
                          : static_cast<TupleObj *>(info_of(v)->hidden.obj())->items()[i - shown];
                return R::Ok;
            }
    return R::NotImpl;
}

R info_eq(Value a, Value b, bool &out)
{
    u32 nx = 0, ny = 0;
    const TupleObj *x = side(a, nx), *y = side(b, ny);
    if (!x || !y)
        return R::NotImpl;
    return seq_eq(x->items(), nx, y->items(), ny, out);
}

R info_order(Value a, Value b, Cmp op, bool &out)
{
    u32 nx = 0, ny = 0;
    const TupleObj *x = side(a, nx), *y = side(b, ny);
    if (!x || !y)
        return R::NotImpl;
    return seq_order(x->items(), nx, y->items(), ny, op, out);
}

R info_contains(Value v, Value item, bool &out)
{
    return seq_contains(items_of(v)->items(), items_of(v)->len, item, out);
}

// The tuple it stands for, so a struct sequence and an equal tuple hash alike.
R info_hash(Value v, u32 &out)
{
    return py_hash(info_of(v)->items, out);
}

Value info_iter(Value v)
{
    return py_iter(info_of(v)->items);
}

R info_repr(Value v, String &out)
{
    TupleObj *n = names_of(v);
    if (!out.append(v.obj()->type->name) || !out.push('('))
        return oom();
    for (u32 i = 0; i < items_of(v)->len; i++) {
        if (i && !out.append(", "))
            return oom();
        if (!out.append(str_of(n->items()[i])->str()) || !out.push('='))
            return oom();
        if (py_repr(items_of(v)->items()[i], out) != R::Ok)
            return R::Err;
    }
    return out.push(')') ? R::Ok : oom();
}

Value info_new(const Type *t, const Value *items, const Str *names, usize n, usize shown)
{
    Roots pin{ const_cast<Value *>(items), n };
    TupleObj *xs = tuple_new(shown);
    if (!xs)
        return oom(), Value();
    for (usize i = 0; i < shown; i++)
        xs->items()[i] = items[i];
    Root rx{ obj_value(xs) };
    TupleObj *hs = tuple_new(n - shown);
    if (!hs)
        return oom(), Value();
    for (usize i = shown; i < n; i++)
        hs->items()[i - shown] = items[i];
    Root rh{ obj_value(hs) };
    TupleObj *ns = tuple_new(n);
    if (!ns)
        return oom(), Value();
    Root rn{ obj_value(ns) };
    for (usize i = 0; i < n; i++) {
        StrObj *k = str_intern(names[i]);
        if (!k)
            return oom(), Value();
        static_cast<TupleObj *>(rn.v.obj())->items()[i] = obj_value(k);
    }
    InfoObj *o = static_cast<InfoObj *>(obj_alloc(t, sizeof(InfoObj)));
    if (!o)
        return oom(), Value();
    o->items  = rx.v;
    o->names  = rn.v;
    o->hidden = rh.v;
    return obj_value(o);
}

R info_construct(const Type *t, const Str *names, usize n, usize shown, const CallArgs &a,
                 Value &out)
{
    Value seq, dict;
    for (u32 k = 0; k < a.nkw; k++) {
        Str nm = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (nm == "sequence" && a.nargs < 1)
            seq = a.kwvals[k];
        else if (nm == "dict" && a.nargs < 2)
            dict = a.kwvals[k];
        else
            return err_set2("TypeError", "structseq() got an unexpected keyword argument", nm);
    }
    if (a.nargs > 2)
        return err_set("TypeError", "structseq() takes at most 2 arguments");
    if (a.nargs > 0)
        seq = a.args[0];
    if (a.nargs > 1)
        dict = a.args[1];
    if (seq.is_nil())
        return err_set("TypeError", "structseq() missing required argument 'sequence' (pos 1)");
    Root rs{ seq }, rd{ dict };
    Root l{ obj_value(py_list_of(rs.v)) };
    if (l.v.is_nil()) {
        err_clear();
        return err_set("TypeError", "constructor requires a sequence");
    }
    usize len = list_of(l.v)->items.size();
    if (len < shown || len > n) {
        char t1[24], t2[24];
        Buf<200> b;
        b.put(t->name).put("() takes ");
        if (shown == n)
            b.put("a ");
        else
            b.put(len < shown ? "an at least " : "an at most ");
        b.put(int_text(t1, sizeof t1, i64(len < shown ? shown : n))).put("-sequence (");
        b.put(int_text(t2, sizeof t2, i64(len))).put("-sequence given)");
        return err_set("TypeError", b.str());
    }
    if (!rd.v.is_nil() && !is_none(rd.v) && !is_dict(rd.v)) {
        Buf<160> b;
        b.put(t->name).put("() takes a dict as second arg, if any");
        return err_set("TypeError", b.str());
    }
    Vec<Value> items;
    for (usize i = 0; i < n; i++) {
        Value v = i < len ? list_of(l.v)->items[i] : value_none();
        if (i >= len && is_dict(rd.v)) {
            StrObj *k = str_intern(names[i]);
            if (!k)
                return oom();
            Value got;
            R r = dict_get(static_cast<DictObj *>(rd.v.obj()), obj_value(k), got);
            if (r == R::Err)
                return R::Err;
            if (r == R::Ok)
                v = got;
        }
        if (!items.push(v))
            return oom();
    }
    Roots pin{ items.data(), items.size() };
    out = info_new(t, items.data(), names, n, shown);
    return out.is_nil() ? R::Err : R::Ok;
}

Value info_reduce(Value v)
{
    Root rv{ v };
    InfoObj *o = static_cast<InfoObj *>(rv.v.obj());
    DictObj *d = dict_new();
    if (!d)
        return err_set("MemoryError", "out of memory"), Value();
    Root rd{ obj_value(d) };
    o                = static_cast<InfoObj *>(rv.v.obj());
    TupleObj *shown  = static_cast<TupleObj *>(o->items.obj());
    TupleObj *names  = static_cast<TupleObj *>(o->names.obj());
    TupleObj *hidden = o->hidden.is_nil() ? nullptr : static_cast<TupleObj *>(o->hidden.obj());
    for (usize i = 0; hidden && i < hidden->len; i++)
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), names->items()[shown->len + i],
                     hidden->items()[i]) != R::Ok)
            return Value();
    Root cls{ type_of_value(rv.v) };
    TupleObj *args = tuple_new(2);
    if (cls.v.is_nil() || !args)
        return err_pending() ? Value() : (err_set("MemoryError", "out of memory"), Value());
    args->items()[0] = static_cast<InfoObj *>(rv.v.obj())->items;
    args->items()[1] = rd.v;
    Root ra{ obj_value(args) };
    TupleObj *t = tuple_new(2);
    if (!t)
        return err_set("MemoryError", "out of memory"), Value();
    t->items()[0] = cls.v;
    t->items()[1] = ra.v;
    return obj_value(t);
}
