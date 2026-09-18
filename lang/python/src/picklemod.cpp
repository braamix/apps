// `_pickle`: PickleBuffer, which protocol 5 hands out-of-band buffers in.
// There is no pickler written in C++: the rest of what CPython's _pickle has
// -- Pickler, dumps and the errors -- is pickle.py's own, answered by name.
#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "gc.h"
#include "import.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

struct BufObj : Obj {
    Value view; // a memoryview of what it wraps; Nil once released
};

extern const Type picklebuffer_type;

void buf_trace(Obj *o)
{
    gc_mark(static_cast<BufObj *>(o)->view);
}

R buf_repr(Value v, String &out)
{
    char tmp[24];
    Buf<64> b;
    b.put("<pickle.PickleBuffer object at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

BufObj *self_buf(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!s.is_obj() || s.obj()->type != &picklebuffer_type) {
        Buf<96> b;
        b.put(who).put("() requires a PickleBuffer");
        return err_set2("TypeError", b.str(), type_name(s)), nullptr;
    }
    return static_cast<BufObj *>(s.obj());
}

R released()
{
    return err_set("ValueError", "operation forbidden on released PickleBuffer object");
}

// raw(): the octets as a flat memoryview of bytes.
R pb_raw(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "raw");
    if (!b || !meth_args(a, "raw", 0, 0))
        return R::Err;
    if (b->view.is_nil())
        return released();
    out = memview_raw(b->view);
    return out.is_nil() ? R::Err : R::Ok;
}

// release(): the buffer may not be used again.
R pb_release(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "release");
    if (!b || !meth_args(a, "release", 0, 0))
        return R::Err;
    b->view = Value();
    out     = value_none();
    return R::Ok;
}

constexpr Method METHODS[] = {
    { "raw", pb_raw },
    { "release", pb_release },
};

constexpr Type picklebuffer_type{ .name  = "pickle.PickleBuffer",
                                  .trace = buf_trace,
                                  .repr  = buf_repr };

R b_picklebuffer(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs != 1)
        return err_set("TypeError", "PickleBuffer() takes exactly one argument");
    Root view{ memview_new(a.args[0]) };
    if (view.v.is_nil())
        return R::Err;
    BufObj *b = static_cast<BufObj *>(obj_alloc(&picklebuffer_type, sizeof(BufObj)));
    if (!b)
        return err_set("MemoryError", "out of memory");
    b->view = view.v;
    out     = obj_value(b);
    return R::Ok;
}

// What CPython's _pickle has besides PickleBuffer, and pickle.py's name for
// each: the pure-Python one it would have used anyway.
struct Alias {
    Str name, in_pickle;
};

constexpr Alias ALIASES[] = {
    { "PickleError", "PickleError" },
    { "PicklingError", "PicklingError" },
    { "UnpicklingError", "UnpicklingError" },
    { "Pickler", "_Pickler" },
    { "Unpickler", "_Unpickler" },
    { "dump", "_dump" },
    { "dumps", "_dumps" },
    { "load", "_load" },
    { "loads", "_loads" },
};

// s[0] the name pickle.py has it under.
R alias_step(ContObj *k, Value in)
{
    if (k->i == 2)
        return cont_done(k, in);
    if (k->i++ == 0) {
        StrObj *pn = str_intern("pickle");
        Value mod;
        if (!pn)
            return err_set("MemoryError", "out of memory");
        if (dict_get(sys_modules(), obj_value(pn), mod) != R::Ok) {
            StrObj *imp = str_intern("__import__");
            Value fn;
            if (!imp || dict_get(builtins_dict(), obj_value(imp), fn) != R::Ok)
                return err_pending() ? R::Err : err_set("SystemError", "no __import__");
            return cont_call(k, fn, obj_value(pn));
        }
        in = mod;
    }
    k->i = 2;
    return cont_attr(k, in, str_of(k->s[0])->str());
}

R pickle_getattr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__getattr__", 1, 1) || !is_str(a.args[0]))
        return err_pending() ? R::Err : err_set("TypeError", "__getattr__ needs a name");
    Str n = str_of(a.args[0])->str();
    for (const Alias &al : ALIASES)
        if (al.name == n) {
            Root name{ str_new(al.in_pickle) };
            Root kv{ name.v.is_nil() ? Value() : cont_new(alias_step) };
            if (kv.v.is_nil())
                return R::Err;
            cont_of(kv.v)->s[0] = name.v;
            out                 = kv.v;
            return R::Ok;
        }
    Buf<96> b;
    b.put("module '_pickle' has no attribute '").put(n).put('\'');
    return err_set("AttributeError", b.str());
}

constexpr ModDef DEFS[] = { { "__getattr__", pickle_getattr } };

} // namespace

Value picklebuffer_view(Value v)
{
    if (!v.is_obj() || v.obj()->type != &picklebuffer_type)
        return Value();
    Value view = static_cast<BufObj *>(v.obj())->view;
    if (view.is_nil())
        released();
    return view;
}

bool pickle_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&picklebuffer_type, METHODS))
        return false;
    return mod_type(static_cast<DictObj *>(rd.v.obj()), &picklebuffer_type, b_picklebuffer) &&
           mod_defs(static_cast<DictObj *>(rd.v.obj()), DEFS);
}
