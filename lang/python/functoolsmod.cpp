// `_functools`: the names CPython writes in C and functools.py prefers over its
// own fallbacks -- reduce, partial and Placeholder, cmp_to_key, and the
// lru_cache wrapper.
//
// Each calls a function the program wrote, so each is a continuation
// rather than loops: ground rule 2 again. The cache is the only one with state
// worth describing -- an insertion-ordered dict, which is what makes "least
// recently used" a matter of deleting a key and putting it back.
#include "builtin.h"
#include "call.h"
#include "gc.h"
#include "info.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// ------------------------------------------------------------------ reduce

// s[0] the list, s[1] the function, s[2] the accumulator.
R reduce_step(ContObj *k, Value in)
{
    ListObj *xs = list_of(k->s[0]);
    if (k->i > 0)
        k->s[2] = in;
    if (k->i >= xs->items.size())
        return cont_done(k, k->s[2]);
    Value item = xs->items[k->i++];
    if (k->s[2].is_nil()) {
        // No initial value: the first item is the accumulator, uncalled.
        k->s[2] = item;
        if (k->i >= xs->items.size())
            return cont_done(k, k->s[2]);
        item = xs->items[k->i++];
    }
    return cont_call(k, k->s[1], k->s[2], 2, item);
}

R b_reduce(const CallArgs &a, Value &out)
{
    if (!args_only(a, "reduce", 2, 3))
        return R::Err;
    if (a.nargs > 1 && iter_needs_vm(a.args[1]))
        return iter_park(a, 1, b_reduce, out);
    ListObj *xs = py_list_of(a.args[1]);
    if (!xs)
        return R::Err;
    Root rx{ obj_value(xs) };
    if (a.nargs < 3 && list_of(rx.v)->items.empty())
        return err_set("TypeError", "reduce() of empty iterable with no initial value");
    Root kv{ cont_new(reduce_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rx.v;
    k->s[1]    = a.args[0];
    k->s[2]    = a.nargs > 2 ? a.args[2] : Value();
    out        = kv.v;
    return R::Ok;
}

// A call already assembled in s[0..3]: make it, and answer with what it
// returned. Two of the three names here end in exactly this.
R call_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call_kw(k, k->s[0], k->s[1], k->s[2], k->s[3]);
    return cont_done(k, in);
}

// The same over s[2..5], which is where the cache's step keeps its call.
R uncached_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call_kw(k, k->s[2], k->s[3], k->s[4], k->s[5]);
    return cont_done(k, in);
}

// ------------------------------------------------------------- Placeholder

extern const Type placeholder_type;

Obj placeholder_obj{ &placeholder_type, nullptr, nullptr, OBJ_IMMORTAL };

bool is_placeholder(Value v)
{
    return v.is_obj() && v.obj() == &placeholder_obj;
}

R placeholder_repr(Value, String &out)
{
    return out.append("Placeholder") ? R::Ok : oom();
}

R ph_reduce(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    out = str_new("Placeholder");
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method PLACEHOLDER_METHODS[] = { { "__reduce__", ph_reduce } };

constexpr Type placeholder_type{ .name  = "functools._PlaceholderType",
                                 .repr  = placeholder_repr,
                                 .final = true };

R b_placeholder(const CallArgs &a, Value &out)
{
    if (a.nargs || a.nkw)
        return err_set("TypeError", "PlaceholderType takes no arguments");
    out = Value::of_obj(&placeholder_obj);
    return R::Ok;
}

// ----------------------------------------------------------------- partial

struct PartObj : Obj {
    Value fn;
    Value args;    // TupleObj, always
    Value kwnames; // TupleObj of StrObj, always
    Value kwvals;
    Value dict;  // __dict__, made when something is stored in it
    u32 phcount; // the Placeholders in args
};

PartObj *part_of(Value v)
{
    return static_cast<PartObj *>(v.obj());
}

void part_trace(Obj *o)
{
    PartObj *p = static_cast<PartObj *>(o);
    gc_mark(p->fn);
    gc_mark(p->args);
    gc_mark(p->kwnames);
    gc_mark(p->kwvals);
    gc_mark(p->dict);
}

R part_repr(Value v, String &out)
{
    PartObj *p  = part_of(v);
    TupleObj *t = static_cast<TupleObj *>(p->args.obj());
    TupleObj *n = static_cast<TupleObj *>(p->kwnames.obj());
    if (!out.append("functools.partial("))
        return oom();
    if (py_repr(p->fn, out) != R::Ok)
        return R::Err;
    for (u32 i = 0; i < t->len; i++) {
        if (!out.append(", "))
            return oom();
        if (py_repr(t->items()[i], out) != R::Ok)
            return R::Err;
    }
    for (u32 i = 0; i < n->len; i++) {
        if (!out.append(", ") || !out.append(str_of(n->items()[i])->str()) || !out.push('='))
            return oom();
        if (py_repr(static_cast<TupleObj *>(p->kwvals.obj())->items()[i], out) != R::Ok)
            return R::Err;
    }
    return out.push(')') ? R::Ok : oom();
}

R part_getattr(Value v, StrObj *name, Value &out)
{
    PartObj *p = part_of(v);
    Str n      = name->str();
    if (n == "func")
        out = p->fn;
    else if (n == "args")
        out = p->args;
    else if (n == "keywords") {
        DictObj *d = dict_new();
        if (!d)
            return oom();
        Root rd{ obj_value(d) };
        TupleObj *names = static_cast<TupleObj *>(part_of(v)->kwnames.obj());
        for (u32 i = 0; i < names->len; i++) {
            Value key = static_cast<TupleObj *>(part_of(v)->kwnames.obj())->items()[i];
            Value val = static_cast<TupleObj *>(part_of(v)->kwvals.obj())->items()[i];
            if (dict_set(static_cast<DictObj *>(rd.v.obj()), key, val) != R::Ok)
                return R::Err;
        }
        out = rd.v;
    } else if (n == "__dict__") {
        if (p->dict.is_nil()) {
            DictObj *d = dict_new();
            if (!d)
                return oom();
            part_of(v)->dict = obj_value(d);
        }
        out = part_of(v)->dict;
    } else {
        if (!p->dict.is_nil()) {
            Value got;
            R r = dict_get(static_cast<DictObj *>(p->dict.obj()), obj_value(name), got);
            if (r == R::Err)
                return R::Err;
            if (r == R::Ok) {
                out = got;
                return R::Ok;
            }
        }
        return R::NotImpl;
    }
    return R::Ok;
}

R part_setattr(Value v, StrObj *name, Value val)
{
    Root rv{ v }, rx{ val };
    if (rx.v.is_nil()) {
        if (part_of(rv.v)->dict.is_nil())
            return err_set2("AttributeError", "no such attribute", name->str());
        R r = dict_del(static_cast<DictObj *>(part_of(rv.v)->dict.obj()), obj_value(name));
        return r == R::NotImpl ? err_set2("AttributeError", "no such attribute", name->str()) : r;
    }
    if (part_of(rv.v)->dict.is_nil()) {
        DictObj *d = dict_new();
        if (!d)
            return oom();
        part_of(rv.v)->dict = obj_value(d);
    }
    return dict_set(static_cast<DictObj *>(part_of(rv.v)->dict.obj()), obj_value(name), rx.v);
}

extern const Type partial_type;

// The kept arguments in front of the call's own, and the kept keywords under
// them: a keyword given again at the call site wins, as CPython's does.
R part_call(const CallArgs &a, Value &out)
{
    Root self{ method_self(a.args[0]) };
    PartObj *p   = part_of(self.v);
    TupleObj *pa = static_cast<TupleObj *>(p->args.obj());
    TupleObj *pn = static_cast<TupleObj *>(p->kwnames.obj());

    u32 given = a.nargs ? a.nargs - 1 : 0;
    u32 holes = p->phcount;
    if (given < holes) {
        char t[24];
        Buf<128> b;
        b.put("missing positional arguments in 'partial' call; expected at least ");
        b.put(int_text(t, sizeof t, i64(holes))).put(", got ");
        b.put(int_text(t, sizeof t, i64(given)));
        return err_set("TypeError", b.str());
    }
    TupleObj *args = tuple_new(pa->len + given - holes);
    if (!args)
        return oom();
    Root ra{ obj_value(args) };
    pa        = static_cast<TupleObj *>(part_of(self.v)->args.obj());
    u32 taken = 1;
    for (u32 i = 0; i < pa->len; i++)
        static_cast<TupleObj *>(ra.v.obj())->items()[i] =
            is_placeholder(pa->items()[i]) ? a.args[taken++] : pa->items()[i];
    for (u32 i = taken; i < a.nargs; i++)
        static_cast<TupleObj *>(ra.v.obj())->items()[pa->len + i - taken] = a.args[i];

    pn      = static_cast<TupleObj *>(part_of(self.v)->kwnames.obj());
    u32 own = 0;
    for (u32 i = 0; i < pn->len; i++) {
        bool shadowed = false;
        for (u32 k = 0; k < a.nkw; k++)
            shadowed = shadowed || a.kwnames[k] == pn->items()[i];
        own += !shadowed;
    }
    TupleObj *names = tuple_new(own + a.nkw);
    if (!names)
        return oom();
    Root rn{ obj_value(names) };
    TupleObj *vals = tuple_new(own + a.nkw);
    if (!vals)
        return oom();
    Root rv{ obj_value(vals) };
    pn     = static_cast<TupleObj *>(part_of(self.v)->kwnames.obj());
    u32 at = 0;
    for (u32 i = 0; i < pn->len; i++) {
        bool shadowed = false;
        for (u32 k = 0; k < a.nkw; k++)
            shadowed = shadowed || a.kwnames[k] == pn->items()[i];
        if (shadowed)
            continue;
        static_cast<TupleObj *>(rn.v.obj())->items()[at] = pn->items()[i];
        static_cast<TupleObj *>(rv.v.obj())->items()[at] =
            static_cast<TupleObj *>(part_of(self.v)->kwvals.obj())->items()[i];
        at++;
    }
    for (u32 k = 0; k < a.nkw; k++) {
        static_cast<TupleObj *>(rn.v.obj())->items()[at] = a.kwnames[k];
        static_cast<TupleObj *>(rv.v.obj())->items()[at] = a.kwvals[k];
        at++;
    }

    Root kv{ cont_new(call_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = part_of(self.v)->fn;
    k->s[1]    = ra.v;
    k->s[2]    = rn.v;
    k->s[3]    = rv.v;
    out        = kv.v;
    return R::Ok;
}

constexpr Method PARTIAL_METHODS[] = { { "__call__", part_call } };

constexpr Type partial_type{ .name    = "functools.partial",
                             .trace   = part_trace,
                             .repr    = part_repr,
                             .getattr = part_getattr,
                             .setattr = part_setattr };

R b_partial(const CallArgs &a, Value &out)
{
    if (a.nargs < 1)
        return err_set("TypeError", "type 'partial' takes at least one argument");
    if (!py_callable(a.args[0]))
        return err_set("TypeError", "the first argument must be callable");
    if (a.nargs > 1 && is_placeholder(a.args[a.nargs - 1]))
        return err_set("TypeError", "trailing Placeholders are not allowed");
    for (u32 i = 0; i < a.nkw; i++)
        if (is_placeholder(a.kwvals[i]))
            return err_set("TypeError", "Placeholder cannot be passed as a keyword argument");

    // partial(partial(f, x), y) is one partial: y fills x's Placeholders
    // first, and its keywords go over x's.
    Root fn{ a.args[0] };
    Root inner_args, inner_names, inner_vals;
    if (fn.v.is_obj() && fn.v.obj()->type == &partial_type && part_of(fn.v)->dict.is_nil()) {
        inner_args  = part_of(fn.v)->args;
        inner_names = part_of(fn.v)->kwnames;
        inner_vals  = part_of(fn.v)->kwvals;
        fn          = part_of(fn.v)->fn;
    }
    u32 old   = inner_args.v.is_nil() ? 0 : static_cast<TupleObj *>(inner_args.v.obj())->len;
    u32 given = a.nargs - 1;
    u32 holes = 0;
    for (u32 i = 0; i < old; i++)
        holes += is_placeholder(static_cast<TupleObj *>(inner_args.v.obj())->items()[i]);
    u32 filled     = given < holes ? given : holes;
    TupleObj *args = tuple_new(old + given - filled);
    if (!args)
        return oom();
    Root ra{ obj_value(args) };
    u32 next = 1;
    for (u32 i = 0; i < old; i++) {
        Value x = static_cast<TupleObj *>(inner_args.v.obj())->items()[i];
        if (is_placeholder(x) && next < a.nargs)
            x = a.args[next++];
        static_cast<TupleObj *>(ra.v.obj())->items()[i] = x;
    }
    for (u32 i = next; i < a.nargs; i++)
        static_cast<TupleObj *>(ra.v.obj())->items()[old + i - next] = a.args[i];
    u32 phcount    = 0;
    TupleObj *made = static_cast<TupleObj *>(ra.v.obj());
    for (u32 i = 0; i < made->len; i++)
        phcount += is_placeholder(made->items()[i]);

    // The kept keywords, less those given again, then the new ones.
    u32 had  = inner_names.v.is_nil() ? 0 : static_cast<TupleObj *>(inner_names.v.obj())->len;
    u32 keep = 0;
    for (u32 i = 0; i < had; i++) {
        bool shadowed = false;
        for (u32 k = 0; k < a.nkw; k++)
            shadowed = shadowed ||
                       a.kwnames[k] == static_cast<TupleObj *>(inner_names.v.obj())->items()[i];
        keep += !shadowed;
    }
    TupleObj *names = tuple_new(keep + a.nkw);
    if (!names)
        return oom();
    Root rn{ obj_value(names) };
    TupleObj *vals = tuple_new(keep + a.nkw);
    if (!vals)
        return oom();
    Root rv{ obj_value(vals) };
    u32 at = 0;
    for (u32 i = 0; i < had; i++) {
        Value key     = static_cast<TupleObj *>(inner_names.v.obj())->items()[i];
        bool shadowed = false;
        for (u32 k = 0; k < a.nkw; k++)
            shadowed = shadowed || a.kwnames[k] == key;
        if (shadowed)
            continue;
        static_cast<TupleObj *>(rn.v.obj())->items()[at] = key;
        static_cast<TupleObj *>(rv.v.obj())->items()[at] =
            static_cast<TupleObj *>(inner_vals.v.obj())->items()[i];
        at++;
    }
    for (u32 k = 0; k < a.nkw; k++, at++) {
        static_cast<TupleObj *>(rn.v.obj())->items()[at] = a.kwnames[k];
        static_cast<TupleObj *>(rv.v.obj())->items()[at] = a.kwvals[k];
    }

    PartObj *p = static_cast<PartObj *>(obj_alloc(&partial_type, sizeof(PartObj)));
    if (!p)
        return oom();
    p->fn      = fn.v;
    p->args    = ra.v;
    p->kwnames = rn.v;
    p->kwvals  = rv.v;
    p->dict    = Value();
    p->phcount = phcount;
    out        = obj_value(p);
    return R::Ok;
}

// -------------------------------------------------------------- cmp_to_key

// KeyWrapper is a class whose methods are natives: an instance compares by
// calling the program's function, which the sort can only do through a
// method. The factory and the keys it makes are both instances; the factory
// keeps no `obj`.
struct KeyHome {
    Value cls;
};

KeyHome *key_home;

void key_mark()
{
    gc_mark(key_home->cls);
}

StrObj *key_name(Str n)
{
    StrObj *k = str_intern(n);
    return k ? k : (oom(), nullptr);
}

// One of the instance's two entries, or Nil.
Value key_get(Value self, Str n)
{
    StrObj *k = key_name(n);
    Value d   = inst_of(self)->dict;
    Value got;
    if (!k || d.is_nil() || dict_get(static_cast<DictObj *>(d.obj()), obj_value(k), got) != R::Ok)
        return Value();
    return got;
}

bool is_key(Value v)
{
    return key_home && is_inst(v) && inst_of(v)->cls == key_home->cls;
}

Value key_make(Value cmp, Value obj)
{
    Root rc{ cmp }, ro{ obj };
    Root self{ inst_new(key_home->cls) };
    if (self.v.is_nil())
        return Value();
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    inst_of(self.v)->dict = obj_value(d);
    StrObj *kc            = key_name("_cmp");
    if (!kc || dict_set(d, obj_value(kc), rc.v) != R::Ok)
        return Value();
    if (!ro.v.is_nil()) {
        StrObj *ko = key_name("obj");
        if (!ko || dict_set(static_cast<DictObj *>(inst_of(self.v)->dict.obj()), obj_value(ko),
                            ro.v) != R::Ok)
            return Value();
    }
    return self.v;
}

R key_call(const CallArgs &a, Value &out)
{
    if (!a.nargs || !is_key(a.args[0]))
        return err_set("TypeError", "KeyWrapper.__call__ requires a KeyWrapper");
    Value obj;
    if (a.nargs == 2 && !a.nkw)
        obj = a.args[1];
    else if (a.nargs == 1 && a.nkw == 1 && is_str(a.kwnames[0]) &&
             str_of(a.kwnames[0])->str() == "obj")
        obj = a.kwvals[0];
    else if (a.nargs == 1 && !a.nkw)
        return err_set("TypeError", "K() missing required argument 'obj' (pos 1)");
    else
        return err_set("TypeError", "K() takes exactly one argument");
    out = key_make(key_get(a.args[0], "_cmp"), obj);
    return out.is_nil() ? R::Err : R::Ok;
}

Str cmp_name(Cmp op)
{
    switch (op) {
    case Cmp::Lt:
        return "__lt__";
    case Cmp::Le:
        return "__le__";
    case Cmp::Gt:
        return "__gt__";
    case Cmp::Ge:
        return "__ge__";
    case Cmp::Eq:
        return "__eq__";
    default:
        return "__ne__";
    }
}

// s[0..2] the function and the two objects; j the operator. The answer is
// `cmp(x, y) op 0`, which may itself be a call.
R key_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        return cont_call(k, k->s[0], k->s[1], 2, k->s[2]);
    case 1: {
        Cmp op = Cmp(k->j);
        if (type_has_py_special(in, cmp_name(op))) {
            Root m{ type_special(in, cmp_name(op)) };
            if (!m.v.is_nil())
                return cont_call(k, m.v, Value::of_int(0));
            if (err_pending())
                return R::Err;
        }
        bool yes = false;
        if (py_cmp(in, Value::of_int(0), op, yes) != R::Ok)
            return R::Err;
        return cont_done(k, value_bool(yes));
    }
    default:
        return cont_done(k, in);
    }
}

R key_compare(const CallArgs &a, Value &out, Cmp op)
{
    if (!meth_args(a, cmp_name(op), 1, 1))
        return R::Err;
    if (!is_key(a.args[0]) || !is_key(a.args[1]))
        return err_set("TypeError", "other argument must be K instance");
    Root x{ key_get(a.args[0], "obj") }, y{ key_get(a.args[1], "obj") };
    Root fn{ key_get(a.args[0], "_cmp") };
    if (x.v.is_nil() || y.v.is_nil() || fn.v.is_nil())
        return err_set("TypeError", "object argument is not set");
    Root kv{ cont_new(key_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = fn.v;
    k->s[1]    = x.v;
    k->s[2]    = y.v;
    k->j       = u32(op);
    out        = kv.v;
    return R::Ok;
}

R key_lt(const CallArgs &a, Value &out)
{
    return key_compare(a, out, Cmp::Lt);
}

R key_le(const CallArgs &a, Value &out)
{
    return key_compare(a, out, Cmp::Le);
}

R key_gt(const CallArgs &a, Value &out)
{
    return key_compare(a, out, Cmp::Gt);
}

R key_ge(const CallArgs &a, Value &out)
{
    return key_compare(a, out, Cmp::Ge);
}

R key_eq(const CallArgs &a, Value &out)
{
    return key_compare(a, out, Cmp::Eq);
}

R key_ne(const CallArgs &a, Value &out)
{
    return key_compare(a, out, Cmp::Ne);
}

R key_init(const CallArgs &, Value &)
{
    return err_set("TypeError", "cannot create 'functools.KeyWrapper' instances");
}

constexpr Method KEY_METHODS[] = {
    { "__call__", key_call }, { "__lt__", key_lt }, { "__le__", key_le }, { "__gt__", key_gt },
    { "__ge__", key_ge },     { "__eq__", key_eq }, { "__ne__", key_ne }, { "__init__", key_init },
};

bool key_class()
{
    key_home = heap_new<KeyHome>();
    if (!key_home)
        return oom() == R::Ok;
    gc_root_hook(key_mark);
    DictObj *body = dict_new();
    if (!body)
        return oom() == R::Ok;
    Root rd{ obj_value(body) };
    for (const Method &m : KEY_METHODS) {
        Root fn{ native_new(m.name, m.fn) };
        StrObj *k = str_intern(m.name);
        if (fn.v.is_nil() || !k)
            return false;
        if (m.fn != key_init && m.fn != key_call)
            fn.v.obj()->flags |= OBJ_PYLIKE;
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(k), fn.v) != R::Ok)
            return false;
    }
    if (!mod_put(static_cast<DictObj *>(rd.v.obj()), "__hash__", value_none()) ||
        !mod_str(static_cast<DictObj *>(rd.v.obj()), "__module__", "functools"))
        return false;
    Root name{ str_new("KeyWrapper") };
    Root base{ type_object() };
    if (name.v.is_nil() || base.v.is_nil())
        return false;
    TupleObj *bases = tuple_new(1);
    if (!bases)
        return oom() == R::Ok;
    bases->items()[0] = base.v;
    Root rb{ obj_value(bases) };
    key_home->cls = type_new(name.v, rb.v, rd.v);
    return !key_home->cls.is_nil();
}

R b_cmp_to_key(const CallArgs &a, Value &out)
{
    Value fn;
    if (a.nargs == 1 && !a.nkw)
        fn = a.args[0];
    else if (!a.nargs && a.nkw == 1 && is_str(a.kwnames[0]) &&
             str_of(a.kwnames[0])->str() == "mycmp")
        fn = a.kwvals[0];
    else if (!a.nargs && !a.nkw)
        return err_set("TypeError", "cmp_to_key() missing required argument 'mycmp' (pos 1)");
    else
        return err_set("TypeError", "cmp_to_key() takes exactly one argument");
    if (!key_home && !key_class())
        return R::Err;
    out = key_make(fn, Value());
    return out.is_nil() ? R::Err : R::Ok;
}

// --------------------------------------------------------------- lru_cache

INFO_TYPE(cache_info_type, "CacheInfo");

struct CacheObj : Obj {
    Value fn;
    Value cache; // the keys in the order they were last used
    Value info;  // what cache_info() is to build with, or Nil for our own
    i64 maxsize; // -1 for no bound
    i64 hits, misses;
    bool typed;
};

CacheObj *cache_of(Value v)
{
    return static_cast<CacheObj *>(v.obj());
}

void cache_trace(Obj *o)
{
    gc_mark(static_cast<CacheObj *>(o)->fn);
    gc_mark(static_cast<CacheObj *>(o)->cache);
    gc_mark(static_cast<CacheObj *>(o)->info);
}

R cache_repr(Value v, String &out)
{
    if (!out.append("<functools._lru_cache_wrapper "))
        return oom();
    if (py_repr(cache_of(v)->fn, out) != R::Ok)
        return R::Err;
    return out.push('>') ? R::Ok : oom();
}

R cache_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() != "__wrapped__")
        return R::NotImpl;
    out = cache_of(v)->fn;
    return R::Ok;
}

extern const Type cache_type;

// The key a call is looked up under. A tuple of the positional arguments, a
// marker, then each keyword as name and value; `typed` puts each argument's
// type in beside it, which is what makes 1 and 1.0 different calls.
Value cache_key(Value self, const CallArgs &a)
{
    Root rs{ self };
    bool typed  = cache_of(rs.v)->typed;
    u32 pos     = a.nargs ? a.nargs - 1 : 0;
    u32 n       = pos + (a.nkw ? 1 + a.nkw * 2 : 0) + (typed ? pos + a.nkw : 0);
    TupleObj *t = tuple_new(n);
    if (!t)
        return oom(), Value();
    Root rt{ obj_value(t) };
    u32 at = 0;
    for (u32 i = 1; i < a.nargs; i++)
        static_cast<TupleObj *>(rt.v.obj())->items()[at++] = a.args[i];
    if (a.nkw) {
        static_cast<TupleObj *>(rt.v.obj())->items()[at++] = value_ellipsis();
        for (u32 k = 0; k < a.nkw; k++) {
            static_cast<TupleObj *>(rt.v.obj())->items()[at++] = a.kwnames[k];
            static_cast<TupleObj *>(rt.v.obj())->items()[at++] = a.kwvals[k];
        }
    }
    if (typed) {
        for (u32 i = 1; i < a.nargs; i++) {
            Value ty = type_of_value(a.args[i]);
            if (ty.is_nil())
                return Value();
            static_cast<TupleObj *>(rt.v.obj())->items()[at++] = ty;
        }
        for (u32 k = 0; k < a.nkw; k++) {
            Value ty = type_of_value(a.kwvals[k]);
            if (ty.is_nil())
                return Value();
            static_cast<TupleObj *>(rt.v.obj())->items()[at++] = ty;
        }
    }
    return rt.v;
}

// The answer has come back: store it, and evict the oldest if the cache is
// full. The dict keeps insertion order, so the oldest is the first entry.
R cache_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call_kw(k, k->s[2], k->s[3], k->s[4], k->s[5]);
    Root self{ k->s[0] }, key{ k->s[1] }, got{ in };
    DictObj *c = static_cast<DictObj *>(cache_of(self.v)->cache.obj());
    if (dict_set(c, key.v, got.v) != R::Ok)
        return R::Err;
    i64 most = cache_of(self.v)->maxsize;
    c        = static_cast<DictObj *>(cache_of(self.v)->cache.obj());
    while (most >= 0 && i64(dict_len(c)) > most) {
        usize at = 0;
        Value oldest, ignored;
        if (!table_next(c->t, at, oldest, ignored))
            break;
        if (dict_del(c, oldest) != R::Ok)
            return R::Err;
        c = static_cast<DictObj *>(cache_of(self.v)->cache.obj());
    }
    return cont_done(k, got.v);
}

R cache_call(const CallArgs &a, Value &out)
{
    Root self{ method_self(a.args[0]) };
    Root key{ cache_key(self.v, a) };
    if (key.v.is_nil())
        return R::Err;
    // maxsize of zero is a wrapper that counts and never keeps anything.
    if (cache_of(self.v)->maxsize != 0) {
        Value had;
        R r = dict_get(static_cast<DictObj *>(cache_of(self.v)->cache.obj()), key.v, had);
        if (r == R::Err)
            return R::Err;
        if (r == R::Ok) {
            cache_of(self.v)->hits++;
            // Used again, so it goes to the end: out and back in.
            DictObj *c = static_cast<DictObj *>(cache_of(self.v)->cache.obj());
            Root rh{ had };
            if (dict_del(c, key.v) != R::Ok || dict_set(c, key.v, rh.v) != R::Ok)
                return R::Err;
            out = rh.v;
            return R::Ok;
        }
    }
    cache_of(self.v)->misses++;

    TupleObj *args = tuple_new(a.nargs ? a.nargs - 1 : 0);
    if (!args)
        return oom();
    for (u32 i = 1; i < a.nargs; i++)
        args->items()[i - 1] = a.args[i];
    Root ra{ obj_value(args) };
    TupleObj *names = tuple_new(a.nkw);
    if (!names)
        return oom();
    for (u32 i = 0; i < a.nkw; i++)
        names->items()[i] = a.kwnames[i];
    Root rn{ obj_value(names) };
    TupleObj *vals = tuple_new(a.nkw);
    if (!vals)
        return oom();
    for (u32 i = 0; i < a.nkw; i++)
        vals->items()[i] = a.kwvals[i];
    Root rv{ obj_value(vals) };

    Root kv{ cont_new(cache_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = self.v;
    k->s[1]    = key.v;
    k->s[2]    = cache_of(self.v)->fn;
    k->s[3]    = ra.v;
    k->s[4]    = rn.v;
    k->s[5]    = rv.v;
    // A maxsize of zero stores nothing, so the step's second half is skipped.
    if (cache_of(self.v)->maxsize == 0)
        k->step = uncached_step;
    out = kv.v;
    return R::Ok;
}

constexpr Str CACHE_FIELDS[4] = { "hits", "misses", "maxsize", "currsize" };

R cache_cache_info(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "cache_info", 0, 0))
        return R::Err;
    Root self{ method_self(a.args[0]) };
    CacheObj *c = cache_of(self.v);
    Value items[4];
    Roots pin{ items, 4 };
    items[0] = int_from_i64(c->hits);
    items[1] = int_from_i64(c->misses);
    items[2] = c->maxsize < 0 ? value_none() : int_from_i64(c->maxsize);
    items[3] = int_from_i64(i64(dict_len(static_cast<DictObj *>(c->cache.obj()))));
    for (u32 i = 0; i < 4; i++)
        if (items[i].is_nil())
            return R::Err;
    // functools.py hands the wrapper the namedtuple its own cache_info is
    // to answer with. Building one is a call, so this parks; without one the
    // struct sequence above is the answer and nothing is called.
    if (!c->info.is_nil() && !is_none(c->info)) {
        TupleObj *args = tuple_new(4);
        if (!args)
            return oom();
        for (u32 i = 0; i < 4; i++)
            args->items()[i] = items[i];
        Root ra{ obj_value(args) };
        Root kv{ cont_new(call_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = cache_of(self.v)->info;
        cont_of(kv.v)->s[1] = ra.v;
        out                 = kv.v;
        return R::Ok;
    }
    out = info_new(&cache_info_type, items, CACHE_FIELDS, 4);
    return out.is_nil() ? R::Err : R::Ok;
}

R cache_cache_clear(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "cache_clear", 0, 0))
        return R::Err;
    Root self{ method_self(a.args[0]) };
    DictObj *d = dict_new();
    if (!d)
        return oom();
    CacheObj *c = cache_of(self.v);
    c->cache    = obj_value(d);
    c->hits = c->misses = 0;
    out                 = value_none();
    return R::Ok;
}

constexpr Method CACHE_METHODS[] = {
    { "__call__", cache_call },
    { "cache_info", cache_cache_info },
    { "cache_clear", cache_cache_clear },
};

constexpr Type cache_type{ .name    = "_lru_cache_wrapper",
                           .trace   = cache_trace,
                           .repr    = cache_repr,
                           .getattr = cache_getattr };

R b_lru_cache(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs < 1 || a.nargs > 4)
        return err_set("TypeError", "_lru_cache_wrapper() takes 1 to 4 arguments");
    i64 most = 128;
    if (a.nargs > 1) {
        if (is_none(a.args[1]))
            most = -1;
        else if (!as_index(a.args[1], most) || most < 0)
            return err_set("TypeError", "maxsize must be a non-negative integer or None");
    }
    bool typed = a.nargs > 2 && py_truth(a.args[2]);
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) }, fn{ a.args[0] };
    CacheObj *c = static_cast<CacheObj *>(obj_alloc(&cache_type, sizeof(CacheObj)));
    if (!c)
        return oom();
    c->fn      = fn.v;
    c->cache   = rd.v;
    c->info    = a.nargs > 3 ? a.args[3] : Value();
    c->maxsize = most;
    c->hits = c->misses = 0;
    c->typed            = typed;
    out                 = obj_value(c);
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "reduce", b_reduce },
    { "_lru_cache_wrapper", b_lru_cache },
    { "cmp_to_key", b_cmp_to_key },
};

} // namespace

bool functools_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&partial_type, PARTIAL_METHODS) ||
        !method_install(&cache_type, CACHE_METHODS) ||
        !method_install(&placeholder_type, PLACEHOLDER_METHODS))
        return false;
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, DEFS) || !mod_type(d, &partial_type, b_partial) ||
        !mod_type(d, &placeholder_type, b_placeholder))
        return false;
    return mod_put(d, "Placeholder", Value::of_obj(&placeholder_obj));
}
