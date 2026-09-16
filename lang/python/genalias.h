// The generic alias: what `list[int]` is.
#pragma once

#include "obj.h"

struct ContObj;

extern const Type genalias_type;

inline bool is_genalias(Value v)
{
    return v.is_obj() && v.obj()->type == &genalias_type;
}

// `origin[item]`. `item` is the subscript as written, so a tuple stays one.
Value genalias_new(Value origin, Value item);

// The continuation `list[int]()` runs: the call is the origin's.
R alias_call_step(ContObj *k, Value in);

// Put a `__class_getitem__` into each of these types' namespaces, and install
// the alias's own methods. Called once, from methods_install().
bool genalias_install(const Type *const *types, usize n);
