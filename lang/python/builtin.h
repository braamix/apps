// The builtins namespace, and the one built-in module there is.
#pragma once

#include "func.h"

// Built on the first call, then the same dict for the life of the process.
// Null with the error pending.
DictObj *builtins_dict();

// `sys`, and nothing else yet: there is no import path. Nil with ImportError
// pending for any other name.
Value builtin_module(Str name);

// What sys.argv answers, set by the driver before the program starts.
void sys_set_argv(Value argv);

// Where print writes. The VM owns the buffer and decides when to flush it.
void print_sink(String *out);
