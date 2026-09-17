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
// its own so that neither is inherited and neither collides with an
// annotation called `__annotations__`.
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

// A class is the only thing these are installed for, and a built-in one has
// neither: `int.__annotations__` is an AttributeError, as it is in CPython.
R refuse(Value v, Str name)
{
    Buf<96> b;
    b.put("type object '").put(type_obj(v)->slots.name).put("' has no attribute '");
    b.put(name).put('\'');
    return err_set("AttributeError", b.str());
}

R n_type_annotations(const CallArgs &a, Value &out)
{
    if (!a.nargs || !is_type(a.args[0]))
        return err_set("TypeError", "__annotations__ wants a class");
    Root self{ a.args[0] };
    if (!type_obj(self.v)->heap)
        return refuse(self.v, "__annotations__");
    Value args;
    StrObj *n = str_intern("__annotations__");
    if (!n)
        return err_set("MemoryError", "out of memory");
    switch (annot_lazy(self.v, n, out, args)) {
    case Got::Ok:
        return R::Ok;
    case Got::Call:
        out = attr_invoke(out, args);
        return out.is_nil() ? R::Err : R::Ok;
    default:
        return R::Err;
    }
}

R n_type_annotate(const CallArgs &a, Value &out)
{
    if (!a.nargs || !is_type(a.args[0]))
        return err_set("TypeError", "__annotate__ wants a class");
    Root self{ a.args[0] };
    if (!type_obj(self.v)->heap)
        return refuse(self.v, "__annotate__");
    out = annot_func(self.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R type_set(const CallArgs &a, Str which, Value &out)
{
    if (a.nargs < 2 || !is_type(a.args[0]))
        return err_set("TypeError", "a class was expected");
    Root self{ a.args[0] }, val{ a.args[1] };
    StrObj *n = str_intern(which);
    if (!n)
        return err_set("MemoryError", "out of memory");
    out = value_none();
    return annot_store(self.v, n, val.v);
}

R n_set_annotations(const CallArgs &a, Value &out)
{
    return type_set(a, "__annotations__", out);
}

R n_set_annotate(const CallArgs &a, Value &out)
{
    return type_set(a, "__annotate__", out);
}

bool put_descriptor(Value cls, Str name, R (*get)(const CallArgs &, Value &),
                    R (*set)(const CallArgs &, Value &))
{
    Root rc{ cls };
    Root g{ native_new(name, get) };
    Root s{ native_new(name, set) };
    if (g.v.is_nil() || s.v.is_nil())
        return false;
    Root p{ property_of(g.v, s.v) };
    StrObj *k = str_intern(name);
    if (p.v.is_nil() || !k)
        return false;
    return dict_set(static_cast<DictObj *>(type_obj(rc.v)->dict.obj()), obj_value(k), p.v) ==
           R::Ok;
}

} // namespace

bool annot_install()
{
    Root cls{ type_wrap(&type_type) };
    if (cls.v.is_nil())
        return false;
    return put_descriptor(cls.v, "__annotations__", n_type_annotations, n_set_annotations) &&
           put_descriptor(cls.v, "__annotate__", n_type_annotate, n_set_annotate);
}
