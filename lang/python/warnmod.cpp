// `_warnings`: the default filters, the once-registry and the lock.
//
// CPython's warnings.py takes these from its C module when there is one, and
// only builds the defaults itself when there is not -- and building them
// compiles a regular expression. So the defaults are here. The machinery is
// _py_warnings.py's: warn, warn_explicit, the filters' version and the
// context variable are its own, reached through this module's __getattr__.
//
// A default filter names its module as a plain string, as CPython's C does.
// _py_warnings matches a filter's module with `.match()`, so that string is
// a str whose match() compares.
#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// name.match(text, pos=0): the rest of text is the name.
R n_match(const CallArgs &a, Value &out)
{
    if (a.nargs < 2 || a.nargs > 3 || a.nkw)
        return err_set("TypeError", "match() takes a string and a position");
    Value self = method_self(a.args[0]);
    if (!is_str(self) || !is_str(a.args[1]))
        return err_set("TypeError", "match() takes a string");
    i64 pos = 0;
    if (a.nargs == 3 && !as_index(a.args[2], pos))
        return err_set("TypeError", "match() position must be an integer");
    Str text = str_of(a.args[1])->str();
    // Positions count codepoints; a module name is ASCII where it matters.
    usize at = pos < 0 ? 0 : usize(pos) > text.size() ? text.size() : usize(pos);
    out      = text.substr(at) == str_of(self)->str() ? value_bool(true) : value_none();
    return R::Ok;
}

constexpr Method NAME_METHODS[] = { { "match", n_match } };

// The class of that string, made once.
struct Home {
    Value name_class;
};

Home *home;

void home_mark()
{
    gc_mark(home->name_class);
}

Value name_class()
{
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return oom(), Value();
        gc_root_hook(home_mark);
    }
    if (!home->name_class.is_nil())
        return home->name_class;
    DictObj *body = dict_new();
    if (!body)
        return oom(), Value();
    Root rd{ obj_value(body) };
    for (const Method &m : NAME_METHODS) {
        Root fn{ native_new(m.name, m.fn) };
        if (fn.v.is_nil() || !mod_put(static_cast<DictObj *>(rd.v.obj()), m.name, fn.v))
            return Value();
    }
    if (!mod_str(static_cast<DictObj *>(rd.v.obj()), "__module__", "_warnings"))
        return Value();
    Root base{ type_wrap(&str_type) };
    Root nm{ str_new("_ModuleName") };
    TupleObj *bases = base.v.is_nil() || nm.v.is_nil() ? nullptr : tuple_new(1);
    if (!bases)
        return err_pending() ? Value() : (oom(), Value());
    bases->items()[0] = base.v;
    Root rb{ obj_value(bases) };
    home->name_class = type_new(nm.v, rb.v, rd.v);
    return home->name_class;
}

Value module_name(Str text)
{
    Root cls{ name_class() };
    if (cls.v.is_nil())
        return Value();
    Root s{ str_new(text) };
    Root self{ inst_new(cls.v) };
    if (s.v.is_nil() || self.v.is_nil())
        return Value();
    inst_of(self.v)->native = s.v;
    return self.v;
}

// One filter: (action, None, category, module, 0).
Value filter(Str action, Str category, Str module)
{
    Root act{ str_new(action) };
    Root cat{ exc_type_value(exc_find(category)) };
    Root mod{ module.empty() ? value_none() : module_name(module) };
    if (act.v.is_nil() || cat.v.is_nil() || mod.v.is_nil())
        return Value();
    TupleObj *t = tuple_new(5);
    if (!t)
        return oom(), Value();
    t->items()[0] = act.v;
    t->items()[1] = value_none();
    t->items()[2] = cat.v;
    t->items()[3] = mod.v;
    t->items()[4] = Value::of_int(0);
    return obj_value(t);
}

R b_nothing(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_acquire_lock", 0, 0))
        return R::Err;
    out = value_none();
    return R::Ok;
}

// The rest is _py_warnings's: import it, and answer its name. s[0] the name.
R forward_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        StrObj *imp = str_intern("__import__");
        Value fn;
        if (!imp || dict_get(builtins_dict(), obj_value(imp), fn) != R::Ok)
            return err_pending() ? R::Err : oom();
        Value mod = str_new("_py_warnings");
        if (mod.is_nil())
            return R::Err;
        return cont_call(k, fn, mod);
    }
    StrObj *n = str_intern(str_of(k->s[0])->str());
    Value out;
    if (!n || py_getattr(in, n, out) != R::Ok)
        return R::Err;
    return cont_done(k, out);
}

constexpr Str FORWARDED[] = { "warn", "warn_explicit", "_filters_mutated_lock_held",
                              "_warnings_context" };

R b_getattr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__getattr__", 1, 1) || !is_str(a.args[0]))
        return err_pending() ? R::Err : err_set("TypeError", "attribute name must be a string");
    Str n     = str_of(a.args[0])->str();
    bool ours = false;
    for (Str f : FORWARDED)
        ours = ours || f == n;
    if (!ours) {
        Buf<128> m;
        m.put("module '_warnings' has no attribute '").put(n).put('\'');
        return err_set("AttributeError", m.str());
    }
    Root rn{ a.args[0] };
    Root kv{ cont_new(forward_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = rn.v;
    out                 = kv.v;
    return R::Ok;
}

// warnings.warn(s[1], s[2], j) from C++: s[0] the module, once imported.
R warn_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0: {
        StrObj *imp = str_intern("__import__");
        Value fn;
        if (!imp || dict_get(builtins_dict(), obj_value(imp), fn) != R::Ok)
            return err_pending() ? R::Err : oom();
        Value mod = str_new("warnings");
        if (mod.is_nil())
            return R::Err;
        return cont_call(k, fn, mod);
    }
    case 1: {
        StrObj *n = str_intern("warn");
        Value fn;
        if (!n || py_getattr(in, n, fn) != R::Ok)
            return R::Err;
        Root rf{ fn };
        TupleObj *t = tuple_new(3);
        if (!t)
            return oom();
        t->items()[0] = k->s[1];
        t->items()[1] = k->s[2];
        t->items()[2] = Value::of_int(i32(k->j));
        return cont_call_v(k, rf.v, obj_value(t));
    }
    default:
        return cont_done(k, value_none());
    }
}

constexpr ModDef DEFS[] = {
    { "_acquire_lock", b_nothing },
    { "_release_lock", b_nothing },
    { "__getattr__", b_getattr },
};

} // namespace

Value warn_cont(Str category, Str message, u32 stacklevel)
{
    Root cat{ exc_type_value(exc_find(category)) };
    Root msg{ str_new(message) };
    if (cat.v.is_nil() || msg.v.is_nil())
        return Value();
    Root kv{ cont_new(warn_step) };
    if (kv.v.is_nil())
        return Value();
    cont_of(kv.v)->s[1] = msg.v;
    cont_of(kv.v)->s[2] = cat.v;
    cont_of(kv.v)->j    = stacklevel;
    return kv.v;
}

bool warnings_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    ListObj *l = list_new();
    if (!l)
        return oom() == R::Ok;
    Root rl{ obj_value(l) };
    struct Row {
        Str action, category, module;
    };
    constexpr Row ROWS[] = {
        { "default", "DeprecationWarning", "__main__" },
        { "ignore", "DeprecationWarning", "" },
        { "ignore", "PendingDeprecationWarning", "" },
        { "ignore", "ImportWarning", "" },
        { "ignore", "ResourceWarning", "" },
    };
    for (const Row &r : ROWS) {
        Value f = filter(r.action, r.category, r.module);
        if (f.is_nil() || !list_push(list_of(rl.v), f))
            return err_pending() ? false : oom() == R::Ok;
    }
    DictObj *reg = dict_new();
    if (!reg)
        return oom() == R::Ok;
    Root rr{ obj_value(reg) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    return mod_put(d, "filters", rl.v) && mod_str(d, "_defaultaction", "default") &&
           mod_put(d, "_onceregistry", rr.v) && mod_defs(d, DEFS);
}
