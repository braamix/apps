// `sentinel` (PEP 661): a unique marker that prints as its name.
//
// CPython's main branch has it as a builtin, and functools.py makes one at
// import time, so it is here beside the other built-in types.
#include "builtin.h"
#include "frame.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"
#include "union.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

struct SentinelObj : Obj {
    Value name;
    Value module;
    Value repr; // Nil for the name
};

SentinelObj *sentinel_of(Value v)
{
    return static_cast<SentinelObj *>(v.obj());
}

void sentinel_trace(Obj *o)
{
    SentinelObj *s = static_cast<SentinelObj *>(o);
    gc_mark(s->name);
    gc_mark(s->module);
    gc_mark(s->repr);
}

R sentinel_repr(Value v, String &out)
{
    SentinelObj *s = sentinel_of(v);
    Value t        = s->repr.is_nil() ? s->name : s->repr;
    return out.append(str_of(t)->str()) ? R::Ok : oom();
}

R sentinel_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() == "__name__")
        out = sentinel_of(v)->name;
    else if (name->str() == "__module__")
        out = sentinel_of(v)->module;
    else
        return R::NotImpl;
    return R::Ok;
}

R sentinel_setattr(Value v, StrObj *name, Value val)
{
    if (name->str() == "__module__" && !val.is_nil()) {
        sentinel_of(v)->module = val;
        return R::Ok;
    }
    if (name->str() == "__name__" || name->str() == "__module__")
        return err_set("AttributeError", "readonly attribute");
    Buf<160> m;
    m.put("'sentinel' object has no attribute '").put(name->str());
    m.put("' and no __dict__ for setting new attributes");
    return err_set("AttributeError", m.str());
}

R s_self(const CallArgs &a, Value &out)
{
    out = a.args[0];
    return R::Ok;
}

R s_copy(const CallArgs &a, Value &out)
{
    return meth_args(a, "__copy__", 0, 0) ? s_self(a, out) : R::Err;
}

R s_deepcopy(const CallArgs &a, Value &out)
{
    return meth_args(a, "__deepcopy__", 1, 1) ? s_self(a, out) : R::Err;
}

// A sentinel pickles as the global of its name.
R s_reduce(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    out = sentinel_of(a.args[0])->name;
    return R::Ok;
}

constexpr Method SENTINEL_METHODS[] = {
    { "__copy__", s_copy },
    { "__deepcopy__", s_deepcopy },
    { "__reduce__", s_reduce },
};

// The module of the function that made it: its globals' __name__.
Value caller_module()
{
    Value f = vm_frame();
    if (f.is_nil() || frame_of(f)->globals.is_nil())
        return value_none();
    StrObj *n = str_intern("__name__");
    Value out;
    if (!n ||
        dict_get(static_cast<DictObj *>(frame_of(f)->globals.obj()), obj_value(n), out) != R::Ok)
        return err_clear(), value_none();
    return out;
}

} // namespace

constexpr Type sentinel_type{ .name    = "sentinel",
                              .trace   = sentinel_trace,
                              .repr    = sentinel_repr,
                              .binop   = union_binop,
                              .getattr = sentinel_getattr,
                              .setattr = sentinel_setattr,
                              .final   = true };

R b_sentinel(const CallArgs &a, Value &out)
{
    Value repr;
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (n != "repr") {
            Buf<128> m;
            m.put("sentinel() got an unexpected keyword argument '").put(n).put('\'');
            return err_set("TypeError", m.str());
        }
        repr = a.kwvals[k];
    }
    if (a.nargs != 1) {
        char t[24];
        Buf<96> m;
        m.put("sentinel() takes exactly 1 positional argument (");
        m.put(int_text(t, sizeof t, i64(a.nargs))).put(" given)");
        return err_set("TypeError", m.str());
    }
    if (!is_str(a.args[0])) {
        Buf<96> m;
        m.put("sentinel() argument 1 must be str, not ").put(type_name(a.args[0]));
        return err_set("TypeError", m.str());
    }
    if (!repr.is_nil() && is_none(repr))
        repr = Value();
    if (!repr.is_nil() && !is_str(repr)) {
        Buf<96> m;
        m.put("sentinel() argument 'repr' must be str or None, not ").put(type_name(repr));
        return err_set("TypeError", m.str());
    }
    Root rn{ a.args[0] }, rr{ repr }, rm{ caller_module() };
    SentinelObj *s = static_cast<SentinelObj *>(obj_alloc(&sentinel_type, sizeof(SentinelObj)));
    if (!s)
        return oom();
    s->name   = rn.v;
    s->module = rm.v;
    s->repr   = rr.v;
    out       = obj_value(s);
    return R::Ok;
}

bool sentinel_methods()
{
    return method_install(&sentinel_type, SENTINEL_METHODS);
}
