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
#include "compile.h"
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
#include "monitor.h"
#include "ops.h"
#include "parse.h"
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
    Value firstiter;    // set_asyncgen_hooks: called when one is first stepped
    Value finalizer;    // and when one is dropped unfinished
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
    gc_mark(home->firstiter);
    gc_mark(home->finalizer);
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

// A field of a struct sequence, before the object is built.
struct Field {
    Str name;
    i64 n;    // used when `text` is empty and `f` is not asked for
    Str text; // a str field
    f64 f;
    bool is_float;
    bool is_bool = false;
};

constexpr Field num(Str name, i64 n)
{
    return Field{ name, n, Str(), 0, false };
}

constexpr Field flag(Str name, bool b)
{
    return Field{ name, b ? 1 : 0, Str(), 0, false, true };
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
                   : fs[i].is_bool      ? value_bool(fs[i].n != 0)
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

// excepthook(type, value, traceback): the report an uncaught exception gets.
R b_excepthook(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs != 3) {
        char t[16];
        Buf<96> b;
        b.put("excepthook expected 3 arguments, got ").put(int_text(t, sizeof t, a.nargs));
        return err_set("TypeError", b.str());
    }
    Value e = a.args[1];
    if (is_exc(e)) {
        Root re{ e };
        if (is_traceback(a.args[2]))
            static_cast<ExcObj *>(re.v.obj())->tb = a.args[2];
        // A sys.stderr of the program's own gets the report as one write.
        Value now = sys_stream("stderr");
        if (!now.is_nil() && !is_none(now) && now != sys_stream("__stderr__")) {
            String text;
            if (!vm_report_text(re.v, text))
                return oom();
            if (sys_write(now, text.str(), out) != R::Ok)
                return R::Err;
            if (out.is_nil())
                out = value_none();
            return R::Ok;
        }
        vm_report(re.v);
    } else {
        return err_set2("TypeError", "print_exception(): Exception expected for value",
                        type_name(e));
    }
    out = value_none();
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
        if (is_exc(e.v) && !static_cast<ExcObj *>(e.v.obj())->tb.is_nil())
            tb = static_cast<ExcObj *>(e.v.obj())->tb;
    }
    TupleObj *o   = static_cast<TupleObj *>(rt.v.obj());
    o->items()[0] = cls;
    o->items()[1] = e.v.is_nil() ? value_none() : e.v;
    o->items()[2] = tb;
    out           = rt.v;
    return R::Ok;
}

// sys.stdlib_module_names: CPython's own list (Python/stdlib_module_names.h),
// space-separated.
constexpr Str STDLIB_NAMES =
    "__future__ _abc _aix_support _android_support _apple_support _ast _ast_unparse _asyncio "
    "_bisect _blake2 _bz2 _codecs _codecs_cn _codecs_hk _codecs_iso2022 _codecs_jp "
    "_codecs_kr _codecs_tw _collections _collections_abc _colorize _compat_pickle "
    "_contextvars _csv _ctypes _curses _curses_panel _datetime _dbm _decimal _elementtree "
    "_frozen_importlib _frozen_importlib_external _functools _gdbm _hashlib _heapq _hmac "
    "_imp _interpchannels _interpqueues _interpreters _io _ios_support _json _locale _lsprof "
    "_lzma _markupbase _math_integer _md5 _multibytecodec _multiprocessing _opcode "
    "_opcode_metadata _operator _osx_support _overlapped _pickle _posixshmem "
    "_posixsubprocess _py_abc _py_warnings _pydatetime _pydecimal _pyio _pylong _pyrepl "
    "_queue _random _remote_debugging _scproxy _sha1 _sha2 _sha3 _signal _sitebuiltins "
    "_socket _sqlite3 _sre _ssl _stat _statistics _string _strptime _struct _suggestions "
    "_symtable _sysconfig _thread _threading_local _tkinter _tokenize _tracemalloc _types "
    "_typing _uuid _warnings _weakref _weakrefset _winapi _wmi _zoneinfo _zstd abc "
    "annotationlib antigravity argparse array ast asyncio atexit base64 bdb binascii bisect "
    "builtins bz2 cProfile calendar cmath cmd code codecs codeop collections colorsys "
    "compileall compression concurrent configparser contextlib contextvars copy copyreg csv "
    "ctypes curses dataclasses datetime dbm decimal difflib dis doctest email encodings "
    "ensurepip enum errno faulthandler fcntl filecmp fileinput fnmatch fractions ftplib "
    "functools gc genericpath getopt getpass gettext glob graphlib grp gzip hashlib heapq "
    "hmac html http idlelib imaplib importlib inspect io ipaddress itertools json keyword "
    "linecache locale logging lzma mailbox marshal math mimetypes mmap modulefinder msvcrt "
    "multiprocessing netrc nt ntpath nturl2path numbers opcode operator optparse os pathlib "
    "pdb pickle pickletools pkgutil platform plistlib poplib posix posixpath pprint profile "
    "profiling pstats pty pwd py_compile pyclbr pydoc pydoc_data pyexpat queue quopri random "
    "re readline reprlib resource rlcompleter runpy sched secrets select selectors shelve "
    "shlex shutil signal site smtplib socket socketserver sqlite3 ssl stat statistics string "
    "stringprep struct subprocess symtable sys sysconfig syslog tabnanny tarfile tempfile "
    "termios textwrap this threading time timeit tkinter token tokenize tomllib trace "
    "traceback tracemalloc tty turtle turtledemo types typing unicodedata unittest urllib "
    "uuid venv warnings wave weakref webbrowser winreg winsound wsgiref xml xmlrpc zipapp "
    "zipfile zipimport zlib zoneinfo";

Value stdlib_names()
{
    SetObj *s = frozenset_new();
    if (!s)
        return oom(), Value();
    Root rs{ obj_value(s) };
    Str all  = STDLIB_NAMES;
    usize at = 0;
    for (usize i = 0; i <= all.size(); i++) {
        if (i < all.size() && all[i] != ' ')
            continue;
        Value n = str_new(all.substr(at, i - at));
        if (n.is_nil() || set_add(set_at(rs.v), n) != R::Ok)
            return Value();
        at = i + 1;
    }
    return rs.v;
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

INFO_TYPE(asyncgen_hooks_type, "asyncgen_hooks");

constexpr Str HOOK_NAMES[2] = { "firstiter", "finalizer" };

// PEP 525's pair. The loop sets them while it runs and puts back what it
// found; gen.cpp calls the first one when an async generator is first
// stepped.
R b_set_asyncgen_hooks(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "firstiter", "finalizer" };
    Value v[2];
    if (!fn_take(a, "set_asyncgen_hooks", NAMES, 2, 0, v))
        return R::Err;
    Home *h = here();
    if (!h)
        return err_set("MemoryError", "out of memory");
    for (u32 i = 0; i < 2; i++) {
        if (v[i].is_nil())
            continue;
        if (!is_none(v[i]) && !py_callable(v[i]))
            return err_set2("TypeError", "callable or None was expected", NAMES[i]);
        (i ? h->finalizer : h->firstiter) = is_none(v[i]) ? Value() : v[i];
    }
    out = value_none();
    return R::Ok;
}

R b_get_asyncgen_hooks(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_asyncgen_hooks", 0, 0))
        return R::Err;
    Home *h = here();
    if (!h)
        return err_set("MemoryError", "out of memory");
    Value items[2] = { h->firstiter.is_nil() ? value_none() : h->firstiter,
                       h->finalizer.is_nil() ? value_none() : h->finalizer };
    Roots pin{ items, 2 };
    out = info_new(&asyncgen_hooks_type, items, HOOK_NAMES, 2);
    return out.is_nil() ? R::Err : R::Ok;
}

// sys.displayhook: what a `single`-mode statement does with its value. The
// repr may be Python's own, so the writing waits on it; builtin.cpp drives
// that, and this is only the default the name starts as.
R b_displayhook(const CallArgs &a, Value &out)
{
    if (!args_only(a, "displayhook", 1, 1))
        return R::Err;
    out = value_none();
    Value k;
    R r = py_display_value(a.args[0], k);
    if (r == R::Ok && !k.is_nil())
        out = k;
    return r;
}

R b_gettrace(const CallArgs &a, Value &out)
{
    if (!args_only(a, "gettrace", 0, 0))
        return R::Err;
    out = vm_trace().is_nil() ? value_none() : vm_trace();
    return R::Ok;
}

R b_settrace(const CallArgs &a, Value &out)
{
    if (!args_only(a, "settrace", 1, 1))
        return R::Err;
    vm_set_trace(is_none(a.args[0]) ? Value() : a.args[0]);
    out = value_none();
    return R::Ok;
}

R b_getprofile(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getprofile", 0, 0))
        return R::Err;
    out = vm_profile().is_nil() ? value_none() : vm_profile();
    return R::Ok;
}

R b_setprofile(const CallArgs &a, Value &out)
{
    if (!args_only(a, "setprofile", 1, 1))
        return R::Err;
    vm_set_profile(is_none(a.args[0]) ? Value() : a.args[0]);
    out = value_none();
    return R::Ok;
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

// sys.breakpointhook's work, in Python because every step of it may be: the
// import, the getattr and the call. CPython's sys_breakpointhook, line for
// line, reading os.environ where C reads getenv.
constexpr Str BREAKPOINTHOOK = R"PY(def breakpointhook(*args, **kws):
    import sys
    hookname = None
    if not sys.flags.ignore_environment:
        import os
        hookname = os.environ.get('PYTHONBREAKPOINT')
    if hookname is None or len(hookname) == 0:
        hookname = 'pdb.set_trace'
    elif hookname == '0':
        return None
    modulepath, dot, attrname = hookname.rpartition('.')
    if not dot:
        modulepath = 'builtins'
    try:
        __import__(modulepath)
        hook = getattr(sys.modules[modulepath], attrname)
    except (ImportError, AttributeError):
        import warnings
        warnings.warn('Ignoring unimportable $PYTHONBREAKPOINT: "{}"'.format(hookname),
                      RuntimeWarning, stacklevel=3)
        return None
    return hook(*args, **kws)
)PY";

// A call's arguments as the tuples cont_call_kw takes. False when out of memory.
bool args_tuples(const CallArgs &a, u32 skip, Root &args, Root &names, Root &vals)
{
    TupleObj *t = tuple_new(a.nargs - skip);
    if (!t)
        return oom() == R::Ok;
    for (u32 i = skip; i < a.nargs; i++)
        t->items()[i - skip] = a.args[i];
    args = obj_value(t);
    if (!a.nkw)
        return true;
    TupleObj *n = tuple_new(a.nkw);
    if (!n)
        return oom() == R::Ok;
    names       = obj_value(n);
    TupleObj *v = tuple_new(a.nkw);
    if (!v)
        return oom() == R::Ok;
    vals = obj_value(v);
    for (u32 i = 0; i < a.nkw; i++) {
        static_cast<TupleObj *>(names.v.obj())->items()[i] = a.kwnames[i];
        static_cast<TupleObj *>(vals.v.obj())->items()[i]  = a.kwvals[i];
    }
    return true;
}

// s[0] the namespace the hook is defined in, s[1..3] the call's arguments,
// s[4] the body that defines it.
R hook_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        k->locals = k->s[0];
        return cont_call(k, k->s[4], Value(), 0);
    case 1: {
        Value fn;
        StrObj *n = str_intern("breakpointhook");
        if (!n || dict_get(static_cast<DictObj *>(k->s[0].obj()), obj_value(n), fn) != R::Ok)
            return err_pending() ? R::Err : oom();
        k->locals = Value();
        return cont_call_kw(k, fn, k->s[1], k->s[2], k->s[3]);
    }
    default:
        return cont_done(k, in);
    }
}

R b_breakpointhook(const CallArgs &a, Value &out)
{
    Root args, names, vals;
    if (!args_tuples(a, 0, args, names, vals))
        return R::Err;
    Ast ast;
    if (!ast.parse(BREAKPOINTHOOK, true))
        return R::Err;
    Root code{ py_compile(ast, "<frozen sys>") };
    if (code.v.is_nil())
        return R::Err;
    Root g{ obj_value(dict_new()) };
    Root nm{ str_new("sys") };
    if (g.v.is_nil() || nm.v.is_nil() ||
        !mod_put(static_cast<DictObj *>(g.v.obj()), "__name__", nm.v) ||
        !put_builtins(static_cast<DictObj *>(g.v.obj())))
        return R::Err;
    Root body{ func_new(code.v, g.v) };
    Root kv{ body.v.is_nil() ? Value() : cont_new(hook_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = g.v;
    k->s[1]    = args.v;
    k->s[2]    = names.v;
    k->s[3]    = vals.v;
    k->s[4]    = body.v;
    out        = kv.v;
    return R::Ok;
}

// s[0] the hook, s[1..3] the call's arguments.
R breakpoint_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call_kw(k, k->s[0], k->s[1], k->s[2], k->s[3]);
    return cont_done(k, in);
}

R b_get_int_max_str_digits(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_int_max_str_digits", 0, 0))
        return R::Err;
    out = int_from_i64(int_max_str_digits());
    return out.is_nil() ? R::Err : R::Ok;
}

R b_set_int_max_str_digits(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "maxdigits" };
    Value v[1];
    if (!fn_take(a, "set_int_max_str_digits", NAMES, 1, v))
        return R::Err;
    i64 n = 0;
    if (!int_to_i64(v[0], n)) {
        if (is_intval(v[0]))
            return err_set("OverflowError", "Python int too large to convert to C int");
        Buf<96> b;
        b.put("'").put(type_name(v[0])).put("' object cannot be interpreted as an integer");
        return err_set("TypeError", b.str());
    }
    if (n != 0 && (n < 640 || n > 0x7fffffff))
        return err_set("ValueError", "maxdigits must be >= 640 or 0 for unlimited");
    int_set_max_str_digits(u32(n));
    out = value_none();
    return R::Ok;
}

constexpr ModDef SYS_DEFS[] = {
    { "breakpointhook", b_breakpointhook },
    { "get_int_max_str_digits", b_get_int_max_str_digits },
    { "set_int_max_str_digits", b_set_int_max_str_digits },
    { "exit", b_exit },
    { "exc_info", b_exc_info },
    { "excepthook", b_excepthook },
    { "exception", b_exception },
    { "getsizeof", b_getsizeof },
    { "getrecursionlimit", b_getrecursionlimit },
    { "gettrace", b_gettrace },
    { "settrace", b_settrace },
    { "getprofile", b_getprofile },
    { "setprofile", b_setprofile },
    { "displayhook", b_displayhook },
    { "set_asyncgen_hooks", b_set_asyncgen_hooks },
    { "get_asyncgen_hooks", b_get_asyncgen_hooks },
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

// The defaults; sys_install puts py_config()'s answers over them.
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
    flag("dev_mode", false),
    num("utf8_mode", 1),
    num("warn_default_encoding", 0),
    flag("safe_path", false),
    num("int_max_str_digits", 4300),
    // Reached by name only, as CPython's are.
    num("gil", 1),
    num("thread_inherit_context", 0),
    num("context_aware_warnings", 0),
    num("lazy_imports", -1),
};

constexpr usize FLAGS_SHOWN = 18;

// FLAGS with what the command line settled put over the defaults.
void flags_now(Field (&out)[sizeof FLAGS / sizeof FLAGS[0]])
{
    const PyConfig &c = py_config();
    struct Set {
        Str name;
        i64 n;
    };
    const Set sets[] = {
        { "inspect", c.inspect },
        { "interactive", c.interactive },
        { "optimize", c.optimize },
        { "no_site", c.no_site },
        { "ignore_environment", c.ignore_env },
        { "verbose", c.verbose },
        { "bytes_warning", c.bytes_warning },
        { "quiet", c.quiet },
        { "isolated", c.isolated },
        { "dev_mode", c.dev_mode },
        { "warn_default_encoding", c.warn_default_encoding },
        { "safe_path", c.safe_path },
        { "int_max_str_digits", c.int_max_str_digits < 0 ? 4300 : c.int_max_str_digits },
    };
    for (usize i = 0; i < sizeof FLAGS / sizeof FLAGS[0]; i++) {
        out[i] = FLAGS[i];
        for (const Set &s : sets)
            if (s.name == FLAGS[i].name)
                out[i].n = s.n;
    }
}

// sys._xoptions: "name=value" is a str, a bare "name" is True.
Value xoptions_new()
{
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    for (Str x : py_config().xoptions) {
        usize eq = x.find('=');
        Str name = eq == Str::npos ? x : x.substr(0, eq);
        Root k{ str_new(name) };
        Root v{ eq == Str::npos ? value_bool(true) : str_new(x.substr(eq + 1)) };
        if (k.v.is_nil() || v.v.is_nil() ||
            dict_set(static_cast<DictObj *>(rd.v.obj()), k.v, v.v) != R::Ok)
            return Value();
    }
    return rd.v;
}

PyConfig *config;

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

// breakpoint(*args, **kws): sys.breakpointhook(*args, **kws).
R sys_breakpoint(const CallArgs &a, Value &out)
{
    Root hook{ sys_stream("breakpointhook") };
    if (hook.v.is_nil())
        return err_set("RuntimeError", "lost sys.breakpointhook");
    Root args, names, vals;
    if (!args_tuples(a, 0, args, names, vals))
        return R::Err;
    Value kv = cont_new(breakpoint_step);
    if (kv.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv);
    k->s[0]    = hook.v;
    k->s[1]    = args.v;
    k->s[2]    = names.v;
    k->s[3]    = vals.v;
    out        = kv;
    return R::Ok;
}

PyConfig &py_config()
{
    if (!config)
        config = heap_new<PyConfig>();
    return *config;
}

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
// With j set, s[0] is the getattr still to run for write itself.
R write_step(ContObj *k, Value in)
{
    if (k->j == 1) {
        k->j = 2;
        return cont_await(k, k->s[0]);
    }
    if (k->j == 2) {
        k->j    = 0;
        k->s[0] = in;
    }
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
    usize parts  = cuts.size() > 1 ? cuts.size() - 1 : 1;
    TupleObj *pt = tuple_new(parts);
    if (!pt)
        return oom();
    Root rt{ obj_value(pt) };
    for (usize i = 0; i < parts; i++) {
        Value piece =
            cuts.size() > 1 ? str_new(text.substr(cuts[i], cuts[i + 1] - cuts[i])) : str_new(text);
        if (piece.is_nil())
            return R::Err;
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = piece;
    }
    StrObj *w = str_intern("write");
    if (!w)
        return oom();
    Root fn;
    // A write found through __getattr__ is Python too.
    Got g = py_attr(rf.v, w, fn.v);
    if (g == Got::Error)
        return R::Err;
    if (g == Got::Missing) {
        Buf<96> m;
        m.put("'").put(type_name(rf.v)).put("' object has no attribute 'write'");
        return err_set("AttributeError", m.str());
    }
    Root kv{ cont_new(write_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->j    = g == Got::Call ? 1 : 0;
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

// sys.executable: the path this binary was reached by. The driver is the only
// one that can know it, and runpy names it in what it prints.
bool sys_set_executable(Str path)
{
    Value m = builtin_module("sys");
    if (m.is_nil())
        return false;
    Root v{ str_new(path) };
    return !v.v.is_nil() && mod_put(module_dict(m), "executable", v.v);
}

// sys.ps1 and sys.ps2, which CPython defines only for an interactive session.
bool sys_set_prompts()
{
    Value m = builtin_module("sys");
    if (m.is_nil())
        return false;
    Root one{ str_new(">>> ") }, two{ str_new("... ") };
    if (one.v.is_nil() || two.v.is_nil())
        return false;
    DictObj *d = module_dict(m);
    return mod_put(d, "ps1", one.v) && mod_put(d, "ps2", two.v);
}

// The prompt as the program has it now. A prompt that is not a str keeps the
// default: str() of one could call Python, and nothing here can.
bool sys_prompt(bool second, String &out)
{
    Value v = sys_stream(second ? Str("ps2") : Str("ps1"));
    if (is_str(v))
        return out.assign(str_of(v)->str());
    return out.assign(second ? Str("... ") : Str(">>> "));
}

bool sys_is_default_displayhook(Value v)
{
    return is_native(v) && static_cast<NativeObj *>(v.obj())->fn == b_displayhook;
}

Value sys_asyncgen_firstiter()
{
    return home ? home->firstiter : Value();
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

    Field flags[sizeof FLAGS / sizeof FLAGS[0]];
    flags_now(flags);
    if (!put_info(d, "version_info", &version_info_type, VERSION_INFO) ||
        !put_info(d, "float_info", &float_info_type, FLOAT_INFO) ||
        !put_info(d, "int_info", &int_info_type, INT_INFO) ||
        !put_info(d, "hash_info", &hash_info_type, HASH_INFO) ||
        !put_info(d, "flags", &flags_type, flags, sizeof flags / sizeof flags[0], FLAGS_SHOWN))
        return false;
    // A SimpleNamespace, as CPython's is. No cache tag: nothing is compiled
    // to a file, so importlib neither reads nor writes a .pyc.
    {
        Root ns{ obj_value(dict_new()) };
        Root ver;
        if (ns.v.is_nil() || dict_get(d, obj_value(str_intern("version_info")), ver.v) != R::Ok)
            return false;
        DictObj *nd = static_cast<DictObj *>(ns.v.obj());
        if (!mod_str(nd, "name", "braam") || !mod_put(nd, "cache_tag", value_none()) ||
            !mod_put(nd, "version", ver.v) || !mod_int(nd, "hexversion", 0x030E00F0) ||
            !mod_str(nd, "_multiarch", "wasm32-braam") ||
            !mod_put(nd, "supports_isolated_interpreters", value_bool(false)))
            return false;
        Root impl{ namespace_new(ns.v) };
        if (impl.v.is_nil() || !mod_put(d, "implementation", impl.v))
            return false;
    }

    if (!mod_str(d, "version", "3.14.0 (braam, Jan  1 2026, 00:00:00) [clang wasm32]") ||
        !mod_str(d, "platform", "braam") || !mod_str(d, "byteorder", "little") ||
        !mod_str(d, "executable", "") || !mod_str(d, "prefix", "/pkg") ||
        !mod_str(d, "exec_prefix", "/pkg") || !mod_str(d, "base_prefix", "/pkg") ||
        !mod_str(d, "base_exec_prefix", "/pkg") || !mod_str(d, "platlibdir", "lib") ||
        !mod_str(d, "abiflags", "") || !mod_str(d, "float_repr_style", "short") ||
        !mod_put(d, "pycache_prefix", value_none()))
        return false;
    if (!mod_int(d, "maxsize", 2147483647) || !mod_int(d, "maxunicode", 1114111) ||
        !mod_int(d, "hexversion", 0x030E00F0))
        return false;
    if (!mod_put(d, "dont_write_bytecode", value_bool(true)) ||
        !mod_str(d, "copyright",
                 "Copyright (c) 2001 Python Software Foundation.\nAll Rights Reserved.\n\n"
                 "Copyright (c) 2000 BeOpen.com.\nAll Rights Reserved.\n\n"
                 "Copyright (c) 1995-2001 Corporation for National Research Initiatives.\n"
                 "All Rights Reserved.\n\n"
                 "Copyright (c) 1991-1995 Stichting Mathematisch Centrum, Amsterdam.\n"
                 "All Rights Reserved."))
        return false;

    Root warn{ obj_value(list_new()) };
    if (warn.v.is_nil())
        return oom() == R::Ok;
    for (Str w : py_config().warnoptions) {
        Value v = str_new(w);
        if (v.is_nil() || !list_push(list_of(warn.v), v))
            return false;
    }
    Root xo{ xoptions_new() };
    if (xo.v.is_nil() || !mod_put(d, "warnoptions", warn.v) || !mod_put(d, "_xoptions", xo.v))
        return false;
    Value hook;
    if (dict_get(d, obj_value(str_intern("excepthook")), hook) != R::Ok ||
        !mod_put(d, "__excepthook__", hook))
        return false;
    if (dict_get(d, obj_value(str_intern("breakpointhook")), hook) != R::Ok ||
        !mod_put(d, "__breakpointhook__", hook))
        return false;
    Root std{ stdlib_names() };
    if (std.v.is_nil() || !mod_put(d, "stdlib_module_names", std.v))
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
    Root mp{ sys_import_state(IMPORT_META_PATH) }, ph{ sys_import_state(IMPORT_PATH_HOOKS) };
    Root pic{ sys_import_state(IMPORT_PATH_CACHE) }, lib{ sys_import_state(IMPORT_STDLIB) };
    if (mp.v.is_nil() || ph.v.is_nil() || pic.v.is_nil() || lib.v.is_nil() ||
        !mod_put(d, "meta_path", mp.v) || !mod_put(d, "path_hooks", ph.v) ||
        !mod_put(d, "path_importer_cache", pic.v) || !mod_put(d, "_stdlib_dir", lib.v))
        return false;
    // sys.monitoring is an attribute and not a module in sys.modules, which
    // is where CPython puts it too.
    {
        Root mon{ monitoring_new() };
        if (mon.v.is_nil() || !mod_put(d, "monitoring", mon.v))
            return false;
    }
    if (!lazy_sys_install(d))
        return false;
    // The same object under both names, so `sys.displayhook is
    // sys.__displayhook__` says what it does in CPython.
    StrObj *dh = str_intern("displayhook");
    Value shown;
    if (!dh || dict_get(d, obj_value(dh), shown) != R::Ok || !mod_put(d, "__displayhook__", shown))
        return false;
    return mod_put(d, "argv", here()->argv.is_nil() ? value_none() : here()->argv);
}
