// What the __reduce__ of a native type answers, built from its parts.
#pragma once

#include "func.h"
#include "obj.h"

// (items...) as a tuple. Nil with the error pending when out of memory.
Value tuple_from(const Value *items, usize n);

template <class... V>
Value tuple_of(V... v)
{
    Value items[] = { v... };
    return tuple_from(items, sizeof...(V));
}

// What pickle keeps of `self` beside its value: the instance dict of a
// subclass instance, or None when there is none or it is empty.
Value inst_state(Value self);

// (type(self), args, state), the usual answer. Nil with the error pending.
Value reduce_of(Value self, Value args, Value state);

// __reduce__ for NotImplemented, Ellipsis, builtin functions and bound methods.
bool reduce_methods();

// functools.partial(*args, **kwargs), for a reduce value that needs one.
R functools_partial(const CallArgs &a, Value &out);

// __reduce__ and __setstate__ for the builtin iterators. iter.cpp.
bool iter_pickle_methods();
