// `sys`: the interpreter looking at itself.
//
// Two things here are not just tables. The three standard streams are real
// objects a program may replace, so print asks sys_write where its text goes
// and hands back a continuation when the answer is a write() of the program's
// own; and the named tuples -- version_info, float_info, flags -- are one
// struct-sequence type repeated under different names, so `sys.version_info >=
// (3, 0)` compares against a plain tuple the way CPython's does.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "gc.h"
#include "import.h"
#include "info.h"
#include "intern.h"
#include "iter.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/sysabi.h"
#include "lazy.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// argv and the three streams outlive every operation, so they live in one
// block the collector is told about.
struct Home {
    Value argv;
    Value in, out, err; // the StdObj each of the three descriptors started as
    bool tty_in, tty_out, tty_err;
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    gc_mark(home->argv);
    gc_mark(home->in);
    gc_mark(home->out);
    gc_mark(home->err);
}

Home *here()
{
    if (!home) {
        home = heap_new<Home>();
        if (home)
            gc_root_hook(home_mark);
    }
    return home;
}

// -------------------------------------------------------- the named tuples

INFO_TYPE(version_info_type, "sys.version_info");
INFO_TYPE(float_info_type, "sys.float_info");
INFO_TYPE(int_info_type, "sys.int_info");
INFO_TYPE(hash_info_type, "sys.hash_info");
INFO_TYPE(flags_type, "sys.flags");
INFO_TYPE(impl_type, "sys.implementation");

// A field of a struct sequence, before the object is built.
struct Field {
    Str name;
    i64 n;    // used when `text` is empty and `f` is not asked for
    Str text; // a str field
    f64 f;
    bool is_float;
};

constexpr Field num(Str name, i64 n)
{
    return Field{ name, n, Str(), 0, false };
}

constexpr Field text(Str name, Str s)
{
    return Field{ name, 0, s, 0, false };
}

constexpr Field real(Str name, f64 x)
{
    return Field{ name, 0, Str(), x, true };
}

// At most as many fields as the widest table below; a Vec would be a heap
// block for something that lives one call.
constexpr usize MAX_FIELDS = 24;

Value fields_new(const Type *t, const Field *fs, usize n)
{
    if (n > MAX_FIELDS)
        return err_set("SystemError", "too many fields"), Value();
    Value items[MAX_FIELDS];
    Str names[MAX_FIELDS];
    Roots pin{ items, n };
    for (usize i = 0; i < n; i++) {
        items[i] = fs[i].is_float       ? float_new(fs[i].f)
                   : fs[i].text.empty() ? int_from_i64(fs[i].n)
                                        : str_new(fs[i].text);
        if (items[i].is_nil())
            return Value();
        names[i] = fs[i].name;
    }
    return info_new(t, items, names, n);
}

// ---------------------------------------------------------- the three streams

// One of sys.stdin, sys.stdout and sys.stderr. There is no io layer until
// phase 21, so a write goes straight into the buffer the VM flushes and a read
// is refused: nothing here can park on a descriptor.
struct StdObj : Obj {
    Value name; // "<stdout>"
    i32 fd;
    bool writable;
};

StdObj *std_of(Value v)
{
    return static_cast<StdObj *>(v.obj());
}

void std_trace(Obj *o)
{
    gc_mark(static_cast<StdObj *>(o)->name);
}

R std_repr(Value v, String &out)
{
    Buf<96> b;
    b.put("<_io.TextIOWrapper name='").put(str_of(std_of(v)->name)->str());
    b.put("' mode='").put(std_of(v)->writable ? Str("w") : Str("r")).put("' encoding='utf-8'>");
    return out.append(b.str()) ? R::Ok : oom();
}

extern const Type std_type;

bool is_std(Value v)
{
    return v.is_obj() && v.obj()->type == &std_type;
}

bool std_tty(const StdObj *s)
{
    Home *h = here();
    if (!h)
        return false;
    return s->fd == SYS_STDIN ? h->tty_in : s->fd == SYS_STDOUT ? h->tty_out : h->tty_err;
}

String *buffer_for(const StdObj *s)
{
    return s->fd == SYS_STDERR ? vm_errout() : vm_out();
}

R std_write(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "write", 1, 1))
        return R::Err;
    StdObj *s = std_of(method_self(a.args[0]));
    if (!s->writable)
        return err_set("OSError", "not writable");
    if (!is_str(a.args[1]))
        return err_set2("TypeError", "write() argument must be str", type_name(a.args[1]));
    String *sink = buffer_for(s);
    StrObj *text = str_of(a.args[1]);
    if (sink && !sink->append(text->str()))
        return oom();
    out = Value::of_int(i32(text->chars));
    return R::Ok;
}

R std_writelines(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "writelines", 1, 1))
        return R::Err;
    StdObj *s = std_of(method_self(a.args[0]));
    if (!s->writable)
        return err_set("OSError", "not writable");
    Root it{ py_iter(a.args[1]) };
    if (it.v.is_nil())
        return R::Err;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl)
            break;
        if (!is_str(got.v))
            return err_set2("TypeError", "writelines() wants str", type_name(got.v));
        String *sink = buffer_for(std_of(method_self(a.args[0])));
        if (sink && !sink->append(str_of(got.v)->str()))
            return oom();
    }
    out = value_none();
    return R::Ok;
}

R std_flush(const CallArgs &a, Value &out)
{
    // The VM decides when a buffer goes out; there is nothing to force here.
    if (!meth_args(a, "flush", 0, 0))
        return R::Err;
    out = value_none();
    return R::Ok;
}

R std_isatty(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "isatty", 0, 0))
        return R::Err;
    out = value_bool(std_tty(std_of(method_self(a.args[0]))));
    return R::Ok;
}

R std_fileno(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "fileno", 0, 0))
        return R::Err;
    out = Value::of_int(std_of(method_self(a.args[0]))->fd);
    return R::Ok;
}

R std_readable(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "readable", 0, 0))
        return R::Err;
    out = value_bool(!std_of(method_self(a.args[0]))->writable);
    return R::Ok;
}

R std_writable(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "writable", 0, 0))
        return R::Err;
    out = value_bool(std_of(method_self(a.args[0]))->writable);
    return R::Ok;
}

R std_seekable(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "seekable", 0, 0))
        return R::Err;
    out = value_bool(false);
    return R::Ok;
}

R std_close(const CallArgs &a, Value &out)
{
    // Closing a standard stream is a no-op here: the descriptor is the
    // process's own and the driver owns it.
    if (!meth_args(a, "close", 0, 0))
        return R::Err;
    out = value_none();
    return R::Ok;
}

// Reading a descriptor needs a Req the VM has not got -- ReqKind::Read names a
// path, not an fd -- so stdin refuses rather than pretending to be empty.
// Phase 21 is where it becomes a file.
R std_read(const CallArgs &a, Value &out)
{
    (void)a;
    (void)out;
    return err_set("OSError", "reading sys.stdin needs the io layer");
}

R std_getattr(Value v, StrObj *name, Value &out)
{
    StdObj *s = std_of(v);
    Str n     = name->str();
    if (n == "name")
        out = s->name;
    else if (n == "mode")
        out = str_new(s->writable ? Str("w") : Str("r"));
    else if (n == "encoding")
        out = str_new("utf-8");
    else if (n == "errors")
        out = str_new("strict");
    else if (n == "newlines")
        out = value_none();
    else if (n == "closed")
        out = value_bool(false);
    else if (n == "line_buffering")
        out = value_bool(std_tty(s));
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method STD_METHODS[] = {
    { "write", std_write },       { "writelines", std_writelines }, { "flush", std_flush },
    { "isatty", std_isatty },     { "fileno", std_fileno },         { "readable", std_readable },
    { "writable", std_writable }, { "seekable", std_seekable },     { "close", std_close },
    { "read", std_read },         { "readline", std_read },         { "readlines", std_read },
};

constexpr Type std_type{ .name    = "TextIOWrapper",
                         .trace   = std_trace,
                         .repr    = std_repr,
                         .getattr = std_getattr };

Value std_new(Str name, i32 fd, bool writable)
{
    Root rn{ str_new(name) };
    if (rn.v.is_nil())
        return Value();
    StdObj *o = static_cast<StdObj *>(obj_alloc(&std_type, sizeof(StdObj)));
    if (!o)
        return oom(), Value();
    o->name     = rn.v;
    o->fd       = fd;
    o->writable = writable;
    return obj_value(o);
}

// ------------------------------------------------------------- the functions

R b_exit(const CallArgs &a, Value &out)
{
    if (!args_only(a, "exit", 0, 1))
        return R::Err;
    TupleObj *args = tuple_new(a.nargs);
    if (!args)
        return oom();
    for (u32 i = 0; i < a.nargs; i++)
        args->items()[i] = a.args[i];
    Value e = exc_new(exc_find("SystemExit"), obj_value(args));
    if (e.is_nil())
        return R::Err;
    out = Value();
    return err_set_value(e);
}

// The exception an `except` clause is working on, as CPython's triple.
// sys.exception(): what is being handled, or None.
R b_exception(const CallArgs &a, Value &out)
{
    if (!args_only(a, "exception", 0, 0))
        return R::Err;
    Value e = vm_handling();
    out     = e.is_nil() ? value_none() : e;
    return R::Ok;
}

R b_exc_info(const CallArgs &a, Value &out)
{
    if (!args_only(a, "exc_info", 0, 0))
        return R::Err;
    Root e{ vm_handling() };
    TupleObj *t = tuple_new(3);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    Value cls = value_none(), tb = value_none();
    if (!e.v.is_nil()) {
        cls = type_of_value(e.v);
        if (cls.is_nil())
            return R::Err;
    }
    TupleObj *o   = static_cast<TupleObj *>(rt.v.obj());
    o->items()[0] = cls;
    o->items()[1] = e.v.is_nil() ? value_none() : e.v;
    o->items()[2] = tb;
    out           = rt.v;
    return R::Ok;
}

// The bytes an object holds, header and payload, as the allocator knows them.
// A static singleton and a small int never went through the heap, so they are
// reported as the header alone.
R b_getsizeof(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getsizeof", 1, 2))
        return R::Err;
    Value v = a.args[0];
    usize n = sizeof(Obj);
    if (v.is_obj() && !(v.obj()->flags & OBJ_IMMORTAL))
        n = heap_usable_size(v.obj());
    out = int_from_i64(i64(n));
    return out.is_nil() ? R::Err : R::Ok;
}

R b_getrecursionlimit(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getrecursionlimit", 0, 0))
        return R::Err;
    out = Value::of_int(i32(vm_recursion_limit()));
    return R::Ok;
}

R b_setrecursionlimit(const CallArgs &a, Value &out)
{
    if (!args_only(a, "setrecursionlimit", 1, 1))
        return R::Err;
    i64 n = 0;
    if (!as_index(a.args[0], n))
        return err_set("TypeError", "an integer is required");
    if (n < 1)
        return err_set("ValueError", "recursion limit must be positive");
    // The frames are heap blocks, so a very deep limit costs memory rather
    // than the 128 KiB native stack; it is still bounded.
    vm_set_recursion_limit(u32(n > 100000 ? 100000 : n));
    out = value_none();
    return R::Ok;
}

R b_intern(const CallArgs &a, Value &out)
{
    if (!args_only(a, "intern", 1, 1))
        return R::Err;
    if (!is_str(a.args[0]))
        return err_set("TypeError", "intern() argument must be str");
    StrObj *s = str_intern(str_of(a.args[0])->str());
    if (!s)
        return oom();
    out = obj_value(s);
    return R::Ok;
}

R b_getframe(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_getframe", 0, 1))
        return R::Err;
    i64 depth = 0;
    if (a.nargs && !as_index(a.args[0], depth))
        return err_set("TypeError", "an integer is required");
    ListObj *fs = vm_frames();
    if (!fs)
        return R::Err;
    if (depth < 0 || usize(depth) >= fs->items.size())
        return err_set("ValueError", "call stack is not deep enough");
    out = fs->items[usize(depth)];
    return R::Ok;
}

// There is no reference count here -- the collector is mark and sweep -- so
// this is the one number that is always true of a reachable object.
R b_getrefcount(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getrefcount", 1, 1))
        return R::Err;
    out = Value::of_int(1);
    return R::Ok;
}

R b_getdefaultencoding(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getdefaultencoding", 0, 0))
        return R::Err;
    out = str_new("utf-8");
    return out.is_nil() ? R::Err : R::Ok;
}

R b_getfilesystemencoding(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getfilesystemencoding", 0, 0))
        return R::Err;
    out = str_new("utf-8");
    return out.is_nil() ? R::Err : R::Ok;
}

R b_is_finalizing(const CallArgs &a, Value &out)
{
    if (!args_only(a, "is_finalizing", 0, 0))
        return R::Err;
    out = value_bool(false);
    return R::Ok;
}

constexpr ModDef SYS_DEFS[] = {
    { "exit", b_exit },
    { "exc_info", b_exc_info },
    { "exception", b_exception },
    { "getsizeof", b_getsizeof },
    { "getrecursionlimit", b_getrecursionlimit },
    { "setrecursionlimit", b_setrecursionlimit },
    { "intern", b_intern },
    { "_getframe", b_getframe },
    { "getrefcount", b_getrefcount },
    { "getdefaultencoding", b_getdefaultencoding },
    { "getfilesystemencoding", b_getfilesystemencoding },
    { "is_finalizing", b_is_finalizing },
};

// The IEEE double this interpreter's float is, field for field.
constexpr Field FLOAT_INFO[] = {
    real("max", 1.7976931348623157e308),
    num("max_exp", 1024),
    num("max_10_exp", 308),
    real("min", 2.2250738585072014e-308),
    num("min_exp", -1021),
    num("min_10_exp", -307),
    num("dig", 15),
    num("mant_dig", 53),
    real("epsilon", 2.220446049250313e-16),
    num("radix", 2),
    num("rounds", 1),
};

// bigint.h's limbs, which are 32 bits with the sign beside them.
constexpr Field INT_INFO[] = {
    num("bits_per_digit", 32),
    num("sizeof_digit", 4),
    num("default_max_str_digits", 4300),
    num("str_digits_check_threshold", 640),
};

constexpr Field HASH_INFO[] = {
    num("width", 32),     num("modulus", 2147483647), num("inf", 314159),
    num("nan", 0),        num("imag", 1000003),       text("algorithm", "fnv"),
    num("hash_bits", 32), num("seed_bits", 0),        num("cutoff", 0),
};

constexpr Field VERSION_INFO[] = {
    num("major", 3),  num("minor", 14), num("micro", 0), text("releaselevel", "final"),
    num("serial", 0),
};

// None of these can be set from the command line yet; they are here because
// the library and CPython's tests read them.
constexpr Field FLAGS[] = {
    num("debug", 0),
    num("inspect", 0),
    num("interactive", 0),
    num("optimize", 0),
    num("dont_write_bytecode", 1),
    num("no_user_site", 1),
    num("no_site", 1),
    num("ignore_environment", 0),
    num("verbose", 0),
    num("bytes_warning", 0),
    num("quiet", 0),
    num("hash_randomization", 0),
    num("isolated", 0),
    num("dev_mode", 0),
    num("utf8_mode", 1),
    num("warn_default_encoding", 0),
    num("safe_path", 0),
    num("int_max_str_digits", -1),
};

constexpr Field IMPLEMENTATION[] = {
    text("name", "braam"),
    num("hexversion", 0x030E00F0),
    text("cache_tag", "braam-0.1"),
    text("_multiarch", "wasm32-braam"),
};

bool put_info(DictObj *into, Str name, const Type *t, const Field *fs, usize n)
{
    Root rd{ obj_value(into) };
    Root v{ fields_new(t, fs, n) };
    return !v.v.is_nil() && mod_put(static_cast<DictObj *>(rd.v.obj()), name, v.v);
}

template <usize N>
inline bool put_info(DictObj *into, Str name, const Type *t, const Field (&fs)[N])
{
    return put_info(into, name, t, fs, N);
}

} // namespace

void sys_set_argv(Value argv)
{
    Home *h = here();
    if (h)
        h->argv = argv;
}

void sys_set_tty(bool in, bool out, bool err)
{
    Home *h = here();
    if (!h)
        return;
    h->tty_in  = in;
    h->tty_out = out;
    h->tty_err = err;
}

namespace {

// The write() of a stream the program put in sys.stdout: one call, and the
// answer is None whatever it returned.
R write_step(ContObj *k, Value in)
{
    (void)in;
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->s[1]);
    return cont_done(k, value_none());
}

} // namespace

R sys_write(Value file, Str text, Value &out)
{
    out = Value();
    Root rf{ file };
    // No file named: whatever sys.stdout is now, which a program may have
    // replaced with an object of its own.
    if (rf.v.is_nil() || is_none(rf.v)) {
        Value m = builtin_module("sys");
        if (m.is_nil())
            return err_pending() ? R::Err : R::Ok;
        Root rm{ m };
        StrObj *n = str_intern("stdout");
        if (!n)
            return oom();
        Value got;
        R r = dict_get(module_dict(rm.v), obj_value(n), got);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl || is_none(got))
            return R::Ok; // sys.stdout is None: print writes nowhere
        rf = got;
    }
    if (is_std(rf.v)) {
        String *sink = buffer_for(std_of(rf.v));
        return sink && !sink->append(text) ? oom() : R::Ok;
    }
    // Something of the program's own: its write() is Python, so the caller
    // gets a continuation and the VM makes the call. What write() answers is
    // thrown away -- print's own answer is None.
    Root rt{ str_new(text) };
    if (rt.v.is_nil())
        return R::Err;
    StrObj *w = str_intern("write");
    Root fn;
    if (!w || py_getattr(rf.v, w, fn.v) != R::Ok)
        return err_pending() ? R::Err : err_set("AttributeError", "write");
    Root kv{ cont_new(write_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = fn.v;
    cont_of(kv.v)->s[1] = rt.v;
    out                 = kv.v;
    return R::Ok;
}

bool sys_install(DictObj *into)
{
    Home *h = here();
    if (!h)
        return oom() == R::Ok;
    Root rd{ obj_value(into) };
    if (!method_install(&std_type, STD_METHODS))
        return false;
    if (h->in.is_nil()) {
        h->in  = std_new("<stdin>", SYS_STDIN, false);
        h->out = std_new("<stdout>", SYS_STDOUT, true);
        h->err = std_new("<stderr>", SYS_STDERR, true);
        if (h->in.is_nil() || h->out.is_nil() || h->err.is_nil())
            return false;
    }
    DictObj *d = static_cast<DictObj *>(rd.v.obj());

    if (!mod_defs(d, SYS_DEFS))
        return false;
    // Both names, because a program that replaces sys.stdout is expected to be
    // able to put the original back.
    struct Stream {
        Str name, orig;
        Value Home::*at;
    };
    static const Stream STREAMS[] = { { "stdin", "__stdin__", &Home::in },
                                      { "stdout", "__stdout__", &Home::out },
                                      { "stderr", "__stderr__", &Home::err } };
    for (const Stream &s : STREAMS)
        if (!mod_put(d, s.name, here()->*s.at) || !mod_put(d, s.orig, here()->*s.at))
            return false;

    if (!put_info(d, "version_info", &version_info_type, VERSION_INFO) ||
        !put_info(d, "float_info", &float_info_type, FLOAT_INFO) ||
        !put_info(d, "int_info", &int_info_type, INT_INFO) ||
        !put_info(d, "hash_info", &hash_info_type, HASH_INFO) ||
        !put_info(d, "flags", &flags_type, FLAGS) ||
        !put_info(d, "implementation", &impl_type, IMPLEMENTATION))
        return false;

    if (!mod_str(d, "version", "3.14.0 (braam)") || !mod_str(d, "platform", "braam") ||
        !mod_str(d, "byteorder", "little") || !mod_str(d, "executable", "") ||
        !mod_str(d, "prefix", "/pkg") || !mod_str(d, "exec_prefix", "/pkg"))
        return false;
    if (!mod_int(d, "maxsize", 2147483647) || !mod_int(d, "maxunicode", 1114111) ||
        !mod_int(d, "hexversion", 0x030E00F0))
        return false;
    if (!mod_put(d, "dont_write_bytecode", value_bool(true)))
        return false;

    ListObj *warn = list_new();
    if (!warn || !mod_put(d, "warnoptions", obj_value(warn)))
        return false;
    Root names{ native_module_names() };
    if (names.v.is_nil() || !mod_put(d, "builtin_module_names", names.v))
        return false;

    // The cache and the search path are the loader's, and a program reads and
    // writes both through here.
    DictObj *sm = sys_modules();
    Root sp{ sys_path() };
    if (!sm || sp.v.is_nil() || !mod_put(d, "modules", obj_value(sm)) || !mod_put(d, "path", sp.v))
        return false;
    if (!lazy_sys_install(d))
        return false;
    return mod_put(d, "argv", here()->argv.is_nil() ? value_none() : here()->argv);
}
