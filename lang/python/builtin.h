// The builtins namespace, and the one built-in module there is.
#pragma once

#include "func.h"

// Built on the first call, then the same dict for the life of the process.
// Null with the error pending.
DictObj *builtins_dict();

// `sys`, and nothing else written in C++ yet. Nil with no error pending for
// any other name: the loader then goes looking for a file.
Value builtin_module(Str name);

// What sys.argv answers, set by the driver before the program starts.
void sys_set_argv(Value argv);

// Where print writes. The VM owns the buffer and decides when to flush it.
void print_sink(String *out);

// A class instance's own __format__, __str__ or __repr__ as a ContObj the VM
// runs; Nil and no error when the type answers none of them in Python. The
// format opcode and format() both go through these.
Value format_special(Value v, Str spec);
Value show_special(Value v, bool want_str);

// One `{value!conv:spec}` of an f-string, which is what the FormatValue
// opcode runs. `out` takes a str, or a ContObj when the conversion or the
// __format__ is written in Python; the VM lands either.
R format_field(Value v, Str spec, u32 conv, i32 min_digits, Value &out);
