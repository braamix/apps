// Cells, functions, builtins and modules.
#include "func.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "ops.h"

namespace {

void cell_trace(Obj *o)
{
    gc_mark(static_cast<CellObj *>(o)->v);
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
}

R func_repr(Value v, String &out)
{
    CodeObj *c = code_of(func_of(v)->code);
    Buf<96> b;
    b.put("<function ").put(is_str(c->name) ? str_of(c->name)->str() : Str("?")).put('>');
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
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
    return dict_get(module_dict(v), obj_value(name), out);
}

} // namespace

constexpr Type cell_type{ .name = "cell", .trace = cell_trace, .repr = cell_repr };

constexpr Type func_type{ .name = "function", .trace = func_trace, .repr = func_repr };

constexpr Type native_type{ .name = "builtin_function_or_method", .repr = native_repr };

constexpr Type module_type{ .name    = "module",
                            .trace   = module_trace,
                            .repr    = module_repr,
                            .getattr = module_getattr };

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
    f->code       = rc.v;
    f->globals    = rg.v;
    f->defaults   = Value();
    f->kwdefaults = Value();
    f->closure    = Value();
    return obj_value(f);
}

Value native_new(Str name, R (*fn)(const CallArgs &, Value &out))
{
    NativeObj *n = static_cast<NativeObj *>(obj_alloc(&native_type, sizeof(NativeObj)));
    if (!n)
        return err_set("MemoryError", "out of memory"), Value();
    n->name = name;
    n->fn   = fn;
    return obj_value(n);
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
    return obj_value(m);
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
