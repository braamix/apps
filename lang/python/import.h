// import: the module cache, the search path, and the loader.
//
// Loading a module reads a file and runs its body. Neither fits inside one
// opcode, so `__import__` is a continuation: cont_read asks the driver for the
// file, cont_call asks the VM to run the body.
#pragma once

#include "func.h"

// sys.modules, made on first use. Null with the error pending.
DictObj *sys_modules();

// sys.path, made on first use. Nil with the error pending.
Value sys_path();

// What goes on sys.path before the program starts: the directory the program
// came from, then the one the shipped library lives in. Either may be empty.
void sys_set_path(Str script_dir, Str library_dir);

// Put `m` in sys.modules under `name`. False with the error pending.
bool module_register(Str name, Value m);

// __import__(name, level, fromlist, globals), in the order the opcode has
// them. Answers a ContObj, never a module; see the note at the top. `globals`
// is what a relative import counts back from.
R py_import(const CallArgs &a, Value &out);

// The same, spelled the way a program calls it:
// __import__(name, globals, locals, fromlist, level).
R b_import(const CallArgs &a, Value &out);

// `from m import *`: every public name of `m` into `into`.
R import_star(Value m, DictObj *into);

// `from m import name` where `m` has no such name. Always returns R::Err.
R import_missing(Value m, StrObj *name);
