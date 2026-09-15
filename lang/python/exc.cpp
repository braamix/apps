// The exception hierarchy.
#include "exc.h"

#include "func.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "ops.h"
#include "type.h"

namespace {

// The type objects, one per row of EXC_TABLE, made on first use. They outlive
// every program, so they are a root rather than heap the collector may take.
struct Types {
    Vec<Value> made;
};

Types *types;

void types_mark()
{
    if (!types)
        return;
    for (usize i = 0; i < types->made.size(); i++)
        gc_mark(types->made[i]);
}

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// A type is called to make an instance: ValueError('x').
R exc_type_call(const CallArgs &a, Value &out, Value cls)
{
    Root rc{ cls };
    if (a.nkw)
        return err_set2("TypeError", "exception takes no keyword arguments", type_name(cls));
    TupleObj *args = tuple_new(a.nargs);
    if (!args)
        return oom();
    for (u32 i = 0; i < a.nargs; i++)
        args->items()[i] = a.args[i];
    out = exc_inst(rc.v, obj_value(args));
    return out.is_nil() ? R::Err : R::Ok;
}

void exc_trace(Obj *o)
{
    ExcObj *e = static_cast<ExcObj *>(o);
    gc_mark(e->cls);
    gc_mark(e->dict);
    gc_mark(e->args);
    gc_mark(e->cause);
    gc_mark(e->context);
}

TupleObj *args_of(Value v)
{
    return static_cast<TupleObj *>(static_cast<ExcObj *>(v.obj())->args.obj());
}

// repr is `ValueError('x')`; str is what the arguments say, which for one
// argument is that argument and for none is empty.
R exc_repr(Value v, String &out)
{
    TupleObj *a = args_of(v);
    if (!out.append(type_name(v)) || !out.push('('))
        return oom();
    for (usize i = 0; i < a->len; i++) {
        if (i && !out.append(", "))
            return oom();
        if (py_repr(a->items()[i], out) != R::Ok)
            return R::Err;
    }
    return out.push(')') ? R::Ok : oom();
}

R exc_str(Value v, String &out)
{
    TupleObj *a = args_of(v);
    if (a->len == 0)
        return R::Ok;
    if (a->len == 1)
        return py_str(a->items()[0], out);
    return py_repr(static_cast<ExcObj *>(v.obj())->args, out);
}

// The class's own names come first -- py_attr asks this only after those.
R exc_getattr(Value v, StrObj *name, Value &out)
{
    ExcObj *e = static_cast<ExcObj *>(v.obj());
    Str n     = name->str();
    if (n == "args") {
        out = e->args;
        return R::Ok;
    }
    if (n == "__cause__") {
        out = e->cause.is_nil() ? value_none() : e->cause;
        return R::Ok;
    }
    if (n == "__context__") {
        out = e->context.is_nil() ? value_none() : e->context;
        return R::Ok;
    }
    // The two the built-in subclasses carry, which upstream's tests read.
    if (n == "value" && exc_is(e->t, exc_find("StopIteration"))) {
        TupleObj *a = args_of(v);
        out         = a->len ? a->items()[0] : value_none();
        return R::Ok;
    }
    if (n == "errno" && exc_is(e->t, exc_find("OSError"))) {
        TupleObj *a = args_of(v);
        out         = a->len ? a->items()[0] : value_none();
        return R::Ok;
    }
    return R::NotImpl;
}

// BaseException.__init__(self, *args): what a subclass reaches through super().
R b_exc_init(const CallArgs &a, Value &out)
{
    if (a.nkw || !a.nargs || !is_exc(a.args[0]))
        return err_set("TypeError", "BaseException.__init__() needs an exception");
    TupleObj *args = tuple_new(a.nargs - 1);
    if (!args)
        return oom();
    for (u32 i = 1; i < a.nargs; i++)
        args->items()[i - 1] = a.args[i];
    static_cast<ExcObj *>(a.args[0].obj())->args = obj_value(args);
    out                                          = value_none();
    return R::Ok;
}

} // namespace

// The hierarchy, base before derived so a forward reference is never needed.
// The order is the one CPython's own docs list it in.
const ExcType EXC_TABLE[] = {
    { "BaseException", nullptr },
    { "SystemExit", &EXC_TABLE[0] },
    { "KeyboardInterrupt", &EXC_TABLE[0] },
    { "GeneratorExit", &EXC_TABLE[0] },
    { "Exception", &EXC_TABLE[0] },

    { "StopIteration", &EXC_TABLE[4] },
    { "StopAsyncIteration", &EXC_TABLE[4] },
    { "ArithmeticError", &EXC_TABLE[4] },
    { "FloatingPointError", &EXC_TABLE[7] },
    { "OverflowError", &EXC_TABLE[7] },
    { "ZeroDivisionError", &EXC_TABLE[7] },
    { "AssertionError", &EXC_TABLE[4] },
    { "AttributeError", &EXC_TABLE[4] },
    { "EOFError", &EXC_TABLE[4] },
    { "ImportError", &EXC_TABLE[4] },
    { "LookupError", &EXC_TABLE[4] },
    { "IndexError", &EXC_TABLE[15] },
    { "KeyError", &EXC_TABLE[15] },
    { "MemoryError", &EXC_TABLE[4] },
    { "NameError", &EXC_TABLE[4] },
    { "UnboundLocalError", &EXC_TABLE[19] },
    { "OSError", &EXC_TABLE[4] },
    { "RuntimeError", &EXC_TABLE[4] },
    { "NotImplementedError", &EXC_TABLE[22] },
    { "RecursionError", &EXC_TABLE[22] },
    { "SyntaxError", &EXC_TABLE[4] },
    { "IndentationError", &EXC_TABLE[25] },
    { "TabError", &EXC_TABLE[26] },
    { "SystemError", &EXC_TABLE[4] },
    { "TypeError", &EXC_TABLE[4] },
    { "ValueError", &EXC_TABLE[4] },
    { "UnicodeError", &EXC_TABLE[30] },

    // Appended rather than slotted in: the bases above are by index, so a new
    // row in the middle would renumber every one after it.
    { "ModuleNotFoundError", &EXC_TABLE[14] },
    { "UnicodeEncodeError", &EXC_TABLE[31] },
    { "UnicodeDecodeError", &EXC_TABLE[31] },
    { "BufferError", &EXC_TABLE[4] },
};

const usize EXC_COUNT = sizeof(EXC_TABLE) / sizeof(EXC_TABLE[0]);

constexpr Type exc_obj_type{ .name    = "Exception",
                             .trace   = exc_trace,
                             .repr    = exc_repr,
                             .str     = exc_str,
                             .getattr = exc_getattr };

const ExcType *exc_find(Str name)
{
    for (usize i = 0; i < EXC_COUNT; i++)
        if (EXC_TABLE[i].name == name)
            return &EXC_TABLE[i];
    return nullptr;
}

bool exc_is(const ExcType *t, const ExcType *base)
{
    for (; t; t = t->base)
        if (t == base)
            return true;
    return false;
}

Value exc_type_value(const ExcType *t)
{
    if (!t)
        return Value();
    if (!types) {
        types = heap_new<Types>();
        if (!types)
            return oom(), Value();
        gc_root_hook(types_mark);
        if (!types->made.resize(EXC_COUNT))
            return oom(), Value();
    }
    usize i = usize(t - EXC_TABLE);
    if (i >= EXC_COUNT)
        return err_set("SystemError", "an exception type outside the table"), Value();
    if (!types->made[i].is_nil())
        return types->made[i];

    // The base first, so the MRO is built over types that already have one.
    Root base{ t->base ? exc_type_value(t->base) : Value() };
    if (t->base && base.v.is_nil())
        return Value();
    Value o = type_make_native(t->name, base.v, &exc_obj_type, t);
    if (o.is_nil())
        return Value();
    types->made[i] = o;
    return o;
}

Value exc_inst(Value cls, Value args)
{
    Root rc{ cls }, ra{ args };
    if (ra.v.is_nil()) {
        TupleObj *e = tuple_new(0);
        if (!e)
            return oom(), Value();
        ra = obj_value(e);
    }
    ExcObj *o = static_cast<ExcObj *>(type_alloc_inst(rc.v, sizeof(ExcObj)));
    if (!o)
        return Value();
    o->flags |= OBJ_EXC;
    o->t       = type_obj(rc.v)->exc;
    o->args    = ra.v;
    o->cause   = Value();
    o->context = Value();
    return obj_value(o);
}

Value exc_new(const ExcType *t, Value args)
{
    Root ra{ args };
    Value cls = exc_type_value(t);
    return cls.is_nil() ? Value() : exc_inst(cls, ra.v);
}

void exc_slots(Type &s)
{
    s.trace   = exc_trace;
    s.repr    = exc_repr;
    s.str     = exc_str;
    s.getattr = exc_getattr;
}

Value exc_make(Str name, Str message)
{
    const ExcType *t = exc_find(name);
    if (!t)
        t = exc_find("Exception");
    if (message.empty())
        return exc_new(t, Value());

    Root s{ str_new(message) };
    if (s.v.is_nil())
        return Value();
    TupleObj *args = tuple_new(1);
    if (!args)
        return oom(), Value();
    args->items()[0] = s.v;
    return exc_new(t, obj_value(args));
}

bool exc_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    Root base{ exc_type_value(&EXC_TABLE[0]) };
    Root fn{ native_new("__init__", b_exc_init) };
    StrObj *init = str_intern("__init__");
    if (base.v.is_nil() || !init || fn.v.is_nil())
        return false;
    if (dict_set(static_cast<DictObj *>(type_obj(base.v)->dict.obj()), obj_value(init), fn.v) !=
        R::Ok)
        return false;
    for (usize i = 0; i < EXC_COUNT; i++) {
        Value t = exc_type_value(&EXC_TABLE[i]);
        if (t.is_nil())
            return false;
        Root rt{ t };
        StrObj *name = str_intern(EXC_TABLE[i].name);
        if (!name)
            return oom() == R::Ok;
        if (dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(name), rt.v) != R::Ok)
            return false;
    }
    return true;
}

bool exc_line(Value e, String &out)
{
    if (!is_exc(e))
        return py_repr(e, out) == R::Ok;
    if (!out.append(type_name(e)))
        return false;
    String tail;
    if (exc_str(e, tail) != R::Ok)
        return false;
    if (tail.empty())
        return true;
    return out.append(": ") && out.append(tail.str());
}

// The call a type answers, reached from the VM: ValueError('x'), and the same
// for a class deriving from one, whose __init__ the VM runs afterwards.
R exc_type_invoke(Value type, const CallArgs &a, Value &out)
{
    return exc_type_call(a, out, type);
}
