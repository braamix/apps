// The method mechanism: a static table becomes natives in a built-in type's
// namespace, and py_attr finds them there.
#include "method.h"

#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "ops.h"

R oom_err()
{
    return err_set("MemoryError", "out of memory");
}

namespace {

// A static method is the WrapObj `staticmethod` makes. type_bind unwraps it,
// so `str.maketrans` is handed no self.
Value static_wrap(Value fn)
{
    Root rf{ fn };
    WrapObj *w = static_cast<WrapObj *>(obj_alloc(&staticmethod_type, sizeof(WrapObj)));
    if (!w)
        return oom_err(), Value();
    w->fn = rf.v;
    return obj_value(w);
}

} // namespace

bool method_install(const Type *t, const Method *tab, usize n)
{
    Root cls{ type_wrap(t) };
    if (cls.v.is_nil())
        return false;
    for (usize i = 0; i < n; i++) {
        Root fn{ native_new(tab[i].name, tab[i].fn) };
        if (fn.v.is_nil())
            return false;
        if (tab[i].stat) {
            fn = static_wrap(fn.v);
            if (fn.v.is_nil())
                return false;
        }
        StrObj *name = str_intern(tab[i].name);
        if (!name)
            return oom_err(), false;
        if (dict_set(static_cast<DictObj *>(type_obj(cls.v)->dict.obj()), obj_value(name), fn.v) !=
            R::Ok)
            return false;
    }
    return true;
}

bool methods_install()
{
    return str_methods() && bytes_methods() && seq_methods() && map_methods() && num_methods();
}

Value method_self(Value v)
{
    if (is_inst(v) && !inst_of(v)->native.is_nil())
        return inst_of(v)->native;
    return v;
}

namespace {

// Answered out of the slots, so no iterator type needs a table of its own.
R b_dunder_next(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__next__", 0, 0))
        return R::Err;
    R r = py_next(a.args[0], out);
    if (r != R::NotImpl)
        return r;
    Value e = exc_new(exc_find("StopIteration"), Value());
    return e.is_nil() ? R::Err : err_set_value(e);
}

R b_dunder_iter(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__iter__", 0, 0))
        return R::Err;
    out = py_iter(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

R method_find(Value v, StrObj *name, Value &out)
{
    Root rv{ v };
    Value cls = type_of_value(v);
    if (cls.is_nil()) // Nil has no type, and so no method
        return err_clear(), R::NotImpl;
    Root rc{ cls }, found, owner;
    R r = type_lookup(rc.v, name, found.v, &owner.v);
    if (r == R::Ok)
        return type_bind(found.v, rv.v, rc.v, out);
    if (r == R::Err)
        return R::Err;

    const Type *t                      = type_of(rv.v);
    Str n                              = name->str();
    R (*fn)(const CallArgs &, Value &) = (n == "__next__" && t->next)   ? b_dunder_next
                                         : (n == "__iter__" && t->iter) ? b_dunder_iter
                                                                        : nullptr;
    if (!fn)
        return R::NotImpl;
    Root f{ native_new(n == "__next__" ? Str("__next__") : Str("__iter__"), fn) };
    if (f.v.is_nil())
        return R::Err;
    out = method_new(f.v, rv.v);
    return out.is_nil() ? R::Err : R::Ok;
}

bool meth_args(const CallArgs &a, Str who, u32 least, u32 most)
{
    if (a.nkw) {
        Buf<96> b;
        b.put(who).put("() takes no keyword arguments");
        return err_set("TypeError", b.str()), false;
    }
    u32 n = a.nargs ? a.nargs - 1 : 0;
    if (n < least || n > most) {
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
        b.put(" arguments (").put(int_text(tmp, sizeof tmp, i64(n))).put(" given)");
        return err_set("TypeError", b.str()), false;
    }
    return true;
}

bool meth_take(const CallArgs &a, Str who, const Str *names, u32 n, u32 least, Value *out)
{
    u32 given = a.nargs ? a.nargs - 1 : 0;
    for (u32 i = 0; i < n; i++)
        out[i] = Value();
    if (given > n) {
        char tmp[24];
        Buf<128> b;
        b.put(who).put("() takes at most ").put(int_text(tmp, sizeof tmp, i64(n)));
        b.put(" arguments (").put(int_text(tmp, sizeof tmp, i64(given))).put(" given)");
        return err_set("TypeError", b.str()), false;
    }
    for (u32 i = 0; i < given; i++)
        out[i] = a.args[i + 1];
    for (u32 k = 0; k < a.nkw; k++) {
        Str nm = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        u32 i  = 0;
        while (i < n && !(names[i] == nm))
            i++;
        if (i == n) {
            Buf<128> b;
            b.put(who).put("() got an unexpected keyword argument '").put(nm).put("'");
            return err_set("TypeError", b.str()), false;
        }
        if (!out[i].is_nil()) {
            Buf<128> b;
            b.put(who).put("() got multiple values for argument '").put(nm).put("'");
            return err_set("TypeError", b.str()), false;
        }
        out[i] = a.kwvals[k];
    }
    for (u32 i = 0; i < least; i++)
        if (out[i].is_nil()) {
            Buf<128> b;
            b.put(who).put("() missing a required argument: '").put(names[i]).put("'");
            return err_set("TypeError", b.str()), false;
        }
    return true;
}

namespace {

// The message for a method reached through the wrong type: `str.upper([])`.
void *wrong(Str who, Value v)
{
    Buf<128> b;
    b.put(who).put("() requires a different self");
    err_set2("TypeError", b.str(), type_name(v));
    return nullptr;
}

} // namespace

StrObj *self_str(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_str(s))
        return static_cast<StrObj *>(wrong(who, s));
    return str_of(s);
}

ListObj *self_list(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_list(s))
        return static_cast<ListObj *>(wrong(who, s));
    return list_of(s);
}

DictObj *self_dict(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_dict(s))
        return static_cast<DictObj *>(wrong(who, s));
    return static_cast<DictObj *>(s.obj());
}

SetObj *self_set(const CallArgs &a, Str who, bool frozen_ok)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!(is_set(s) || (frozen_ok && is_frozenset(s))))
        return static_cast<SetObj *>(wrong(who, s));
    return set_at(s);
}

TupleObj *self_tuple(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_tuple(s))
        return static_cast<TupleObj *>(wrong(who, s));
    return static_cast<TupleObj *>(s.obj());
}

bool bytes_like(Value v, Str &out)
{
    if (is_bytes(v))
        return out = static_cast<BytesObj *>(v.obj())->str(), true;
    if (is_bytearray(v))
        return out = array_of(v)->str(), true;
    if (is_memview(v))
        return memview_bytes(v, out);
    return false;
}

Value str_of_bytes(Str s)
{
    StrObj *o = str_raw(s);
    return o ? obj_value(o) : (oom_err(), Value());
}
