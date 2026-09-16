// Lazy imports (PEP 810), after CPython's Objects/lazyimportobject.c and
// the lazy half of Python/import.c; see lazy.h.
#include "lazy.h"

#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "frame.h"
#include "gc.h"
#include "import.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
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

usize tuple_len(Value v)
{
    return static_cast<TupleObj *>(v.obj())->len;
}

Value tuple_at(Value v, usize i)
{
    return static_cast<TupleObj *>(v.obj())->items()[i];
}

struct LazyObj : Obj {
    Value from; // the absolute module name
    Value attr; // Nil, a name taken from it, or the fromlist it was made with
    bool loading;
};

LazyObj *lz_of(Value v)
{
    return static_cast<LazyObj *>(v.obj());
}

void lz_trace(Obj *o)
{
    gc_mark(static_cast<LazyObj *>(o)->from);
    gc_mark(static_cast<LazyObj *>(o)->attr);
}

// `from`, `from.attr`, or `from...` for a whole fromlist.
bool lz_name(Value v, String &out)
{
    LazyObj *l = lz_of(v);
    if (!out.append(str_of(l->from)->str()))
        return false;
    if (is_str(l->attr))
        return out.push('.') && out.append(str_of(l->attr)->str());
    if (!l->attr.is_nil())
        return out.append("...");
    return true;
}

R lz_repr(Value v, String &out)
{
    String name;
    if (!lz_name(v, name))
        return oom();
    return out.append("<lazy_import '") && out.append(name.str()) && out.append("'>") ? R::Ok
                                                                                      : oom();
}

// Anything but a method is an attribute of what it will be, and it is not
// that yet.
R lz_getattr(Value v, StrObj *name, Value &out)
{
    (void)out;
    Str n = name->str();
    if (n == "resolve" || (n.size() > 4 && n[0] == '_' && n[1] == '_'))
        return R::NotImpl;
    String what;
    if (!lz_name(v, what))
        return oom();
    Buf<192> b;
    b.put("cannot access attribute '").put(n).put("' on unresolved lazy import '");
    b.put(what.str()).put("'");
    return err_set("AttributeError", b.str());
}

// The mode, the filter, sys.lazy_modules and the submodules a lazy import
// has promised its parent.
struct State {
    bool all = false;
    Value filter;
    Value modules; // SetObj
    Value pending; // DictObj: parent name -> SetObj of child names
};

State *state;

void state_mark()
{
    if (!state)
        return;
    gc_mark(state->filter);
    gc_mark(state->modules);
    gc_mark(state->pending);
}

State *here()
{
    if (state)
        return state;
    state = heap_new<State>();
    if (!state)
        return oom(), nullptr;
    gc_root_hook(state_mark);
    SetObj *s = set_new();
    if (!s)
        return oom(), nullptr;
    state->modules = obj_value(s);
    DictObj *d     = dict_new();
    if (!d)
        return oom(), nullptr;
    state->pending = obj_value(d);
    return state;
}

// `parent.child` promises `child` to `parent`, and so on up.
bool promise(Str name)
{
    State *st = here();
    if (!st)
        return false;
    for (;;) {
        usize dot = name.size();
        while (dot && name[dot - 1] != '.')
            dot--;
        Str parent = dot ? name.substr(0, dot - 1) : name;
        Root key{ str_new(parent) };
        if (key.v.is_nil())
            return false;
        Value set;
        R r = dict_get(static_cast<DictObj *>(st->pending.obj()), key.v, set);
        if (r == R::Err)
            return false;
        Root rs{ set };
        if (r == R::NotImpl) {
            SetObj *made = set_new();
            if (!made)
                return oom() == R::Ok;
            rs = obj_value(made);
            if (dict_set(static_cast<DictObj *>(st->pending.obj()), key.v, rs.v) != R::Ok)
                return false;
        }
        if (!dot)
            return true;
        Root child{ str_new(name.substr(dot)) };
        if (child.v.is_nil() || set_add(set_at(rs.v), child.v) != R::Ok)
            return false;
        name = parent;
    }
}

bool note_module(Value name)
{
    State *st = here();
    return st && set_add(set_at(st->modules), name) == R::Ok;
}

Value lz_new(Value from, Value attr)
{
    Root rf{ from }, ra{ attr };
    LazyObj *l = static_cast<LazyObj *>(obj_alloc(&lazy_type, sizeof(LazyObj)));
    if (!l)
        return oom(), Value();
    l->from    = rf.v;
    l->attr    = ra.v;
    l->loading = false;
    return obj_value(l);
}

// A proxy for `abs` with `fromlist`, and what it promises.
Value made(Value abs, Value fromlist)
{
    Root ra{ abs }, rf{ fromlist };
    bool some = is_tuple(rf.v) && tuple_len(rf.v);
    Root l{ lz_new(ra.v, some ? rf.v : Value()) };
    if (l.v.is_nil() || !note_module(ra.v))
        return Value();
    if (!some)
        return promise(str_of(ra.v)->str()) ? l.v : Value();
    for (usize i = 0; i < tuple_len(rf.v); i++) {
        Value item = tuple_at(rf.v, i);
        if (!is_str(item))
            continue;
        String full;
        if (!full.append(str_of(ra.v)->str()) || !full.push('.') ||
            !full.append(str_of(item)->str()))
            return oom(), Value();
        Root fn{ str_new(full.str()) };
        if (fn.v.is_nil() || !note_module(fn.v) || !promise(full.str()))
            return Value();
    }
    return l.v;
}

// The builtins' __import__, which a lazy import runs as CPython's does.
Value import_fn()
{
    StrObj *k  = str_intern("__import__");
    Value f    = vm_frame();
    DictObj *b = f.is_nil() ? builtins_dict() : static_cast<DictObj *>(frame_of(f)->builtins.obj());
    Value out;
    if (!k || !b || dict_get(b, obj_value(k), out) != R::Ok)
        return err_pending() ? Value() : (err_set("ImportError", "__import__ not found"), Value());
    return out;
}

Value frame_globals()
{
    Value f = vm_frame();
    return f.is_nil() ? value_none() : frame_of(f)->globals;
}

Value args_of(const Value *v, usize n)
{
    TupleObj *t = tuple_new(n);
    if (!t)
        return oom(), Value();
    for (usize i = 0; i < n; i++)
        t->items()[i] = v[i].is_nil() ? value_none() : v[i];
    return obj_value(t);
}

// A ContObj made callable: calling this hands it back to be run.
R n_hand(const CallArgs &a, Value &out)
{
    out = a.args[0];
    return R::Ok;
}

// s[0] the proxy, s[1] where the answer is stored, s[2] under what name.
R reify_step(ContObj *k, Value in)
{
    LazyObj *l = lz_of(k->s[0]);
    switch (k->i++) {
    case 0: {
        if (l->loading) {
            String name;
            if (!lz_name(k->s[0], name))
                return oom();
            Root text{ str_new(name.str()) };
            String rep;
            if (text.v.is_nil() || py_repr(text.v, rep) != R::Ok)
                return R::Err;
            Buf<192> b;
            b.put("cannot import name ")
                .put(rep.str())
                .put(" (most likely due to a circular import)");
            return err_set("ImportCycleError", b.str());
        }
        Root fn{ import_fn() };
        if (fn.v.is_nil())
            return R::Err;
        Root fromlist;
        if (is_str(l->attr)) {
            fromlist = args_of(&l->attr, 1);
            if (fromlist.v.is_nil())
                return R::Err;
        } else if (!l->attr.is_nil()) {
            fromlist = l->attr;
        }
        Value g       = frame_globals();
        Value five[5] = { l->from, g, g, fromlist.v, Value::of_int(0) };
        Root args{ args_of(five, 5) };
        if (args.v.is_nil())
            return R::Err;
        l->loading = true;
        return cont_call_v(k, fn.v, args.v);
    }
    case 1:
        if (is_str(l->attr)) {
            // The name, off the module; the submodule when it is one.
            Root mod{ in };
            Value got;
            Got g = Got::Missing;
            // The proxy being resolved is not an answer for itself.
            if (!is_module(mod.v) || dict_get(module_dict(mod.v), l->attr, got) != R::Ok ||
                !is_lazy(got) || !lz_of(got)->loading)
                g = py_attr(mod.v, str_of(l->attr), got);
            if (g == Got::Error)
                return R::Err;
            if (g == Got::Call) {
                // A module __getattr__: the answer comes back to case 2.
                Root pending{ got };
                Root fn{ native_new("_hand", n_hand) };
                Root bound{ fn.v.is_nil() ? Value() : method_new(fn.v, pending.v) };
                if (bound.v.is_nil())
                    return R::Err;
                return cont_call(k, bound.v, Value(), 0);
            }
            if (g == Got::Missing) {
                String full;
                if (!full.append(str_of(l->from)->str()) || !full.push('.') ||
                    !full.append(str_of(l->attr)->str()))
                    return oom();
                Root fn{ str_new(full.str()) };
                if (fn.v.is_nil())
                    return R::Err;
                DictObj *mods = sys_modules();
                if (!mods || dict_get(mods, fn.v, got) != R::Ok)
                    return err_pending() ? R::Err : import_missing(mod.v, str_of(l->attr));
            }
            in = got;
        }
        k->i = 2;
        [[fallthrough]];
    default:
        l->loading = false;
        k->fail    = nullptr;
        if (is_dict(k->s[1]) && !k->s[2].is_nil() &&
            dict_set(static_cast<DictObj *>(k->s[1].obj()), k->s[2], in) != R::Ok)
            return R::Err;
        return cont_done(k, in);
    }
}

// An exception out of the import says where the lazy import was written.
void reify_failed(ContObj *k)
{
    LazyObj *l = lz_of(k->s[0]);
    l->loading = false;
    if (!is_exc(k->caught))
        return;
    String name;
    if (!lz_name(k->s[0], name))
        return;
    Buf<192> b;
    b.put("lazy import of '").put(name.str()).put("' raised an exception during resolution");
    Root caught{ k->caught };
    Value cause = exc_make("ImportError", b.str());
    if (!cause.is_nil())
        static_cast<ExcObj *>(caught.v.obj())->cause = cause;
}

R n_reify(const CallArgs &a, Value &out)
{
    out =
        lazy_reify(a.args[0], a.nargs > 1 ? a.args[1] : Value(), a.nargs > 2 ? a.args[2] : Value());
    return out.is_nil() ? R::Err : R::Ok;
}

R lz_resolve(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "resolve", 0, 0))
        return R::Err;
    out = lazy_reify(a.args[0], Value(), Value());
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method LAZY_METHODS[] = {
    { "resolve", lz_resolve },
};

// s[0] the filter, s[1] its arguments, s[2] the absolute name, s[3] the
// fromlist, s[4] the eager import's arguments.
R filter_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        return cont_call_v(k, k->s[0], k->s[1]);
    case 1:
        if (py_truth(in)) {
            Value l = made(k->s[2], k->s[3]);
            return l.is_nil() ? R::Err : cont_done(k, l);
        } else {
            Root fn{ import_fn() };
            if (fn.v.is_nil())
                return R::Err;
            return cont_call_v(k, fn.v, k->s[4]);
        }
    default:
        return cont_done(k, in);
    }
}

// sys.set_lazy_imports and the rest.
R s_set_mode(const CallArgs &a, Value &out)
{
    if (!args_only(a, "set_lazy_imports", 1, 1))
        return R::Err;
    if (!is_str(a.args[0]))
        return err_set("TypeError", "mode must be a string: 'normal' or 'all'");
    Str m = str_of(a.args[0])->str();
    if (m != Str("normal") && m != Str("all"))
        return err_set("ValueError", "mode must be 'normal' or 'all'");
    State *st = here();
    if (!st)
        return R::Err;
    st->all = m == Str("all");
    out     = value_none();
    return R::Ok;
}

R s_get_mode(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_lazy_imports", 0, 0))
        return R::Err;
    State *st = here();
    if (!st)
        return R::Err;
    out = str_new(st->all ? Str("all") : Str("normal"));
    return out.is_nil() ? R::Err : R::Ok;
}

R s_set_filter(const CallArgs &a, Value &out)
{
    if (!args_only(a, "set_lazy_imports_filter", 1, 1))
        return R::Err;
    State *st = here();
    if (!st)
        return R::Err;
    st->filter = is_none(a.args[0]) ? Value() : a.args[0];
    out        = value_none();
    return R::Ok;
}

R s_get_filter(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_lazy_imports_filter", 0, 0))
        return R::Err;
    State *st = here();
    if (!st)
        return R::Err;
    out = st->filter.is_nil() ? value_none() : st->filter;
    return R::Ok;
}

constexpr ModDef SYS_DEFS[] = {
    { "set_lazy_imports", s_set_mode },
    { "get_lazy_imports", s_get_mode },
    { "set_lazy_imports_filter", s_set_filter },
    { "get_lazy_imports_filter", s_get_filter },
};

} // namespace

constexpr Type lazy_type{ .name    = "lazy_import",
                          .trace   = lz_trace,
                          .repr    = lz_repr,
                          .getattr = lz_getattr };

R lazy_import(Value name, Value level, Value fromlist, Value globals, Value &out)
{
    Root rn{ name }, rl{ level }, rf{ fromlist }, rg{ globals };
    if (!is_str(rn.v)) {
        Buf<96> b;
        b.put("module name must be a string, got ").put(type_name(rn.v));
        return err_set("TypeError", b.str());
    }
    i64 lv = 0;
    if (!as_index(rl.v, lv))
        lv = 0;
    if (lv < 0)
        return err_set("ValueError", "level must be >= 0");
    Root abs;
    if (import_absolute(str_of(rn.v)->str(), lv, rg.v, abs.v) != R::Ok)
        return R::Err;
    State *st = here();
    if (!st)
        return R::Err;
    Root list{ is_none(rf.v) ? Value() : rf.v };
    if (is_str(list.v)) {
        list = args_of(&rf.v, 1);
        if (list.v.is_nil())
            return R::Err;
    }
    if (st->filter.is_nil()) {
        out = made(abs.v, list.v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    // The filter may say no, and then this is an ordinary import.
    StrObj *nk = str_intern("__name__");
    Value mod;
    if (!nk || !is_dict(rg.v) ||
        dict_get(static_cast<DictObj *>(rg.v.obj()), obj_value(nk), mod) != R::Ok)
        mod = value_none();
    err_clear();
    Value three[3] = { mod, abs.v, list.v };
    Root fargs{ args_of(three, 3) };
    Value five[5] = { rn.v, rg.v, rg.v, list.v, rl.v };
    Root iargs{ args_of(five, 5) };
    Root kv{ cont_new(filter_step) };
    if (fargs.v.is_nil() || iargs.v.is_nil() || kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = st->filter;
    k->s[1]    = fargs.v;
    k->s[2]    = abs.v;
    k->s[3]    = list.v;
    k->s[4]    = iargs.v;
    out        = kv.v;
    return R::Ok;
}

bool lazy_wanted(Value name, Value level, Value globals)
{
    State *st = here();
    if (!st)
        return err_clear(), false;
    if (st->all)
        return true;
    if (!is_dict(globals))
        return false;
    StrObj *k = str_intern("__lazy_modules__");
    Value names;
    if (!k || dict_get(static_cast<DictObj *>(globals.obj()), obj_value(k), names) != R::Ok)
        return err_clear(), false;
    Root rn{ names };
    i64 lv = 0;
    as_index(level, lv);
    Root abs;
    bool in = false;
    if (import_absolute(str_of(name)->str(), lv, globals, abs.v) != R::Ok ||
        py_contains(rn.v, abs.v, in) != R::Ok)
        return err_clear(), false;
    return in;
}

R lazy_from(Value lazy, StrObj *name, Value &out)
{
    Root rl{ lazy }, rn{ obj_value(name) };
    LazyObj *l = lz_of(rl.v);
    // A module already loaded answers at once.
    DictObj *mods = sys_modules();
    Value mod;
    if (mods && dict_get(mods, l->from, mod) == R::Ok && is_module(mod)) {
        Value got;
        if (dict_get(module_dict(mod), rn.v, got) == R::Ok) {
            out = got;
            return R::Ok;
        }
    }
    err_clear();
    Str from = str_of(lz_of(rl.v)->from)->str();
    String base;
    if (is_str(lz_of(rl.v)->attr)) {
        if (!base.append(from) || !base.push('.') || !base.append(str_of(lz_of(rl.v)->attr)->str()))
            return oom();
    } else if (lz_of(rl.v)->attr.is_nil()) {
        usize dot = 0;
        while (dot < from.size() && from[dot] != '.')
            dot++;
        if (!base.append(from.substr(0, dot)))
            return oom();
    } else if (!base.append(from)) {
        return oom();
    }
    Root rb{ str_new(base.str()) };
    if (rb.v.is_nil())
        return R::Err;
    out = lz_new(rb.v, rn.v);
    return out.is_nil() ? R::Err : R::Ok;
}

Value lazy_reify(Value lazy, Value space, Value name)
{
    Root rl{ lazy }, rs{ space }, rn{ name };
    Root kv{ cont_new(reify_step) };
    if (kv.v.is_nil())
        return Value();
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rl.v;
    k->s[1]    = rs.v;
    k->s[2]    = rn.v;
    k->fail    = reify_failed;
    return kv.v;
}

Got lazy_module_attr(Value module, StrObj *name, Value &out, Value &args)
{
    Root rm{ module }, rn{ obj_value(name) };
    DictObj *d = module_dict(rm.v);
    Value got;
    R r = dict_get(d, rn.v, got);
    if (r == R::Err)
        return Got::Error;
    Root target;
    if (r == R::Ok) {
        if (!is_lazy(got))
            return Got::Missing;
        target = got;
    } else {
        // A submodule a lazy import promised this module.
        if (!state || !dict_len(static_cast<DictObj *>(state->pending.obj())))
            return Got::Missing;
        StrObj *nk = str_intern("__name__");
        Value own, set;
        if (!nk || dict_get(d, obj_value(nk), own) != R::Ok || !is_str(own) ||
            dict_get(static_cast<DictObj *>(state->pending.obj()), own, set) != R::Ok)
            return err_clear(), Got::Missing;
        bool has = false;
        if (set_has(set_at(set), rn.v, has) != R::Ok || !has)
            return err_clear(), Got::Missing;
        Root rown{ own }, rset{ set };
        if (set_discard(set_at(rset.v), rn.v, has) != R::Ok)
            return Got::Error;
        target = lz_new(rown.v, rn.v);
        if (target.v.is_nil())
            return Got::Error;
    }
    Root fn{ native_new("_reify", n_reify) };
    if (fn.v.is_nil())
        return Got::Error;
    Value three[3] = { target.v, obj_value(d), rn.v };
    args           = args_of(three, 3);
    if (args.is_nil())
        return Got::Error;
    out = fn.v;
    return Got::Call;
}

bool lazy_sys_install(DictObj *sys)
{
    Root rd{ obj_value(sys) };
    State *st = here();
    return st && mod_defs(static_cast<DictObj *>(rd.v.obj()), SYS_DEFS) &&
           mod_put(static_cast<DictObj *>(rd.v.obj()), "lazy_modules", st->modules);
}

R b_lazy_import(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "name", "globals", "locals", "fromlist", "level" };
    Value v[5];
    if (a.nargs > 5)
        return err_set("TypeError", "__lazy_import__() takes at most 5 arguments");
    for (u32 i = 0; i < a.nargs; i++)
        v[i] = a.args[i];
    for (u32 k = 0; k < a.nkw; k++) {
        Str key = str_of(a.kwnames[k])->str();
        usize i = 0;
        while (i < 5 && NAMES[i] != key)
            i++;
        if (i == 5) {
            Buf<128> b;
            b.put("__lazy_import__() got an unexpected keyword argument '").put(key).put("'");
            return err_set("TypeError", b.str());
        }
        v[i] = a.kwvals[k];
    }
    if (v[0].is_nil())
        return err_set("TypeError", "__lazy_import__() missing required argument 'name'");
    Value g = v[1].is_nil() || is_none(v[1]) ? frame_globals() : v[1];
    if (!is_dict(g)) {
        Buf<96> b;
        b.put("expect dict for globals, got ").put(type_name(g));
        return err_set("TypeError", b.str());
    }
    Value f = vm_frame();
    if (!f.is_nil() && frame_of(f)->globals != frame_of(f)->locals)
        return err_set("SyntaxError", "'lazy import' is only allowed at module level");
    return lazy_import(v[0], v[4].is_nil() ? Value::of_int(0) : v[4], v[3], g, out);
}

bool lazy_methods()
{
    return method_install(&lazy_type, LAZY_METHODS);
}
