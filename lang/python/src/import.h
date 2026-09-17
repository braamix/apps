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

// sys.meta_path, sys.path_hooks, sys.path_importer_cache and
// sys._stdlib_dir, made on first use. Nil with the error pending.
enum ImportState { IMPORT_META_PATH, IMPORT_PATH_HOOKS, IMPORT_PATH_CACHE, IMPORT_STDLIB };
Value sys_import_state(ImportState which);

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

// The absolute name `level` dots back from the module whose globals are
// `where`, with `name` on the end.
R import_absolute(Str name, i64 level, Value where, Value &out);

// `from m import name` where `m` has no such name. Always returns R::Err.
R import_missing(Value m, StrObj *name);

// `m.name` as sys.modules["<m.__name__>.<name>"], for a relative import that
// came round on itself: the submodule is loading and the package has no
// attribute for it yet. Nil, with no error, when there is no such module.
Value import_submodule(Value m, StrObj *name);
