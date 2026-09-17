// The run-time half of `match`: see patma.h. The compiler's half is in
// compile.cpp, and follows CPython's codegen.c pattern for pattern.
#include "patma.h"

#include "call.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"

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

Value tuple_of(Value list)
{
    Root rl{ list };
    TupleObj *t = tuple_new(list_of(rl.v)->items.size());
    if (!t)
        return oom(), Value();
    for (usize i = 0; i < t->len; i++)
        t->items()[i] = list_of(rl.v)->items[i];
    return obj_value(t);
}

// The class an instance or a built-in value belongs to, without allocating
// where it can be helped.
u8 flags_of(Value cls)
{
    Value mro = type_obj(cls)->mro;
    for (usize i = 0; i < tuple_len(mro); i++) {
        u8 f = type_obj(tuple_at(mro, i))->slots.patma & (PATMA_SEQ | PATMA_MAP);
        if (f)
            return f;
    }
    return 0;
}

// A marker no Python value can be: what `get` and getattr answer for
// "not there".
Value sentinel()
{
    Root ob{ type_object() };
    return ob.v.is_nil() ? Value() : inst_new(ob.v);
}

// getattr(obj, name, default), which a continuation can call.
R n_getattr(const CallArgs &a, Value &out)
{
    Got g = py_attr_opt(a.args[0], str_of(a.args[1]), out, a.args[2], false);
    switch (g) {
    case Got::Ok:
    case Got::Call:
        return R::Ok;
    case Got::Missing:
        out = a.args[2];
        return R::Ok;
    case Got::Error:
        break;
    }
    return R::Err;
}

Value three(Value a, Value b, Value c)
{
    Root ra{ a }, rb{ b }, rc{ c };
    TupleObj *t = tuple_new(3);
    if (!t)
        return oom(), Value();
    t->items()[0] = ra.v;
    t->items()[1] = rb.v;
    t->items()[2] = rc.v;
    return obj_value(t);
}

// s[0] the bound `get`, s[1] the keys, s[2] the values so far, s[3] the
// sentinel; j is the next key.
R keys_step(ContObj *k, Value in)
{
    if (k->i++) {
        if (in == k->s[3])
            return cont_done(k, value_none());
        if (!list_push(list_of(k->s[2]), in))
            return oom();
    }
    if (k->j == tuple_len(k->s[1])) {
        Value t = tuple_of(k->s[2]);
        return t.is_nil() ? R::Err : cont_done(k, t);
    }
    Value key = tuple_at(k->s[1], k->j);
    for (u32 p = 0; p < k->j; p++) {
        bool same = false;
        if (py_eq(tuple_at(k->s[1], p), key, same) != R::Ok)
            return R::Err;
        if (same) {
            String text;
            if (py_repr(key, text) != R::Ok)
                return R::Err;
            Buf<128> b;
            b.put("mapping pattern checks duplicate key (").put(text.str()).put(")");
            return err_set("ValueError", b.str());
        }
    }
    k->j++;
    return cont_call(k, k->s[0], key, 2, k->s[3]);
}

// s[0] the subject, s[1] the class, s[2] the keyword names, s[3] the
// attributes so far, s[4] __match_args__, s[5] the sentinel, s[6] the
// isinstance and s[7] the getattr to call; j is how many are positional.
enum : u32 { MC_START, MC_CHECKED, MC_ARGS, MC_NEXT, MC_GOT };

Str name_of(Value cls)
{
    return type_obj(cls)->slots.name;
}

R class_step(ContObj *k, Value in)
{
    u32 nargs  = k->j;
    u32 nattrs = nargs + u32(tuple_len(k->s[2]));
    switch (k->i) {
    case MC_START:
        k->i = MC_CHECKED;
        return cont_call(k, k->s[6], k->s[0], 2, k->s[1]);

    case MC_CHECKED:
        if (!py_truth(in))
            return cont_done(k, value_none());
        if (!nargs) {
            k->i = MC_NEXT;
            return class_step(k, Value());
        }
        k->i = MC_ARGS;
        {
            Value name = obj_value(str_intern("__match_args__"));
            Value args = three(k->s[1], name, k->s[5]);
            if (args.is_nil())
                return R::Err;
            return cont_call_v(k, k->s[7], args);
        }

    case MC_ARGS: {
        bool self = false;
        if (in == k->s[5]) {
            TupleObj *none = tuple_new(0);
            if (!none)
                return oom();
            k->s[4] = obj_value(none);
            self    = patma_self(k->s[1]);
        } else if (!is_tuple(in)) {
            Buf<128> b;
            b.put(name_of(k->s[1])).put(".__match_args__ must be a tuple (got ");
            b.put(type_name(in)).put(")");
            return err_set("TypeError", b.str());
        } else {
            k->s[4] = in;
        }
        u32 allowed = self ? 1 : u32(tuple_len(k->s[4]));
        if (allowed < nargs) {
            char tmp[24];
            Buf<160> b;
            b.put(name_of(k->s[1])).put("() accepts ");
            b.put(int_text(tmp, sizeof tmp, i64(allowed))).put(" positional sub-pattern");
            b.put(allowed == 1 ? "" : "s").put(" (");
            b.put(int_text(tmp, sizeof tmp, i64(nargs))).put(" given)");
            return err_set("TypeError", b.str());
        }
        if (self && !list_push(list_of(k->s[3]), k->s[0]))
            return oom();
        k->i = MC_NEXT;
        return class_step(k, Value());
    }

    case MC_GOT:
        if (in == k->s[5])
            return cont_done(k, value_none());
        if (!list_push(list_of(k->s[3]), in))
            return oom();
        [[fallthrough]];

    case MC_NEXT: {
        u32 at = u32(list_of(k->s[3])->items.size());
        if (at == nattrs) {
            Value t = tuple_of(k->s[3]);
            return t.is_nil() ? R::Err : cont_done(k, t);
        }
        Value name = at < nargs ? tuple_at(k->s[4], at) : tuple_at(k->s[2], at - nargs);
        if (!is_str(name)) {
            Buf<128> b;
            b.put("__match_args__ elements must be strings (got ").put(type_name(name)).put(")");
            return err_set("TypeError", b.str());
        }
        // A name asked for twice, which only a positional one can make.
        if (nargs && nattrs > 1)
            for (u32 p = 0; p < at; p++) {
                Value other = p < nargs ? tuple_at(k->s[4], p) : tuple_at(k->s[2], p - nargs);
                bool same   = false;
                if (is_str(other) && py_eq(other, name, same) != R::Ok)
                    return R::Err;
                if (same) {
                    Buf<160> b;
                    b.put(name_of(k->s[1])).put("() got multiple sub-patterns for attribute '");
                    b.put(str_of(name)->str()).put("'");
                    return err_set("TypeError", b.str());
                }
            }
        k->i        = MC_GOT;
        Value args3 = three(k->s[0], obj_value(str_intern(str_of(name)->str())), k->s[5]);
        if (args3.is_nil())
            return R::Err;
        return cont_call_v(k, k->s[7], args3);
    }
    }
    return err_set("SystemError", "a class pattern lost its place");
}

// s[0] the dict constructor.
R copy_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->s[1]);
    return cont_done(k, in);
}

} // namespace

u8 patma_kind(Value subject)
{
    if (is_inst(subject))
        return flags_of(inst_of(subject)->cls);
    const Type *t = type_of(subject);
    if (!t)
        return 0;
    if (t->owner)
        return flags_of(obj_value(t->owner));
    return t->patma & (PATMA_SEQ | PATMA_MAP);
}

bool patma_self(Value cls)
{
    // Inherited from the built-in the class is laid out over.
    Value mro = type_obj(cls)->mro;
    for (usize i = 0; i < tuple_len(mro); i++) {
        TypeObj *c = type_obj(tuple_at(mro, i));
        if (!c->heap)
            return (c->slots.patma & PATMA_SELF) != 0;
    }
    return false;
}

void patma_set(Value cls, u8 flags)
{
    Type &s = type_obj(cls)->slots;
    s.patma = u8((s.patma & ~(PATMA_SEQ | PATMA_MAP)) | flags);
}

R patma_keys(Value subject, Value keys, Value &out)
{
    Root rs{ subject }, rk{ keys };
    if (!tuple_len(rk.v)) {
        TupleObj *none = tuple_new(0);
        if (!none)
            return oom();
        out = obj_value(none);
        return R::Ok;
    }
    // A dict, or a subclass that did not write its own get: looked up here.
    Value inner = method_self(rs.v);
    if (is_dict(inner) && !type_has_py_special(rs.v, "get")) {
        TupleObj *t = tuple_new(tuple_len(rk.v));
        if (!t)
            return oom();
        Root rt{ obj_value(t) };
        for (usize i = 0; i < tuple_len(rk.v); i++) {
            Value key = tuple_at(rk.v, i);
            for (usize p = 0; p < i; p++) {
                bool same = false;
                if (py_eq(tuple_at(rk.v, p), key, same) != R::Ok)
                    return R::Err;
                if (same) {
                    String text;
                    if (py_repr(key, text) != R::Ok)
                        return R::Err;
                    Buf<128> b;
                    b.put("mapping pattern checks duplicate key (").put(text.str()).put(")");
                    return err_set("ValueError", b.str());
                }
            }
            Value got;
            R r = dict_get(static_cast<DictObj *>(method_self(rs.v).obj()), key, got);
            if (r == R::Err)
                return R::Err;
            if (r == R::NotImpl) {
                out = value_none();
                return R::Ok;
            }
            static_cast<TupleObj *>(rt.v.obj())->items()[i] = got;
        }
        out = rt.v;
        return R::Ok;
    }

    Root get;
    StrObj *gn = str_intern("get");
    if (!gn)
        return oom();
    Got g = py_attr(rs.v, gn, get.v);
    if (g == Got::Error)
        return R::Err;
    if (g == Got::Missing)
        return err_set2("AttributeError", "object has no attribute 'get'", type_name(rs.v));
    if (g == Got::Call)
        return err_set("TypeError", "a mapping pattern needs a plain get method");
    Root mark{ sentinel() };
    ListObj *vals = list_new();
    if (mark.v.is_nil() || !vals)
        return vals ? R::Err : oom();
    Root rv{ obj_value(vals) };
    Root kv{ cont_new(keys_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = get.v;
    k->s[1]    = rk.v;
    k->s[2]    = rv.v;
    k->s[3]    = mark.v;
    out        = kv.v;
    return R::Ok;
}

R patma_class(Value subject, Value cls, u32 nargs, Value names, Value &out)
{
    if (!is_type(cls))
        return err_set("TypeError", "called match pattern must be a class");
    Root rs{ subject }, rc{ cls }, rn{ names };
    Root inst{ native_new("isinstance", py_isinstance) };
    Root attr{ native_new("getattr", n_getattr) };
    Root mark{ sentinel() };
    ListObj *got = list_new();
    if (inst.v.is_nil() || attr.v.is_nil() || mark.v.is_nil() || !got)
        return got ? R::Err : oom();
    Root rg{ obj_value(got) };
    Root kv{ cont_new(class_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->s[1]    = rc.v;
    k->s[2]    = rn.v;
    k->s[3]    = rg.v;
    k->s[5]    = mark.v;
    k->s[6]    = inst.v;
    k->s[7]    = attr.v;
    k->j       = nargs;
    out        = kv.v;
    return R::Ok;
}

R patma_copy(Value subject, Value &out)
{
    Root rs{ subject };
    Value inner = method_self(rs.v);
    if (is_dict(inner) && !type_has_py_special(rs.v, "__iter__") &&
        !type_has_py_special(rs.v, "keys")) {
        DictObj *d = dict_new();
        if (!d)
            return oom();
        Root rd{ obj_value(d) };
        usize at = 0;
        Value key, val;
        while (table_next(static_cast<DictObj *>(method_self(rs.v).obj())->t, at, key, val))
            if (dict_set(static_cast<DictObj *>(rd.v.obj()), key, val) != R::Ok)
                return R::Err;
        out = rd.v;
        return R::Ok;
    }
    Root ctor{ type_wrap(&dict_type) };
    if (ctor.v.is_nil())
        return R::Err;
    Root kv{ cont_new(copy_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = ctor.v;
    cont_of(kv.v)->s[1] = rs.v;
    out                 = kv.v;
    return R::Ok;
}
