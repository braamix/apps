// `_types`: the names for built-in types that are not builtins.
//
// CPython's types.py opens with `try: from _types import *` and falls back to
// deriving each name from an expression -- `type(lambda: None)`, `type(_g())`,
// `type(int | str)` -- when there is no such module. The module is what
// types.py takes first, so each name is this implementation's own.
//
// The names that are missing are missing honestly: there is no capsule here,
// so nothing stands in for it. What is here is exact for this implementation
// -- `type(str.join)` really is the same native type as `type(len)`, because
// both are NativeObj.
#include "builtin.h"
#include "code.h"
#include "exc.h"
#include "gc.h"
#include "gen.h"
#include "genalias.h"
#include "intern.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "lazy.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "reduce.h"
#include "type.h"
#include "union.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// ------------------------------------------------------------ SimpleNamespace

// An object whose attributes are its whole content. CPython writes it in C and
// types.py re-exports it; a fair amount of code builds one instead of a class.
struct SpaceObj : Obj {
    Value dict;
};

SpaceObj *space_of(Value v)
{
    return static_cast<SpaceObj *>(v.obj());
}

void space_trace(Obj *o)
{
    gc_mark(static_cast<SpaceObj *>(o)->dict);
}

DictObj *space_dict(Value v)
{
    return static_cast<DictObj *>(space_of(v)->dict.obj());
}

R space_repr(Value v, String &out)
{
    Root rv{ v };
    if (!out.append("namespace("))
        return oom();
    usize at = 0;
    Value k, x;
    bool first = true;
    while (table_next(space_dict(rv.v)->t, at, k, x)) {
        if (!first && !out.append(", "))
            return oom();
        first = false;
        if (!is_str(k))
            continue;
        if (!out.append(str_of(k)->str()) || !out.push('='))
            return oom();
        if (py_repr(x, out) != R::Ok)
            return R::Err;
    }
    return out.push(')') ? R::Ok : oom();
}

R space_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() == "__dict__") {
        out = space_of(v)->dict;
        return R::Ok;
    }
    return dict_get(space_dict(v), obj_value(name), out);
}

R space_setattr(Value v, StrObj *name, Value val)
{
    Root rv{ v }, rx{ val };
    if (rx.v.is_nil())
        return dict_del(space_dict(rv.v), obj_value(name)) == R::NotImpl
                   ? err_set2("AttributeError", "no such attribute", name->str())
                   : R::Ok;
    return dict_set(space_dict(rv.v), obj_value(name), rx.v);
}

// Two namespaces are equal when their attributes are, which is what CPython's
// comparison says.
R space_eq(Value a, Value b, bool &out)
{
    if (!b.is_obj() || b.obj()->type != a.obj()->type)
        return R::NotImpl;
    return py_eq(space_of(a)->dict, space_of(b)->dict, out);
}

constexpr Type space_type{ .name    = "types.SimpleNamespace",
                           .trace   = space_trace,
                           .eq      = space_eq,
                           .repr    = space_repr,
                           .getattr = space_getattr,
                           .setattr = space_setattr };

// __reduce__: (type, (), the attributes).
R space_reduce(const CallArgs &a, Value &out)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != &space_type ||
        !meth_args(a, "__reduce__", 0, 0))
        return err_pending() ? R::Err : err_set("TypeError", "__reduce__ needs a namespace");
    Root self{ a.args[0] };
    Root none{ tuple_of() };
    if (none.v.is_nil())
        return R::Err;
    out = reduce_of(self.v, none.v, space_of(self.v)->dict);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method SPACE_METHODS[] = { { "__reduce__", space_reduce } };

R b_space(const CallArgs &a, Value &out)
{
    if (a.nargs)
        return err_set("TypeError", "SimpleNamespace() takes no positional arguments");
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) };
    for (u32 k = 0; k < a.nkw; k++)
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), a.kwnames[k], a.kwvals[k]) != R::Ok)
            return R::Err;
    SpaceObj *o = static_cast<SpaceObj *>(obj_alloc(&space_type, sizeof(SpaceObj)));
    if (!o)
        return oom();
    o->dict = rd.v;
    out     = obj_value(o);
    return R::Ok;
}

} // namespace

Value namespace_new(Value dict)
{
    Root rd{ dict };
    SpaceObj *o = static_cast<SpaceObj *>(obj_alloc(&space_type, sizeof(SpaceObj)));
    if (!o)
        return oom(), Value();
    o->dict = rd.v;
    return obj_value(o);
}

namespace {

// --------------------------------------------------------------- mappingproxy

// A read-only view of a mapping: `type.__dict__`, and what enum hands out.
// The slots read the mapping natively; the methods call the mapping's own,
// so a mapping written in Python answers those.
struct ProxyObj : Obj {
    Value inner;
};

Value inner_of(Value v)
{
    return static_cast<ProxyObj *>(v.obj())->inner;
}

void mp_trace(Obj *o)
{
    gc_mark(static_cast<ProxyObj *>(o)->inner);
}

R mp_repr(Value v, String &out)
{
    Root rv{ v };
    if (!out.append("mappingproxy("))
        return oom();
    if (py_repr(inner_of(rv.v), out) != R::Ok)
        return R::Err;
    return out.push(')') ? R::Ok : oom();
}

R mp_str(Value v, String &out)
{
    return py_str(inner_of(v), out);
}

R mp_len(Value v, usize &out)
{
    return py_len(inner_of(v), out);
}

R mp_getitem(Value v, Value key, Value &out)
{
    return py_getitem(inner_of(v), key, out);
}

R mp_contains(Value v, Value key, bool &out)
{
    return py_contains(inner_of(v), key, out);
}

R mp_hash(Value v, u32 &out)
{
    return py_hash(inner_of(v), out);
}

Value mp_iter(Value v)
{
    return py_iter(inner_of(v));
}

R mp_eq(Value a, Value b, bool &out)
{
    if (!is_mappingproxy(a)) {
        Value t = a;
        a       = b;
        b       = t;
    }
    return py_eq(inner_of(a), is_mappingproxy(b) ? inner_of(b) : b, out);
}

// `proxy | x` is `mapping | x`, and the other way round.
R mp_binop(Value a, Value b, Op op, Value &out)
{
    if (op != Op::Or)
        return R::NotImpl;
    return py_binop(is_mappingproxy(a) ? inner_of(a) : a, is_mappingproxy(b) ? inner_of(b) : b, op,
                    out);
}

// The mapping's own method of that name, called with what was given.
R forward(const CallArgs &a, Str name, Value &out)
{
    if (!a.nargs || !is_mappingproxy(a.args[0]))
        return err_set2("TypeError", "a mappingproxy method needs a mappingproxy", name);
    StrObj *n = str_intern(name);
    if (!n)
        return oom();
    Root m;
    if (py_getattr(inner_of(a.args[0]), n, m.v) != R::Ok)
        return R::Err;
    TupleObj *t = tuple_new(a.nargs - 1);
    if (!t)
        return oom();
    for (u32 i = 1; i < a.nargs; i++)
        t->items()[i - 1] = a.args[i];
    Root rt{ obj_value(t) };
    out = attr_invoke(m.v, rt.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R mp_get(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "get() takes no keyword arguments");
    if (a.nargs < 2 || a.nargs > 3) {
        char n[24];
        Buf<96> b;
        b.put("get expected ").put(a.nargs < 2 ? "at least 1 argument" : "at most 2 arguments");
        b.put(", got ").put(int_text(n, sizeof n, i64(a.nargs ? a.nargs - 1 : 0)));
        return err_set("TypeError", b.str());
    }
    return forward(a, "get", out);
}

R mp_keys(const CallArgs &a, Value &out)
{
    return meth_args(a, "keys", 0, 0) ? forward(a, "keys", out) : R::Err;
}

R mp_values(const CallArgs &a, Value &out)
{
    return meth_args(a, "values", 0, 0) ? forward(a, "values", out) : R::Err;
}

R mp_items(const CallArgs &a, Value &out)
{
    return meth_args(a, "items", 0, 0) ? forward(a, "items", out) : R::Err;
}

R mp_copy(const CallArgs &a, Value &out)
{
    return meth_args(a, "copy", 0, 0) ? forward(a, "copy", out) : R::Err;
}

R mp_reversed(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__reversed__", 0, 0) || !is_mappingproxy(a.args[0]))
        return R::Err;
    out = reversed_new(inner_of(a.args[0]));
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method MP_METHODS[] = {
    { "get", mp_get },     { "keys", mp_keys }, { "values", mp_values },
    { "items", mp_items }, { "copy", mp_copy }, { "__reversed__", mp_reversed },
};

// What CPython's PyMapping_Check says, less the sequences it would admit.
bool mapping_like(Value v)
{
    if (is_inst(v))
        return type_has_special(v, "__getitem__") && !is_list(inst_of(v)->native) &&
               !is_tuple(inst_of(v)->native);
    const Type *t = type_of(v);
    return t && t->getitem && !is_list(v) && !is_tuple(v) && !is_str(v) && !is_bytes(v) &&
           !is_bytearray(v) && t != &range_type && t != &memview_type;
}

R b_mappingproxy(const CallArgs &a, Value &out)
{
    Value m;
    if (a.nargs == 1 && !a.nkw)
        m = a.args[0];
    else if (!a.nargs && a.nkw == 1 && is_str(a.kwnames[0]) &&
             str_of(a.kwnames[0])->str() == "mapping")
        m = a.kwvals[0];
    else if (!a.nargs && !a.nkw)
        return err_set("TypeError", "mappingproxy() missing required argument 'mapping' (pos 1)");
    else
        return err_set("TypeError", "mappingproxy() takes exactly one argument");
    if (!mapping_like(m)) {
        Buf<128> b;
        b.put("mappingproxy() argument must be a mapping, not ").put(type_name(m));
        return err_set("TypeError", b.str());
    }
    out = mappingproxy_new(m);
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

constexpr Type mappingproxy_type{ .name     = "mappingproxy",
                                  .trace    = mp_trace,
                                  .hash     = mp_hash,
                                  .eq       = mp_eq,
                                  .repr     = mp_repr,
                                  .str      = mp_str,
                                  .len      = mp_len,
                                  .getitem  = mp_getitem,
                                  .contains = mp_contains,
                                  .binop    = mp_binop,
                                  .iter     = mp_iter,
                                  .patma    = PATMA_MAP,
                                  .final    = true };

Value mappingproxy_new(Value mapping)
{
    Root rm{ mapping };
    ProxyObj *p = static_cast<ProxyObj *>(obj_alloc(&mappingproxy_type, sizeof(ProxyObj)));
    if (!p)
        return oom(), Value();
    p->inner = rm.v;
    return obj_value(p);
}

Value mappingproxy_inner(Value proxy)
{
    return inner_of(proxy);
}

bool mappingproxy_methods()
{
    if (!method_install(&mappingproxy_type, MP_METHODS))
        return false;
    Root fn{ native_new("mappingproxy", b_mappingproxy) };
    static const Type *const GENERIC[] = { &mappingproxy_type };
    return !fn.v.is_nil() && type_set_ctor(&mappingproxy_type, fn.v) &&
           genalias_install(GENERIC, 1);
}

namespace {

// ------------------------------------------------------------- the type names

struct Named {
    Str name;
    const Type *t;
};

// `LambdaType` and `FunctionType` are one type here and so are the four
// descriptor names, because every builtin in this interpreter is a NativeObj.
// That is a fact about this implementation, not a shortcut.
const Named NAMES[] = {
    { "FunctionType", &func_type },
    { "LambdaType", &func_type },
    { "CodeType", &code_type },
    { "CellType", &cell_type },
    { "MethodType", &method_type },
    { "ModuleType", &module_type },
    { "GeneratorType", &gen_type },
    { "CoroutineType", &coro_type },
    { "AsyncGeneratorType", &agen_type },
    { "FrameType", &frame_type },
    { "BuiltinFunctionType", &native_type },
    { "BuiltinMethodType", &native_type },
    { "WrapperDescriptorType", &native_type },
    { "MethodWrapperType", &native_type },
    { "MethodDescriptorType", &native_type },
    { "ClassMethodDescriptorType", &native_type },
    { "MemberDescriptorType", &member_type },
    { "GetSetDescriptorType", &property_type },
    { "GenericAlias", &genalias_type },
    { "UnionType", &union_type },
    { "LazyImportType", &lazy_type },
    { "EllipsisType", &ellipsis_type },
    { "NoneType", &none_type },
    { "NotImplementedType", &notimpl_type },
    { "MappingProxyType", &mappingproxy_type },
};

} // namespace

bool types_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    for (const Named &one : NAMES) {
        Root w{ type_wrap(one.t) };
        if (w.v.is_nil() || !mod_put(d, one.name, w.v))
            return false;
    }
    if (!method_install(&space_type, SPACE_METHODS) || !mod_type(d, &space_type, b_space))
        return false;
    Root tb{ type_wrap(&traceback_type) };
    Root fn{ native_new("traceback", traceback_ctor) };
    return !tb.v.is_nil() && !fn.v.is_nil() && type_set_ctor(&traceback_type, fn.v) &&
           mod_put(static_cast<DictObj *>(rd.v.obj()), "TracebackType", tb.v);
}

// ------------------------------------------------------------- ModuleType

// GenericAlias(origin, args): `origin[args]`, made by hand.
R b_genericalias(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "GenericAlias() takes no keyword arguments");
    if (a.nargs != 2) {
        char n[24];
        Buf<96> b;
        b.put("GenericAlias expected 2 arguments, got ").put(int_text(n, sizeof n, i64(a.nargs)));
        return err_set("TypeError", b.str());
    }
    out = genalias_new(a.args[0], a.args[1]);
    return out.is_nil() ? R::Err : R::Ok;
}

// method(function, instance): a bound method made by hand.
R b_method(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "method() takes no keyword arguments");
    if (a.nargs != 2) {
        char n[24];
        Buf<96> b;
        b.put("method expected 2 arguments, got ").put(int_text(n, sizeof n, i64(a.nargs)));
        return err_set("TypeError", b.str());
    }
    if (!py_callable(a.args[0]))
        return err_set("TypeError", "first argument must be callable");
    if (is_none(a.args[1]))
        return err_set("TypeError", "instance must not be None");
    out = method_new(a.args[0], a.args[1]);
    return out.is_nil() ? R::Err : R::Ok;
}

// module(name, doc=None): what `import` makes, made by hand.
R b_module(const CallArgs &a, Value &out)
{
    static constexpr Str PARAMS[] = { "name", "doc" };
    Value got[2];
    for (u32 i = 0; i < a.nargs && i < 2; i++)
        got[i] = a.args[i];
    if (a.nargs > 2)
        return err_set("TypeError", "module() takes at most 2 arguments");
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        u32 i = n == PARAMS[0] ? 0 : n == PARAMS[1] ? 1 : 2;
        if (i == 2) {
            Buf<96> b;
            b.put("module() got an unexpected keyword argument '").put(n).put('\'');
            return err_set("TypeError", b.str());
        }
        got[i] = a.kwvals[k];
    }
    if (got[0].is_nil())
        return err_set("TypeError", "module() missing required argument 'name' (pos 1)");
    if (!is_str(got[0])) {
        Buf<96> b;
        b.put("module() argument 'name' must be str, not ").put(type_name(got[0]));
        return err_set("TypeError", b.str());
    }
    Root doc{ got[1].is_nil() ? value_none() : got[1] };
    Root m{ module_new(str_of(got[0])->str()) };
    if (m.v.is_nil())
        return R::Err;
    DictObj *d = module_dict(m.v);
    if (!mod_put(d, "__doc__", doc.v) || !mod_put(module_dict(m.v), "__package__", value_none()) ||
        !mod_put(module_dict(m.v), "__loader__", value_none()) ||
        !mod_put(module_dict(m.v), "__spec__", value_none()))
        return R::Err;
    out = m.v;
    return R::Ok;
}
