// The exception hierarchy, and the two objects it needs to be Python-visible:
// the type that `except` matches against and the instance that carries the
// arguments.
//
// A type is a static descriptor with a base pointer, not an object with a
// dict: classes are phase 9, and until there is a metatype there is nothing a
// heap type would buy. Matching walks the base chain, so it is a few pointer
// compares and no allocation.
#pragma once

#include "obj.h"

struct ExcType {
    Str name;
    const ExcType *base; // null only for BaseException
};

// The whole hierarchy, in one table so a name lookup is a linear scan over it
// and nothing can be declared without being findable.
extern const ExcType EXC_TABLE[];
extern const usize EXC_COUNT;

// By name, or null. This is how the error channel's kind becomes an object.
const ExcType *exc_find(Str name);

// Is `t` a `base`, or something under it?
bool exc_is(const ExcType *t, const ExcType *base);

extern const Type exc_obj_type; // the handlers an exception instance uses

// An instance. The first three fields are InstObj's, in InstObj's order, so a
// user class deriving from an exception is an instance in every other respect:
// it has a class, an attribute dict and the whole descriptor protocol.
struct ExcObj : Obj {
    Value cls;    // TypeObj
    Value dict;   // instance attributes
    Value unused; // InstObj's native slot; an exception delegates to nothing
    const ExcType *t;
    Value args;    // TupleObj, always
    Value cause;   // `raise X from Y`, or Nil
    Value context; // what was being handled when this was raised, or Nil
};

// A user class deriving from one of these carries its own type, so the flag
// rather than the descriptor is what says an object is an exception.
inline bool is_exc(Value v)
{
    return v.is_obj() && (v.obj()->flags & OBJ_EXC) != 0;
}

inline const ExcType *exc_type_of(Value v)
{
    return is_exc(v) ? static_cast<ExcObj *>(v.obj())->t : nullptr;
}

// The type object for a row of the table, made on first use and kept:
// `ValueError` names the same object every time it is looked up.
Value exc_type_value(const ExcType *t);

// A fresh instance of `cls`, which is a class deriving from an exception.
Value exc_inst(Value cls, Value args);

// An instance. `args` may be Nil for none.
Value exc_new(const ExcType *t, Value args);

// The shorthand every raise from C++ wants: one string argument, or none.
Value exc_make(Str name, Str message);

// Put every name in the table into a namespace.
bool exc_install(DictObj *into);

// "ValueError: bad thing", the last line of a traceback.
bool exc_line(Value e, String &out);

struct CallArgs;

// Calling a type makes an instance: ValueError('x'). The VM dispatches here.
R exc_type_invoke(Value type, const CallArgs &a, Value &out);

// The handlers a class deriving from an exception takes for its own slots.
void exc_slots(Type &s);
