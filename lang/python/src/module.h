// The built-in modules written in C++: the registry, and the helpers every
// installer fills a namespace with.
//
// A native module is a name and a function that fills a fresh module's dict.
// The loader asks here first -- import.cpp's begin_load -- and goes looking for
// a file only when the name is not one of these.
#pragma once

#include "codec.h"
#include "func.h"
#include "kernel/vec.h"

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

// A class `module.name` deriving from the built-in exception `base`. Nil with
// the error pending.
Value mod_exc_class(Str module, Str name, Str base);

// `cls(msg)`, pending: an exception of a class a module made. Always R::Err.
R mod_raise(Value cls, Str msg);

// The octets of anything that has a buffer here: bytes, bytearray,
// memoryview and array. False for anything else, with nothing pending.
bool buffer_like(Value v, Str &out);

// `s` made exactly `n` zero octets long, collecting first when the heap has
// no room. False when out of memory, with nothing pending.
bool string_sized(String &s, usize n);

// CPython's _Py_strhex_impl: `data` in hex, a separator every `per` octets
// counted from the right, or from the left when `per` is negative. `sep` is
// Nil for none, or a str or bytes of length one. False with the error pending.
bool hex_with_sep(Str data, Value sep, i64 per, bool bytes_out, String &out);

// ------------------------------------------------------------------- sys

// What the command line and the PYTHON* variables asked for, settled by the
// driver before vm_start. sys.flags is made from it; the compiler, the loader
// and the prompt read it. The Strs view argv and the environment.
struct PyConfig {
    u32 optimize              = 0; // -O, twice for -OO
    u32 verbose               = 0; // -v
    u32 bytes_warning         = 0; // -b, twice for -bb
    bool inspect              = false;
    bool interactive          = false;
    bool quiet                = false;
    bool unbuffered           = false;
    bool no_site              = false;
    bool ignore_env           = false;
    bool isolated             = false;
    bool safe_path            = false;
    bool dev_mode             = false;
    bool import_time          = false;
    u32 warn_default_encoding = 0;
    i32 int_max_str_digits    = -1; // -1: the default, 4300
    Vec<Str> warnoptions;           // -W, after PYTHONWARNINGS
    Vec<Str> xoptions;              // -X, each "name" or "name=value"
};

// The one config, made on first use.
PyConfig &py_config();

// The builtin breakpoint(), which is sys.breakpointhook's caller.
R sys_breakpoint(const CallArgs &a, Value &out);

// What sys.argv answers, set by the driver before the program starts.
void sys_set_argv(Value argv);

// What the driver learned about the three descriptors before the program ran,
// which is the only thing isatty() can be answered from: tty_of is a syscall
// and the interpreter under vm_burst never awaits one.
void sys_set_tty(bool in, bool out, bool err);

// sys.stdout and the others as they are now: Nil when sys has no such name.
Value sys_stream(Str name);

// Whether `v` is the displayhook sys started with, so PrintExpr can do the
// work itself rather than calling back into Python for it.
bool sys_is_default_displayhook(Value v);

// sys.executable, which only the driver can know.
bool sys_set_executable(Str path);

// sys.ps1 and sys.ps2, which exist only once a prompt is being read.
bool sys_set_prompts();

// The prompt to write, ps2 for a command that is not finished.
bool sys_prompt(bool second, String &out);

// Where print and a diagnostic write. `file` is Nil for sys.stdout; `out`
// takes a ContObj when the destination is an object of the program's own, and
// Nil when the text has already been buffered. `cuts`, when given, are where
// print's pieces start and end, each written by its own call to such a write().
R sys_write(Value file, Str text, Value &out, Span<const usize> cuts = {});

// The modules written in C++, as a tuple. sys.builtin_module_names.
Value native_module_names();

// Whether `name` is one of them.
bool native_module_known(Str name);

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
bool math_integer_install(DictObj *into);
bool time_install(DictObj *into);
bool errno_install(DictObj *into);
bool gcmod_install(DictObj *into);
bool types_install(DictObj *into);
bool typing_install(DictObj *into);
bool ast_install(DictObj *into);
bool dismod_install(DictObj *into);

// sys.set_asyncgen_hooks's firstiter, or Nil. gen.cpp calls it (PEP 525).
Value sys_asyncgen_firstiter();
bool unicodedata_install(DictObj *into);
bool thread_install(DictObj *into);
bool contextvars_install(DictObj *into);
bool string_install(DictObj *into);
bool warnings_install(DictObj *into);

// A ContObj that calls warnings.warn(message, category, stacklevel) and
// answers None. Nil with the error pending.
Value warn_cont(Str category, Str message, u32 stacklevel);

// A builtin whose answer is `answer` but which warns first: `out` takes the
// continuation that does both. R::Err with the error pending.
R warn_then(Str category, Str message, u32 stacklevel, Value answer, Value &out);

// warnings.warn_explicit(message, category, filename, lineno), as warn_cont.
Value warn_explicit_cont(Str category, Str message, Str filename, u32 lineno);

// `warning`, a ContObj warn_cont made, then `answer`.
R warn_then_cont(Value warning, Value answer, Value &out);
bool atexit_install(DictObj *into);
bool sre_install(DictObj *into);
bool posix_install(DictObj *into);
bool signal_install(DictObj *into);
bool posixsubprocess_install(DictObj *into);
bool select_install(DictObj *into);
bool pickle_install(DictObj *into);
bool io_install(DictObj *into);
bool csv_install(DictObj *into);
bool pyexpat_install(DictObj *into);
bool binascii_install(DictObj *into);

bool zlib_install(DictObj *into);

bool bz2_install(DictObj *into);

bool lzma_install(DictObj *into);

bool zstd_install(DictObj *into);
bool md5_install(DictObj *into);
bool sha1_install(DictObj *into);
bool sha2_install(DictObj *into);
bool sha3_install(DictObj *into);
bool blake2_install(DictObj *into);
bool tokenize_install(DictObj *into);
bool marshal_install(DictObj *into);
bool imp_install(DictObj *into);
bool colorize_install(DictObj *into);
bool faulthandler_install(DictObj *into);
