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

// One member of an alias or a union as `typing` prints it: a class by its
// module and name, None and ... as written, anything else by its repr.
R typing_repr(Value v, String &out);

// The type variables among `args`, in order and each once: a variable itself,
// or what a generic alias or a union among them has.
Value typing_params(Value args);

// `args` with each of `params` replaced by what `item` gives for it: the
// substitution `list[T][int]` and `(T | None)[int]` make. `self` is what was
// subscripted, for the complaint.
Value typing_subst(Value self, Value args, Value params, Value item);

// The continuation `list[int]()` runs: the call is the origin's.
R alias_call_step(ContObj *k, Value in);

// Put a `__class_getitem__` into each of these types' namespaces, and install
// the alias's own methods. Called once, from methods_install().
bool genalias_install(const Type *const *types, usize n);
