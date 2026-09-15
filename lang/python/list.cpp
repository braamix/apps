// list: a growable run of values.
#include "gc.h"
#include "ops.h"

namespace {

void list_trace(Obj *o)
{
    ListObj *l = static_cast<ListObj *>(o);
    for (usize i = 0; i < l->items.size(); i++)
        gc_mark(l->items[i]);
}

void list_fini(Obj *o)
{
    static_cast<ListObj *>(o)->items.~Vec();
}

R list_len(Value v, usize &out)
{
    out = list_of(v)->items.size();
    return R::Ok;
}

R list_eq(Value a, Value b, bool &out)
{
    if (!is_list(b))
        return R::NotImpl;
    ListObj *x = list_of(a), *y = list_of(b);
    return seq_eq(x->items.data(), x->items.size(), y->items.data(), y->items.size(), out);
}

R list_order(Value a, Value b, Cmp op, bool &out)
{
    if (!is_list(b))
        return R::NotImpl;
    ListObj *x = list_of(a), *y = list_of(b);
    return seq_order(x->items.data(), x->items.size(), y->items.data(), y->items.size(), op, out);
}

R list_getitem(Value v, Value key, Value &out)
{
    ListObj *l = list_of(v);
    usize i    = 0;
    if (index_of(key, l->items.size(), i) != R::Ok)
        return R::Err;
    out = l->items[i];
    return R::Ok;
}

R list_setitem(Value v, Value key, Value item)
{
    ListObj *l = list_of(v);
    usize i    = 0;
    if (index_of(key, l->items.size(), i) != R::Ok)
        return R::Err;
    l->items[i] = item;
    return R::Ok;
}

R list_contains(Value v, Value item, bool &out)
{
    ListObj *l = list_of(v);
    return seq_contains(l->items.data(), l->items.size(), item, out);
}

R list_binop(Value a, Value b, Op op, Value &out)
{
    if (op == Op::Add && is_list(a) && is_list(b)) {
        Root ra{ a }, rb{ b };
        ListObj *r = list_new();
        if (!r)
            return err_set("MemoryError", "out of memory");
        Root rr{ obj_value(r) };
        Value sides[2] = { ra.v, rb.v };
        for (Value side : sides) {
            ListObj *s = list_of(side);
            for (usize i = 0; i < s->items.size(); i++)
                if (!list_push(list_of(rr.v), s->items[i]))
                    return err_set("MemoryError", "out of memory");
        }
        out = rr.v;
        return R::Ok;
    }
    if (op == Op::Mul) {
        Value s = is_list(a) ? a : b;
        Value n = is_list(a) ? b : a;
        i64 count;
        if (!is_list(s) || !as_index(n, count))
            return R::NotImpl;
        Root rs{ s };
        ListObj *r = list_new();
        if (!r)
            return err_set("MemoryError", "out of memory");
        Root rr{ obj_value(r) };
        for (i64 k = 0; k < count; k++) {
            ListObj *x = list_of(rs.v);
            for (usize i = 0; i < x->items.size(); i++)
                if (!list_push(list_of(rr.v), x->items[i]))
                    return err_set("MemoryError", "out of memory");
        }
        out = rr.v;
        return R::Ok;
    }
    return R::NotImpl;
}

} // namespace

R list_repr(Value v, String &out);

constexpr Type list_type{ .name     = "list",
                          .trace    = list_trace,
                          .fini     = list_fini,
                          .eq       = list_eq,
                          .order    = list_order,
                          .repr     = list_repr,
                          .len      = list_len,
                          .getitem  = list_getitem,
                          .setitem  = list_setitem,
                          .contains = list_contains,
                          .binop    = list_binop };

ListObj *list_new()
{
    ListObj *o = static_cast<ListObj *>(obj_alloc(&list_type, sizeof(ListObj)));
    if (!o)
        return nullptr;
    new (&o->items) Vec<Value>();
    return o;
}

bool list_push(ListObj *l, Value v)
{
    return l->items.push(v);
}
