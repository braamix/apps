// Classes: the type object, the instance, and what a name on either resolves
// to.
//
// One shape for both halves: a built-in type is a TypeObj wrapping the static
// Type its instances already point at, a `class` is a TypeObj carrying a Type
// of its own. A slot cannot call Python -- ground rule 2 -- so a special method
// written in Python is not in the slot table; type_lookup finds it and the VM
// makes the call.
#pragma once

#include "func.h"

struct TypeObj : Obj {
    Type slots;                // what an instance of this type points at
    Value name;                // StrObj
    Value dict;                // DictObj
    Value bases;               // TupleObj of TypeObj
    Value mro;                 // TupleObj of TypeObj, this one first
    Value native;              // the TypeObj of the built-in laid out inside, or Nil
    const Type *desc;          // the static descriptor wrapped, null for a heap type
    const struct ExcType *exc; // the built-in exception it is or derives from
    bool heap;                 // written by `class`, not a wrapper round a static Type
};

extern const Type type_type;

inline bool is_type(Value v)
{
    return v.is_obj() && v.obj()->type == &type_type;
}

inline TypeObj *type_obj(Value v)
{
    return static_cast<TypeObj *>(v.obj());
}

// An instance. `native` is the built-in value a subclass of a built-in
// delegates to: `class L(list)` keeps a real list in it.
struct InstObj : Obj {
    Value cls;
    Value dict; // made on the first attribute stored
    Value native;
};

// A heap type's descriptor names its TypeObj; a static one's does not.
inline bool is_inst(Value v)
{
    return v.is_obj() && v.obj()->type->owner != nullptr;
}

inline InstObj *inst_of(Value v)
{
    return static_cast<InstObj *>(v.obj());
}

// The type of a value as an object. Nil with the error pending.
Value type_of_value(Value v);

// The TypeObj wrapping a static descriptor, made on first use.
Value type_wrap(const Type *t);

// `name` on `t` or its bases; `owner` takes the class it was found on.
R type_lookup(Value t, StrObj *name, Value &out, Value *owner = nullptr);

bool type_issub(Value t, Value base);

// A type that can appear after `except`.
inline bool is_exc_type(Value v)
{
    return is_type(v) && type_obj(v)->exc != nullptr;
}
bool type_isinstance(Value v, Value t);

// The namespace a class body left behind becomes a type.
Value type_new(Value name, Value bases, Value dict);

// Fresh, with nothing in its dict. For a class that is about to be called.
Value inst_new(Value cls);

// The answer to `v.name`. `Call` means `out` is a getter the VM must run with
// no arguments; `Missing` means nothing was found and `out` takes the class's
// __getattr__, if any; `Error` leaves one pending.
enum class Got : u8 { Ok, Call, Missing, Error };

Got py_attr(Value v, StrObj *name, Value &out);

// A property found on the class of `v`, or Nil. `which` is 0 get, 1 set,
// 2 delete; the VM runs what comes back.
Value type_property(Value v, StrObj *name, u32 which);

R inst_setattr(Value v, StrObj *name, Value val);
R inst_delattr(Value v, StrObj *name);

struct MethodObj : Obj {
    Value fn;
    Value self;
};

extern const Type method_type;

inline bool is_method(Value v)
{
    return v.is_obj() && v.obj()->type == &method_type;
}

Value method_new(Value fn, Value self);

// A name found in a class dict, resolved against what it was reached through.
// `self` is Nil for a lookup on the class itself.
R type_bind(Value found, Value self, Value cls, Value &out);

struct PropObj : Obj {
    Value get, set, del;
};

extern const Type property_type;
extern const Type staticmethod_type;
extern const Type classmethod_type;

inline bool is_property(Value v)
{
    return v.is_obj() && v.obj()->type == &property_type;
}

// What a staticmethod or a classmethod holds.
struct WrapObj : Obj {
    Value fn;
};

// super(C, obj): lookup starts after C in obj's MRO.
struct SuperObj : Obj {
    Value cls;
    Value self;
};

extern const Type super_type;

inline bool is_super(Value v)
{
    return v.is_obj() && v.obj()->type == &super_type;
}

R super_getattr(Value v, StrObj *name, Value &out);

// The class's own `name`, bound to `v`. Nil when `v` is not an instance or
// its class does not have one; the caller keeps `v` rooted.
Value type_special(Value v, Str name);

// The __new__ a class wrote itself, rather than the one object lends it.
Value type_own_new(Value cls);

// An immutable built-in builds itself from the call's arguments even when the
// subclass writes __init__; a mutable one is made empty and filled there.
bool type_native_takes_args(Value cls);

// object, type, the built-in types and the class builtins.
bool type_install(DictObj *into);

// What calling a built-in type runs: `list(x)` is `list.__new__(x)`.
bool type_set_ctor(const Type *t, Value fn);

// The `object` type every class ends its MRO at.
Value type_object();

// A built-in type object built by hand: exc.cpp makes its hierarchy this way.
// `base` is the base's TypeObj, or Nil for object.
Value type_make_native(Str name, Value base, const Type *desc, const struct ExcType *exc);

// Allocate an object of `cls`, `bytes` long, with cls and dict already set.
Obj *type_alloc_inst(Value cls, usize bytes);
