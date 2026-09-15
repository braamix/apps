// The singletons, the type descriptors, and the three objects phase 1 needs.
#include "obj.h"

#include "gc.h"
#include "kernel/hash.h"

namespace {

void tuple_trace(Obj *o)
{
    TupleObj *t = static_cast<TupleObj *>(o);
    for (usize i = 0; i < t->len; i++)
        gc_mark(t->items()[i]);
}

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

bool started = false;

} // namespace

// Constant-initialised, all of them: a namespace-scope global here must be
// trivially destructible, and nothing guarantees a runtime constructor runs.
constexpr Type none_type{ "NoneType", nullptr, nullptr };
constexpr Type bool_type{ "bool", nullptr, nullptr };
constexpr Type int_type{ "int", nullptr, nullptr };
constexpr Type str_type{ "str", nullptr, nullptr };
constexpr Type tuple_type{ "tuple", tuple_trace, nullptr };
constexpr Type list_type{ "list", list_trace, list_fini };

Obj none_obj{ &none_type, nullptr, nullptr, OBJ_IMMORTAL };
Obj true_obj{ &bool_type, nullptr, nullptr, OBJ_IMMORTAL };
Obj false_obj{ &bool_type, nullptr, nullptr, OBJ_IMMORTAL };

void py_init()
{
    if (started)
        return;
    started = true;
    gc_immortal(&none_obj);
    gc_immortal(&true_obj);
    gc_immortal(&false_obj);
}

const Type *type_of(Value v)
{
    if (v.is_int())
        return &int_type;
    return v.is_obj() ? v.obj()->type : nullptr;
}

StrObj *str_new(Str s)
{
    StrObj *o = static_cast<StrObj *>(obj_alloc(&str_type, sizeof(StrObj) + s.size()));
    if (!o)
        return nullptr;
    o->len  = u32(s.size());
    o->hash = hash_key(s);
    for (usize i = 0; i < s.size(); i++)
        o->bytes()[i] = s[i];
    return o;
}

TupleObj *tuple_new(usize n)
{
    TupleObj *o =
        static_cast<TupleObj *>(obj_alloc(&tuple_type, sizeof(TupleObj) + n * sizeof(Value)));
    if (!o)
        return nullptr;
    o->len = u32(n);
    for (usize i = 0; i < n; i++)
        o->items()[i] = Value();
    return o;
}

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
