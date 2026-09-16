// Weak references: `_weakref`, the floor CPython's weakref.py stands on.
#pragma once

#include "obj.h"

extern const Type weakref_type;

inline bool is_weakref(Value v)
{
    return v.is_obj() && v.obj()->type == &weakref_type;
}

// Fill a module namespace with ref, ReferenceType and the two counters, and
// register the sweep hook that clears a reference whose target has gone.
bool weak_install(DictObj *into);
