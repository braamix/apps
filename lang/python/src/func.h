// What a call can land on: a Python function, a builtin written in C++, or a
// module (which is not callable, but lives here because nothing else wants it).
#pragma once

#include "code.h"

// A closed-over local. Nil means the name is not bound yet.
struct CellObj : Obj {
    Value v;
};

extern const Type cell_type;
CellObj *cell_new();

// The three names come from the code object and are then the function's own:
// __name__ and __qualname__ may be assigned, and a decorator that wraps one
// function in another copies them across.
struct FuncObj : Obj {
    Value code;       // CodeObj
    Value globals;    // DictObj
    Value defaults;   // TupleObj, or Nil
    Value kwdefaults; // DictObj, or Nil
    Value closure;    // TupleObj of CellObj, or Nil
    Value name;       // StrObj
    Value qualname;   // StrObj
    Value doc;        // the body's first string, or Nil
    Value dict;       // __dict__, made when something is stored in it
    Value annotate;   // PEP 649's __annotate__, or Nil
    Value annotations; // what it last answered, or Nil
};

extern const Type func_type;
Value func_new(Value code, Value globals);

inline bool is_func(Value v)
{
    return v.is_obj() && v.obj()->type == &func_type;
}

inline FuncObj *func_of(Value v)
{
    return static_cast<FuncObj *>(v.obj());
}

// The positional and keyword arguments of one call, as they sit on the value
// stack: `kwvals` follows `args`, and `kwnames` is the tuple the call named.
struct CallArgs {
    const Value *args    = nullptr;
    u32 nargs            = 0;
    const Value *kwvals  = nullptr;
    const Value *kwnames = nullptr; // StrObj values
    u32 nkw              = 0;
};

struct NativeObj : Obj {
    Str name; // a literal: the bytes outlive the object
    R (*fn)(const CallArgs &, Value &out);
    Value owner; // the type it is a method of, its module's name, or Nil
};

extern const Type native_type;

// `name` must outlive the object, so pass a literal.
Value native_new(Str name, R (*fn)(const CallArgs &, Value &out));

inline bool is_native(Value v)
{
    return v.is_obj() && v.obj()->type == &native_type;
}

struct ModuleObj : Obj {
    Value name; // StrObj
    Value dict; // DictObj
};

extern const Type module_type;
// __doc__, __package__, __loader__ and __spec__ as None, where `d` has not
// got them. False with the error pending.
bool module_defaults(DictObj *d);

Value module_new(Str name);

inline bool is_module(Value v)
{
    return v.is_obj() && v.obj()->type == &module_type;
}

inline DictObj *module_dict(Value v)
{
    return static_cast<DictObj *>(static_cast<ModuleObj *>(v.obj())->dict.obj());
}

// A helper every builtin needs: refuse keywords it does not take, and check
// the count. False leaves a TypeError pending.
bool args_only(const CallArgs &a, Str who, u32 least, u32 most);
