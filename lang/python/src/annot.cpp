// PEP 649, the runtime half: __annotations__ as a call the VM makes.
#include "annot.h"

#include "builtin.h"
#include "call.h"
#include "err.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"

namespace {

// Where the three owners keep the pair. A class hides both behind names of
// its own, so neither is inherited.
struct Names {
    Str annotate;
    Str cache;
};

Names names_of(Value v)
{
    if (is_type(v))
        return { "__annotate_func__", "__annotations_cache__" };
    return { "__annotate__", "__annotations__" };
}

// The dict an owner keeps its names in, or null for a function, which keeps
// them in fields.
DictObj *home_of(Value v)
{
    if (is_type(v))
        return static_cast<DictObj *>(type_obj(v)->dict.obj());
    if (is_module(v))
        return module_dict(v);
    return nullptr;
}

// `key` in the owner's own namespace, never a base's. Nil for absent.
Value own(Value v, Str key)
{
    DictObj *d = home_of(v);
    StrObj *k  = str_intern(key);
    if (!d || !k)
        return Value();
    Value got;
    return dict_get(d, obj_value(k), got) == R::Ok ? got : Value();
}

// A Nil value removes the name.
bool own_set(Value v, Str key, Value x)
{
    Root rv{ v }, rx{ x };
    StrObj *k = str_intern(key);
    if (!k)
        return err_set("MemoryError", "out of memory") == R::Ok;
    DictObj *d = home_of(rv.v);
    if (!d)
        return false;
    if (rx.v.is_nil())
        return dict_del(d, obj_value(k)) != R::Err;
    return dict_set(d, obj_value(k), rx.v) == R::Ok;
}

// What __annotate__(VALUE) answered, kept where the next ask will find it.
R keep(Value v, Value d)
{
    if (is_func(v)) {
        func_of(v)->annotations = d;
        return R::Ok;
    }
    return own_set(v, names_of(v).cache, d) ? R::Ok : R::Err;
}

// s[0] the owner, s[1] its __annotate__.
R annot_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[1], Value::of_int(ANN_VALUE));
    if (!is_dict(in))
        return err_set("TypeError", "__annotate__ returned a non-dict");
    if (keep(k->s[0], in) != R::Ok)
        return R::Err;
    return cont_done(k, in);
}

// The native the lazy attribute hands back, bound to the owner.
R n_annotations(const CallArgs &a, Value &out)
{
    Root self{ a.args[0] };
    Root fn{ annot_func(self.v) };
    if (fn.v.is_nil())
        return R::Err;
    Root kv{ cont_new(annot_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = self.v;
    cont_of(kv.v)->s[1] = fn.v;
    out                 = kv.v;
    return R::Ok;
}

} // namespace

Value annot_func(Value v)
{
    if (is_func(v)) {
        FuncObj *f = func_of(v);
        return f->annotate.is_nil() ? value_none() : f->annotate;
    }
    // A class body that wrote __annotate__ itself is asked first.
    Value got = is_type(v) ? own(v, "__annotate__") : Value();
    if (got.is_nil())
        got = own(v, names_of(v).annotate);
    return got.is_nil() ? value_none() : got;
}

Value annot_cached(Value v)
{
    if (is_func(v)) {
        FuncObj *f = func_of(v);
        if (!f->annotations.is_nil())
            return f->annotations;
        if (!f->annotate.is_nil())
            return Value();
    } else {
        Names n   = names_of(v);
        Value got = is_type(v) ? own(v, "__annotations__") : Value();
        if (got.is_nil())
            got = own(v, n.cache);
        if (!got.is_nil())
            return got;
        got = annot_func(v);
        if (got.is_nil())
            return Value();
        if (!is_none(got))
            return Value();
    }
    // Nothing to evaluate: an empty dict, kept, so that two reads are one
    // object and something stored in it stays.
    Root rv{ v };
    Root d{ obj_value(dict_new()) };
    if (d.v.is_nil())
        return err_set("MemoryError", "out of memory"), Value();
    return keep(rv.v, d.v) == R::Ok ? d.v : Value();
}

Got annot_lazy(Value v, StrObj *name, Value &out, Value &args)
{
    Str n = name->str();
    if (n == "__annotate__") {
        out = annot_func(v);
        return out.is_nil() ? Got::Error : Got::Ok;
    }
    if (n != "__annotations__")
        return Got::Missing;

    Value got = annot_cached(v);
    if (!got.is_nil()) {
        out = got;
        return Got::Ok;
    }
    if (err_pending())
        return Got::Error;

    Root rv{ v };
    Root fn{ native_new("__annotations__", n_annotations) };
    if (fn.v.is_nil())
        return Got::Error;
    Root bound{ method_new(fn.v, rv.v) };
    if (bound.v.is_nil())
        return Got::Error;
    args = obj_value(tuple_new(0));
    if (args.is_nil())
        return Got::Error;
    out = bound.v;
    return Got::Call;
}

R annot_store(Value v, StrObj *name, Value val)
{
    Str n = name->str();
    if (n != "__annotations__" && n != "__annotate__")
        return R::NotImpl;
    bool cache = n == "__annotations__";
    if (cache && !is_dict(val))
        return err_set("TypeError", "__annotations__ must be set to a dict object");
    if (!cache && !is_none(val) && !py_callable(val))
        return err_set("TypeError", "__annotate__ must be callable or None");

    Root rv{ v }, rx{ val };
    if (is_func(rv.v)) {
        FuncObj *f = func_of(rv.v);
        if (cache) {
            f->annotations = rx.v;
        } else {
            f->annotate    = is_none(rx.v) ? Value() : rx.v;
            f->annotations = Value();
        }
        return R::Ok;
    }
    Names nm = names_of(rv.v);
    if (!own_set(rv.v, cache ? nm.cache : nm.annotate, rx.v))
        return R::Err;
    // A new evaluator makes what the old one answered stale.
    if (!cache && !own_set(rv.v, nm.cache, Value()))
        return R::Err;
    return R::Ok;
}

// ------------------------------------------------- the descriptors on `type`

namespace {

// A built-in type has neither: `int.__annotations__` is an AttributeError.
R refuse(Value v, Str name)
{
    Buf<96> b;
    b.put("type object '").put(type_obj(v)->slots.name).put("' has no attribute '");
    b.put(name).put('\'');
    return err_set("AttributeError", b.str());
}

Got g_annotations(Value self, Value &out, Value &args)
{
    if (!is_type(self))
        return err_set("TypeError", "__annotations__ wants a class"), Got::Error;
    if (!type_obj(self)->heap)
        return refuse(self, "__annotations__"), Got::Error;
    StrObj *n = str_intern("__annotations__");
    if (!n)
        return err_set("MemoryError", "out of memory"), Got::Error;
    return annot_lazy(self, n, out, args);
}

Got g_annotate(Value self, Value &out, Value &args)
{
    (void)args;
    if (!is_type(self))
        return err_set("TypeError", "__annotate__ wants a class"), Got::Error;
    if (!type_obj(self)->heap)
        return refuse(self, "__annotate__"), Got::Error;
    out = annot_func(self);
    return out.is_nil() ? Got::Error : Got::Ok;
}

R s_annotations(Value self, Value val)
{
    StrObj *n = str_intern("__annotations__");
    return n ? annot_store(self, n, val) : err_set("MemoryError", "out of memory");
}

R s_annotate(Value self, Value val)
{
    StrObj *n = str_intern("__annotate__");
    return n ? annot_store(self, n, val) : err_set("MemoryError", "out of memory");
}

// inspect reads an MRO through `type.__dict__["__mro__"].__get__`.
Got g_own(Value self, Str name, Value &out)
{
    if (!is_type(self))
        return err_set("TypeError", "a class was expected"), Got::Error;
    if (!type_own_attr(self, name, out))
        return refuse(self, name), Got::Error;
    return Got::Ok;
}

Got g_mro(Value self, Value &out, Value &args)
{
    (void)args;
    return g_own(self, "__mro__", out);
}

Got g_dict(Value self, Value &out, Value &args)
{
    (void)args;
    return g_own(self, "__dict__", out);
}

bool put_getset(Value cls, Str name, Got (*get)(Value, Value &, Value &), R (*set)(Value, Value))
{
    Root rc{ cls };
    Root d{ getset_new(name, get, set) };
    StrObj *k = str_intern(name);
    if (d.v.is_nil() || !k)
        return false;
    return dict_set(static_cast<DictObj *>(type_obj(rc.v)->dict.obj()), obj_value(k), d.v) == R::Ok;
}

} // namespace

bool annot_install()
{
    Root cls{ type_wrap(&type_type) };
    if (cls.v.is_nil())
        return false;
    return put_getset(cls.v, "__annotations__", g_annotations, s_annotations) &&
           put_getset(cls.v, "__annotate__", g_annotate, s_annotate) &&
           put_getset(cls.v, "__mro__", g_mro, nullptr) &&
           put_getset(cls.v, "__dict__", g_dict, nullptr);
}
