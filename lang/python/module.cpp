// The registry: which names are modules written in C++, and the cache that
// makes two imports of one name the same object.
#include "module.h"

#include "abc.h"
#include "builtin.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
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
    { "_collections", coll_install },
    { "_functools", functools_install },
    { "itertools", itertools_install },
    { "operator", operator_install },
    { "_operator", operator_install },
    { "_random", random_install },
    { "_struct", struct_install },
    { "array", array_install },
    { "math", math_install },
    { "cmath", cmath_install },
    { "time", time_install },
    { "errno", errno_install },
    { "gc", gcmod_install },
    { "_types", types_install },
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
    for (usize i = 0; i < n; i++) {
        Root fn{ native_new(tab[i].name, tab[i].fn) };
        if (fn.v.is_nil() || !mod_put(static_cast<DictObj *>(rd.v.obj()), tab[i].name, fn.v))
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
    return mod_put(static_cast<DictObj *>(rd.v.obj()), t->name, w.v);
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
