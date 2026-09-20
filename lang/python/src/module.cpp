// The registry: which names are modules written in C++, and the cache that
// makes two imports of one name the same object.
#include "module.h"

#include "abc.h"
#include "binfmt.h"
#include "builtin.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "method.h"
#include "ops.h"
#include "type.h"
#include "weak.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The modules already built. A program may delete a name from sys.modules and
// import it again; this is what makes the second import the first object.
struct Home {
    Value cache;
};

Home *home;

void home_mark()
{
    if (home)
        gc_mark(home->cache);
}

DictObj *cache()
{
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return oom(), nullptr;
        gc_root_hook(home_mark);
    }
    if (home->cache.is_nil()) {
        DictObj *d = dict_new();
        if (!d)
            return oom(), nullptr;
        home->cache = obj_value(d);
    }
    return static_cast<DictObj *>(home->cache.obj());
}

// `_weakref` and `_abc` take an argument their installers want, so they are
// wrapped rather than named directly.
bool weakref_install(DictObj *into)
{
    return weak_install(into);
}

bool abcmod_install(DictObj *into)
{
    StrObj *n = str_intern("issubclass");
    Value fn;
    if (!n)
        return oom() == R::Ok;
    if (dict_get(builtins_dict(), obj_value(n), fn) != R::Ok)
        return false;
    return abc_install(into, fn);
}

struct Native {
    Str name;
    bool (*install)(DictObj *into);
};

constexpr Native NATIVES[] = {
    { "sys", sys_install },
    { "_weakref", weakref_install },
    { "_abc", abcmod_install },
    { "_codecs", codecs_install },
    { "unicodedata", unicodedata_install },
    { "_collections", coll_install },
    { "_functools", functools_install },
    { "itertools", itertools_install },
    { "_operator", operator_install },
    { "_random", random_install },
    { "_struct", struct_install },
    { "_sre", sre_install },
    { "array", array_install },
    { "math", math_install },
    { "cmath", cmath_install },
    { "_math_integer", math_integer_install },
    { "time", time_install },
    { "errno", errno_install },
    { "gc", gcmod_install },
    { "_types", types_install },
    { "_typing", typing_install },
    { "_ast", ast_install },
    { "dis", dismod_install },
    { "_thread", thread_install },
    { "_contextvars", contextvars_install },
    { "_string", string_install },
    { "_warnings", warnings_install },
    { "atexit", atexit_install },
    { "posix", posix_install },
    { "_signal", signal_install },
    { "_posixsubprocess", posixsubprocess_install },
    { "select", select_install },
    { "_socket", socket_install },
    { "_symtable", symtable_install },
    { "_sysconfig", sysconfig_install },
    // The name sysconfig.py derives from sys.abiflags, sys.platform and
    // sys.implementation._multiarch; CPython generates a file of it.
    { "_sysconfigdata__braam_wasm32-braam", sysconfigdata_install },
    { "_pickle", pickle_install },
    { "_io", io_install },
    { "_csv", csv_install },
    { "pyexpat", pyexpat_install },
    { "binascii", binascii_install },
    { "zlib", zlib_install },
    { "_bz2", bz2_install },
    { "_lzma", lzma_install },
    { "_zstd", zstd_install },
    { "_md5", md5_install },
    { "_sha1", sha1_install },
    { "_sha2", sha2_install },
    { "_sha3", sha3_install },
    { "_blake2", blake2_install },
    { "_tokenize", tokenize_install },
    { "marshal", marshal_install },
    { "_imp", imp_install },
    { "_colorize", colorize_install },
    { "faulthandler", faulthandler_install },
};

} // namespace

bool mod_put(DictObj *into, Str name, Value v)
{
    Root rd{ obj_value(into) }, rv{ v };
    StrObj *k = str_intern(name);
    if (!k)
        return oom() == R::Ok;
    return dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(k), rv.v) == R::Ok;
}

bool mod_int(DictObj *into, Str name, i64 v)
{
    Root rv{ int_from_i64(v) };
    return !rv.v.is_nil() && mod_put(into, name, rv.v);
}

bool mod_str(DictObj *into, Str name, Str v)
{
    Root rv{ str_new(v) };
    return !rv.v.is_nil() && mod_put(into, name, rv.v);
}

bool mod_float(DictObj *into, Str name, f64 v)
{
    Root rv{ float_new(v) };
    return !rv.v.is_nil() && mod_put(into, name, rv.v);
}

bool mod_defs(DictObj *into, const ModDef *tab, usize n)
{
    Root rd{ obj_value(into) };
    // Each function's __module__ is the module it is installed in.
    Root modname;
    StrObj *nk = str_intern("__name__");
    if (!nk)
        return oom() == R::Ok;
    if (dict_get(into, obj_value(nk), modname.v) != R::Ok || !is_str(modname.v))
        modname = Value();
    err_clear();
    for (usize i = 0; i < n; i++) {
        Root fn{ native_new(tab[i].name, tab[i].fn) };
        if (fn.v.is_nil())
            return false;
        fn.v.obj()->flags |= OBJ_PLAINFN;
        static_cast<NativeObj *>(fn.v.obj())->owner = modname.v;
        if (!mod_put(static_cast<DictObj *>(rd.v.obj()), tab[i].name, fn.v))
            return false;
    }
    return true;
}

bool mod_type(DictObj *into, const Type *t, R (*ctor)(const CallArgs &, Value &out))
{
    Root rd{ obj_value(into) };
    Root w{ type_wrap(t) };
    if (w.v.is_nil())
        return false;
    if (ctor) {
        Root fn{ native_new(t->name, ctor) };
        if (fn.v.is_nil() || !type_set_ctor(t, fn.v))
            return false;
    }
    // `typing.TypeVar` is TypeVar in its module.
    Str name  = t->name;
    usize dot = name.size();
    while (dot && name[dot - 1] != '.')
        dot--;
    return mod_put(static_cast<DictObj *>(rd.v.obj()), name.substr(dot), w.v);
}

Value mod_exc_class(Str module, Str name, Str base)
{
    Root b{ exc_type_value(exc_find(base)) };
    TupleObj *bases = b.v.is_nil() ? nullptr : tuple_new(1);
    if (!bases)
        return err_pending() ? Value() : (oom(), Value());
    bases->items()[0] = b.v;
    Root rb{ obj_value(bases) };
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    Root mod{ str_new(module) };
    Root nm{ str_new(name) };
    StrObj *key = str_intern("__module__");
    if (mod.v.is_nil() || nm.v.is_nil() || !key ||
        dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(key), mod.v) != R::Ok)
        return err_pending() ? Value() : (oom(), Value());
    return type_new(nm.v, rb.v, rd.v);
}

R mod_raise(Value cls, Str msg)
{
    Root c{ cls };
    Root m{ str_new(msg) };
    TupleObj *t = m.v.is_nil() ? nullptr : tuple_new(1);
    if (!t)
        return err_pending() ? R::Err : oom();
    t->items()[0] = m.v;
    Root rt{ obj_value(t) };
    Value e = exc_construct(c.v, rt.v);
    return e.is_nil() ? R::Err : err_set_value(e);
}

bool buffer_like(Value v, Str &out)
{
    char code = 0;
    return bytes_like(v, out) || array_bytes(v, out, code);
}

bool string_sized(String &s, usize n)
{
    s.clear();
    if (!gc_room(s, n))
        return false;
    for (usize i = 0; i < n; i++)
        if (!s.push(0))
            return false;
    return true;
}

bool hex_with_sep(Str data, Value sep, i64 per, bool bytes_out, String &out)
{
    const char *HEX = "0123456789abcdef";
    char sc         = 0;
    if (!sep.is_nil()) {
        if (is_str(sep)) {
            if (str_of(sep)->chars != 1)
                return err_set("ValueError", "sep must be length 1.") == R::Ok;
            // A one-octet kind is what CPython takes: Latin-1 into bytes.
            u32 cp = str_char_at(str_of(sep), 0);
            if (cp > 255)
                return err_set("ValueError", "sep must be ASCII.") == R::Ok;
            sc = char(cp);
        } else if (is_bytes(sep)) {
            Str s = static_cast<BytesObj *>(sep.obj())->str();
            if (s.size() != 1)
                return err_set("ValueError", "sep must be length 1.") == R::Ok;
            sc = s[0];
        } else {
            usize n = 0;
            if (py_len(sep, n) != R::Ok)
                return false;
            if (n != 1)
                return err_set("ValueError", "sep must be length 1.") == R::Ok;
            return err_set("TypeError", "sep must be str or bytes.") == R::Ok;
        }
        if (u8(sc) > 127 && !bytes_out)
            return err_set("ValueError", "sep must be ASCII.") == R::Ok;
    } else {
        per = 0;
    }
    usize len  = data.size();
    usize step = usize(per < 0 ? -per : per);
    usize seps = per && len ? (len - 1) / step : 0;
    usize n    = len * 2 + seps;
    if (step >= len) {
        per  = 0;
        step = 0;
    }
    out.clear();
    if (!string_sized(out, n))
        return oom() == R::Ok;
    char *b = out.data();
    if (per == 0) {
        for (usize i = 0; i < len; i++) {
            b[2 * i]     = HEX[u8(data[i]) >> 4];
            b[2 * i + 1] = HEX[u8(data[i]) & 15];
        }
        return true;
    }
    usize chunks = (len - 1) / step;
    if (per < 0) {
        usize i = 0, j = 0;
        for (usize c = 0; c < chunks; c++) {
            for (usize k = 0; k < step; k++) {
                u8 ch  = u8(data[i++]);
                b[j++] = HEX[ch >> 4];
                b[j++] = HEX[ch & 15];
            }
            b[j++] = sc;
        }
        while (i < len) {
            u8 ch  = u8(data[i++]);
            b[j++] = HEX[ch >> 4];
            b[j++] = HEX[ch & 15];
        }
        return true;
    }
    usize i = len, j = n;
    for (usize c = 0; c < chunks; c++) {
        for (usize k = 0; k < step; k++) {
            u8 ch  = u8(data[--i]);
            b[--j] = HEX[ch & 15];
            b[--j] = HEX[ch >> 4];
        }
        b[--j] = sc;
    }
    while (i > 0) {
        u8 ch  = u8(data[--i]);
        b[--j] = HEX[ch & 15];
        b[--j] = HEX[ch >> 4];
    }
    return true;
}

Value native_module_names()
{
    TupleObj *t = tuple_new(sizeof NATIVES / sizeof NATIVES[0] + 1);
    if (!t)
        return oom(), Value();
    Root rt{ obj_value(t) };
    Value first = str_new("builtins");
    if (first.is_nil())
        return Value();
    static_cast<TupleObj *>(rt.v.obj())->items()[0] = first;
    for (usize i = 0; i < sizeof NATIVES / sizeof NATIVES[0]; i++) {
        Value v = str_new(NATIVES[i].name);
        if (v.is_nil())
            return Value();
        static_cast<TupleObj *>(rt.v.obj())->items()[i + 1] = v;
    }
    return rt.v;
}

bool native_module_known(Str name)
{
    if (name == "builtins")
        return true;
    for (const Native &one : NATIVES)
        if (one.name == name)
            return true;
    return false;
}

Value builtin_module(Str name)
{
    // `builtins` is the namespace every frame already falls back to, wrapped
    // in a module so it can be imported like anything else.
    if (name == "builtins")
        return builtins_module();

    DictObj *c = cache();
    if (!c)
        return Value();
    Root key{ str_new(name) };
    if (key.v.is_nil())
        return Value();
    Value had;
    R r = dict_get(c, key.v, had);
    if (r == R::Err)
        return Value();
    if (r == R::Ok)
        return had;

    const Native *e = nullptr;
    for (const Native &one : NATIVES)
        if (one.name == name)
            e = &one;
    // Nil and no error: the loader goes looking for a file instead.
    if (!e)
        return Value();

    Root m{ module_new(name) };
    if (m.v.is_nil())
        return Value();
    // In the cache before the body runs, so an installer that imports its own
    // name does not build a second one.
    if (dict_set(cache(), key.v, m.v) != R::Ok)
        return Value();
    if (!e->install(module_dict(m.v))) {
        dict_del(cache(), key.v);
        return Value();
    }
    return m.v;
}
