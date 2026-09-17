// The struct sequence's handlers. One set, shared by every descriptor
// INFO_TYPE declares.
#include "info.h"

#include "gc.h"
#include "intern.h"
#include "ops.h"

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
    TupleObj *n = names_of(v);
    u32 shown   = items_of(v)->len;
    for (u32 i = 0; i < n->len; i++)
        if (str_of(n->items()[i])->str() == name->str()) {
            out = i < shown ? items_of(v)->items()[i]
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
