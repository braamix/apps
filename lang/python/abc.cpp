// `_abc`: the seven functions CPython's abc.py prefers over its own fallback.
//
// An abstract base class is a metaclass whose __instancecheck__ and
// __subclasscheck__ answer for classes that were never derived from it -- the
// registry -- and for classes that merely have the right methods, which is what
// __subclasshook__ decides. Both of those are Python, so the check owns a
// continuation and asks for one call at a time; that is the same rule the
// descriptor protocol follows next door.
//
// The registry holds its classes strongly, where CPython holds them in a
// WeakSet. Nothing here ever retires a class, so the difference is not
// observable; the note is here because it is a difference.
#include "abc.h"

#include "call.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "patma.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// Bumped by every register(), which is what makes a negative answer stale.
i32 counter = 1;

// What one ABC keeps: who has registered with it, and what has been decided.
struct AbcObj : Obj {
    Value cls;
    Value registry; // ListObj of classes
    Value cache;    // ListObj of classes known to be subclasses
    Value negative; // ListObj of classes known not to be, as of `negver`
    i32 negver;
};

void abc_trace(Obj *o)
{
    AbcObj *a = static_cast<AbcObj *>(o);
    gc_mark(a->cls);
    gc_mark(a->registry);
    gc_mark(a->cache);
    gc_mark(a->negative);
}

R abc_repr(Value v, String &out)
{
    (void)v;
    return out.append("<_abc_data object>") ? R::Ok : oom();
}

constexpr Type abc_type{ .name = "_abc_data", .trace = abc_trace, .repr = abc_repr };

AbcObj *abc_of(Value v)
{
    return static_cast<AbcObj *>(v.obj());
}

// `issubclass`, kept so the check can ask for it as a call rather than
// answering a virtual subclass itself.
Value *sub_fn;

Value issubclass_fn()
{
    return sub_fn ? *sub_fn : Value();
}

Vec<Value> &items_of(Value v)
{
    return list_of(v)->items;
}

bool holds(Value list, Value x)
{
    for (usize i = 0; i < items_of(list).size(); i++)
        if (items_of(list)[i] == x)
            return true;
    return false;
}

// The _abc_impl a class keeps in its own namespace, or Nil.
Value impl_of(Value cls)
{
    StrObj *n = str_intern("_abc_impl");
    Value v;
    if (!n || !is_type(cls) || type_obj(cls)->dict.is_nil())
        return Value();
    return dict_get(static_cast<DictObj *>(type_obj(cls)->dict.obj()), obj_value(n), v) == R::Ok
               ? v
               : Value();
}

bool is_abc(Value v)
{
    return v.is_obj() && v.obj()->type == &abc_type;
}

// Whether `v` was marked with @abstractmethod. Read without the descriptor
// protocol: the flag is an ordinary attribute of the function it decorated.
bool is_abstract(Value v)
{
    StrObj *n = str_intern("__isabstractmethod__");
    Value got;
    if (!n)
        return false;
    if (py_getattr(v, n, got) != R::Ok)
        return err_clear(), false;
    return py_truth(got);
}

// The names a class leaves abstract: what it declares itself, plus what it
// inherits and has not overridden with something concrete.
R compute_abstract(Value cls, Value &out)
{
    Root rc{ cls };
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    DictObj *d = static_cast<DictObj *>(type_obj(rc.v)->dict.obj());
    usize at   = 0;
    Value k, v;
    while (table_next(d->t, at, k, v))
        if (is_abstract(v) && !list_push(list_of(rl.v), k))
            return oom();

    StrObj *am = str_intern("__abstractmethods__");
    if (!am)
        return oom();
    Value mro = type_obj(rc.v)->mro;
    for (usize i = 1; i < static_cast<TupleObj *>(mro.obj())->len; i++) {
        Value base = static_cast<TupleObj *>(mro.obj())->items()[i];
        Value names;
        if (!is_type(base) || type_obj(base)->dict.is_nil())
            continue;
        if (dict_get(static_cast<DictObj *>(type_obj(base)->dict.obj()), obj_value(am), names) !=
            R::Ok)
            continue;
        for (usize j = 0; j < items_of(names).size(); j++) {
            Value name = items_of(names)[j];
            Value found;
            if (holds(rl.v, name) || !is_str(name))
                continue;
            StrObj *ns = str_of(name);
            // Still abstract only if what the class resolves to is.
            if (type_lookup(rc.v, ns, found) == R::Ok && !is_abstract(found))
                continue;
            if (!list_push(list_of(rl.v), name))
                return oom();
        }
    }
    out = rl.v;
    return R::Ok;
}

R b_abc_init(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_abc_init", 1, 1) || !is_type(a.args[0]))
        return err_set2("TypeError", "_abc_init() takes a class", type_name(a.args[0]));
    Root rc{ a.args[0] };
    Root names;
    if (compute_abstract(rc.v, names.v) != R::Ok)
        return R::Err;

    // collections.abc.Sequence and Mapping say what a match statement takes
    // their instances for, and the name goes once it has been read.
    StrObj *tf = str_intern("__abc_tpflags__");
    Root flags;
    if (!tf)
        return oom();
    R fr = dict_get(static_cast<DictObj *>(type_obj(rc.v)->dict.obj()), obj_value(tf), flags.v);
    if (fr == R::Err)
        return R::Err;
    if (fr == R::Ok) {
        if (dict_del(static_cast<DictObj *>(type_obj(rc.v)->dict.obj()), obj_value(tf)) == R::Err)
            return R::Err;
        i64 bits = 0;
        if (flags.v.is_int() && as_index(flags.v, bits)) {
            bool seq = (bits & (1 << 5)) != 0, map = (bits & (1 << 6)) != 0;
            if (seq && map)
                return err_set("TypeError",
                               "__abc_tpflags__ cannot be both "
                               "Py_TPFLAGS_SEQUENCE and Py_TPFLAGS_MAPPING");
            patma_set(rc.v, u8((seq ? PATMA_SEQ : 0) | (map ? PATMA_MAP : 0)));
        }
    }
    StrObj *am = str_intern("__abstractmethods__");
    if (!am || dict_set(static_cast<DictObj *>(type_obj(rc.v)->dict.obj()), obj_value(am),
                        names.v) != R::Ok)
        return R::Err;

    AbcObj *o = static_cast<AbcObj *>(obj_alloc(&abc_type, sizeof(AbcObj)));
    if (!o)
        return oom();
    o->cls      = rc.v;
    o->registry = o->cache = o->negative = Value();
    o->negver                            = counter;
    Root ro{ obj_value(o) };
    for (u32 i = 0; i < 3; i++) {
        ListObj *l = list_new();
        if (!l)
            return oom();
        AbcObj *x                                                = abc_of(ro.v);
        (i == 0 ? x->registry : i == 1 ? x->cache : x->negative) = obj_value(l);
    }
    StrObj *im = str_intern("_abc_impl");
    if (!im ||
        dict_set(static_cast<DictObj *>(type_obj(rc.v)->dict.obj()), obj_value(im), ro.v) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R b_abc_register(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_abc_register", 2, 2))
        return R::Err;
    Value impl = impl_of(a.args[0]);
    if (!is_abc(impl))
        return err_set("TypeError", "_abc_register() argument 1 is not an ABC");
    if (!is_type(a.args[1]))
        return err_set2("TypeError", "can only register classes", type_name(a.args[1]));
    if (type_issub(a.args[0], a.args[1]))
        return err_set("RuntimeError", "refusing to create an inheritance cycle");
    Root ri{ impl }, rs{ a.args[1] };
    if (!holds(abc_of(ri.v)->registry, rs.v) && !list_push(list_of(abc_of(ri.v)->registry), rs.v))
        return oom();
    counter++;
    // A registered class is matched as the ABC's instances are.
    u8 kind = type_obj(a.args[0])->slots.patma & (PATMA_SEQ | PATMA_MAP);
    if (kind)
        patma_set(rs.v, kind);
    out = rs.v;
    return R::Ok;
}

// ------------------------------------------------------------- the check

// s[0] the ABC, s[1] the class asked about, s[2] its _abc_impl, s[3] the
// candidates still to try; j is where in them we are.
R sub_step(ContObj *k, Value in)
{
    AbcObj *impl = abc_of(k->s[2]);
    switch (k->i) {
    case 0: {
        k->i = 1;
        if (holds(impl->cache, k->s[1]))
            return cont_done(k, value_bool(true));
        if (impl->negver == counter && holds(impl->negative, k->s[1]))
            return cont_done(k, value_bool(false));
        Root hook{ type_special(k->s[0], "__subclasshook__") };
        if (hook.v.is_nil()) {
            // Reached through the class rather than an instance, which is what
            // a classmethod on the ABC is.
            StrObj *n = str_intern("__subclasshook__");
            Value found;
            if (!n)
                return oom();
            if (type_lookup(k->s[0], n, found) == R::Ok &&
                type_bind(found, Value(), k->s[0], hook.v) != R::Ok)
                return R::Err;
        }
        if (hook.v.is_nil())
            return sub_step(k, value_notimpl());
        return cont_call(k, hook.v, k->s[1]);
    }
    case 1: {
        k->i = 2;
        if (!is_notimpl(in)) {
            bool yes    = py_truth(in);
            Value where = yes ? impl->cache : impl->negative;
            if (!yes)
                impl->negver = counter;
            if (!holds(where, k->s[1]) && !list_push(list_of(where), k->s[1]))
                return oom();
            return cont_done(k, value_bool(yes));
        }
        // An ordinary subclass, and then the virtual ones: what has registered
        // with this ABC, and what has been derived from it.
        if (type_issub(k->s[1], k->s[0])) {
            if (!holds(impl->cache, k->s[1]) && !list_push(list_of(impl->cache), k->s[1]))
                return oom();
            return cont_done(k, value_bool(true));
        }
        ListObj *c = list_new();
        if (!c)
            return oom();
        k->s[3] = obj_value(c);
        for (usize i = 0; i < items_of(impl->registry).size(); i++)
            if (!list_push(list_of(k->s[3]), items_of(impl->registry)[i]))
                return oom();
        Value subs = type_obj(k->s[0])->subs;
        for (usize i = 0; !subs.is_nil() && i < items_of(subs).size(); i++)
            if (!list_push(list_of(k->s[3]), items_of(subs)[i]))
                return oom();
        return sub_step(k, Value());
    }
    default:
        if (!in.is_nil() && py_truth(in)) {
            if (!holds(impl->cache, k->s[1]) && !list_push(list_of(impl->cache), k->s[1]))
                return oom();
            return cont_done(k, value_bool(true));
        }
        if (k->j < items_of(k->s[3]).size()) {
            Value one = items_of(k->s[3])[k->j++];
            return cont_call(k, issubclass_fn(), k->s[1], 2, one);
        }
        impl->negver = counter;
        if (!holds(impl->negative, k->s[1]) && !list_push(list_of(impl->negative), k->s[1]))
            return oom();
        return cont_done(k, value_bool(false));
    }
}

Value sub_check(Value cls, Value subclass)
{
    Root rc{ cls }, rs{ subclass };
    Root impl{ impl_of(rc.v) };
    if (!is_abc(impl.v))
        return err_set("TypeError", "_abc_subclasscheck() argument 1 is not an ABC"), Value();
    if (!is_type(rs.v))
        return err_set2("TypeError", "issubclass() arg 1 must be a class", type_name(rs.v)),
               Value();
    Value kv = cont_new(sub_step);
    if (kv.is_nil())
        return Value();
    cont_of(kv)->s[0] = rc.v;
    cont_of(kv)->s[1] = rs.v;
    cont_of(kv)->s[2] = impl.v;
    return kv;
}

R b_abc_subclasscheck(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_abc_subclasscheck", 2, 2))
        return R::Err;
    out = sub_check(a.args[0], a.args[1]);
    return out.is_nil() ? R::Err : R::Ok;
}

// isinstance is the subclass check over type(x), and again over x.__class__
// where a class has lied about one of them.
R inst_step(ContObj *k, Value in)
{
    if (k->i++ && !in.is_nil() && py_truth(in))
        return cont_done(k, value_bool(true));
    while (k->j < 2) {
        Value t = k->s[k->j == 0 ? 2 : 3];
        k->j++;
        // The declared class is worth a second check only when it differs.
        if (t.is_nil() || (k->j == 2 && t == k->s[2]))
            continue;
        Value c = sub_check(k->s[0], t);
        if (c.is_nil())
            return R::Err;
        // The check is a continuation of its own; this one waits behind it.
        cont_of(c)->next = Value::of_obj(k);
        return cont_done(k, c);
    }
    return cont_done(k, value_bool(false));
}

R b_abc_instancecheck(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_abc_instancecheck", 2, 2))
        return R::Err;
    Root rc{ a.args[0] }, ri{ a.args[1] };
    Root t{ type_of_value(ri.v) };
    if (t.v.is_nil())
        return R::Err;
    // What the object says it is, which need not be what it is.
    Root declared;
    StrObj *n = str_intern("__class__");
    Value got;
    if (n && py_getattr(ri.v, n, got) == R::Ok && is_type(got))
        declared = got;
    else
        err_clear();
    Value kv = cont_new(inst_step);
    if (kv.is_nil())
        return R::Err;
    cont_of(kv)->s[0] = rc.v;
    cont_of(kv)->s[1] = ri.v;
    cont_of(kv)->s[2] = t.v;
    cont_of(kv)->s[3] = declared.v;
    out               = kv;
    return R::Ok;
}

R b_get_cache_token(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_cache_token", 0, 0))
        return R::Err;
    out = Value::of_int(counter);
    return R::Ok;
}

// The four halves of one answer, as abc.py's _dump_registry prints them.
R b_get_dump(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_get_dump", 1, 1))
        return R::Err;
    Value impl = impl_of(a.args[0]);
    if (!is_abc(impl))
        return err_set("TypeError", "_get_dump() argument is not an ABC");
    Root ri{ impl };
    TupleObj *t = tuple_new(4);
    if (!t)
        return oom();
    t->items()[0] = abc_of(ri.v)->registry;
    t->items()[1] = abc_of(ri.v)->cache;
    t->items()[2] = abc_of(ri.v)->negative;
    t->items()[3] = Value::of_int(abc_of(ri.v)->negver);
    out           = obj_value(t);
    return R::Ok;
}

R clear_lists(const CallArgs &a, Str who, bool registry, Value &out)
{
    if (!args_only(a, who, 1, 1))
        return R::Err;
    Value impl = impl_of(a.args[0]);
    if (!is_abc(impl))
        return err_set2("TypeError", "argument is not an ABC", who);
    if (registry)
        items_of(abc_of(impl)->registry).clear();
    else {
        items_of(abc_of(impl)->cache).clear();
        items_of(abc_of(impl)->negative).clear();
    }
    out = value_none();
    return R::Ok;
}

R b_reset_registry(const CallArgs &a, Value &out)
{
    return clear_lists(a, "_reset_registry", true, out);
}

R b_reset_caches(const CallArgs &a, Value &out)
{
    return clear_lists(a, "_reset_caches", false, out);
}

struct Named {
    Str name;
    R (*fn)(const CallArgs &, Value &out);
};

constexpr Named ABC_NAMES[] = {
    { "get_cache_token", b_get_cache_token },      { "_abc_init", b_abc_init },
    { "_abc_register", b_abc_register },           { "_abc_instancecheck", b_abc_instancecheck },
    { "_abc_subclasscheck", b_abc_subclasscheck }, { "_get_dump", b_get_dump },
    { "_reset_registry", b_reset_registry },       { "_reset_caches", b_reset_caches },
};

} // namespace

bool abc_install(DictObj *into, Value issubclass)
{
    Root rd{ obj_value(into) };
    if (!sub_fn) {
        sub_fn = heap_new<Value>();
        if (!sub_fn)
            return false;
        gc_root_hook([] {
            if (sub_fn)
                gc_mark(*sub_fn);
        });
    }
    *sub_fn = issubclass;
    for (const Named &e : ABC_NAMES) {
        Root fn{ native_new(e.name, e.fn) };
        StrObj *n = str_intern(e.name);
        if (fn.v.is_nil() || !n ||
            dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(n), fn.v) != R::Ok)
            return false;
    }
    return true;
}

bool abc_abstract(Value cls, Str &first)
{
    StrObj *n = str_intern("__abstractmethods__");
    Value names;
    if (!n || !is_type(cls) || type_obj(cls)->dict.is_nil())
        return false;
    if (dict_get(static_cast<DictObj *>(type_obj(cls)->dict.obj()), obj_value(n), names) != R::Ok)
        return false;
    if (!is_list(names) || items_of(names).empty())
        return false;
    Value one = items_of(names)[0];
    first     = is_str(one) ? str_of(one)->str() : Str("?");
    return true;
}
