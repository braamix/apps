// The singletons and the two types that are only singletons.
#include "obj.h"

#include "gc.h"

namespace {

bool none_truth(Value)
{
    return false;
}

bool bool_truth(Value v)
{
    return is_true(v);
}

R bool_hash(Value v, u32 &out)
{
    out = is_true(v) ? 1 : 0;
    return R::Ok;
}

bool started;

} // namespace

R none_repr(Value, String &out);
R bool_repr(Value, String &out);
R ellipsis_repr(Value, String &out);

constexpr Type none_type{ .name = "NoneType", .truth = none_truth, .repr = none_repr };

constexpr Type ellipsis_type{ .name = "ellipsis", .repr = ellipsis_repr };

constexpr Type bool_type{ .name  = "bool",
                          .truth = bool_truth,
                          .hash  = bool_hash,
                          .repr  = bool_repr };

Obj none_obj{ &none_type, nullptr, nullptr, OBJ_IMMORTAL };
Obj true_obj{ &bool_type, nullptr, nullptr, OBJ_IMMORTAL };
Obj false_obj{ &bool_type, nullptr, nullptr, OBJ_IMMORTAL };
Obj ellipsis_obj{ &ellipsis_type, nullptr, nullptr, OBJ_IMMORTAL };

void py_init()
{
    if (started)
        return;
    started = true;
    gc_immortal(&none_obj);
    gc_immortal(&true_obj);
    gc_immortal(&false_obj);
    gc_immortal(&ellipsis_obj);
}

const Type *type_of(Value v)
{
    if (v.is_int())
        return &int_type;
    return v.is_obj() ? v.obj()->type : nullptr;
}

Str type_name(Value v)
{
    const Type *t = type_of(v);
    return t ? t->name : Str("nil");
}

Str cmp_symbol(Cmp op)
{
    switch (op) {
    case Cmp::Eq:
        return "==";
    case Cmp::Ne:
        return "!=";
    case Cmp::Lt:
        return "<";
    case Cmp::Le:
        return "<=";
    case Cmp::Gt:
        return ">";
    case Cmp::Ge:
        return ">=";
    case Cmp::In:
        return "in";
    case Cmp::NotIn:
        return "not in";
    case Cmp::Is:
        return "is";
    case Cmp::IsNot:
        return "is not";
    }
    return "?";
}
