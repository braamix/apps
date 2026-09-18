// The protocol methods in a built-in type's namespace.
//
// `len(x)` reaches a slot, and a slot is not an entry: until now
// `'__len__' in list.__dict__` was False and `dir(list)` did not list one.
// That is a difference from CPython on its own, and it is also what stops
// collections.abc working, because every abstract base class there decides
// membership with a __subclasshook__ that looks exactly these names up.
//
// So each is a small native over the generic operation in ops.h, and the set
// installed on a type is decided by the slots that type actually fills. The
// one that is more than a wrapper is __getitem__, which is where a dict
// subclass's __missing__ is finally consulted.
#include "bigint.h"
#include "call.h"
#include "compare.h"
#include "complex.h"
#include "exc.h"
#include "gc.h"
#include "genalias.h"
#include "intern.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"
#include "weak.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The built-in a method acts on: a subclass instance stands for the built-in
// laid out inside it.
Value me(const CallArgs &a)
{
    return method_self(a.args[0]);
}

R s_len(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__len__", 0, 0))
        return R::Err;
    usize n = 0;
    if (py_len(me(a), n) != R::Ok)
        return R::Err;
    out = int_from_i64(i64(n));
    return out.is_nil() ? R::Err : R::Ok;
}

// The step a __missing__ was called for: its answer is the subscript's.
R missing_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->s[1]);
    return cont_done(k, in);
}

R s_getitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__getitem__", 1, 1))
        return R::Err;
    Root self{ a.args[0] }, key{ a.args[1] };
    R r = py_getitem(me(a), key.v, out);
    if (r != R::Err || err_kind() != "KeyError")
        return r;
    // A dict subclass may answer for a key it has not got. CPython's
    // dict.__getitem__ is where that hook lives, and so is this.
    Root m{ type_special(self.v, "__missing__") };
    if (m.v.is_nil())
        return R::Err;
    err_clear();
    Root kv{ cont_new(missing_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = m.v;
    cont_of(kv.v)->s[1] = key.v;
    out                 = kv.v;
    return R::Ok;
}

R s_setitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__setitem__", 2, 2))
        return R::Err;
    if (py_setitem(me(a), a.args[1], a.args[2]) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R s_delitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__delitem__", 1, 1))
        return R::Err;
    if (py_delitem(me(a), a.args[1]) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R s_contains(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__contains__", 1, 1))
        return R::Err;
    Root self{ me(a) }, item{ a.args[1] };
    // An __eq__ written in Python makes each step a call, so the search is
    // handed back for the VM to drive.
    Root seq{ cmp_items(self.v) };
    if (!seq.v.is_nil() &&
        (cmp_is_python(item.v, false) || cmp_any_python(list_of(seq.v)->items, false))) {
        out = cmp_find(seq.v, item.v, CMP_IN, 0, list_of(seq.v)->items.size());
        return out.is_nil() ? R::Err : R::Ok;
    }
    bool got = false;
    if (py_contains(self.v, item.v, got) != R::Ok)
        return R::Err;
    out = value_bool(got);
    return R::Ok;
}

R s_iter(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__iter__", 0, 0))
        return R::Err;
    out = py_iter(me(a));
    return out.is_nil() ? R::Err : R::Ok;
}

R s_next(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__next__", 0, 0))
        return R::Err;
    R r = py_next(me(a), out);
    if (r != R::NotImpl)
        return r;
    Value e = exc_new(exc_find("StopIteration"), Value());
    return e.is_nil() ? R::Err : err_set_value(e);
}

R s_hash(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__hash__", 0, 0))
        return R::Err;
    u32 h = 0;
    if (py_hash(me(a), h) != R::Ok)
        return R::Err;
    out = int_from_i64(i32(h));
    return out.is_nil() ? R::Err : R::Ok;
}

R s_repr(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__repr__", 0, 0))
        return R::Err;
    // A frozendict subclass prints under its own name, so its instance is
    // asked rather than the dict inside it.
    String text;
    Value self = is_inst(a.args[0]) && is_frozendict(me(a)) ? a.args[0] : me(a);
    if (py_repr(self, text) != R::Ok)
        return R::Err;
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R s_str(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__str__", 0, 0))
        return R::Err;
    String text;
    if (py_str(me(a), text) != R::Ok)
        return R::Err;
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R s_bool(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__bool__", 0, 0))
        return R::Err;
    out = value_bool(py_truth(me(a)));
    return R::Ok;
}

R s_index(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__index__", 0, 0))
        return R::Err;
    Value v = me(a);
    i64 n   = 0;
    if (!as_index(v, n))
        return err_set2("TypeError", "cannot be interpreted as an integer", type_name(v));
    out = v;
    return R::Ok;
}

R s_reversed(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__reversed__", 0, 0))
        return R::Err;
    out = reversed_new(me(a));
    return out.is_nil() ? R::Err : R::Ok;
}

// The six comparisons, one native each. NotImplemented rather than a
// TypeError, which is what a comparison method owes its caller.
template <usize I>
R s_cmp(const CallArgs &a, Value &out)
{
    constexpr Cmp OPS[6]   = { Cmp::Eq, Cmp::Ne, Cmp::Lt, Cmp::Le, Cmp::Gt, Cmp::Ge };
    constexpr Str NAMES[6] = { "__eq__", "__ne__", "__lt__", "__le__", "__gt__", "__ge__" };
    if (!meth_args(a, NAMES[I], 1, 1))
        return R::Err;
    const Type *t = type_of(me(a));
    // Equality falls back to identity; an order that no slot answers does not.
    if (I >= 2 && (!t || !t->order)) {
        out = value_notimpl();
        return R::Ok;
    }
    bool got = false;
    if (py_cmp(me(a), a.args[1], OPS[I], got) != R::Ok) {
        if (err_kind() != "TypeError")
            return R::Err;
        err_clear();
        out = value_notimpl();
        return R::Ok;
    }
    out = value_bool(got);
    return R::Ok;
}

// The twelve binary operators, and the reflected half of each.
template <usize I, bool REFL>
R s_binop(const CallArgs &a, Value &out)
{
    constexpr Op OPS[12] = { Op::Add, Op::Sub, Op::Mul, Op::Div, Op::FloorDiv, Op::Mod,
                             Op::Pow, Op::And, Op::Or,  Op::Xor, Op::Lsh,      Op::Rsh };
    if (a.nargs != 2 || a.nkw)
        return err_set("TypeError", "expected one argument");
    Value x = REFL ? a.args[1] : me(a);
    Value y = REFL ? me(a) : a.args[1];
    R r     = py_binop_try(x, y, OPS[I], out);
    if (r == R::NotImpl) {
        out = value_notimpl();
        return R::Ok;
    }
    if (r == R::Err && err_kind() == "TypeError") {
        err_clear();
        out = value_notimpl();
        return R::Ok;
    }
    return r;
}

R s_neg(const CallArgs &a, Value &out)
{
    return meth_args(a, "__neg__", 0, 0) ? py_neg(me(a), out) : R::Err;
}

R s_pos(const CallArgs &a, Value &out)
{
    return meth_args(a, "__pos__", 0, 0) ? py_pos(me(a), out) : R::Err;
}

R s_invert(const CallArgs &a, Value &out)
{
    return meth_args(a, "__invert__", 0, 0) ? py_invert(me(a), out) : R::Err;
}

R s_abs(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__abs__", 0, 0))
        return R::Err;
    Value v = me(a);
    if (is_intval(v))
        return int_absolute(v, out);
    if (is_float(v)) {
        f64 x = float_of(v);
        out   = float_new(x < 0 ? -x : x);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_complex(v))
        return complex_abs(v, out);
    return err_set2("TypeError", "bad operand type for abs()", type_name(v));
}

// Every dunder that can be installed, with the slot whose presence asks for it.
enum : u32 {
    NEED_LEN    = 1u << 0,
    NEED_GET    = 1u << 1,
    NEED_SET    = 1u << 2,
    NEED_DEL    = 1u << 3,
    NEED_IN     = 1u << 4,
    NEED_ITER   = 1u << 5,
    NEED_NEXT   = 1u << 6,
    NEED_HASH   = 1u << 7,
    NEED_ORDER  = 1u << 8,
    NEED_NUM    = 1u << 9,  // a number: neg, pos, abs
    NEED_INT    = 1u << 10, // an integer: index, invert
    NEED_SEQ    = 1u << 11, // indexed by position: __reversed__
    NEED_STR    = 1u << 13, // a str that is not the repr
    NEED_TRUTH  = 1u << 14, // truth of its own, rather than through len
    NEED_ALWAYS = 1u << 12,
};

// Which operators a type answers. `Type::binop` is one slot for all twelve, so
// it cannot say; and it matters, because collections.abc.Set decides
// membership by asking a class for __and__, __or__, __xor__ and __sub__.
enum : u32 {
    OP_ADD     = 1u << 0,
    OP_SUB     = 1u << 1,
    OP_MUL     = 1u << 2,
    OP_DIV     = 1u << 3,
    OP_FLOOR   = 1u << 4,
    OP_MOD     = 1u << 5,
    OP_POW     = 1u << 6,
    OP_AND     = 1u << 7,
    OP_OR      = 1u << 8,
    OP_XOR     = 1u << 9,
    OP_LSH     = 1u << 10,
    OP_RSH     = 1u << 11,
    OP_INT     = 0xfff, // every one of them
    OP_REAL    = OP_ADD | OP_SUB | OP_MUL | OP_DIV | OP_FLOOR | OP_MOD | OP_POW,
    OP_COMPLEX = OP_ADD | OP_SUB | OP_MUL | OP_DIV | OP_POW,
    OP_TEXT    = OP_ADD | OP_MUL | OP_MOD,
    OP_SEQ     = OP_ADD | OP_MUL,
    OP_SET     = OP_AND | OP_OR | OP_XOR | OP_SUB,
};

struct Slot {
    Str name;
    R (*fn)(const CallArgs &, Value &out);
    u32 need;
};

constexpr Slot SLOTS[] = {
    { "__len__", s_len, NEED_LEN },          { "__getitem__", s_getitem, NEED_GET },
    { "__setitem__", s_setitem, NEED_SET },  { "__delitem__", s_delitem, NEED_DEL },
    { "__contains__", s_contains, NEED_IN }, { "__iter__", s_iter, NEED_ITER },
    { "__next__", s_next, NEED_NEXT },       { "__reversed__", s_reversed, NEED_SEQ },
    { "__hash__", s_hash, NEED_HASH },       { "__repr__", s_repr, NEED_ALWAYS },
    { "__str__", s_str, NEED_STR },          { "__bool__", s_bool, NEED_TRUTH },
    { "__index__", s_index, NEED_INT },      { "__invert__", s_invert, NEED_INT },
    { "__neg__", s_neg, NEED_NUM },          { "__pos__", s_pos, NEED_NUM },
    { "__abs__", s_abs, NEED_NUM },          { "__eq__", s_cmp<0>, NEED_ALWAYS },
    { "__ne__", s_cmp<1>, NEED_ALWAYS },     { "__lt__", s_cmp<2>, NEED_ORDER },
    { "__le__", s_cmp<3>, NEED_ORDER },      { "__gt__", s_cmp<4>, NEED_ORDER },
    { "__ge__", s_cmp<5>, NEED_ORDER },
};

#define BIN_ROW(i, plain, refl)              \
    { plain, s_binop<i, false>, 1u << (i) }, \
    {                                        \
        refl, s_binop<i, true>, 1u << (i)    \
    }

constexpr Slot BINOPS[] = {
    BIN_ROW(0, "__add__", "__radd__"),
    BIN_ROW(1, "__sub__", "__rsub__"),
    BIN_ROW(2, "__mul__", "__rmul__"),
    BIN_ROW(3, "__truediv__", "__rtruediv__"),
    BIN_ROW(4, "__floordiv__", "__rfloordiv__"),
    BIN_ROW(5, "__mod__", "__rmod__"),
    BIN_ROW(6, "__pow__", "__rpow__"),
    BIN_ROW(7, "__and__", "__rand__"),
    BIN_ROW(8, "__or__", "__ror__"),
    BIN_ROW(9, "__xor__", "__rxor__"),
    BIN_ROW(10, "__lshift__", "__rlshift__"),
    BIN_ROW(11, "__rshift__", "__rrshift__"),
};

#undef BIN_ROW

// What `t` fills in, as the flags above.
u32 needs_of(const Type *t, u32 extra)
{
    u32 n = NEED_ALWAYS | extra;
    if (t->len)
        n |= NEED_LEN;
    if (t->getitem)
        n |= NEED_GET;
    if (t->setitem)
        n |= NEED_SET;
    if (t->delitem)
        n |= NEED_DEL;
    if (t->contains)
        n |= NEED_IN;
    if (t->iter)
        n |= NEED_ITER;
    if (t->next)
        n |= NEED_NEXT;
    if (t->hash)
        n |= NEED_HASH;
    if (t->order)
        n |= NEED_ORDER;
    // A null `str` slot means "use repr", so the type owes no __str__ of its
    // own -- and installing one would shadow a subclass's __repr__.
    if (t->str)
        n |= NEED_STR;
    // CPython gives __bool__ only to a type with a truth of its own; one that
    // is empty-or-not answers through __len__, and so does this.
    if (t->truth)
        n |= NEED_TRUTH;
    return n;
}

// Only where the type has not got one already: a method table's own entry --
// str.__format__, bytes.__contains__ -- is the real one and wins.
bool add_if_absent(Value cls, Str name, Value fn)
{
    Root rc{ cls }, rf{ fn };
    StrObj *n = str_intern(name);
    if (!n)
        return oom() == R::Ok;
    DictObj *d = static_cast<DictObj *>(type_obj(rc.v)->dict.obj());
    Value had;
    R r = dict_get(d, obj_value(n), had);
    if (r == R::Err)
        return false;
    if (r == R::Ok)
        return true;
    return dict_set(static_cast<DictObj *>(type_obj(rc.v)->dict.obj()), obj_value(n), rf.v) ==
           R::Ok;
}

bool install_for(const Type *t, u32 extra, u32 ops)
{
    Root cls{ type_wrap(t) };
    if (cls.v.is_nil())
        return false;
    u32 need = needs_of(t, extra);
    for (const Slot &s : SLOTS) {
        if ((need & s.need) != s.need)
            continue;
        Root fn{ native_new(s.name, s.fn) };
        if (fn.v.is_nil())
            return false;
        static_cast<NativeObj *>(fn.v.obj())->owner = cls.v;
        if (!add_if_absent(cls.v, s.name, fn.v))
            return false;
    }
    for (const Slot &s : BINOPS) {
        if (!(ops & s.need))
            continue;
        Root fn{ native_new(s.name, s.fn) };
        if (fn.v.is_nil())
            return false;
        static_cast<NativeObj *>(fn.v.obj())->owner = cls.v;
        if (!add_if_absent(cls.v, s.name, fn.v))
            return false;
    }
    // An unhashable built-in says so the way CPython does, with the name bound
    // to None rather than missing: that is what `type_unhashable` reads.
    if (!t->hash && !add_if_absent(cls.v, "__hash__", value_none()))
        return false;
    return true;
}

struct Row {
    const Type *t;
    u32 extra; // what the slot table cannot say
    u32 ops;   // which operators this type answers
};

} // namespace

// __getnewargs__ of an immutable built-in: the plain value, as a 1-tuple,
// which is what pickle and copy rebuild one from.
R s_getnewargs(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__getnewargs__", 0, 0))
        return R::Err;
    Root v{ me(a) };
    if (is_bool(v.v))
        v = Value::of_int(is_true(v.v) ? 1 : 0);
    // A complex is rebuilt from its two parts.
    if (v.v.is_obj() && v.v.obj()->type == &complex_type) {
        ComplexObj *c = static_cast<ComplexObj *>(v.v.obj());
        Root re{ float_new(c->re) };
        Root im{ float_new(static_cast<ComplexObj *>(v.v.obj())->im) };
        TupleObj *t = re.v.is_nil() || im.v.is_nil() ? nullptr : tuple_new(2);
        if (!t)
            return err_pending() ? R::Err : err_set("MemoryError", "out of memory");
        t->items()[0] = re.v;
        t->items()[1] = im.v;
        out           = obj_value(t);
        return R::Ok;
    }
    TupleObj *t = tuple_new(1);
    if (!t)
        return err_set("MemoryError", "out of memory");
    t->items()[0] = v.v;
    out           = obj_value(t);
    return R::Ok;
}

constexpr Method NEWARGS[] = { { "__getnewargs__", s_getnewargs } };

// f.__call__(*args, **kwargs): the call itself, for the callables whose call
// is a slot. s[0] the callable, s[1..3] the arguments.
R call_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        if (k->s[2].is_nil())
            return cont_call_v(k, k->s[0], k->s[1]);
        return cont_call_kw(k, k->s[0], k->s[1], k->s[2], k->s[3]);
    }
    return cont_done(k, in);
}

R s_call(const CallArgs &a, Value &out)
{
    if (a.nargs < 1)
        return err_set("TypeError", "__call__() needs a callable");
    Root self{ a.args[0] };
    TupleObj *args = tuple_new(a.nargs - 1);
    if (!args)
        return err_set("MemoryError", "out of memory");
    for (u32 i = 1; i < a.nargs; i++)
        args->items()[i - 1] = a.args[i];
    Root ra{ obj_value(args) }, names, vals;
    if (a.nkw) {
        TupleObj *n = tuple_new(a.nkw);
        if (!n)
            return err_set("MemoryError", "out of memory");
        for (u32 i = 0; i < a.nkw; i++)
            n->items()[i] = a.kwnames[i];
        names       = obj_value(n);
        TupleObj *v = tuple_new(a.nkw);
        if (!v)
            return err_set("MemoryError", "out of memory");
        for (u32 i = 0; i < a.nkw; i++)
            v->items()[i] = a.kwvals[i];
        vals = obj_value(v);
    }
    Root kv{ cont_new(call_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = self.v;
    k->s[1]    = ra.v;
    k->s[2]    = names.v;
    k->s[3]    = vals.v;
    out        = kv.v;
    return R::Ok;
}

constexpr Method CALL[] = { { "__call__", s_call } };

bool slot_methods()
{
    static const Type *const IMMUTABLE[] = { &int_type, &float_type, &complex_type,
                                             &str_type, &bytes_type, &tuple_type };
    for (const Type *t : IMMUTABLE)
        if (!method_install(t, NEWARGS))
            return false;
    static const Type *const CALLABLE[] = { &func_type, &native_type, &method_type };
    for (const Type *t : CALLABLE)
        if (!method_install(t, CALL))
            return false;
    const Row ROWS[] = {
        { &int_type, NEED_NUM | NEED_INT, OP_INT },
        { &bool_type, NEED_NUM | NEED_INT, OP_INT },
        { &float_type, NEED_NUM, OP_REAL },
        { &complex_type, NEED_NUM, OP_COMPLEX },
        { &str_type, NEED_SEQ, OP_TEXT },
        { &bytes_type, NEED_SEQ, OP_TEXT },
        { &bytearray_type, NEED_SEQ, OP_TEXT },
        { &tuple_type, NEED_SEQ, OP_SEQ },
        { &list_type, NEED_SEQ, OP_SEQ },
        { &dict_type, 0, OP_OR },
        { &frozendict_type, 0, OP_OR },
        { &set_type, 0, OP_SET },
        { &frozenset_type, 0, OP_SET },
        { &view_type, 0, OP_SET },
        { &range_type, NEED_SEQ | NEED_TRUTH, 0 },
        { &slice_type, 0, 0 },
        { &memview_type, NEED_SEQ, 0 },
        { &none_type, 0, 0 },
        { &weakref_type, 0, 0 },
    };
    for (const Row &r : ROWS)
        if (!install_for(r.t, r.extra, r.ops))
            return false;
    // PEP 560: the containers answer a subscript with a generic alias, which
    // is what makes `list[int]` a value and `types.GenericAlias` a name.
    static const Type *const GENERIC[] = {
        &list_type,    &dict_type,  &set_type,  &frozenset_type,  &tuple_type,
        &memview_type, &range_type, &type_type, &frozendict_type,
    };
    return genalias_install(GENERIC, sizeof GENERIC / sizeof GENERIC[0]);
}
