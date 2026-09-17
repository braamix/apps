// `sys`: the interpreter looking at itself.
//
// Two things here are not just tables. The three standard streams are _io's
// own objects, which a program may replace, so print asks sys_write where its
// text goes and hands back a continuation when the answer is a write() of the
// program's own; and the named tuples -- version_info, float_info, flags -- are one
// struct-sequence type repeated under different names, so `sys.version_info >=
// (3, 0)` compares against a plain tuple the way CPython's does.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "codec.h"
#include "exc.h"
#include "gc.h"
#include "import.h"
#include "info.h"
#include "intern.h"
#include "io.h"
#include "iter.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/sysabi.h"
#include "lazy.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "type.h"
#include "ustr.h"
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
    Value in, out, err; // the stream each of the three descriptors started as
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

Value fields_new(const Type *t, const Field *fs, usize n, usize shown)
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
    return info_new(t, items, names, n, shown);
}

// ---------------------------------------------------------- the three streams

// _io's own layers over descriptors 0, 1 and 2: a FileIO, a buffer that
// writes straight through, and a TextIOWrapper in UTF-8 mode.
Value std_new(Str name, i32 fd)
{
    bool writable = fd != SYS_STDIN;
    Root raw{ fileio_std(fd, writable) };
    Root rn{ str_new(name) };
    Root mode{ str_new(writable ? Str("w") : Str("r")) };
    if (raw.v.is_nil() || rn.v.is_nil() || mode.v.is_nil())
        return Value();
    StrObj *nk = str_intern("name");
    StrObj *mk = str_intern("mode");
    if (!nk || !mk || io_dict_set(raw.v, nk, rn.v) != R::Ok)
        return err_pending() ? Value() : (oom(), Value());
    Root buf{ buffered_std(raw.v, writable) };
    if (buf.v.is_nil())
        return Value();
    Root text{ textio_std(buf.v, fd) };
    if (text.v.is_nil() || io_dict_set(text.v, mk, mode.v) != R::Ok)
        return Value();
    return text.v;
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

R b_getfilesystemencodeerrors(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getfilesystemencodeerrors", 0, 0))
        return R::Err;
    out = str_new("surrogateescape");
    return out.is_nil() ? R::Err : R::Ok;
}

// There are no audit hooks here: an event is raised and nobody hears it.
R b_audit(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "audit() takes no keyword arguments");
    if (!a.nargs)
        return err_set("TypeError", "audit() missing 1 required positional argument: 'event'");
    if (!is_str(a.args[0]))
        return err_set2("TypeError", "expected str for argument 'event', not",
                        type_name(a.args[0]));
    out = value_none();
    return R::Ok;
}

R b_addaudithook(const CallArgs &a, Value &out)
{
    if (!args_only(a, "addaudithook", 1, 1))
        return R::Err;
    out = value_none();
    return R::Ok;
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
    { "getfilesystemencodeerrors", b_getfilesystemencodeerrors },
    { "audit", b_audit },
    { "addaudithook", b_addaudithook },
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
    // Reached by name only, as CPython's are.
    num("gil", 1),
    num("thread_inherit_context", 0),
    num("context_aware_warnings", 0),
    num("lazy_imports", -1),
};

constexpr usize FLAGS_SHOWN = 18;

constexpr Field IMPLEMENTATION[] = {
    text("name", "braam"),
    num("hexversion", 0x030E00F0),
    text("cache_tag", "braam-0.1"),
    text("_multiarch", "wasm32-braam"),
};

bool put_info(DictObj *into, Str name, const Type *t, const Field *fs, usize n, usize shown)
{
    Root rd{ obj_value(into) };
    Root v{ fields_new(t, fs, n, shown) };
    return !v.v.is_nil() && mod_put(static_cast<DictObj *>(rd.v.obj()), name, v.v);
}

template <usize N>
inline bool put_info(DictObj *into, Str name, const Type *t, const Field (&fs)[N])
{
    return put_info(into, name, t, fs, N, N);
}

} // namespace

void sys_set_argv(Value argv)
{
    Home *h = here();
    if (h)
        h->argv = argv;
}

bool sys_tty(i32 fd, bool &known)
{
    Home *h = here();
    known   = h && fd >= 0 && fd <= 2;
    if (!known)
        return false;
    return fd == 0 ? h->tty_in : fd == 1 ? h->tty_out : h->tty_err;
}

void sys_set_tty(bool in, bool out, bool err)
{
    Home *h = here();
    if (!h)
        return;
    h->tty_in  = in;
    h->tty_out = out;
    h->tty_err = err;
    // A stream made before this was known is told now.
    if (!h->in.is_nil())
        textio_set_line_buffering(h->in, in);
    if (!h->out.is_nil())
        textio_set_line_buffering(h->out, out);
}

namespace {

// The write() of a stream the program put in sys.stdout: one call a piece, s[1]
// being a tuple of them, and the answer is None whatever it returned.
R write_step(ContObj *k, Value in)
{
    (void)in;
    TupleObj *t = static_cast<TupleObj *>(k->s[1].obj());
    if (k->i < t->len)
        return cont_call(k, k->s[0], t->items()[k->i++]);
    return cont_done(k, value_none());
}

// s[0] a continuation whose answer is dropped.
R discard_step(ContObj *k, Value)
{
    if (k->i++ == 0)
        return cont_await(k, k->s[0]);
    return cont_done(k, value_none());
}

} // namespace

R sys_write(Value file, Str text, Value &out, Span<const usize> cuts)
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
    R r = textio_print(rf.v, text, out);
    if (r == R::Ok && !is_cont(out))
        out = Value();
    if (r == R::Ok && is_cont(out)) {
        // What the write answers is not print's answer.
        Root inner{ out };
        Root kv{ cont_new(discard_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = inner.v;
        out                 = kv.v;
    }
    if (r != R::NotImpl)
        return r;
    // Something of the program's own: its write() is Python, so the caller
    // gets a continuation and the VM makes the call. What write() answers is
    // thrown away -- print's own answer is None.
    usize parts = cuts.size() > 1 ? cuts.size() - 1 : 1;
    TupleObj *pt = tuple_new(parts);
    if (!pt)
        return oom();
    Root rt{ obj_value(pt) };
    for (usize i = 0; i < parts; i++) {
        Value piece = cuts.size() > 1 ? str_new(text.substr(cuts[i], cuts[i + 1] - cuts[i]))
                                      : str_new(text);
        if (piece.is_nil())
            return R::Err;
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = piece;
    }
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

Value sys_stream(Str name)
{
    Value m = builtin_module("sys");
    if (m.is_nil())
        return Value();
    StrObj *n = str_intern(name);
    Value got;
    if (!n || dict_get(module_dict(m), obj_value(n), got) != R::Ok)
        return Value();
    return got;
}

bool sys_install(DictObj *into)
{
    Home *h = here();
    if (!h)
        return oom() == R::Ok;
    Root rd{ obj_value(into) };
    // The streams are _io's, whose types have to be made first.
    Root io{ builtin_module("_io") };
    if (io.v.is_nil())
        return err_pending() ? false : oom() == R::Ok;
    if (h->in.is_nil()) {
        h->in  = std_new("<stdin>", SYS_STDIN);
        h->out = std_new("<stdout>", SYS_STDOUT);
        h->err = std_new("<stderr>", SYS_STDERR);
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
        !put_info(d, "flags", &flags_type, FLAGS, sizeof FLAGS / sizeof FLAGS[0], FLAGS_SHOWN) ||
        !put_info(d, "implementation", &impl_type, IMPLEMENTATION))
        return false;

    if (!mod_str(d, "version", "3.14.0 (braam)") || !mod_str(d, "platform", "braam") ||
        !mod_str(d, "byteorder", "little") || !mod_str(d, "executable", "") ||
        !mod_str(d, "prefix", "/pkg") || !mod_str(d, "exec_prefix", "/pkg") ||
        !mod_str(d, "base_prefix", "/pkg") || !mod_str(d, "base_exec_prefix", "/pkg") ||
        !mod_str(d, "platlibdir", "lib") || !mod_str(d, "float_repr_style", "short") ||
        !mod_put(d, "pycache_prefix", value_none()))
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
