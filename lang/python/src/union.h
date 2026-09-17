// `int | str`: the union type, which is `typing.Union` and `types.UnionType`
// both since 3.14.
#pragma once

#include "obj.h"

extern const Type union_type;

inline bool is_union(Value v)
{
    return v.is_obj() && v.obj()->type == &union_type;
}

// The members, a tuple in the order first written, None as NoneType.
Value union_args(Value v);

// What `|` makes a union of: None, a class, a generic alias, a union, a type
// alias.
bool is_unionable(Value v);

// The binop slot of every type that makes a union: `|` and nothing else.
R union_binop(Value a, Value b, Op op, Value &out);

// `a | b` for two of those. NotImpl when either is something else.
R union_or(Value a, Value b, Value &out);

// `typing.Union[args]`. One member is that member itself, and none is an
// error.
Value union_from(Value args);

// The union's own methods, and __or__ and __ror__ on the types that make one.
bool union_install();

// `isinstance(x, int | str)` is `isinstance(x, (int, str))`: the tuple a
// union stands for, or `v` itself. A parameterized generic in it is refused.
Value union_as_tuple(Value v, Str who);
