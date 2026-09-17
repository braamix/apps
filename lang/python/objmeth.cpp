// The rest of what `object` lends every class: the comparisons, __hash__, the
// pickle helpers __reduce_ex__, __reduce__ and __getstate__, __dir__,
// __sizeof__ and __subclasshook__.
//
// Each is the default, so a special-method lookup the VM makes passes over
// it (type_special) and the native answer stands; these are what a program
// reaches by name: `object.__eq__`, `Mapping.__ne__`, `super().__reduce_ex__`.
// On a built-in value, which has no method of its own for these, they answer
// through the type's slots.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "exc.h"
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

Value notimpl()
{
    return value_notimpl();
}

// A class written in Python that lends no layout of a built-in.
bool plain_inst(Value v)
{
    return is_inst(v) && inst_of(v)->native.is_nil() && !is_exc(v);
}

// Identity, for anything; a type's own __eq__ is its own method.
R o_eq(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__eq__", 2, 2))
        return R::Err;
    out = a.args[0] == a.args[1] ? value_bool(true) : notimpl();
    return R::Ok;
}

// The type's own equality, where it has one.
R slot_eq(Value x, Value y, Value &out)
{
    if (x == y) {
        out = value_bool(true);
        return R::Ok;
    }
    const Type *t = type_of(x);
    if (plain_inst(x) || !t || !t->eq) {
        out = notimpl();
        return R::Ok;
    }
    bool yes = false;
    R r      = t->eq(x, y, yes);
    if (r == R::Err)
        return R::Err;
    out = r == R::NotImpl ? notimpl() : value_bool(yes);
    return R::Ok;
}

// __ne__ is __eq__ inverted, and __eq__ may be the class's own.
R ne_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->s[1]);
    if (is_notimpl(in))
        return cont_done(k, in);
    return cont_done(k, value_bool(!py_truth(in)));
}

R o_ne(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__ne__", 2, 2))
        return R::Err;
    if (type_has_py_special(a.args[0], "__eq__")) {
        Root m{ type_special(a.args[0], "__eq__") };
        if (m.v.is_nil())
            return R::Err;
        Root kv{ cont_new(ne_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = m.v;
        cont_of(kv.v)->s[1] = a.args[1];
        out                 = kv.v;
        return R::Ok;
    }
    if (slot_eq(a.args[0], a.args[1], out) != R::Ok)
        return R::Err;
    if (!is_notimpl(out))
        out = value_bool(!is_true(out));
    return R::Ok;
}

// NotImplemented, whatever the two are.
R order(const CallArgs &a, Value &out, Cmp op, Str who)
{
    (void)op;
    if (!args_only(a, who, 2, 2))
        return R::Err;
    out = notimpl();
    return R::Ok;
}

R o_lt(const CallArgs &a, Value &out)
{
    return order(a, out, Cmp::Lt, "__lt__");
}

R o_le(const CallArgs &a, Value &out)
{
    return order(a, out, Cmp::Le, "__le__");
}

R o_gt(const CallArgs &a, Value &out)
{
    return order(a, out, Cmp::Gt, "__gt__");
}

R o_ge(const CallArgs &a, Value &out)
{
    return order(a, out, Cmp::Ge, "__ge__");
}

// Identity for an instance, whatever its class says about being hashable.
R o_hash(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__hash__", 1, 1))
        return R::Err;
    u32 h = 0;
    if (is_inst(a.args[0]))
        h = u32(usize(a.args[0].obj())) >> 4;
    else if (py_hash(a.args[0], h) != R::Ok)
        return R::Err;
    out = Value::of_int(i32(h & 0x3fffffff));
    return R::Ok;
}

R o_sizeof(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__sizeof__", 1, 1))
        return R::Err;
    out = Value::of_int(16);
    return R::Ok;
}

R o_dir(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__dir__", 1, 1))
        return R::Err;
    return py_dir(a, out);
}

R o_subclasshook(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs != 2) {
        char n[24];
        Buf<128> m;
        m.put(a.nargs ? type_obj(a.args[0])->slots.name : Str("object"));
        m.put(".__subclasshook__() takes exactly one argument (");
        m.put(int_text(n, sizeof n, i64(a.nargs ? a.nargs - 1 : 0))).put(" given)");
        return err_set("TypeError", m.str());
    }
    out = notimpl();
    return R::Ok;
}

// ------------------------------------------------------------ the reduction

// What a lookup of a special name on the value finds, bound: the class's
// method for an instance, the type's for a built-in. Nil when there is none,
// or when it is object's own.
Value special_of(Value v, Str name)
{
    Root rv{ v };
    Root cls{ type_of_value(rv.v) };
    StrObj *n = str_intern(name);
    if (cls.v.is_nil() || !n)
        return Value();
    Root found;
    if (type_lookup(cls.v, n, found.v) != R::Ok || is_object_default(found.v))
        return err_clear(), Value();
    Value out;
    if (type_bind(found.v, rv.v, cls.v, out) != R::Ok)
        return Value();
    return out;
}

enum : u32 {
    ST_START,      // __reduce_ex__: a __reduce__ of the class's own first
    ST_COMMON,     // import copyreg
    ST_IMPORTED,   // copyreg in hand
    ST_NEWARGS_EX, // __getnewargs_ex__ answered
    ST_NEWARGS,    // __getnewargs__ answered
    ST_STATE,      // decide how the state is made
    ST_SLOTNAMES,  // copyreg._slotnames answered
    ST_GOTSTATE,   // the state is in hand
    ST_DONE,       // hand back what the last call answered
};

enum : u32 { F_GETSTATE = 1u << 8, F_HASARGS = 1u << 9 }; // j: the protocol below
constexpr u32 PROTO = 0xff;

// s[0] self, s[1] copyreg, s[2] the args tuple, s[3] the kwargs dict, s[4]
// the state.
bool is_list_like(Value v)
{
    return is_list(method_self(v));
}

bool is_dict_like(Value v)
{
    return is_dict(method_self(v));
}

// object.__getstate__ with the slot names in hand: the instance dict, or None
// for an empty one, and the slots beside it.
R default_state(ContObj *k, Value slotnames, bool required)
{
    Value self = k->s[0];
    if (required && !is_inst(self)) {
        Buf<128> m;
        m.put("cannot pickle '").put(type_name(self)).put("' object");
        return err_set("TypeError", m.str());
    }
    Root state{ value_none() };
    if (is_inst(self)) {
        Value d = inst_of(self)->dict;
        if (!d.is_nil() && static_cast<DictObj *>(d.obj())->t.live)
            state = d;
    }
    if (is_list(slotnames) && list_of(slotnames)->items.size()) {
        Root rn{ slotnames };
        DictObj *slots = dict_new();
        if (!slots)
            return oom();
        Root rs{ obj_value(slots) };
        for (usize i = 0; i < list_of(rn.v)->items.size(); i++) {
            Value name = list_of(rn.v)->items[i];
            if (!is_str(name))
                continue;
            StrObj *n = str_intern(str_of(name)->str());
            if (!n)
                return oom();
            Root got, args;
            Got g = attr_plain(k->s[0], n, got.v, args.v);
            // An unset slot is not in the state.
            if (g == Got::Error && err_kind() == Str("AttributeError")) {
                err_clear();
                continue;
            }
            if (g == Got::Error)
                return R::Err;
            if (g != Got::Ok)
                continue;
            if (dict_set(static_cast<DictObj *>(rs.v.obj()), obj_value(n), got.v) != R::Ok)
                return R::Err;
        }
        if (static_cast<DictObj *>(rs.v.obj())->t.live) {
            TupleObj *t = tuple_new(2);
            if (!t)
                return oom();
            t->items()[0] = state.v;
            t->items()[1] = rs.v;
            state         = obj_value(t);
        }
    }
    k->s[4] = state.v;
    return R::Ok;
}

Value copyreg_attr(ContObj *k, Str name)
{
    StrObj *n = str_intern(name);
    Value out;
    if (!n || py_getattr(k->s[1], n, out) != R::Ok)
        return Value();
    return out;
}

// The five-tuple, once every part is known.
R finish(ContObj *k)
{
    bool ex = !k->s[3].is_nil() && static_cast<DictObj *>(k->s[3].obj())->t.live;
    Root newobj{ copyreg_attr(k, ex ? Str("__newobj_ex__") : Str("__newobj__")) };
    if (newobj.v.is_nil())
        return R::Err;
    Root cls{ type_of_value(k->s[0]) };
    if (cls.v.is_nil())
        return R::Err;
    Root newargs;
    if (ex) {
        TupleObj *t = tuple_new(3);
        if (!t)
            return oom();
        t->items()[0] = cls.v;
        t->items()[1] = k->s[2];
        t->items()[2] = k->s[3];
        newargs       = obj_value(t);
    } else {
        usize n     = k->s[2].is_nil() ? 0 : static_cast<TupleObj *>(k->s[2].obj())->len;
        TupleObj *t = tuple_new(n + 1);
        if (!t)
            return oom();
        t->items()[0] = cls.v;
        for (usize i = 0; i < n; i++)
            t->items()[i + 1] = static_cast<TupleObj *>(k->s[2].obj())->items()[i];
        newargs = obj_value(t);
    }
    Root list_items{ value_none() }, dict_items{ value_none() };
    if (is_list_like(k->s[0])) {
        list_items = py_iter(method_self(k->s[0]));
        if (list_items.v.is_nil())
            return R::Err;
    }
    if (is_dict_like(k->s[0])) {
        Root view{ dict_view(method_self(k->s[0]), VIEW_ITEMS) };
        if (view.v.is_nil())
            return R::Err;
        dict_items = py_iter(view.v);
        if (dict_items.v.is_nil())
            return R::Err;
    }
    TupleObj *t = tuple_new(5);
    if (!t)
        return oom();
    t->items()[0] = newobj.v;
    t->items()[1] = newargs.v;
    t->items()[2] = k->s[4];
    t->items()[3] = list_items.v;
    t->items()[4] = dict_items.v;
    return cont_done(k, obj_value(t));
}

R reduce_step(ContObj *k, Value in)
{
    for (;;) {
        switch (k->i) {
        case ST_START: {
            k->i = ST_COMMON;
            Root m{ special_of(k->s[0], "__reduce__") };
            if (!m.v.is_nil()) {
                k->i = ST_DONE;
                return cont_call(k, m.v, Value(), 0);
            }
            if (err_pending())
                return R::Err;
            continue;
        }
        case ST_COMMON: {
            k->i        = ST_IMPORTED;
            StrObj *imp = str_intern("__import__");
            Value fn;
            if (!imp || dict_get(builtins_dict(), obj_value(imp), fn) != R::Ok)
                return err_pending() ? R::Err : oom();
            Value name = str_new("copyreg");
            if (name.is_nil())
                return R::Err;
            return cont_call(k, fn, name);
        }
        case ST_IMPORTED: {
            k->s[1] = in;
            if (k->j & F_GETSTATE) {
                k->i = ST_STATE;
                continue;
            }
            u32 proto = k->j & PROTO;
            if (proto < 2) {
                Root fn{ copyreg_attr(k, "_reduce_ex") };
                if (fn.v.is_nil())
                    return R::Err;
                k->i = ST_DONE;
                return cont_call(k, fn.v, k->s[0], 2, Value::of_int(i32(proto)));
            }
            Root ex{ special_of(k->s[0], "__getnewargs_ex__") };
            if (!ex.v.is_nil()) {
                k->i = ST_NEWARGS_EX;
                return cont_call(k, ex.v, Value(), 0);
            }
            Root plain{ special_of(k->s[0], "__getnewargs__") };
            if (!plain.v.is_nil()) {
                k->i = ST_NEWARGS;
                return cont_call(k, plain.v, Value(), 0);
            }
            if (err_pending())
                return R::Err;
            k->i = ST_STATE;
            continue;
        }
        case ST_NEWARGS_EX: {
            if (!is_tuple(in)) {
                Buf<128> m;
                m.put("__getnewargs_ex__ should return a tuple, not '")
                    .put(type_name(in))
                    .put('\'');
                return err_set("TypeError", m.str());
            }
            TupleObj *t = static_cast<TupleObj *>(in.obj());
            if (t->len != 2) {
                char n[24];
                Buf<128> m;
                m.put("__getnewargs_ex__ should return a tuple of length 2, not ");
                m.put(int_text(n, sizeof n, i64(t->len)));
                return err_set("ValueError", m.str());
            }
            if (!is_tuple(t->items()[0])) {
                Buf<160> m;
                m.put(
                    "first item of the tuple returned by __getnewargs_ex__ must be a tuple, not '");
                m.put(type_name(t->items()[0])).put('\'');
                return err_set("TypeError", m.str());
            }
            if (!is_dict(t->items()[1])) {
                Buf<160> m;
                m.put(
                    "second item of the tuple returned by __getnewargs_ex__ must be a dict, not '");
                m.put(type_name(t->items()[1])).put('\'');
                return err_set("TypeError", m.str());
            }
            k->s[2] = t->items()[0];
            k->s[3] = t->items()[1];
            k->j |= F_HASARGS;
            k->i = ST_STATE;
            continue;
        }
        case ST_NEWARGS:
            if (!is_tuple(in)) {
                Buf<128> m;
                m.put("__getnewargs__ should return a tuple, not '").put(type_name(in)).put('\'');
                return err_set("TypeError", m.str());
            }
            k->s[2] = in;
            k->j |= F_HASARGS;
            k->i = ST_STATE;
            continue;
        case ST_STATE: {
            // A __getstate__ of the class's own is called; the default is
            // made here, and needs the slot names.
            if (!(k->j & F_GETSTATE)) {
                Root own{ special_of(k->s[0], "__getstate__") };
                if (!own.v.is_nil()) {
                    k->i = ST_GOTSTATE;
                    return cont_call(k, own.v, Value(), 0);
                }
                if (err_pending())
                    return R::Err;
            }
            if (!is_inst(k->s[0])) {
                if (default_state(k, value_none(),
                                  (k->j & F_GETSTATE)
                                      ? false
                                      : !(k->j & F_HASARGS) && !is_list_like(k->s[0]) &&
                                            !is_dict_like(k->s[0])) != R::Ok)
                    return R::Err;
                k->i = ST_GOTSTATE;
                in   = k->s[4];
                continue;
            }
            Root cls{ type_of_value(k->s[0]) };
            Root fn{ copyreg_attr(k, "_slotnames") };
            if (cls.v.is_nil() || fn.v.is_nil())
                return R::Err;
            k->i = ST_SLOTNAMES;
            return cont_call(k, fn.v, cls.v);
        }
        case ST_SLOTNAMES:
            if (!is_list(in) && !is_none(in))
                return err_set("TypeError", "copyreg._slotnames didn't return a list or None");
            if (default_state(k, in, false) != R::Ok)
                return R::Err;
            k->i = ST_GOTSTATE;
            in   = k->s[4];
            continue;
        case ST_GOTSTATE:
            k->s[4] = in;
            if (k->j & F_GETSTATE)
                return cont_done(k, in);
            return finish(k);
        default:
            return cont_done(k, in);
        }
    }
}

R reduce_start(Value self, u32 start, u32 flags, Value &out)
{
    Root rs{ self };
    Root kv{ cont_new(reduce_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->i       = start;
    k->j       = flags;
    out        = kv.v;
    return R::Ok;
}

R o_reduce_ex(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__reduce_ex__", 2, 2))
        return R::Err;
    i64 proto = 0;
    if (!is_intval(a.args[1]) || !as_index(a.args[1], proto)) {
        Buf<96> m;
        m.put('\'').put(type_name(a.args[1])).put("' object cannot be interpreted as an integer");
        return err_set("TypeError", m.str());
    }
    if (proto < 0)
        proto = 0;
    return reduce_start(a.args[0], ST_START, u32(proto > 5 ? 5 : proto), out);
}

R o_reduce(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__reduce__", 1, 1))
        return R::Err;
    return reduce_start(a.args[0], ST_COMMON, 0, out);
}

R o_getstate(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__getstate__", 1, 1))
        return R::Err;
    return reduce_start(a.args[0], ST_COMMON, F_GETSTATE, out);
}

struct Def {
    Str name;
    R (*fn)(const CallArgs &, Value &out);
    bool cls;
};

constexpr Def DEFS[] = {
    { "__eq__", o_eq, false },
    { "__ne__", o_ne, false },
    { "__lt__", o_lt, false },
    { "__le__", o_le, false },
    { "__gt__", o_gt, false },
    { "__ge__", o_ge, false },
    { "__hash__", o_hash, false },
    { "__reduce_ex__", o_reduce_ex, false },
    { "__reduce__", o_reduce, false },
    { "__getstate__", o_getstate, false },
    { "__dir__", o_dir, false },
    { "__sizeof__", o_sizeof, false },
    { "__subclasshook__", o_subclasshook, true },
};

} // namespace

bool objmeth_is(R (*fn)(const CallArgs &, Value &))
{
    for (const Def &d : DEFS)
        if (d.fn == fn)
            return true;
    return false;
}

bool objmeth_install(Value dict)
{
    Root rd{ dict };
    for (const Def &d : DEFS) {
        Root fn{ native_new("object", d.fn) };
        if (fn.v.is_nil())
            return false;
        if (d.cls) {
            fn = classmethod_new(fn.v);
            if (fn.v.is_nil())
                return false;
        }
        StrObj *n = str_intern(d.name);
        if (!n)
            return oom() == R::Ok;
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(n), fn.v) != R::Ok)
            return false;
    }
    return true;
}
