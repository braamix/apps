// The dispatch loop.
//
// Plain C++ throughout: it runs until it has something for the driver, and
// says what. A Python call pushes a frame and the loop carries on -- there is
// no C++ recursion for it, and no co_await anywhere near here.
//
// The value stack is never left holding an unrooted temporary: an operand is
// read where it lies, the result computed, and only then is `sp` moved. The
// collector roots what a frame holds, so that discipline is the whole of it.
#include "vm.h"

#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "frame.h"
#include "func.h"
#include "gc.h"
#include "intern.h"
#include "iter.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "ops.h"
#include "proc/io.h"
#include "type.h"

namespace {

constexpr usize FLUSH_AT = 4000; // bytes buffered before a write is asked for
constexpr u32 MAX_FRAMES = 200;  // the frames are heap, but a limit says so

// How many instructions a burst runs before letting the driver back in. A
// compute loop parks nowhere, so nothing else -- a ^C above all -- can reach
// the process until it does.
constexpr u32 BURST = 20000;

struct VM {
    Value frame;              // the innermost FrameObj
    Value globals;            // __main__'s namespace
    Value builtins;           // the builtins namespace
    Value handling;           // the exception an `except` clause is working on
    Vec<Value> flat;          // CallEx's arguments, flattened
    Vec<Value> kwnames;       // and their names
    Vec<Value> bound;         // self, then a bound method's own arguments
    Vec<String> tb;           // the traceback, innermost first, as it unwinds
    String out;               // what print has buffered
    String err;               // what goes to stderr, once there is any
    String *sent   = nullptr; // which of the two the driver is writing
    u32 depth      = 0;
    u32 budget     = 0;
    i32 status     = 0;
    bool failed    = false;
    bool finished  = false;
    bool reported  = false;
    bool interrupt = false; // a ^C the driver saw, to raise at the next step
};

// A String has a destructor, so this lives in a heap block rather than at file
// scope; every namespace-scope global here must be trivially destructible.
VM *vm;

void vm_mark()
{
    if (!vm)
        return;
    gc_mark(vm->frame);
    gc_mark(vm->globals);
    gc_mark(vm->builtins);
    gc_mark(vm->handling);
    for (usize i = 0; i < vm->flat.size(); i++)
        gc_mark(vm->flat[i]);
    for (usize i = 0; i < vm->kwnames.size(); i++)
        gc_mark(vm->kwnames[i]);
    for (usize i = 0; i < vm->bound.size(); i++)
        gc_mark(vm->bound[i]);
}

R oom()
{
    return err_set("MemoryError", "out of memory");
}

DictObj *dict_at(Value v)
{
    return static_cast<DictObj *>(v.obj());
}

bool push(FrameObj *f, Value v)
{
    if (f->nlocals + f->sp >= f->nslots)
        return err_set("SystemError", "value stack overflow"), false;
    f->stack()[f->sp++] = v;
    return true;
}

// ------------------------------------------------------------------- naming

R lookup(FrameObj *f, StrObj *name, Value &out)
{
    if (!f->locals.is_nil()) {
        R r = dict_get(dict_at(f->locals), obj_value(name), out);
        if (r != R::NotImpl)
            return r;
    }
    R r = dict_get(dict_at(f->globals), obj_value(name), out);
    if (r != R::NotImpl)
        return r;
    return dict_get(dict_at(f->builtins), obj_value(name), out);
}

R no_attr(Value v, StrObj *name)
{
    Buf<96> m;
    m.put("'").put(type_name(v)).put("' object has no attribute '").put(name->str()).put("'");
    return err_set("AttributeError", m.str());
}

R name_error(Str kind, StrObj *name)
{
    Buf<96> b;
    b.put("name '").put(name->str()).put("' is not defined");
    return err_set(kind, b.str());
}

R unbound(Str kind, Str what, Value name)
{
    Buf<96> b;
    b.put(what).put(" '").put(is_str(name) ? str_of(name)->str() : Str("?"));
    b.put("' referenced before assignment");
    return err_set(kind, b.str());
}

// A cell slot is a cellvar or, past those, a freevar.
R unbound_cell(CodeObj *co, u32 slot)
{
    usize own = co->cellvars.size();
    return slot < own ? unbound("NameError", "local variable", co->cellvars[slot])
                      : unbound("NameError", "free variable", co->freevars[slot - own]);
}

// --------------------------------------------------------------- the frames

FrameObj *frame_push(CodeObj *co, Value globals, Value locals, Value cells)
{
    if (vm->depth >= MAX_FRAMES)
        return err_set("RecursionError", "maximum recursion depth exceeded"), nullptr;
    Root rg{ globals }, rl{ locals }, rc{ cells };
    FrameObj *f = frame_new(co);
    if (!f)
        return nullptr;
    f->globals  = rg.v;
    f->locals   = rl.v;
    f->cells    = rc.v;
    f->builtins = vm->builtins;
    f->back     = vm->frame;
    f->handling = vm->handling;
    vm->frame   = obj_value(f);
    vm->depth++;
    return f;
}

// The cells a call needs: one fresh per cellvar, then the closure's.
Value make_cells(CodeObj *co, Value closure)
{
    usize n = co->cellvars.size() + co->freevars.size();
    if (!n)
        return Value();
    Root rc{ closure };
    TupleObj *t = tuple_new(n);
    if (!t)
        return oom(), Value();
    Root rt{ obj_value(t) };
    for (usize i = 0; i < co->cellvars.size(); i++) {
        CellObj *c = cell_new();
        if (!c)
            return Value();
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = obj_value(c);
    }
    for (usize i = 0; i < co->freevars.size(); i++) {
        TupleObj *cl = static_cast<TupleObj *>(rc.v.obj());
        if (!cl || i >= cl->len)
            return err_set("SystemError", "closure does not match the code object"), Value();
        static_cast<TupleObj *>(rt.v.obj())->items()[co->cellvars.size() + i] = cl->items()[i];
    }
    return rt.v;
}

// ------------------------------------------------------------------- calling

// What to call a callable in a diagnostic.
Str call_name(Value v)
{
    if (is_func(v)) {
        Value n = code_of(func_of(v)->code)->name;
        return is_str(n) ? str_of(n)->str() : Str("?");
    }
    if (is_native(v))
        return static_cast<NativeObj *>(v.obj())->name;
    return type_name(v);
}

R do_call(Value callable, const CallArgs &a, Value &out, bool &entered);

// `__init__` has returned; the answer is the instance it was given.
R init_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return err_set("SystemError", "an init continuation was not started");
    if (!is_none(in))
        return err_set2("TypeError", "__init__() should return None", type_name(in));
    return cont_done(k, k->s[0]);
}

// A class that wrote its own __new__: call it, then __init__ on what it made
// if that is an instance of the class. s[0] is the class, s[1] the arguments
// __new__ takes and s[2] the ones __init__ does.
R new_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        return cont_call_v(k, k->s[3], k->s[1]);
    case 1: {
        if (!type_isinstance(in, k->s[0]))
            return cont_done(k, in);
        StrObj *n = str_intern("__init__");
        Value init;
        if (!n || type_lookup(k->s[0], n, init) != R::Ok)
            return cont_done(k, in);
        k->s[4]       = in;
        TupleObj *t   = static_cast<TupleObj *>(k->s[2].obj());
        t->items()[0] = in;
        return cont_call_v(k, init, k->s[2]);
    }
    default:
        if (!is_none(in))
            return err_set2("TypeError", "__init__() should return None", type_name(in));
        return cont_done(k, k->s[4]);
    }
}

// The instance a class deriving from an exception starts life as.
Value exc_type_invoke_new(Value cls, const CallArgs &a)
{
    Root rc{ cls };
    TupleObj *args = tuple_new(a.nargs);
    if (!args)
        return oom(), Value();
    for (u32 i = 0; i < a.nargs; i++)
        args->items()[i] = a.args[i];
    return exc_inst(rc.v, obj_value(args));
}

// Making an instance: __new__ where a built-in is involved, then __init__.
R type_call(Value cls, const CallArgs &a, Value &out, bool &entered)
{
    Root rc{ cls }, ctor, init, owner;
    StrObj *nw = str_intern("__new__");
    StrObj *in = str_intern("__init__");
    if (!nw || !in)
        return oom();

    if (type_lookup(rc.v, nw, ctor.v) == R::Err)
        return R::Err;
    if (type_obj(rc.v)->exc && !type_obj(rc.v)->heap)
        return exc_type_invoke(rc.v, a, out);
    if (!type_obj(rc.v)->heap) {
        if (ctor.v.is_nil())
            return err_set2("TypeError", "this type cannot be instantiated",
                            type_obj(rc.v)->slots.name);
        return do_call(ctor.v, a, out, entered);
    }

    R r = type_lookup(rc.v, in, init.v);
    if (r == R::Err)
        return R::Err;

    // A class that wrote __new__ decides what it gets, and gets `cls` first.
    Root own{ type_own_new(rc.v) };
    if (!own.v.is_nil()) {
        TupleObj *na = tuple_new(a.nargs + 1);
        if (!na)
            return oom();
        na->items()[0] = rc.v;
        for (u32 i = 0; i < a.nargs; i++)
            na->items()[i + 1] = a.args[i];
        Root rna{ obj_value(na) };
        TupleObj *ia = tuple_new(a.nargs + 1);
        if (!ia)
            return oom();
        for (u32 i = 0; i < a.nargs; i++)
            ia->items()[i + 1] = a.args[i];
        Root ria{ obj_value(ia) };
        Root kv{ cont_new(new_step) };
        if (kv.v.is_nil())
            return R::Err;
        ContObj *k = cont_of(kv.v);
        k->s[0]    = rc.v;
        k->s[1]    = rna.v;
        k->s[2]    = ria.v;
        k->s[3]    = own.v;
        out        = kv.v;
        return R::Ok;
    }

    // A class deriving from an exception is one: BaseException.__new__ keeps
    // the arguments, and __init__ runs over the instance as usual.
    Root self{ type_obj(rc.v)->exc ? exc_type_invoke_new(rc.v, a) : inst_new(rc.v) };
    if (self.v.is_nil())
        return R::Err;
    // A subclass of a built-in keeps one of those inside it. Its own __init__
    // takes the arguments where it has one, so the built-in is made empty.
    if (!type_obj(rc.v)->native.is_nil()) {
        Root base;
        if (type_lookup(type_obj(rc.v)->native, nw, base.v) == R::Err)
            return R::Err;
        if (!is_native(base.v))
            return err_set("TypeError", "this built-in cannot be subclassed");
        CallArgs none;
        Value made;
        bool e         = false;
        bool give_args = r != R::Ok || type_native_takes_args(rc.v);
        if (do_call(base.v, give_args ? a : none, made, e) != R::Ok)
            return R::Err;
        inst_of(self.v)->native = made;
    }
    if (r == R::NotImpl) {
        out = self.v;
        return R::Ok;
    }

    // self before the call's own arguments, which is what a method call is.
    vm->bound.clear();
    if (!vm->bound.push(self.v))
        return oom();
    for (u32 i = 0; i < a.nargs; i++)
        if (!vm->bound.push(a.args[i]))
            return oom();
    CallArgs b;
    b.args    = vm->bound.data();
    b.nargs   = a.nargs + 1;
    b.kwvals  = a.kwvals;
    b.kwnames = a.kwnames;
    b.nkw     = a.nkw;

    Value got;
    bool e = false;
    if (do_call(init.v, b, got, e) != R::Ok)
        return R::Err;
    if (!e) {
        if (!is_none(got))
            return err_set2("TypeError", "__init__() should return None", type_name(got));
        out = self.v;
        return R::Ok;
    }
    Root kv{ cont_new(init_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0]       = self.v;
    cont_of(kv.v)->i          = 1;
    frame_of(vm->frame)->cont = kv.v;
    entered                   = true;
    return R::Ok;
}

// A call, whatever it lands on. `entered` says a Python frame was pushed and
// the loop must not store a result; otherwise `out` is the answer -- or a
// ContObj, which is a builtin saying it needs Python run before it can finish.
R do_call(Value callable, const CallArgs &a, Value &out, bool &entered)
{
    entered = false;
    if (is_native(callable))
        return static_cast<NativeObj *>(callable.obj())->fn(a, out);
    if (is_type(callable))
        return type_call(callable, a, out, entered);

    if (is_method(callable)) {
        MethodObj *m = static_cast<MethodObj *>(callable.obj());
        Root rf{ m->fn }, rs{ m->self };
        vm->bound.clear();
        if (!vm->bound.push(rs.v))
            return oom();
        for (u32 i = 0; i < a.nargs; i++)
            if (!vm->bound.push(a.args[i]))
                return oom();
        CallArgs b;
        b.args    = vm->bound.data();
        b.nargs   = a.nargs + 1;
        b.kwvals  = a.kwvals;
        b.kwnames = a.kwnames;
        b.nkw     = a.nkw;
        return do_call(rf.v, b, out, entered);
    }

    // An instance is callable when its class says __call__.
    if (is_inst(callable)) {
        StrObj *cl = str_intern("__call__");
        Root fn;
        if (!cl)
            return oom();
        if (type_lookup(inst_of(callable)->cls, cl, fn.v) == R::Ok) {
            Root bound{ method_new(fn.v, callable) };
            if (bound.v.is_nil())
                return R::Err;
            return do_call(bound.v, a, out, entered);
        }
    }

    if (!is_func(callable))
        return err_set2("TypeError", "object is not callable", type_name(callable));

    FuncObj *fn = func_of(callable);
    CodeObj *co = code_of(fn->code);
    Root cells{ make_cells(co, fn->closure) };
    if (err_pending())
        return R::Err;

    FrameObj *nf = frame_push(co, fn->globals, Value(), cells.v);
    if (!nf)
        return R::Err;
    R r = bind_args(fn, co, nf, a);
    if (r != R::Ok) {
        vm->handling = nf->handling;
        vm->frame    = nf->back;
        vm->depth--;
        return R::Err;
    }
    entered = true;
    return R::Ok;
}

// Ground rule 2, made concrete: run a suspended builtin's continuation until it
// stops asking for Python. A request that lands on another builtin is answered
// on the spot; one that lands on a Python function pushes a frame and leaves,
// with the continuation recorded on it -- Return brings us back here. Nothing
// re-enters the dispatch loop, so the native stack does not grow.
bool run_cont(Value kv, Value in)
{
    Root rk{ kv }, ri{ in };
    for (;;) {
        ContObj *k = cont_of(rk.v);
        k->fn      = Value();
        if (k->step(k, ri.v) != R::Ok)
            return false;

        if (k->fn.is_nil()) {
            if (k->next.is_nil())
                return k->drop ? true : push(frame_of(vm->frame), k->out);
            ri = k->out;
            rk = k->next;
            continue;
        }

        CallArgs a;
        a.args  = k->a;
        a.nargs = k->nargs;
        if (!k->argv.is_nil()) {
            a.args  = static_cast<TupleObj *>(k->argv.obj())->items();
            a.nargs = u32(static_cast<TupleObj *>(k->argv.obj())->len);
        }
        Value out;
        bool entered = false;
        if (do_call(k->fn, a, out, entered) != R::Ok)
            return false;
        if (entered) {
            FrameObj *nf = frame_of(vm->frame);
            nf->cont     = rk.v;
            if (!cont_of(rk.v)->locals.is_nil())
                nf->locals = cont_of(rk.v)->locals;
            return true;
        }
        if (is_cont(out)) { // a builtin that suspends in its turn
            cont_of(out)->next = rk.v;
            rk                 = out;
            ri                 = Value();
            continue;
        }
        ri = out;
    }
}

// What a call left behind: an entered frame pushes its own answer, a suspended
// builtin is driven here, and anything else is the value itself.
bool land(Value out, bool entered)
{
    if (entered)
        return true;
    if (is_cont(out))
        return run_cont(out, Value());
    return push(frame_of(vm->frame), out);
}

// ------------------------------------------------------- the special methods

// The dunder a class writes for each operator, and the reflected one.
struct Dunder {
    Str name, refl;
};

Dunder op_dunder(Op op)
{
    switch (op) {
    case Op::Add:
        return { "__add__", "__radd__" };
    case Op::Sub:
        return { "__sub__", "__rsub__" };
    case Op::Mul:
        return { "__mul__", "__rmul__" };
    case Op::Div:
        return { "__truediv__", "__rtruediv__" };
    case Op::FloorDiv:
        return { "__floordiv__", "__rfloordiv__" };
    case Op::Mod:
        return { "__mod__", "__rmod__" };
    case Op::Pow:
        return { "__pow__", "__rpow__" };
    case Op::And:
        return { "__and__", "__rand__" };
    case Op::Or:
        return { "__or__", "__ror__" };
    case Op::Xor:
        return { "__xor__", "__rxor__" };
    case Op::Lsh:
        return { "__lshift__", "__rlshift__" };
    case Op::Rsh:
        return { "__rshift__", "__rrshift__" };
    }
    return { "?", "?" };
}

Str inplace_dunder(Op op)
{
    switch (op) {
    case Op::Add:
        return "__iadd__";
    case Op::Sub:
        return "__isub__";
    case Op::Mul:
        return "__imul__";
    case Op::Div:
        return "__itruediv__";
    case Op::FloorDiv:
        return "__ifloordiv__";
    case Op::Mod:
        return "__imod__";
    case Op::Pow:
        return "__ipow__";
    case Op::And:
        return "__iand__";
    case Op::Or:
        return "__ior__";
    case Op::Xor:
        return "__ixor__";
    case Op::Lsh:
        return "__ilshift__";
    case Op::Rsh:
        return "__irshift__";
    }
    return "?";
}

Dunder cmp_dunder(Cmp op)
{
    switch (op) {
    case Cmp::Eq:
        return { "__eq__", "__eq__" };
    case Cmp::Ne:
        return { "__ne__", "__ne__" };
    case Cmp::Lt:
        return { "__lt__", "__gt__" };
    case Cmp::Le:
        return { "__le__", "__ge__" };
    case Cmp::Gt:
        return { "__gt__", "__lt__" };
    case Cmp::Ge:
        return { "__ge__", "__le__" };
    default:
        break;
    }
    return { "?", "?" };
}

// What to do with a special method's answer.
enum : u32 { SP_KEEP = 0, SP_DROP = 1, SP_BOOL = 2, SP_NOT = 4 };

// One call, and then whatever `j` says about the answer.
R once_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->s[1], k->nargs, k->s[2]);
    if (k->j & SP_BOOL)
        in = value_bool(py_truth(in) != ((k->j & SP_NOT) != 0));
    return cont_done(k, in);
}

// One turn of a `for` over a class instance: the item is pushed, and the
// StopIteration this continuation catches pops the iterator and jumps.
// `drop` is set, so the push and the jump are both ours.
R next_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    FrameObj *f = frame_of(vm->frame);
    if (in.is_nil()) {
        f->sp--;
        f->pc = k->j;
        return cont_done(k, value_none());
    }
    return push(f, in) ? cont_done(k, value_none()) : R::Err;
}

// Run a bound special method: pop `pop` values off the frame, then the answer
// lands where a call's would, unless `what` says otherwise.
bool run_special(FrameObj *f, Value m, const Value *args, u32 n, u32 pop, u32 what = SP_KEEP)
{
    // cont_new allocates, and nothing else points at `m` yet.
    Root rm{ m };
    Root kv{ cont_new(once_step) };
    if (kv.v.is_nil())
        return false;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rm.v;
    if (n > 0)
        k->s[1] = args[0];
    if (n > 1)
        k->s[2] = args[1];
    k->nargs = n;
    k->j     = what;
    k->drop  = (what & SP_DROP) != 0;
    f->sp -= pop;
    return run_cont(kv.v, Value());
}

// s[0] the bound method and s[1] its argument, s[2]/s[3] the reflected pair;
// j is the operator, with bit 8 set for a comparison.
R dunder_step(ContObj *k, Value in)
{
    u32 phase = k->i++;
    if (phase == 0)
        return cont_call(k, k->s[0], k->s[1]);
    if (!is_notimpl(in))
        return cont_done(k, in);
    if (phase == 1 && !k->s[2].is_nil())
        return cont_call(k, k->s[2], k->s[3]);

    bool compare = (k->j & 0x100) != 0;
    Value a = k->s[4], b = k->s[5];
    if (compare) {
        Cmp op = Cmp(k->j & 0xff);
        if (op == Cmp::Eq || op == Cmp::Ne)
            return cont_done(k, value_bool((a == b) == (op == Cmp::Eq)));
        Buf<96> m;
        m.put("'").put(cmp_symbol(op)).put("' not supported between instances of '");
        m.put(type_name(a)).put("' and '").put(type_name(b)).put("'");
        return err_set("TypeError", m.str());
    }
    Buf<96> m;
    m.put("unsupported operand type(s) for ").put(op_symbol(Op(k->j & 0xff)));
    m.put(": '").put(type_name(a)).put("' and '").put(type_name(b)).put("'");
    return err_set("TypeError", m.str());
}

// A binary operator where one side is a class instance. False on failure;
// `done` is false when neither side had anything and the ordinary path stands.
bool dunder_binop(FrameObj *f, Value a, Value b, u32 tag, Dunder d, bool &done)
{
    Root left{ type_special(a, d.name) };
    Root right;
    // The reflected call only where the other side is a different class.
    if (!(is_inst(a) && is_inst(b) && inst_of(a)->cls == inst_of(b)->cls))
        right = type_special(b, d.refl);
    done = !left.v.is_nil() || !right.v.is_nil();
    if (!done)
        return true;

    Root kv{ cont_new(dunder_step) };
    if (kv.v.is_nil())
        return false;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = left.v;
    k->s[1]    = b;
    k->s[2]    = right.v;
    k->s[3]    = a;
    k->s[4]    = a;
    k->s[5]    = b;
    k->j       = tag;
    // Nothing on this side: start at the reflected call.
    if (left.v.is_nil()) {
        k->s[0] = right.v;
        k->s[1] = a;
        k->s[2] = Value();
        k->s[3] = b;
    }
    f->sp -= 2;
    return run_cont(kv.v, Value());
}

// ---------------------------------------------------------------- unwinding

// `except T` catches an instance of T or of anything under it -- a user class
// deriving from an exception included, since its MRO holds the built-in one.
bool exc_matches(Value e, Value want)
{
    if (!is_exc(e))
        return false;
    Value t = type_of_value(e);
    return !t.is_nil() && type_issub(t, want);
}

// The pending error as an object. Most errors are set as a kind and a message
// -- err_set("TypeError", ...) -- and only become an object here, where an
// `except` might want one.
Value pending_exception()
{
    Value v = err_value();
    if (!v.is_nil())
        return v;
    v = exc_make(err_kind(), err_message());
    if (v.is_nil())
        v = exc_type_value(exc_find("MemoryError"));
    return v;
}

// One line of the traceback, recorded as a frame is left behind.
bool note_frame(FrameObj *f)
{
    CodeObj *c = code_of(f->code);
    char tmp[24];
    Buf<192> b;
    b.put("  File \"").put(is_str(c->filename) ? str_of(c->filename)->str() : Str("?"));
    b.put("\", line ");
    b.put(int_text(tmp, sizeof tmp, i64(code_line(c, f->pc ? f->pc - 1 : 0))));
    b.put(", in ").put(is_str(c->name) ? str_of(c->name)->str() : Str("?")).put('\n');
    String line;
    return line.append(b.str()) && vm->tb.push(static_cast<String &&>(line));
}

// Print an exception the way CPython does, its cause or context first. The
// depth is a bound: a context chain can be made to loop.
void report(Value e, u32 depth = 0)
{
    if (is_exc(e) && depth < 8) {
        Value under = static_cast<ExcObj *>(e.obj())->cause;
        Str joiner  = "\nThe above exception was the direct cause of the following exception:\n\n";
        if (under.is_nil()) {
            under  = static_cast<ExcObj *>(e.obj())->context;
            joiner = "\nDuring handling of the above exception, another exception occurred:\n\n";
        }
        if (!under.is_nil() && under != e) {
            report(under, depth + 1);
            vm->err.append(joiner);
        }
    }
    if (vm->tb.size())
        vm->err.append("Traceback (most recent call last):\n");
    for (usize k = vm->tb.size(); k > 0; k--)
        vm->err.append(vm->tb[k - 1].str());
    vm->tb.clear();
    exc_line(e, vm->err);
    vm->err.push('\n');
}

// Nothing caught it. SystemExit is the one that is not an error.
void uncaught(Value e)
{
    vm->finished = true;
    if (is_exc(e) && exc_is(exc_type_of(e), exc_find("SystemExit"))) {
        TupleObj *a = static_cast<TupleObj *>(static_cast<ExcObj *>(e.obj())->args.obj());
        i64 code    = 0;
        if (a->len && !is_none(a->items()[0])) {
            if (as_index(a->items()[0], code)) {
                vm->status = i32(code);
            } else {
                py_str(a->items()[0], vm->err);
                vm->err.push('\n');
                vm->status = 1;
            }
        }
        vm->tb.clear();
        return;
    }
    vm->failed = true;
    vm->status = 1;
    report(e);
}

// Find the handler that wants `e`, unwinding frames until one does. False
// when nothing did, and the program is over.
// A suspended builtin that said it would catch this. `StopIteration` out of a
// __next__ is the reason the mechanism exists.
bool cont_catches(Value kv, Value e)
{
    if (kv.is_nil() || !is_exc(e))
        return false;
    u32 c = cont_of(kv)->catching;
    if (c == CATCH_NONE)
        return false;
    return exc_is(exc_type_of(e), exc_find(c == CATCH_STOP ? "StopIteration" : "AttributeError"));
}

bool dispatch(Value e)
{
    Root re{ e };
    err_clear();
    for (;;) {
        FrameObj *f = frame_of(vm->frame);
        if (f->nb) {
            Block b = f->blocks()[--f->nb];
            f->sp   = b.sp;
            f->pc   = b.handler;
            return push(f, re.v);
        }
        if (!note_frame(f))
            return uncaught(re.v), false;
        if (f->back.is_nil()) {
            uncaught(re.v);
            return false;
        }
        Value kv     = f->cont;
        vm->handling = f->handling;
        vm->frame    = f->back;
        vm->depth--;
        if (cont_catches(kv, re.v)) {
            vm->tb.clear();
            return run_cont(kv, Value());
        }
    }
}

// Start an exception on its way. `e` may be a type, which is instantiated.
bool raise_value(Value e)
{
    Root re{ e };
    if (is_exc_type(re.v)) {
        Value made = exc_inst(re.v, Value());
        if (made.is_nil())
            return dispatch(pending_exception());
        re = made;
    }
    if (!is_exc(re.v)) {
        err_set2("TypeError", "exceptions must derive from BaseException", type_name(re.v));
        return dispatch(pending_exception());
    }
    // Raised while handling another: CPython remembers what that was.
    ExcObj *o = static_cast<ExcObj *>(re.v.obj());
    if (o->context.is_nil() && !vm->handling.is_nil() && vm->handling != re.v)
        o->context = vm->handling;
    vm->tb.clear();
    return dispatch(re.v);
}

// -------------------------------------------------------------- the loop

void interpret()
{
    vm->budget = BURST;
    for (;;) {
        if (vm->finished || vm->out.size() >= FLUSH_AT || !vm->budget--)
            return;

        // A ^C the driver noticed between bursts, delivered here, which is the
        // only place the stack is in a state an exception can unwind from.
        if (vm->interrupt) {
            vm->interrupt = false;
            err_set("KeyboardInterrupt", "");
            vm->tb.clear();
            if (!raise_value(pending_exception()))
                return;
            continue;
        }

        FrameObj *f = frame_of(vm->frame);
        CodeObj *co = code_of(f->code);
        if (f->pc >= co->code.size()) {
            err_set("SystemError", "ran off the end of a code object");
            goto oops;
        }

        {
            Instr in  = co->code[f->pc++];
            Value *st = f->stack();
            u32 arg   = in.arg;

            switch (in.op) {
            case Bc::Nop:
                break;

            case Bc::PopTop:
                f->sp--;
                break;
            case Bc::DupTop:
                if (!push(f, st[f->sp - 1]))
                    goto oops;
                break;
            case Bc::DupTop2:
                if (!push(f, st[f->sp - 2]) || !push(f, st[f->sp - 2]))
                    goto oops;
                break;
            case Bc::RotTwo: {
                Value t       = st[f->sp - 1];
                st[f->sp - 1] = st[f->sp - 2];
                st[f->sp - 2] = t;
                break;
            }
            case Bc::RotThree: {
                Value t       = st[f->sp - 1];
                st[f->sp - 1] = st[f->sp - 2];
                st[f->sp - 2] = st[f->sp - 3];
                st[f->sp - 3] = t;
                break;
            }
            case Bc::RotFour: {
                Value t       = st[f->sp - 1];
                st[f->sp - 1] = st[f->sp - 2];
                st[f->sp - 2] = st[f->sp - 3];
                st[f->sp - 3] = st[f->sp - 4];
                st[f->sp - 4] = t;
                break;
            }

            case Bc::LoadConst:
                if (!push(f, co->consts[arg]))
                    goto oops;
                break;

            case Bc::LoadName: {
                StrObj *n = str_of(co->names[arg]);
                Value out;
                R r = lookup(f, n, out);
                if (r == R::Err)
                    goto oops;
                if (r == R::NotImpl) {
                    name_error("NameError", n);
                    goto oops;
                }
                if (!push(f, out))
                    goto oops;
                break;
            }
            case Bc::StoreName:
                if (dict_set(dict_at(f->locals), co->names[arg], st[f->sp - 1]) != R::Ok)
                    goto oops;
                f->sp--;
                break;
            case Bc::DeleteName: {
                R r = dict_del(dict_at(f->locals), co->names[arg]);
                if (r == R::Err)
                    goto oops;
                if (r == R::NotImpl) {
                    name_error("NameError", str_of(co->names[arg]));
                    goto oops;
                }
                break;
            }

            case Bc::LoadFast:
                if (f->slots()[arg].is_nil()) {
                    unbound("UnboundLocalError", "local variable", co->varnames[arg]);
                    goto oops;
                }
                if (!push(f, f->slots()[arg]))
                    goto oops;
                break;
            case Bc::StoreFast:
                f->slots()[arg] = st[f->sp - 1];
                f->sp--;
                break;
            case Bc::DeleteFast:
                if (f->slots()[arg].is_nil()) {
                    unbound("UnboundLocalError", "local variable", co->varnames[arg]);
                    goto oops;
                }
                f->slots()[arg] = Value();
                break;

            case Bc::LoadGlobal: {
                Value out;
                R r = dict_get(dict_at(f->globals), co->names[arg], out);
                if (r == R::NotImpl)
                    r = dict_get(dict_at(f->builtins), co->names[arg], out);
                if (r == R::Err)
                    goto oops;
                if (r == R::NotImpl) {
                    name_error("NameError", str_of(co->names[arg]));
                    goto oops;
                }
                if (!push(f, out))
                    goto oops;
                break;
            }
            case Bc::StoreGlobal:
                if (dict_set(dict_at(f->globals), co->names[arg], st[f->sp - 1]) != R::Ok)
                    goto oops;
                f->sp--;
                break;
            case Bc::DeleteGlobal: {
                R r = dict_del(dict_at(f->globals), co->names[arg]);
                if (r == R::Err)
                    goto oops;
                if (r == R::NotImpl) {
                    name_error("NameError", str_of(co->names[arg]));
                    goto oops;
                }
                break;
            }

            case Bc::LoadDeref: {
                CellObj *c = static_cast<CellObj *>(
                    static_cast<TupleObj *>(f->cells.obj())->items()[arg].obj());
                if (c->v.is_nil()) {
                    unbound_cell(co, arg);
                    goto oops;
                }
                if (!push(f, c->v))
                    goto oops;
                break;
            }
            case Bc::StoreDeref: {
                CellObj *c = static_cast<CellObj *>(
                    static_cast<TupleObj *>(f->cells.obj())->items()[arg].obj());
                c->v = st[f->sp - 1];
                f->sp--;
                break;
            }
            case Bc::DeleteDeref: {
                CellObj *c = static_cast<CellObj *>(
                    static_cast<TupleObj *>(f->cells.obj())->items()[arg].obj());
                if (c->v.is_nil()) {
                    unbound_cell(co, arg);
                    goto oops;
                }
                c->v = Value();
                break;
            }
            case Bc::LoadClosure:
                if (!push(f, static_cast<TupleObj *>(f->cells.obj())->items()[arg]))
                    goto oops;
                break;

            case Bc::LoadAttr: {
                StrObj *name = str_of(co->names[arg]);
                Value got;
                Got g = py_attr(st[f->sp - 1], name, got);
                if (g == Got::Error)
                    goto oops;
                if (g == Got::Missing) {
                    // A __getattr__ is the last word; without one it is an error.
                    if (got.is_nil()) {
                        no_attr(st[f->sp - 1], name);
                        goto oops;
                    }
                    Value key = obj_value(name);
                    CallArgs a;
                    a.args       = &key;
                    a.nargs      = 1;
                    bool entered = false;
                    f->sp--;
                    if (do_call(got, a, got, entered) != R::Ok || !land(got, entered))
                        goto oops;
                    break;
                }
                f->sp--;
                if (g == Got::Ok) {
                    if (!push(f, got))
                        goto oops;
                    break;
                }
                // A property: its getter is Python, so the VM runs it.
                CallArgs a;
                bool entered = false;
                if (do_call(got, a, got, entered) != R::Ok || !land(got, entered))
                    goto oops;
                break;
            }
            case Bc::StoreAttr: {
                StrObj *name = str_of(co->names[arg]);
                Value m      = type_property(st[f->sp - 1], name, 1);
                if (!m.is_nil()) {
                    if (!run_special(f, m, &st[f->sp - 2], 1, 2, SP_DROP))
                        goto oops;
                    break;
                }
                if (inst_setattr(st[f->sp - 1], name, st[f->sp - 2]) != R::Ok)
                    goto oops;
                f->sp -= 2;
                break;
            }
            case Bc::DeleteAttr: {
                StrObj *name = str_of(co->names[arg]);
                Value m      = type_property(st[f->sp - 1], name, 2);
                if (!m.is_nil()) {
                    if (!run_special(f, m, nullptr, 0, 1, SP_DROP))
                        goto oops;
                    break;
                }
                if (inst_delattr(st[f->sp - 1], name) != R::Ok)
                    goto oops;
                f->sp--;
                break;
            }

            case Bc::LoadSubscr: {
                Value m = type_special(st[f->sp - 2], "__getitem__");
                if (!m.is_nil()) {
                    if (!run_special(f, m, &st[f->sp - 1], 1, 2))
                        goto oops;
                    break;
                }
                Value out;
                if (py_getitem(st[f->sp - 2], st[f->sp - 1], out) != R::Ok)
                    goto oops;
                f->sp -= 2;
                st[f->sp++] = out;
                break;
            }
            case Bc::StoreSubscr: {
                Value m = type_special(st[f->sp - 2], "__setitem__");
                if (!m.is_nil()) {
                    Value av[2] = { st[f->sp - 1], st[f->sp - 3] };
                    if (!run_special(f, m, av, 2, 3, SP_DROP))
                        goto oops;
                    break;
                }
                if (py_setitem(st[f->sp - 2], st[f->sp - 1], st[f->sp - 3]) != R::Ok)
                    goto oops;
                f->sp -= 3;
                break;
            }
            case Bc::DeleteSubscr: {
                Value m = type_special(st[f->sp - 2], "__delitem__");
                if (!m.is_nil()) {
                    if (!run_special(f, m, &st[f->sp - 1], 1, 2, SP_DROP))
                        goto oops;
                    break;
                }
                if (py_delitem(st[f->sp - 2], st[f->sp - 1]) != R::Ok)
                    goto oops;
                f->sp -= 2;
                break;
            }

            case Bc::UnaryOp: {
                Value out;
                Value a = st[f->sp - 1];
                if (is_inst(a) && Un(arg) != Un::Not) {
                    Str d   = Un(arg) == Un::Invert ? Str("__invert__")
                              : Un(arg) == Un::UAdd ? Str("__pos__")
                                                    : Str("__neg__");
                    Value m = type_special(a, d);
                    if (!m.is_nil()) {
                        if (!run_special(f, m, nullptr, 0, 1))
                            goto oops;
                        break;
                    }
                }
                R r = R::Ok;
                switch (Un(arg)) {
                case Un::Invert:
                    r = py_invert(a, out);
                    break;
                case Un::Not:
                    out = value_bool(!py_truth(a));
                    break;
                case Un::UAdd:
                    r = py_pos(a, out);
                    break;
                case Un::USub:
                    r = py_neg(a, out);
                    break;
                }
                if (r != R::Ok)
                    goto oops;
                st[f->sp - 1] = out;
                break;
            }

            case Bc::BinaryOp:
            case Bc::InplaceOp: {
                Value a = st[f->sp - 2], b = st[f->sp - 1];
                if (is_inst(a) || is_inst(b)) {
                    // `a += b` asks for __iadd__ first and falls back to __add__.
                    if (in.op == Bc::InplaceOp) {
                        Value m = type_special(a, inplace_dunder(Op(arg)));
                        if (!m.is_nil()) {
                            if (!run_special(f, m, &b, 1, 2))
                                goto oops;
                            break;
                        }
                    }
                    bool done = false;
                    if (!dunder_binop(f, a, b, arg, op_dunder(Op(arg)), done))
                        goto oops;
                    if (done)
                        break;
                }
                Value out;
                R r = in.op == Bc::BinaryOp ? py_binop(a, b, Op(arg), out)
                                            : py_inplace(a, b, Op(arg), out);
                if (r != R::Ok)
                    goto oops;
                f->sp -= 2;
                st[f->sp++] = out;
                break;
            }

            case Bc::CompareOp: {
                Cmp op  = Cmp(arg);
                bool ok = false;
                if (op == Cmp::Is || op == Cmp::IsNot) {
                    ok = (st[f->sp - 2] == st[f->sp - 1]) == (op == Cmp::Is);
                } else if (op == Cmp::In || op == Cmp::NotIn) {
                    // `not in` on a class needs the answer negated, which the
                    // ordinary path does and a call cannot.
                    Value m = type_special(st[f->sp - 1], "__contains__");
                    if (!m.is_nil()) {
                        u32 w = SP_BOOL | (op == Cmp::NotIn ? SP_NOT : 0);
                        if (!run_special(f, m, &st[f->sp - 2], 1, 2, w))
                            goto oops;
                        break;
                    }
                    if (py_contains(st[f->sp - 1], st[f->sp - 2], ok) != R::Ok)
                        goto oops;
                    if (op == Cmp::NotIn)
                        ok = !ok;
                } else if (is_inst(st[f->sp - 2]) || is_inst(st[f->sp - 1])) {
                    bool done = false;
                    if (!dunder_binop(f, st[f->sp - 2], st[f->sp - 1], arg | 0x100, cmp_dunder(op),
                                      done))
                        goto oops;
                    if (done)
                        break;
                    if (py_cmp(st[f->sp - 2], st[f->sp - 1], op, ok) != R::Ok)
                        goto oops;
                } else if (py_cmp(st[f->sp - 2], st[f->sp - 1], op, ok) != R::Ok) {
                    goto oops;
                }
                f->sp -= 2;
                st[f->sp++] = value_bool(ok);
                break;
            }

            case Bc::Jump:
                f->pc = arg;
                break;
            case Bc::PopJumpIfFalse:
                if (!py_truth(st[--f->sp]))
                    f->pc = arg;
                break;
            case Bc::PopJumpIfTrue:
                if (py_truth(st[--f->sp]))
                    f->pc = arg;
                break;
            case Bc::JumpIfFalseOrPop:
                if (!py_truth(st[f->sp - 1]))
                    f->pc = arg;
                else
                    f->sp--;
                break;
            case Bc::JumpIfTrueOrPop:
                if (py_truth(st[f->sp - 1]))
                    f->pc = arg;
                else
                    f->sp--;
                break;

            case Bc::GetIter: {
                Value m = type_special(st[f->sp - 1], "__iter__");
                if (!m.is_nil()) {
                    if (!run_special(f, m, nullptr, 0, 1))
                        goto oops;
                    break;
                }
                Value it = py_iter(st[f->sp - 1]);
                if (it.is_nil())
                    goto oops;
                st[f->sp - 1] = it;
                break;
            }
            case Bc::ForIter: {
                Value m = type_special(st[f->sp - 1], "__next__");
                if (!m.is_nil()) {
                    Root kv{ cont_new(next_step) };
                    if (kv.v.is_nil())
                        goto oops;
                    cont_of(kv.v)->s[0]     = m;
                    cont_of(kv.v)->j        = arg;
                    cont_of(kv.v)->catching = CATCH_STOP;
                    cont_of(kv.v)->drop     = true;
                    if (!run_cont(kv.v, Value()))
                        goto oops;
                    break;
                }
                Value out;
                R r = py_next(st[f->sp - 1], out);
                if (r == R::Err)
                    goto oops;
                if (r == R::NotImpl) {
                    f->sp--;
                    f->pc = arg;
                } else if (!push(f, out)) {
                    goto oops;
                }
                break;
            }

            case Bc::BuildTuple: {
                TupleObj *t = tuple_new(arg);
                if (!t) {
                    oom();
                    goto oops;
                }
                for (u32 i = 0; i < arg; i++)
                    t->items()[i] = st[f->sp - arg + i];
                f->sp -= arg;
                st[f->sp++] = obj_value(t);
                break;
            }
            case Bc::BuildList: {
                ListObj *l = list_new();
                if (!l) {
                    oom();
                    goto oops;
                }
                // The items are still under sp, so the pushes below are safe.
                Root rl{ obj_value(l) };
                for (u32 i = 0; i < arg; i++)
                    if (!list_push(list_of(rl.v), st[f->sp - arg + i])) {
                        oom();
                        goto oops;
                    }
                f->sp -= arg;
                st[f->sp++] = rl.v;
                break;
            }
            case Bc::BuildSet: {
                SetObj *s = set_new();
                if (!s) {
                    oom();
                    goto oops;
                }
                Root rs{ obj_value(s) };
                for (u32 i = 0; i < arg; i++)
                    if (set_add(static_cast<SetObj *>(rs.v.obj()), st[f->sp - arg + i]) != R::Ok)
                        goto oops;
                f->sp -= arg;
                st[f->sp++] = rs.v;
                break;
            }
            case Bc::BuildMap: {
                DictObj *d = dict_new();
                if (!d) {
                    oom();
                    goto oops;
                }
                Root rd{ obj_value(d) };
                for (u32 i = 0; i < arg; i++) {
                    Value *pair = &st[f->sp - 2 * arg + 2 * i];
                    if (dict_set(dict_at(rd.v), pair[0], pair[1]) != R::Ok)
                        goto oops;
                }
                f->sp -= 2 * arg;
                st[f->sp++] = rd.v;
                break;
            }
            case Bc::BuildSlice: {
                Value step = arg == 3 ? st[f->sp - 1] : value_none();
                Value s    = slice_new(st[f->sp - arg], st[f->sp - arg + 1], step);
                if (s.is_nil())
                    goto oops;
                f->sp -= arg;
                st[f->sp++] = s;
                break;
            }
            case Bc::ListToTuple: {
                ListObj *l  = list_of(st[f->sp - 1]);
                TupleObj *t = tuple_new(l->items.size());
                if (!t) {
                    oom();
                    goto oops;
                }
                l = list_of(st[f->sp - 1]);
                for (usize i = 0; i < l->items.size(); i++)
                    t->items()[i] = l->items[i];
                st[f->sp - 1] = obj_value(t);
                break;
            }

            case Bc::ListAppend:
                if (!list_push(list_of(st[f->sp - 1 - arg]), st[f->sp - 1])) {
                    oom();
                    goto oops;
                }
                f->sp--;
                break;
            case Bc::SetAdd:
                if (set_add(static_cast<SetObj *>(st[f->sp - 1 - arg].obj()), st[f->sp - 1]) !=
                    R::Ok)
                    goto oops;
                f->sp--;
                break;
            case Bc::MapAdd:
                if (dict_set(dict_at(st[f->sp - 2 - arg]), st[f->sp - 2], st[f->sp - 1]) != R::Ok)
                    goto oops;
                f->sp -= 2;
                break;

            case Bc::ListExtend:
            case Bc::SetUpdate: {
                Root it{ py_iter(st[f->sp - 1]) };
                if (it.v.is_nil())
                    goto oops;
                Value into = st[f->sp - 1 - arg];
                for (;;) {
                    Root got;
                    R r = py_next(it.v, got.v);
                    if (r == R::Err)
                        goto oops;
                    if (r == R::NotImpl)
                        break;
                    if (in.op == Bc::ListExtend) {
                        if (!list_push(list_of(into), got.v)) {
                            oom();
                            goto oops;
                        }
                    } else if (set_add(static_cast<SetObj *>(into.obj()), got.v) != R::Ok) {
                        goto oops;
                    }
                }
                f->sp--;
                break;
            }
            case Bc::DictUpdate:
            case Bc::DictMerge: {
                Value from = st[f->sp - 1];
                if (!is_dict(from)) {
                    err_set2("TypeError", "argument after ** must be a mapping", type_name(from));
                    goto oops;
                }
                Value into = st[f->sp - 1 - arg];
                usize at   = 0;
                Value k, v;
                while (table_next(dict_at(from)->t, at, k, v)) {
                    // Two keywords for one parameter: the callable is under
                    // the map and the positional tuple.
                    Value had;
                    if (in.op == Bc::DictMerge && dict_get(dict_at(into), k, had) == R::Ok) {
                        Buf<128> m;
                        m.put(call_name(st[f->sp - 3 - arg]));
                        m.put("() got multiple values for keyword argument '");
                        m.put(is_str(k) ? str_of(k)->str() : Str("?")).put("'");
                        err_set("TypeError", m.str());
                        goto oops;
                    }
                    if (dict_set(dict_at(into), k, v) != R::Ok)
                        goto oops;
                }
                f->sp--;
                break;
            }

            case Bc::UnpackSequence:
            case Bc::UnpackEx: {
                u32 before = in.op == Bc::UnpackEx ? (arg & 0xffff) : arg;
                u32 after  = in.op == Bc::UnpackEx ? (arg >> 16) : 0;
                vm->flat.clear();
                Root it{ py_iter(st[f->sp - 1]) };
                if (it.v.is_nil())
                    goto oops;
                for (;;) {
                    Root got;
                    R r = py_next(it.v, got.v);
                    if (r == R::Err)
                        goto oops;
                    if (r == R::NotImpl)
                        break;
                    if (!vm->flat.push(got.v)) {
                        oom();
                        goto oops;
                    }
                }
                usize n    = vm->flat.size();
                usize want = usize(before) + after;
                if (in.op == Bc::UnpackSequence ? n != before : n < want) {
                    char tmp[24];
                    Buf<128> m;
                    if (n < want) {
                        m.put("not enough values to unpack (expected ");
                        if (in.op == Bc::UnpackEx)
                            m.put("at least ");
                        m.put(int_text(tmp, sizeof tmp, i64(want))).put(", got ");
                    } else {
                        m.put("too many values to unpack (expected ");
                        m.put(int_text(tmp, sizeof tmp, i64(before))).put(", got ");
                    }
                    m.put(int_text(tmp, sizeof tmp, i64(n))).put(')');
                    err_set("ValueError", m.str());
                    goto oops;
                }
                f->sp--;
                // Pushed back to front: the first target pops first.
                for (usize i = n; i > n - after; i--)
                    if (!push(f, vm->flat[i - 1]))
                        goto oops;
                if (in.op == Bc::UnpackEx) {
                    ListObj *mid = list_new();
                    if (!mid) {
                        oom();
                        goto oops;
                    }
                    Root rm{ obj_value(mid) };
                    for (usize i = before; i < n - after; i++)
                        if (!list_push(list_of(rm.v), vm->flat[i])) {
                            oom();
                            goto oops;
                        }
                    if (!push(f, rm.v))
                        goto oops;
                }
                for (usize i = before; i > 0; i--)
                    if (!push(f, vm->flat[i - 1]))
                        goto oops;
                vm->flat.clear();
                break;
            }

            case Bc::Call:
            case Bc::CallKw:
            case Bc::CallEx: {
                CallArgs a;
                u32 consumed = 0;
                Value callable;

                if (in.op == Bc::Call) {
                    a.args   = &st[f->sp - arg];
                    a.nargs  = arg;
                    consumed = arg + 1;
                    callable = st[f->sp - arg - 1];
                } else if (in.op == Bc::CallKw) {
                    TupleObj *names = static_cast<TupleObj *>(st[f->sp - 1].obj());
                    u32 nkw         = u32(names->len);
                    a.nargs         = arg - nkw;
                    a.args          = &st[f->sp - 1 - arg];
                    a.kwvals        = a.args + a.nargs;
                    a.kwnames       = names->items();
                    a.nkw           = nkw;
                    consumed        = arg + 2;
                    callable        = st[f->sp - 2 - arg];
                } else {
                    u32 extra = (arg & CX_KWARGS) ? 2 : 1;
                    callable  = st[f->sp - 1 - extra];
                    vm->flat.clear();
                    vm->kwnames.clear();
                    TupleObj *t = static_cast<TupleObj *>(st[f->sp - extra].obj());
                    for (usize i = 0; i < t->len; i++)
                        if (!vm->flat.push(t->items()[i])) {
                            oom();
                            goto oops;
                        }
                    a.nargs = u32(vm->flat.size());
                    if (arg & CX_KWARGS) {
                        Value kw = st[f->sp - 1];
                        if (!is_dict(kw)) {
                            err_set2("TypeError", "argument after ** must be a mapping",
                                     type_name(kw));
                            goto oops;
                        }
                        usize at = 0;
                        Value k, v;
                        while (table_next(dict_at(kw)->t, at, k, v))
                            if (!vm->kwnames.push(k) || !vm->flat.push(v)) {
                                oom();
                                goto oops;
                            }
                    }
                    a.args    = vm->flat.data();
                    a.kwvals  = vm->flat.data() + a.nargs;
                    a.kwnames = vm->kwnames.data();
                    a.nkw     = u32(vm->kwnames.size());
                    consumed  = extra + 1;
                }

                Value out;
                bool entered = false;
                if (do_call(callable, a, out, entered) != R::Ok)
                    goto oops;
                f->sp -= consumed;
                if (!land(out, entered))
                    goto oops;
                break;
            }

            case Bc::MakeFunction: {
                u32 extra = __builtin_popcount(arg);
                Value fv  = func_new(st[f->sp - 1], f->globals);
                if (fv.is_nil())
                    goto oops;
                FuncObj *fo = func_of(fv);
                u32 at      = f->sp - 1;
                if (arg & MF_CLOSURE)
                    fo->closure = st[--at];
                if (arg & MF_KWDEFAULTS)
                    fo->kwdefaults = st[--at];
                if (arg & MF_DEFAULTS)
                    fo->defaults = st[--at];
                f->sp -= extra + 1;
                st[f->sp++] = fv;
                break;
            }

            case Bc::Return: {
                Value v = st[f->sp - 1];
                if (f->back.is_nil()) {
                    vm->finished = true;
                    return;
                }
                Value k   = f->cont;
                vm->frame = f->back;
                vm->depth--;
                // A builtin was waiting on this call rather than the caller.
                if (!k.is_nil() ? !run_cont(k, v) : !push(frame_of(vm->frame), v))
                    goto oops;
                break;
            }

            case Bc::LoadBuildClass: {
                Value fn;
                StrObj *n = str_intern("__build_class__");
                if (!n || dict_get(dict_at(f->builtins), obj_value(n), fn) != R::Ok) {
                    err_set("SystemError", "__build_class__ is missing");
                    goto oops;
                }
                if (!push(f, fn))
                    goto oops;
                break;
            }

            case Bc::ImportName: {
                Value m = builtin_module(str_of(co->names[arg])->str());
                if (m.is_nil())
                    goto oops;
                f->sp -= 2;
                st[f->sp++] = m;
                break;
            }
            case Bc::ImportFrom: {
                Value out;
                if (py_getattr(st[f->sp - 1], str_of(co->names[arg]), out) != R::Ok)
                    goto oops;
                if (!push(f, out))
                    goto oops;
                break;
            }

                // ------------------------------------------------- exceptions

            case Bc::SetupFinally:
            case Bc::SetupWith: {
                if (f->nb >= f->nblocks) {
                    err_set("SystemError", "block stack overflow");
                    goto oops;
                }
                // A `with` keeps the manager's __exit__ below the cut, so the
                // handler still has it when the body has been discarded.
                u32 keep             = in.op == Bc::SetupWith ? f->sp - 1 : f->sp;
                f->blocks()[f->nb++] = Block{ arg, keep };
                break;
            }
            case Bc::PopBlock:
                if (f->nb)
                    f->nb--;
                break;

            case Bc::PushExcInfo: {
                Value exc     = st[f->sp - 1];
                st[f->sp - 1] = vm->handling.is_nil() ? value_none() : vm->handling;
                if (!push(f, exc))
                    goto oops;
                vm->handling = exc;
                break;
            }
            case Bc::PopExcept: {
                Value saved  = st[--f->sp];
                vm->handling = is_exc(saved) ? saved : Value();
                break;
            }

            case Bc::CheckExcMatch: {
                Value want = st[f->sp - 1];
                Value exc  = st[f->sp - 2];
                bool hit   = false;
                // `except (A, B)` is one tuple of types, and nothing else.
                if (is_tuple(want)) {
                    TupleObj *t = static_cast<TupleObj *>(want.obj());
                    for (usize k = 0; k < t->len && !hit; k++) {
                        if (!is_exc_type(t->items()[k])) {
                            err_set("TypeError",
                                    "catching classes that do not inherit from "
                                    "BaseException is not allowed");
                            goto oops;
                        }
                        hit = exc_matches(exc, t->items()[k]);
                    }
                } else if (is_exc_type(want)) {
                    hit = exc_matches(exc, want);
                } else {
                    err_set("TypeError",
                            "catching classes that do not inherit from "
                            "BaseException is not allowed");
                    goto oops;
                }
                // The type and the copy of the exception the compiler made
                // for this rung both go; the original stays below.
                f->sp -= 2;
                st[f->sp++] = value_bool(hit);
                break;
            }

            case Bc::Reraise: {
                Value exc = st[--f->sp];
                if (arg) {
                    Value saved  = st[--f->sp];
                    vm->handling = is_exc(saved) ? saved : Value();
                }
                if (!dispatch(exc))
                    return;
                continue;
            }

            case Bc::Raise: {
                if (!arg) {
                    if (vm->handling.is_nil()) {
                        err_set("RuntimeError", "No active exception to re-raise");
                        goto oops;
                    }
                    if (!dispatch(vm->handling))
                        return;
                    continue;
                }
                Value cause = arg == 2 ? st[f->sp - 1] : Value();
                Value exc   = st[f->sp - (arg == 2 ? 2 : 1)];
                f->sp -= arg;
                if (!cause.is_nil()) {
                    // `raise X from Y` needs X built before the cause is set.
                    Root rc{ cause }, re{ exc };
                    if (is_exc_type(re.v)) {
                        Value made = exc_inst(re.v, Value());
                        if (made.is_nil())
                            goto oops;
                        re = made;
                    }
                    if (!is_exc(re.v)) {
                        err_set2("TypeError", "exceptions must derive from BaseException",
                                 type_name(re.v));
                        goto oops;
                    }
                    static_cast<ExcObj *>(re.v.obj())->cause = rc.v;
                    exc                                      = re.v;
                }
                if (!raise_value(exc))
                    return;
                continue;
            }

            case Bc::LoadAssertionError: {
                Value t = exc_type_value(exc_find("AssertionError"));
                if (t.is_nil() || !push(f, t))
                    goto oops;
                break;
            }

            case Bc::BeforeWith: {
                // Looking __enter__ up allocates, so pin __exit__ first.
                Value enter;
                Root exit;
                if (py_getattr(st[f->sp - 1], str_intern("__exit__"), exit.v) != R::Ok ||
                    py_getattr(st[f->sp - 1], str_intern("__enter__"), enter) != R::Ok)
                    goto oops;
                st[f->sp - 1] = exit.v;
                if (!push(f, enter)) // the callable stays rooted while it runs
                    goto oops;
                CallArgs a;
                Value got;
                bool entered = false;
                if (do_call(st[f->sp - 1], a, got, entered) != R::Ok)
                    goto oops;
                f->sp--;
                // A Python __enter__ pushes its own answer when it returns.
                if (!land(got, entered))
                    goto oops;
                break;
            }

            case Bc::WithExceptStart: {
                Value exc = st[f->sp - 1];
                Value t   = exc_type_value(exc_type_of(exc));
                if (t.is_nil()) {
                    err_set("SystemError", "a with handler without an exception");
                    goto oops;
                }
                if (!push(f, t) || !push(f, exc) || !push(f, value_none()))
                    goto oops;
                // [exit, exc] became [exit, exc, type, exc, None]: __exit__ is
                // five down, and the three above it are its arguments.
                CallArgs a;
                a.args  = &st[f->sp - 3];
                a.nargs = 3;
                Value got;
                bool entered = false;
                if (do_call(st[f->sp - 5], a, got, entered) != R::Ok)
                    goto oops;
                f->sp -= 3;
                if (!land(got, entered))
                    goto oops;
                break;
            }

            default:
                err_set2("SystemError", "opcode not implemented yet", bc_name(in.op));
                goto oops;
            }
        }
        continue;

        // Every failure lands here, wherever it was set: the pending error
        // becomes an exception and goes looking for a handler. It is a new
        // one, so whatever traceback was being collected is not its.
    oops:
        vm->tb.clear();
        if (!raise_value(pending_exception()))
            return;
    }
}

} // namespace

bool vm_start(Value code, Args argv)
{
    py_init();
    if (!vm) {
        vm = heap_new<VM>();
        if (!vm)
            return err_set("MemoryError", "out of memory") == R::Ok;
        gc_root_hook(vm_mark);
    }

    DictObj *b = builtins_dict();
    if (!b || !exc_install(b))
        return false;
    vm->builtins = obj_value(b);

    DictObj *g = dict_new();
    if (!g)
        return err_set("MemoryError", "out of memory") == R::Ok;
    vm->globals = obj_value(g);

    Root rc{ code };
    ListObj *av = list_new();
    if (!av)
        return err_set("MemoryError", "out of memory") == R::Ok;
    Root ra{ obj_value(av) };
    for (usize i = 0; i < argv.size(); i++) {
        Value s = str_new(argv[i]);
        if (s.is_nil() || !list_push(list_of(ra.v), s))
            return false;
    }
    sys_set_argv(ra.v);
    print_sink(&vm->out);

    StrObj *name = str_intern("__name__");
    Value main   = str_new("__main__");
    if (!name || main.is_nil())
        return false;
    if (dict_set(dict_at(vm->globals), obj_value(name), main) != R::Ok)
        return false;

    FrameObj *f = frame_push(code_of(rc.v), vm->globals, vm->globals, Value());
    return f != nullptr;
}

Req vm_burst()
{
    for (;;) {
        bool spent = false;
        if (!vm->finished) {
            interpret();
            spent = !vm->finished && vm->out.size() < FLUSH_AT;
        }

        if (!vm->out.empty()) {
            vm->sent = &vm->out;
            return Req{ ReqKind::Write, SYS_STDOUT, vm->out.str(), 0 };
        }
        if (!vm->finished) {
            // The budget ran out rather than the work: give the driver its
            // turn, which is the only way a signal reaches this process.
            if (spent)
                return Req{ ReqKind::Tick, 0, Str(), 0 };
            continue;
        }
        if (!vm->err.empty() && !vm->reported) {
            vm->reported = true;
            vm->sent     = &vm->err;
            return Req{ ReqKind::Write, SYS_STDERR, vm->err.str(), 0 };
        }
        return Req{ ReqKind::Exit, 0, Str(), vm->status };
    }
}

Value vm_frame()
{
    return vm ? vm->frame : Value();
}

void vm_interrupt()
{
    vm->interrupt = true;
}

void vm_write_done(bool ok)
{
    if (vm->sent)
        vm->sent->clear();
    vm->sent = nullptr;
    if (!ok) {
        vm->finished = true;
        vm->failed   = true;
        vm->reported = true;
    }
}
