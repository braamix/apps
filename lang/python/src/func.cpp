// Cells, functions, builtins and modules.
#include "func.h"

#include "annot.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "lazy.h"
#include "ops.h"

namespace {

void cell_trace(Obj *o)
{
    gc_mark(static_cast<CellObj *>(o)->v);
}

// cell.cell_contents, which annotationlib reads and writes to rebuild the
// closure of an __annotate__.
R cell_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() != "cell_contents")
        return R::NotImpl;
    CellObj *c = static_cast<CellObj *>(v.obj());
    if (c->v.is_nil())
        return err_set("ValueError", "Cell is empty");
    out = c->v;
    return R::Ok;
}

R cell_setattr(Value v, StrObj *name, Value val)
{
    if (name->str() != "cell_contents")
        return R::NotImpl;
    static_cast<CellObj *>(v.obj())->v = val;
    return R::Ok;
}

R cell_repr(Value v, String &out)
{
    (void)v;
    return out.append("<cell>") ? R::Ok : err_set("MemoryError", "out of memory");
}

void func_trace(Obj *o)
{
    FuncObj *f = static_cast<FuncObj *>(o);
    gc_mark(f->code);
    gc_mark(f->globals);
    gc_mark(f->defaults);
    gc_mark(f->kwdefaults);
    gc_mark(f->closure);
    gc_mark(f->name);
    gc_mark(f->qualname);
    gc_mark(f->doc);
    gc_mark(f->dict);
    gc_mark(f->annotate);
    gc_mark(f->annotations);
}

R func_repr(Value v, String &out)
{
    FuncObj *f = func_of(v);
    Buf<96> b;
    b.put("<function ").put(is_str(f->qualname) ? str_of(f->qualname)->str() : Str("?")).put('>');
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

// The dict a function keeps its own attributes in, made on first use.
DictObj *func_dict(Value v)
{
    Root rv{ v };
    if (func_of(rv.v)->dict.is_nil()) {
        DictObj *d = dict_new();
        if (!d)
            return err_set("MemoryError", "out of memory"), nullptr;
        func_of(rv.v)->dict = obj_value(d);
    }
    return static_cast<DictObj *>(func_of(rv.v)->dict.obj());
}

// The fixed attributes, then whatever was stored on the function itself.
R func_getattr(Value v, StrObj *name, Value &out)
{
    FuncObj *f = func_of(v);
    Str n      = name->str();
    if (n == "__name__")
        out = f->name;
    else if (n == "__qualname__")
        out = f->qualname;
    else if (n == "__doc__")
        out = f->doc.is_nil() ? value_none() : f->doc;
    else if (n == "__code__")
        out = f->code;
    else if (n == "__globals__")
        out = f->globals;
    else if (n == "__defaults__")
        out = f->defaults.is_nil() ? value_none() : f->defaults;
    else if (n == "__kwdefaults__")
        out = f->kwdefaults.is_nil() ? value_none() : f->kwdefaults;
    else if (n == "__closure__")
        out = f->closure.is_nil() ? value_none() : f->closure;
    else if (n == "__dict__")
        out = obj_value(func_dict(v));
    else if (n == "__builtins__") {
        StrObj *k = str_intern("__builtins__");
        if (!k)
            return err_set("MemoryError", "out of memory");
        R r = dict_get(static_cast<DictObj *>(f->globals.obj()), obj_value(k), out);
        if (r == R::NotImpl)
            return r;
        // A module's __builtins__ is the module; what is wanted is its dict.
        if (is_module(out))
            out = obj_value(module_dict(out));
        return r;
    } else if (n == "__module__") {
        StrObj *k = str_intern("__name__");
        if (!k)
            return err_set("MemoryError", "out of memory");
        R r = dict_get(static_cast<DictObj *>(f->globals.obj()), obj_value(k), out);
        if (r == R::NotImpl)
            out = value_none();
        return r == R::Err ? r : R::Ok;
    } else {
        R r = f->dict.is_nil()
                  ? R::NotImpl
                  : dict_get(static_cast<DictObj *>(f->dict.obj()), obj_value(name), out);
        if (r == R::NotImpl && n == "__type_params__") {
            TupleObj *none = tuple_new(0);
            if (!none)
                return err_set("MemoryError", "out of memory");
            out = obj_value(none);
            return R::Ok;
        }
        return r;
    }
    return out.is_nil() ? R::NotImpl : R::Ok;
}

R wrong(Str name, Str want)
{
    Buf<96> b;
    b.put(name).put(" must be set to ").put(want);
    return err_set("TypeError", b.str());
}

R func_setattr(Value v, StrObj *name, Value val)
{
    Root rv{ v }, rx{ val };
    FuncObj *f = func_of(rv.v);
    Str n      = name->str();
    if (n == "__name__" || n == "__qualname__") {
        if (!is_str(rx.v))
            return wrong(n, "a string object");
        (n == "__name__" ? f->name : f->qualname) = rx.v;
        return R::Ok;
    }
    if (n == "__doc__") {
        f->doc = rx.v;
        return R::Ok;
    }
    if (n == "__code__") {
        if (!is_code(rx.v))
            return wrong(n, "a code object");
        f->code = rx.v;
        return R::Ok;
    }
    if (n == "__defaults__") {
        if (!is_none(rx.v) && !is_tuple(rx.v))
            return wrong(n, "a tuple object");
        f->defaults = is_none(rx.v) ? Value() : rx.v;
        return R::Ok;
    }
    if (n == "__kwdefaults__") {
        if (!is_none(rx.v) && !is_dict(rx.v))
            return wrong(n, "a dict object");
        f->kwdefaults = is_none(rx.v) ? Value() : rx.v;
        return R::Ok;
    }
    if (n == "__globals__" || n == "__closure__")
        return err_set2("AttributeError", "readonly attribute", n);
    R r = annot_store(rv.v, name, rx.v);
    if (r != R::NotImpl)
        return r;
    DictObj *d = func_dict(rv.v);
    return d ? dict_set(d, obj_value(name), rx.v) : R::Err;
}

// __annotations__ is what __annotate__ answers, so reading it may be a call.
Got func_lazy(Value v, StrObj *name, Value &out, Value &args)
{
    return annot_lazy(v, name, out, args);
}

R native_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "__module__") {
        // A method of a built-in type has no module, as CPython's has none:
        // the attribute is there and is None, which is what `inspect` and
        // doctest's _from_module ask for.
        Value owner = static_cast<NativeObj *>(v.obj())->owner;
        out         = is_str(owner) ? owner : value_none();
        return R::Ok;
    }
    if (n != "__name__" && n != "__qualname__")
        return R::NotImpl;
    out = str_new(static_cast<NativeObj *>(v.obj())->name);
    return out.is_nil() ? R::Err : R::Ok;
}

void native_trace(Obj *o)
{
    gc_mark(static_cast<NativeObj *>(o)->owner);
}

R native_repr(Value v, String &out)
{
    Buf<96> b;
    b.put("<built-in function ").put(static_cast<NativeObj *>(v.obj())->name).put('>');
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

void module_trace(Obj *o)
{
    ModuleObj *m = static_cast<ModuleObj *>(o);
    gc_mark(m->name);
    gc_mark(m->dict);
}

R module_repr(Value v, String &out)
{
    ModuleObj *m = static_cast<ModuleObj *>(v.obj());
    Buf<96> b;
    b.put("<module '").put(is_str(m->name) ? str_of(m->name)->str() : Str("?")).put("'>");
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

R module_getattr(Value v, StrObj *name, Value &out)
{
    R r = dict_get(module_dict(v), obj_value(name), out);
    if (r != R::NotImpl)
        return r;
    // The namespace itself, which is not in the namespace.
    if (name->str() == "__dict__") {
        out = obj_value(module_dict(v));
        return R::Ok;
    }
    return R::NotImpl;
}

// A name a lazy import still owes, then PEP 649's pair.
Got module_lazy(Value v, StrObj *name, Value &out, Value &args)
{
    Got g = lazy_module_attr(v, name, out, args);
    return g == Got::Missing ? annot_lazy(v, name, out, args) : g;
}

// `sys.stdout = x` and `del mod.name`: a module's namespace is its dict, and
// an attribute of one is an entry in it.
R module_setattr(Value v, StrObj *name, Value val)
{
    Root rv{ v }, rn{ obj_value(name) }, rx{ val };
    if (rx.v.is_nil()) {
        R r = dict_del(module_dict(rv.v), rn.v);
        return r == R::NotImpl ? err_set2("AttributeError", "module has no attribute", name->str())
                               : r;
    }
    return dict_set(module_dict(rv.v), rn.v, rx.v);
}

} // namespace

constexpr Type cell_type{ .name    = "cell",
                          .trace   = cell_trace,
                          .repr    = cell_repr,
                          .getattr = cell_getattr,
                          .setattr = cell_setattr };

constexpr Type func_type{ .name     = "function",
                          .trace    = func_trace,
                          .repr     = func_repr,
                          .getattr  = func_getattr,
                          .setattr  = func_setattr,
                          .lazyattr = func_lazy };

constexpr Type native_type{ .name    = "builtin_function_or_method",
                            .trace   = native_trace,
                            .repr    = native_repr,
                            .getattr = native_getattr };

constexpr Type module_type{ .name     = "module",
                            .trace    = module_trace,
                            .repr     = module_repr,
                            .getattr  = module_getattr,
                            .setattr  = module_setattr,
                            .lazyattr = module_lazy };

CellObj *cell_new()
{
    CellObj *c = static_cast<CellObj *>(obj_alloc(&cell_type, sizeof(CellObj)));
    if (!c)
        return err_set("MemoryError", "out of memory"), nullptr;
    c->v = Value();
    return c;
}

Value func_new(Value code, Value globals)
{
    Root rc{ code }, rg{ globals };
    FuncObj *f = static_cast<FuncObj *>(obj_alloc(&func_type, sizeof(FuncObj)));
    if (!f)
        return err_set("MemoryError", "out of memory"), Value();
    f->code        = rc.v;
    f->globals     = rg.v;
    f->defaults    = Value();
    f->kwdefaults  = Value();
    f->closure     = Value();
    f->name        = code_of(rc.v)->name;
    f->qualname    = code_of(rc.v)->qualname;
    f->doc         = code_of(rc.v)->doc;
    f->dict        = Value();
    f->annotate    = Value();
    f->annotations = Value();
    return obj_value(f);
}

Value native_new(Str name, R (*fn)(const CallArgs &, Value &out))
{
    NativeObj *n = static_cast<NativeObj *>(obj_alloc(&native_type, sizeof(NativeObj)));
    if (!n)
        return err_set("MemoryError", "out of memory"), Value();
    n->name  = name;
    n->fn    = fn;
    n->owner = Value();
    return obj_value(n);
}

bool module_defaults(DictObj *d)
{
    Root rd{ obj_value(d) };
    constexpr Str NONES[] = { "__doc__", "__package__", "__loader__", "__spec__" };
    for (Str k : NONES) {
        StrObj *ks = str_intern(k);
        if (!ks)
            return err_set("MemoryError", "out of memory"), false;
        Value had;
        R r = dict_get(static_cast<DictObj *>(rd.v.obj()), obj_value(ks), had);
        if (r == R::Err)
            return false;
        if (r == R::NotImpl &&
            dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(ks), value_none()) != R::Ok)
            return false;
    }
    return true;
}

Value module_new(Str name)
{
    Root n{ str_new(name) };
    if (n.v.is_nil())
        return Value();
    DictObj *d = dict_new();
    if (!d)
        return err_set("MemoryError", "out of memory"), Value();
    Root rd{ obj_value(d) };
    ModuleObj *m = static_cast<ModuleObj *>(obj_alloc(&module_type, sizeof(ModuleObj)));
    if (!m)
        return err_set("MemoryError", "out of memory"), Value();
    m->name = n.v;
    m->dict = rd.v;
    Root rm{ obj_value(m) };
    // The body of the module reads its own name, so it is in the namespace
    // and not only on the object; the other four are what ModuleType's own
    // __init__ puts there.
    StrObj *key = str_intern("__name__");
    if (!key || dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(key), n.v) != R::Ok)
        return Value();
    constexpr Str NONES[] = { "__doc__", "__package__", "__loader__", "__spec__" };
    for (Str k : NONES) {
        StrObj *ks = str_intern(k);
        if (!ks ||
            dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(ks), value_none()) != R::Ok)
            return err_pending() ? Value() : (err_set("MemoryError", "out of memory"), Value());
    }
    return rm.v;
}

bool args_only(const CallArgs &a, Str who, u32 least, u32 most)
{
    if (a.nkw) {
        Buf<96> b;
        b.put(who).put("() takes no keyword arguments");
        return err_set("TypeError", b.str()), false;
    }
    if (a.nargs < least || a.nargs > most) {
        char tmp[24];
        Buf<128> b;
        b.put(who).put("() takes ");
        if (least == most)
            b.put("exactly ").put(int_text(tmp, sizeof tmp, i64(least)));
        else
            b.put("from ")
                .put(int_text(tmp, sizeof tmp, i64(least)))
                .put(" to ")
                .put(int_text(tmp, sizeof tmp, i64(most)));
        b.put(" arguments (").put(int_text(tmp, sizeof tmp, i64(a.nargs))).put(" given)");
        return err_set("TypeError", b.str()), false;
    }
    return true;
}
