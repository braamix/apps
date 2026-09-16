// The built-in modules written in C++: the registry, and the helpers every
// installer fills a namespace with.
//
// A native module is a name and a function that fills a fresh module's dict.
// The loader asks here first -- import.cpp's begin_load -- and goes looking for
// a file only when the name is not one of these.
#pragma once

#include "func.h"

// One name in a module's namespace, and the native behind it.
struct ModDef {
    Str name; // a literal: native_new keeps the bytes
    R (*fn)(const CallArgs &, Value &out);
};

// The helpers an installer uses. Each returns false with the error pending.
bool mod_put(DictObj *into, Str name, Value v);
bool mod_int(DictObj *into, Str name, i64 v);
bool mod_str(DictObj *into, Str name, Str v);
bool mod_float(DictObj *into, Str name, f64 v);
bool mod_defs(DictObj *into, const ModDef *tab, usize n);

template <usize N>
inline bool mod_defs(DictObj *into, const ModDef (&tab)[N])
{
    return mod_defs(into, tab, N);
}

// A native type under its own name, callable when `ctor` is not null.
bool mod_type(DictObj *into, const Type *t, R (*ctor)(const CallArgs &, Value &out) = nullptr);

// ------------------------------------------------------------------- sys

// What sys.argv answers, set by the driver before the program starts.
void sys_set_argv(Value argv);

// What the driver learned about the three descriptors before the program ran,
// which is the only thing isatty() can be answered from: tty_of is a syscall
// and the interpreter under vm_burst never awaits one.
void sys_set_tty(bool in, bool out, bool err);

// Where print and a diagnostic write. `file` is Nil for sys.stdout; `out`
// takes a ContObj when the destination is an object of the program's own, and
// Nil when the text has already been buffered.
R sys_write(Value file, Str text, Value &out);

// The modules written in C++, as a tuple. sys.builtin_module_names.
Value native_module_names();

// ------------------------------------------------------------------- time

// One reading of the wall clock, taken by the driver before the program ran:
// the epoch in milliseconds, the browser's offset from UTC, and what
// proc_now() said at that instant. time.time() counts on from here, because
// Sys::Now is monotonic and cannot name a day.
void time_set_clock(u64 epoch_ms, i32 tz_min, u32 at_now);

// Each module's installer, defined beside the module it installs.
bool sys_install(DictObj *into);
bool coll_install(DictObj *into);
bool functools_install(DictObj *into);
bool itertools_install(DictObj *into);
bool operator_install(DictObj *into);
bool random_install(DictObj *into);
bool struct_install(DictObj *into);
bool array_install(DictObj *into);
bool math_install(DictObj *into);
bool cmath_install(DictObj *into);
bool time_install(DictObj *into);
bool errno_install(DictObj *into);
bool gcmod_install(DictObj *into);
bool types_install(DictObj *into);
bool typing_install(DictObj *into);
