// Lazy imports (PEP 810): the proxy a lazy import binds, and what turns it
// into the module or the name it stands for.
//
// A proxy is made at the import statement and kept in the module's
// namespace; the first time the name is read -- as a global, as a module
// attribute, by resolve() -- the import runs and the answer replaces it.
// Running an import is a continuation, so each of those is one too.
#pragma once

#include "func.h"

extern const Type lazy_type;

inline bool is_lazy(Value v)
{
    return v.is_obj() && v.obj()->type == &lazy_type;
}

// LazyImportName: the proxy for `name` imported with `fromlist` at `level`
// from `globals`, or a ContObj answering it -- a filter is Python, and one
// that says no makes the import eager.
R lazy_import(Value name, Value level, Value fromlist, Value globals, Value &out);

// Whether ImportName at a module's top level is lazy anyway: the mode is
// "all", or `globals` has a __lazy_modules__ naming the module.
bool lazy_wanted(Value name, Value level, Value globals);

// ImportFrom on a proxy: the name itself when the module is already loaded,
// or a proxy for it.
R lazy_from(Value lazy, StrObj *name, Value &out);

// A ContObj answering what `lazy` stands for, which it also stores into
// `space` under `name` where those are given.
Value lazy_reify(Value lazy, Value space, Value name);

// A module attribute that is a proxy, or a submodule a lazy import promised:
// the call that resolves it. Missing otherwise. The module type's lazyattr.
Got lazy_module_attr(Value module, StrObj *name, Value &out, Value &args);

// sys.lazy_modules and the four functions over the mode and the filter.
bool lazy_sys_install(DictObj *sys);

// builtins.__lazy_import__, and the proxy's own methods.
R b_lazy_import(const CallArgs &a, Value &out);
bool lazy_methods();
