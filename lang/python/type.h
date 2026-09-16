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
    Value qualname;            // StrObj, or Nil for the same as name
    Value dict;                // DictObj
    Value bases;               // TupleObj of TypeObj
    Value mro;                 // TupleObj of TypeObj, this one first
    Value native;              // the TypeObj of the built-in laid out inside, or Nil
    Value origbases;           // what `class` was written with, before __mro_entries__
    Value subs;                // ListObj of the classes made directly under this one
    const Type *desc;          // the static descriptor wrapped, null for a heap type
    const struct ExcType *exc; // the built-in exception it is or derives from
    u32 nslots;                // __slots__ over the whole MRO
    u32 slotoff;               // where an instance's slot array starts
    bool heap;                 // written by `class`, not a wrapper round a static Type
    bool meta;                 // derives from `type`: its instances are classes
    bool nodict;               // __slots__ all the way down: no instance dict
    bool hasdel;               // writes __del__: its instances are finalized
};

extern const Type type_type;

// A TypeObj carries the flag whatever metaclass it belongs to, so this is not
// a compare against one descriptor.
inline bool is_type(Value v)
{
    return v.is_obj() && (v.obj()->flags & OBJ_TYPE) != 0;
}

inline TypeObj *type_obj(Value v)
{
    return static_cast<TypeObj *>(v.obj());
}

// An instance. `native` is the built-in value a subclass of a built-in
// delegates to: `class L(list)` keeps a real list in it. A __slots__ class
// puts its slot array straight after the header; see inst_slots.
struct InstObj : Obj {
    Value cls;
    Value dict; // made on the first attribute stored, Nil under __slots__
    Value native;
};

// A heap type's descriptor names its TypeObj; a static one's does not. A class
// is an instance of its metaclass and answers that way to the type system, but
// it is not laid out like one, so it is not an InstObj.
inline bool is_inst(Value v)
{
    return v.is_obj() && v.obj()->type->owner != nullptr && !(v.obj()->flags & OBJ_TYPE);
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

// The namespace a class body left behind becomes a type. `meta` is the
// metaclass, or Nil for `type`.
Value type_new(Value name, Value bases, Value dict);
Value type_new_meta(Value meta, Value name, Value bases, Value dict);

// Fresh, with nothing in its dict. For a class that is about to be called.
Value inst_new(Value cls);

// The answer to `v.name`. `Call` means `out` is a ContObj the caller must run
// -- a property getter, a descriptor's __get__, a __getattribute__ or a
// __getattr__, all of which are Python; `Missing` means there is nothing and
// nothing to try; `Error` leaves one pending.
enum class Got : u8 { Ok, Call, Missing, Error };

Got py_attr(Value v, StrObj *name, Value &out);

// The same, for getattr(o, n, default) and hasattr: an AttributeError out of
// the call is caught and answered with `dflt`, or with False when `found`.
Got py_attr_opt(Value v, StrObj *name, Value &out, Value dflt, bool found);

// `v.name = val` and `del v.name`. R::Ok with `fn` Nil means it is done;
// otherwise `fn` is a ContObj the caller must run.
R attr_store(Value v, StrObj *name, Value val, Value &fn);
R attr_delete(Value v, StrObj *name, Value &fn);

// The same, refusing rather than suspending where a Python call is needed.
R inst_setattr(Value v, StrObj *name, Value val);
R inst_delattr(Value v, StrObj *name);

// The algorithm without the two hooks round it: object.__getattribute__,
// object.__setattr__ and object.__delattr__ are exactly these. `args` takes
// the arguments of a Got::Call, as a tuple or Nil.
Got attr_plain(Value v, StrObj *name, Value &out, Value &args);
R attr_plain_store(Value v, StrObj *name, Value val, Value &fn);
R attr_plain_delete(Value v, StrObj *name, Value &fn);

// `fn(*args)` as a ContObj, for a native that has to hand one back.
Value attr_invoke(Value fn, Value args);

// The slot array of a __slots__ instance, and how many slots it has.
Value *inst_slots(Obj *o, u32 &n);

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
    Value get, set, del, doc;
    Value pname; // what __set_name__ was told, for a diagnostic
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

// One name of a __slots__ class: an index into the instance's slot array.
struct MemberObj : Obj {
    Value name;
    Value cls; // the class that declared it
    u32 index;
};

extern const Type member_type;

// What a name found in a class namespace turns out to be. `data` says it comes
// before the instance namespace rather than after it.
enum : u8 { D_NONE, D_BIND, D_PROP, D_MEMBER, D_PY };

u8 descr_of(Value d, bool &data);

// A subclass of a built-in descriptor keeps the real one inside it, and it is
// the real one the protocol acts on.
Value descr_inner(Value d);

// super(C, obj): lookup starts after C in obj's MRO.
struct SuperObj : Obj {
    Value cls;
    Value self;
};

extern const Type super_type;

// `object` itself: what every type reaches last through its MRO, and where
// the methods every object answers -- __format__ -- are installed.
extern const Type object_type;

inline bool is_super(Value v)
{
    return v.is_obj() && v.obj()->type == &super_type;
}

// The class's own `name`, bound to `v`. Nil when `v` is not an instance or
// its class does not have one; the caller keeps `v` rooted.
Value type_special(Value v, Str name);

// A class whose metaclass a class statement made, and which may therefore
// answer an operator in Python.
inline bool is_meta_inst(Value v)
{
    return v.is_obj() && (v.obj()->flags & OBJ_TYPE) && v.obj()->type->owner != nullptr;
}

// type_special for an instance or for such a class, whose metaclass is asked.
Value operand_special(Value v, Str name);

// Whether the class has one, without binding it. A predicate that allocates
// would be wrong in a hot path.
bool type_has_special(Value v, Str name);

// The same, counting only a method written in Python. slotmeth.cpp puts a
// native for every protocol method in each built-in type's namespace, so a
// built-in subclass answers type_has_special for all of them; what the VM
// wants to know before it suspends is whether a frame has to be pushed.
bool type_has_py_special(Value v, Str name);

// `C[x]`: the metaclass's __getitem__ where it has one, else the class's own
// __class_getitem__, which is an implicit classmethod (PEP 560). Nil for
// anything that is not a class or does not answer.
Value type_getitem_of(Value v);

// The class says `__hash__ = None`, which is also what writing an __eq__ and
// no __hash__ means.
bool type_unhashable(Value v);

// The __new__ a class wrote itself, rather than the one object lends it.
Value type_own_new(Value cls);

// An immutable built-in builds itself from the call's arguments even when the
// subclass writes __init__; a mutable one is made empty and filled there.
bool type_native_takes_args(Value cls);

// The class's own `name`, unbound and without the descriptor protocol: what an
// implicit lookup is, which is how CPython reaches a special method too.
// Nil when the class has not got one, or the one it has is object's own.
Value type_hook(Value cls, Str name);

// One of the methods `object` lends every class -- __init__, __new__,
// __getattribute__, __setattr__, __delattr__, __init_subclass__. Finding one
// is finding the default and not something the class wrote.
bool is_object_default(Value v);

// The hooks a fresh class owes, as a ContObj the caller runs: __set_name__ over
// its namespace, then __init_subclass__ on its base. Nil with the error
// pending; the continuation answers the class itself.
Value type_hooks(Value cls, Value kwnames, Value kwvals);

// isinstance() itself, for a continuation to call.
R py_isinstance(const CallArgs &a, Value &out);

// object, type, the built-in types and the class builtins.
bool type_install(DictObj *into);

// What calling a built-in type runs: `list(x)` is `list.__new__(x)`.
bool type_set_ctor(const Type *t, Value fn);

// The `object` type every class ends its MRO at.
Value type_object();

// A built-in type object built by hand: exc.cpp makes its hierarchy this way.
// `base` is the base's TypeObj, a tuple of them, or Nil for object.
Value type_make_native(Str name, Value base, const Type *desc, const struct ExcType *exc);

// Allocate an object of `cls`, `bytes` long, with cls and dict already set.
Obj *type_alloc_inst(Value cls, usize bytes);

// Whether the class writes a __del__, worked out again. Called at birth and
// whenever something is stored on a heap type, since `C.__del__ = f` is legal.
void type_note_del(Value cls);
