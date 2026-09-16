// The builtins namespace, and the one built-in module there is.
#pragma once

#include "func.h"

// Built on the first call, then the same dict for the life of the process.
// Null with the error pending.
DictObj *builtins_dict();

// The builtins namespace as a module, so `import builtins` finds it.
Value builtins_module();

// A module written in C++, built on the first import and cached. Nil with no
// error pending for a name that is not one: the loader then goes looking for a
// file. module.cpp holds the registry.
Value builtin_module(Str name);

// Where print writes. The VM owns the buffer and decides when to flush it.
void print_sink(String *out);

// `__builtins__` in a namespace, which is where a program looks to see what it
// has. Every module's globals gets one, and so does a dict exec() is handed.
bool put_builtins(DictObj *into);

// What PrintExpr runs. A value's repr is printed, unless it is None. `out`
// takes a ContObj when the repr is written in Python, and Nil when there was
// nothing to do; the VM lands it.
R py_display(Value v, Value &out);

// A class instance's own __format__, __str__ or __repr__ as a ContObj the VM
// runs; Nil and no error when the type answers none of them in Python. The
// format opcode and format() both go through these.
Value format_special(Value v, Str spec);
Value show_special(Value v, bool want_str);

// One `{value!conv:spec}` of an f-string, which is what the FormatValue
// opcode runs. `out` takes a str, or a ContObj when the conversion or the
// __format__ is written in Python; the VM lands either.
R format_field(Value v, Str spec, u32 conv, i32 min_digits, Value &out);

// callable(v).
bool py_callable(Value v);
