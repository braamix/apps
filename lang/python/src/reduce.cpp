#include "reduce.h"

#include "builtin.h"
#include "exc.h"
#include "func.h"
#include "gc.h"
#include "intern.h"
#include "method.h"
#include "ops.h"
#include "type.h"

Value tuple_from(const Value *items, usize n)
{
    Roots pin{ const_cast<Value *>(items), n };
    TupleObj *t = tuple_new(n);
    if (!t)
        return err_set("MemoryError", "out of memory"), Value();
    for (usize i = 0; i < n; i++)
        t->items()[i] = items[i];
    return obj_value(t);
}

Value inst_state(Value self)
{
    if (!is_inst(self))
        return value_none();
    Value d = inst_of(self)->dict;
    return d.is_nil() || !static_cast<DictObj *>(d.obj())->t.live ? value_none() : d;
}

Value reduce_of(Value self, Value args, Value state)
{
    Root rs{ self }, ra{ args }, rt{ state };
    Root cls{ type_of_value(rs.v) };
    if (cls.v.is_nil())
        return Value();
    return tuple_of(cls.v, ra.v, rt.v);
}

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// NotImplemented and Ellipsis: a name, which pickle looks up in builtins.
R singleton_reduce(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__reduce__", 1, 1))
        return R::Err;
    out = str_new(a.args[0].obj()->type == &notimpl_type ? Str("NotImplemented") : Str("Ellipsis"));
    return out.is_nil() ? R::Err : R::Ok;
}

// A builtin function: its name, found again in the module that has it. A
// method of a type is getattr(type, name), since the type is what has it.
R native_reduce(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__reduce__", 1, 1) || !is_native(a.args[0]))
        return err_pending() ? R::Err : err_set("TypeError", "__reduce__ needs a builtin");
    Root self{ a.args[0] };
    Root name{ str_new(static_cast<NativeObj *>(self.v.obj())->name) };
    Value owner = static_cast<NativeObj *>(self.v.obj())->owner;
    if (!owner.is_nil() && !is_type(owner))
        owner = Value(); // a module's: its name is enough, and pickle finds it
    // The name it is kept under: object's defaults are all named "object".
    if (!owner.is_nil()) {
        usize at = 0;
        Value k, v;
        while (table_next(static_cast<DictObj *>(type_obj(owner)->dict.obj())->t, at, k, v))
            if (v == self.v) {
                name = k;
                break;
            }
    }
    if (name.v.is_nil() || owner.is_nil()) {
        out = name.v;
        return out.is_nil() ? R::Err : R::Ok;
    }
    StrObj *gn = str_intern("getattr");
    Root getattr_fn;
    if (!gn || dict_get(builtins_dict(), obj_value(gn), getattr_fn.v) != R::Ok)
        return err_pending() ? R::Err : err_set("SystemError", "no builtins.getattr");
    Root args{ tuple_of(owner, name.v) };
    if (args.v.is_nil())
        return R::Err;
    out = tuple_of(getattr_fn.v, args.v);
    return out.is_nil() ? R::Err : R::Ok;
}

// A bound method: getattr(self, name).
R method_reduce(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__reduce__", 1, 1) || !is_method(a.args[0]))
        return err_pending() ? R::Err : err_set("TypeError", "__reduce__ needs a method");
    Root m{ a.args[0] };
    StrObj *gn = str_intern("getattr");
    StrObj *nn = str_intern("__name__");
    if (!gn || !nn)
        return oom();
    Root getattr_fn, name;
    if (dict_get(builtins_dict(), obj_value(gn), getattr_fn.v) != R::Ok)
        return err_pending() ? R::Err : err_set("SystemError", "no builtins.getattr");
    if (py_getattr(static_cast<MethodObj *>(m.v.obj())->fn, nn, name.v) != R::Ok)
        return R::Err;
    Root args{ tuple_of(static_cast<MethodObj *>(m.v.obj())->self, name.v) };
    if (args.v.is_nil())
        return R::Err;
    out = tuple_of(getattr_fn.v, args.v);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method SINGLETON[] = { { "__reduce__", singleton_reduce } };
constexpr Method NATIVE[]    = { { "__reduce__", native_reduce } };
constexpr Method METHOD[]    = { { "__reduce__", method_reduce } };

} // namespace

bool reduce_methods()
{
    return method_install(&notimpl_type, SINGLETON) && method_install(&ellipsis_type, SINGLETON) &&
           method_install(&native_type, NATIVE) && method_install(&method_type, METHOD);
}
