// `_types`: the names for built-in types that are not builtins.
//
// CPython's types.py opens with `try: from _types import *` and falls back to
// deriving each name from an expression -- `type(lambda: None)`, `type(_g())`,
// `type(int | str)` -- when there is no such module. That fallback needs
// `async def` and the union operator, neither of which this interpreter has
// yet, so the module is the floor that makes types.py borrowable at all.
//
// The names that are missing are missing honestly: there is no coroutine, no
// union type and no capsule here, so nothing stands in for them. What is here
// is exact for this implementation -- `type(str.join)` really is the same
// native type as `type(len)`, because both are NativeObj.
#include "code.h"
#include "gc.h"
#include "gen.h"
#include "genalias.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "module.h"
#include "ops.h"
#include "type.h"

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

constexpr Type space_type{ .name    = "SimpleNamespace",
                           .trace   = space_trace,
                           .eq      = space_eq,
                           .repr    = space_repr,
                           .getattr = space_getattr,
                           .setattr = space_setattr };

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
    { "EllipsisType", &ellipsis_type },
    { "NoneType", &none_type },
    { "NotImplementedType", &notimpl_type },
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
    return mod_type(d, &space_type, b_space);
}
