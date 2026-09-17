// `_typing`: the type parameters PEP 695 makes, the alias the `type`
// statement makes, and Generic -- the whole of what typing.py takes from C.
//
// CPython's versions of these call back into typing.py for the parts written
// in Python: substitution, Generic's __class_getitem__ and __init_subclass__,
// unpacking a TypeVarTuple. There is no typing.py yet, so those parts are
// answered natively here, as far as a program that does not import typing
// can see; phase 27 is where typing.py arrives and they are handed to it.
#pragma once

#include "obj.h"

// TypeVar, ParamSpec and TypeVarTuple: what a generic alias substitutes.
bool is_typevar_like(Value v);

// `*Ts` as a parameter: the TypeVarTuple it unpacks, or Nil.
Value typing_unpacked(Value v);

// What `type X = ...` binds.
bool is_typealias(Value v);

// typing.Generic.
bool is_generic_class(Value v);

// The intrinsics the compiler emits for PEP 695; see code.h.
enum : u32 {
    TI_TYPEVAR,
    TI_TYPEVAR_BOUND,
    TI_TYPEVAR_CONSTRAINTS,
    TI_PARAMSPEC,
    TI_TYPEVARTUPLE,
    TI_SET_DEFAULT,
    TI_SUBSCRIPT_GENERIC,
    TI_FUNCTION_TYPE_PARAMS,
    TI_TYPEALIAS,
};

Str intrinsic_name(u32 kind);

// How many values an intrinsic takes off the stack; it pushes one.
u32 intrinsic_arity(u32 kind);

// Run one. `args` are its operands, bottom first; `module` is the __name__
// of the globals it runs in.
R typing_intrinsic(u32 kind, const Value *args, Value module, Value &out);

// The layout-free base: a class over Generic is laid out as object's.
extern const Type generic_type;

// The types' methods, from methods_install().
bool typing_methods();

// The `_typing` module.
bool typing_install(DictObj *into);
