// Weak references: `_weakref`, the floor CPython's weakref.py stands on.
#pragma once

#include "obj.h"

extern const Type weakref_type;

// A reference or a proxy: the target is raw, so the collector does not
// follow it, and Null once it has gone.
struct WeakRefObj : Obj {
    Obj *target;
    Value callback; // called with this reference once the target has gone
    Value owner;    // the instance of a subclass of ref this is laid inside, or Nil
    u32 hash;       // the target's identity, kept so a dead ref still hashes
};

inline bool is_weakref(Value v)
{
    return v.is_obj() && v.obj()->type == &weakref_type;
}

extern const Type proxy_type;
extern const Type callable_proxy_type;

inline bool is_weakproxy(Value v)
{
    return v.is_obj() && (v.obj()->type == &proxy_type || v.obj()->type == &callable_proxy_type);
}

// What a proxy stands for: the referent, or Nil with ReferenceError pending
// once it has gone.
Value proxy_target(Value v);

// `v` itself, or the referent where `v` is a proxy. Nil only for a dead one.
inline Value unproxy(Value v)
{
    return is_weakproxy(v) ? proxy_target(v) : v;
}

// `made` is laid inside `self`, an instance of a subclass of ref: what to lay
// there. A shared reference is copied first, and the callback is then handed
// `self`. Anything that is not a reference comes back as it is.
Value weak_adopt(Value made, Value self);

// Fill a module namespace with ref, ReferenceType and the two counters, and
// register the sweep hook that clears a reference whose target has gone.
bool weak_install(DictObj *into);
