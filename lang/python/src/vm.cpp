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

#include "abc.h"
#include "atexit.h"
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "codec.h"
#include "compare.h"
#include "egroup.h"
#include "exc.h"
#include "frame.h"
#include "func.h"
#include "gc.h"
#include "gen.h"
#include "import.h"
#include "intern.h"
#include "io.h"
#include "iter.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "lazy.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "patma.h"
#include "posix.h"
#include "proc/io.h"
#include "templatelib.h"
#include "type.h"
#include "typevar.h"
#include "ustr.h"
#include "weak.h"

namespace {

constexpr usize FLUSH_AT     = 4000; // bytes buffered before a write is asked for
constexpr u32 FRAMES_DEFAULT = 200;  // the frames are heap, but a limit says so
u32 max_frames               = FRAMES_DEFAULT;

// How many instructions a burst runs before letting the driver back in. A
// compute loop parks nowhere, so nothing else -- a ^C above all -- can reach
// the process until it does.
constexpr u32 BURST = 20000;

struct VM {
    FrameObj *between = nullptr; // the frame a ^C is raised in, between two instructions
    bool exiting      = false;   // atexit's calls have been started
    u32 finalizing    = 0;       // owed finalizers whose frames are running
    Value frame;                 // the innermost FrameObj
    Value globals;               // __main__'s namespace
    Value builtins;              // the builtins namespace
    Value handling;              // the exception an `except` clause is working on
    Vec<Value> flat;             // CallEx's arguments, flattened
    Vec<Value> kwnames;          // and their names
    Vec<Value> bound;            // self, then a bound method's own arguments
    Vec<String> tb;              // the traceback, innermost first, as it unwinds
    String out;                  // what print has buffered
    String err;                  // what goes to stderr, once there is any
    String *sent = nullptr;      // which of the two the driver is writing
    Value reading;               // the ContObj waiting on a file or a sleep, or Nil
    u32 nap_ms   = 0;            // how long, when it is a sleep
    bool napping = false;
    bool calling = false; // it is a system call, `sys`
    SysReq sys;           // whose strings view the three below
    String sys_path, sys_path2, sys_data;
    SysAns answer;          // what the last one said
    Value resume;           // the same, once the answer is in
    Value thrown;           // what gen.throw passed, to raise at the resume point
    String want;            // the file it asked for
    String text;            // what came back
    bool found     = false; // whether there was such a name
    bool isdir     = false; // and whether it is a directory
    u32 depth      = 0;
    u32 budget     = 0;
    i32 status     = 0;
    bool failed    = false;
    bool finished  = false;
    bool interrupt = false; // a ^C the driver saw, to raise at the next step
    u32 signals    = 0;     // any other, a bit each, for its handler
    bool prompt    = false; // a command at a prompt: its end is not the exit
    bool quitting  = false; // and this one asked to leave
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
    gc_mark(vm->reading);
    gc_mark(vm->resume);
    gc_mark(vm->thrown);
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

// A frame's globals are usually a plain dict, but FunctionType takes any
// mapping: annotationlib hands over a dict subclass whose __missing__ answers
// every name. The real dict inside one is where a name is looked up first.
DictObj *globals_at(Value v)
{
    return dict_at(is_anydict(v) ? v : method_self(v));
}

bool push(FrameObj *f, Value v)
{
    if (f->nlocals + f->sp >= f->nslots)
        return err_set("SystemError", "value stack overflow"), false;
    f->stack()[f->sp++] = v;
    return true;
}

// ------------------------------------------------------------------- naming

// The dict a namespace keeps its names in: itself, the dict inside a dict
// subclass, or Nil for a mapping written in Python.
Value names_dict(Value space)
{
    if (is_dict(space))
        return space;
    if (is_inst(space) && is_dict(inst_of(space)->native))
        return inst_of(space)->native;
    return Value();
}

R lookup(FrameObj *f, StrObj *name, Value &out)
{
    if (!f->locals.is_nil()) {
        Value d = names_dict(f->locals);
        R r     = d.is_nil() ? py_getitem(f->locals, obj_value(name), out)
                             : dict_get(dict_at(d), obj_value(name), out);
        if (r == R::Err && d.is_nil() && err_kind() == "KeyError") {
            err_clear();
            r = R::NotImpl;
        }
        if (r != R::NotImpl)
            return r;
    }
    R r = dict_get(globals_at(f->globals), obj_value(name), out);
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
    if (vm->depth >= max_frames)
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

// A list to search: the list itself, or a fresh one holding a tuple's items.
// Nil for anything else, and for an empty one -- which needs no search at all.
Value list_or_tuple_of(Value v)
{
    // A subclass of one keeps the real sequence inside it.
    v = method_self(v);
    if (is_list(v))
        return list_of(v)->items.empty() ? Value() : v;
    if (!is_tuple(v) || !static_cast<TupleObj *>(v.obj())->len)
        return Value();
    Root rv{ v };
    ListObj *l = list_new();
    if (!l)
        return oom(), Value();
    Root rl{ obj_value(l) };
    TupleObj *t = static_cast<TupleObj *>(rv.v.obj());
    for (usize i = 0; i < t->len; i++)
        if (!list_push(list_of(rl.v), static_cast<TupleObj *>(rv.v.obj())->items()[i]))
            return oom(), Value();
    return rl.v;
}

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
R gen_resume(Value gv, u8 how, const CallArgs &a, Value &out, bool &entered);
R run_resume(Value run, const CallArgs &a, Value &out, bool &entered);
Value pending_exception();
bool cont_catches(Value kv, Value e);
Value cont_catcher(Value kv, Value e);
bool raise_value(Value e);

// `__init__` has returned; the answer is the instance it was given.
R init_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        // A native __init__ that has calls to make first: s[1] its
        // continuation, whose answer is what __init__ returned.
        if (!k->s[1].is_nil())
            return cont_await(k, k->s[1]);
        return err_set("SystemError", "an init continuation was not started");
    }
    if (!is_none(in))
        return err_set2("TypeError", "__init__() should return None", type_name(in));
    return cont_done(k, k->s[0]);
}

// A class that wrote its own __new__: call it, then __init__ on what it made
// if that is an instance of the class. s[0] is the class, s[1] the arguments
// __new__ takes, s[2] the ones __init__ does and s[5]/s[6] the keywords both
// of them take.
R new_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        return cont_call_kw(k, k->s[3], k->s[1], k->s[5], k->s[6]);
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
        return cont_call_kw(k, init, k->s[2], k->s[5], k->s[6]);
    }
    default:
        if (!is_none(in))
            return err_set2("TypeError", "__init__() should return None", type_name(in));
        return cont_done(k, k->s[4]);
    }
}

// The call's keywords as two tuples, for a continuation to hand on.
bool kw_tuples(const CallArgs &a, Root &names, Root &vals)
{
    TupleObj *n = tuple_new(a.nkw);
    if (!n)
        return oom() == R::Ok;
    names       = obj_value(n);
    TupleObj *v = tuple_new(a.nkw);
    if (!v)
        return oom() == R::Ok;
    vals = obj_value(v);
    for (u32 i = 0; i < a.nkw; i++) {
        static_cast<TupleObj *>(names.v.obj())->items()[i] = a.kwnames[i];
        static_cast<TupleObj *>(vals.v.obj())->items()[i]  = a.kwvals[i];
    }
    return true;
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
    Root ra{ obj_value(args) };
    return exc_construct(rc.v, ra.v);
}

// Making an instance: __new__ where a built-in is involved, then __init__.
R type_call(Value cls, const CallArgs &a, Value &out, bool &entered)
{
    Root rc{ cls }, ctor, init, owner;
    StrObj *nw = str_intern("__new__");
    StrObj *in = str_intern("__init__");
    if (!nw || !in)
        return oom();

    if (type_lookup(rc.v, nw, ctor.v, &owner.v) == R::Err)
        return R::Err;
    if (type_obj(rc.v)->exc && !type_obj(rc.v)->heap)
        return exc_type_invoke(rc.v, a, out);
    if (!type_obj(rc.v)->heap) {
        // A built-in type with no constructor of its own is not object's to
        // make: its instances have a layout object.__new__ knows nothing of.
        if (ctor.v.is_nil() || (owner.v == type_object() && rc.v != type_object())) {
            Buf<128> m;
            m.put("cannot create '").put(type_obj(rc.v)->slots.name).put("' instances");
            return err_set("TypeError", m.str());
        }
        return do_call(ctor.v, a, out, entered);
    }

    R r = type_lookup(rc.v, in, init.v);
    if (r == R::Err)
        return R::Err;
    // object lends every class an __init__ that does nothing, so finding one
    // is not the same as the class having written one.
    bool own_init = r == R::Ok && !is_object_default(init.v);

    // A class that wrote __new__ decides what it gets, and gets `cls` first.
    // A metaclass with none takes type's, which makes the class it is called
    // to make; every other class without one is made here.
    Root own{ type_own_new(rc.v) };
    if (own.v.is_nil() && type_obj(rc.v)->meta)
        own = ctor.v;
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
        Root kwn, kwv;
        if (!kw_tuples(a, kwn, kwv))
            return R::Err;
        Root kv{ cont_new(new_step) };
        if (kv.v.is_nil())
            return R::Err;
        ContObj *k = cont_of(kv.v);
        k->s[0]    = rc.v;
        k->s[1]    = rna.v;
        k->s[2]    = ria.v;
        k->s[3]    = own.v;
        k->s[5]    = kwn.v;
        k->s[6]    = kwv.v;
        out        = kv.v;
        return R::Ok;
    }

    // An abstract class is one whose methods are not all there yet.
    Str missing;
    if (abc_abstract(rc.v, missing)) {
        Buf<96> m;
        m.put("Can't instantiate abstract class ").put(type_obj(rc.v)->slots.name);
        m.put(" with abstract method ").put(missing);
        return err_set("TypeError", m.str());
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
        bool give_args = !own_init || type_native_takes_args(rc.v);
        if (do_call(base.v, give_args ? a : none, made, e) != R::Ok)
            return R::Err;
        made = weak_adopt(made, self.v);
        if (made.is_nil())
            return R::Err;
        inst_of(self.v)->native = made;
    }
    if (!own_init) {
        // object.__init__ refuses arguments a __new__ did not take, which is
        // how `object(1)` and `C(1)` for a C with neither become errors.
        if ((a.nargs || a.nkw) && type_own_new(rc.v).is_nil() && type_obj(rc.v)->native.is_nil() &&
            !type_obj(rc.v)->exc)
            return err_set2("TypeError", "this class takes no arguments",
                            type_obj(rc.v)->slots.name);
        out = self.v;
        return R::Ok;
    }

    // self before the call's own arguments, which is what a method call is.
    Vec<Value> next; // a.args may be in vm->bound already
    if (!next.push(self.v))
        return oom();
    for (u32 i = 0; i < a.nargs; i++)
        if (!next.push(a.args[i]))
            return oom();
    vm->bound = static_cast<Vec<Value> &&>(next);
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
    if (!e && is_cont(got)) {
        Root rg{ got };
        Root kv{ cont_new(init_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = self.v;
        cont_of(kv.v)->s[1] = rg.v;
        out                 = kv.v;
        return R::Ok;
    }
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
    if (is_weakproxy(callable)) {
        callable = proxy_target(callable);
        if (callable.is_nil())
            return R::Err;
    }
    if (is_native(callable))
        return static_cast<NativeObj *>(callable.obj())->fn(a, out);
    if (is_type(callable))
        return type_call(callable, a, out, entered);
    if (is_newwrap(callable))
        return newwrap_call(callable, a, out);
    // Resuming a generator pushes a frame, and only the loop may do that. So
    // gen.send is an object of its own rather than a native; see gen.h.
    if (is_genrun(callable))
        return run_resume(callable, a, out, entered);

    if (is_method(callable)) {
        MethodObj *m = static_cast<MethodObj *>(callable.obj());
        Root rf{ m->fn }, rs{ m->self };
        // The arguments may already be in vm->bound, when the function is
        // itself a callable object: the list is built apart and then swapped.
        Vec<Value> next;
        if (!next.push(rs.v))
            return oom();
        for (u32 i = 0; i < a.nargs; i++)
            if (!next.push(a.args[i]))
                return oom();
        vm->bound = static_cast<Vec<Value> &&>(next);
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
            // A plain native does not bind, so it is called without the
            // instance: `typing.NewType.__call__` is `_typing._idfunc`.
            if (fn.v.is_obj() && (fn.v.obj()->flags & OBJ_PLAINFN))
                return do_call(fn.v, a, out, entered);
            Root bound{ method_new(fn.v, callable) };
            if (bound.v.is_nil())
                return R::Err;
            return do_call(bound.v, a, out, entered);
        }
    }

    // A staticmethod is called as the function it wraps, as it is since 3.10.
    if (callable.is_obj() && callable.obj()->type == &staticmethod_type)
        return do_call(static_cast<WrapObj *>(callable.obj())->fn, a, out, entered);

    if (!is_func(callable)) {
        // A built-in object may answer __call__ out of its own method table:
        // a weak reference is called to get its target back.
        StrObj *cl = str_intern("__call__");
        Root fn;
        if (!cl)
            return oom();
        if (callable.is_obj() && method_find(callable, cl, fn.v) == R::Ok)
            return do_call(fn.v, a, out, entered);
        Buf<96> m;
        m.put("'").put(type_name(callable)).put("' object is not callable");
        return err_set("TypeError", m.str());
    }

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
    // A generator function binds its arguments and stops. The frame is parked
    // in the object rather than entered, and none of the body has run.
    if (co->flags & CO_SUSPENDS) {
        vm->handling = nf->handling;
        vm->frame    = nf->back;
        vm->depth--;
        Root rf{ obj_value(nf) };
        frame_of(rf.v)->back = Value();
        out                  = gen_new(rf.v);
        if (out.is_nil())
            return R::Err;
        frame_of(rf.v)->gen = out;
        return R::Ok;
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
        k->kwnames = Value();
        k->kwvals  = Value();
        if (k->step(k, ri.v) != R::Ok) {
            // The builtin this one was answering for may be waiting to catch
            // exactly this: an await that ends in StopIteration.
            Root e{ pending_exception() };
            Value c = cont_catcher(k->next, e.v);
            if (c.is_nil())
                return err_set_value(e.v), false;
            err_clear();
            cont_of(c)->caught = e.v;
            rk                 = c;
            ri                 = Value();
            continue;
        }

        // The step wants a file. Park it: vm_burst asks the driver, and
        // vm_read_done puts it back on vm->resume with the answer.
        if (k->reading) {
            k->reading  = false;
            vm->reading = rk.v;
            return true;
        }

        if (k->fn.is_nil()) {
            // The step's answer is another suspension. Chain it in front
            // here rather than calling back into this loop, so that a long
            // delegation costs no native stack.
            if (is_cont(k->out)) {
                Value nx = k->out;
                if (cont_of(nx)->next.is_nil()) {
                    cont_of(nx)->next = k->next;
                    if (k->next.is_nil() && k->drop)
                        cont_of(nx)->drop = true;
                }
                rk = nx;
                ri = Value();
                continue;
            }
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
        if (!k->kwnames.is_nil()) {
            a.kwnames = static_cast<TupleObj *>(k->kwnames.obj())->items();
            a.kwvals  = static_cast<TupleObj *>(k->kwvals.obj())->items();
            a.nkw     = u32(static_cast<TupleObj *>(k->kwnames.obj())->len);
        }
        Value out;
        bool entered = false;
        if (do_call(k->fn, a, out, entered) != R::Ok) {
            // The call failed without entering a frame, so dispatch never
            // passes this continuation. A generator that has already stopped
            // answers __next__ this way.
            Root e{ pending_exception() };
            Value c = cont_catcher(rk.v, e.v);
            if (c.is_nil())
                return err_set_value(e.v), false;
            err_clear();
            cont_of(c)->caught = e.v;
            rk                 = c;
            ri                 = Value();
            continue;
        }
        if (entered) {
            // A frame that already answers to a continuation of its own --
            // a class's __init__, whose answer is the instance -- hands on.
            FrameObj *nf = frame_of(vm->frame);
            if (is_cont(nf->cont) && cont_of(nf->cont)->next.is_nil())
                cont_of(nf->cont)->next = rk.v;
            else
                nf->cont = rk.v;
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
    case Op::MatMul:
        return { "__matmul__", "__rmatmul__" };
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
    case Op::MatMul:
        return "__imatmul__";
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

// The jumps and `not` over such a test. s[0] the call, s[1] the jump target;
// j the opcode, with bit 8 set for __len__. The step does the jump itself.
R truth_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    bool yes = false;
    if (truth_answer(in, k->j & 0x100, yes) != R::Ok)
        return R::Err;
    FrameObj *f = frame_of(vm->frame);
    Value *st   = f->stack();
    u32 target  = u32(k->s[1].as_int());
    switch (Bc(k->j & 0xff)) {
    case Bc::PopJumpIfFalse:
        f->sp--;
        if (!yes)
            f->pc = target;
        break;
    case Bc::PopJumpIfTrue:
        f->sp--;
        if (yes)
            f->pc = target;
        break;
    case Bc::JumpIfFalseOrPop:
        if (!yes)
            f->pc = target;
        else
            f->sp--;
        break;
    case Bc::JumpIfTrueOrPop:
        if (yes)
            f->pc = target;
        else
            f->sp--;
        break;
    default: // `not`
        st[f->sp - 1] = value_bool(!yes);
        break;
    }
    return cont_done(k, value_none());
}

// A truth test the operand answers in Python. False on failure; `done` false
// when the native test stands.
bool truth_python(FrameObj *f, Bc op, u32 target, bool &done)
{
    bool len = false;
    Root m{ truth_special(f->stack()[f->sp - 1], len) };
    done = !m.v.is_nil();
    if (!done)
        return !err_pending();
    Root kv{ cont_new(truth_step) };
    if (kv.v.is_nil())
        return false;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = m.v;
    k->s[1]    = Value::of_int(i32(target));
    k->j       = u32(op) | (len ? 0x100 : 0);
    k->drop    = true;
    return run_cont(kv.v, Value());
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

// A finalizer: s[0] the callable, s[1] its one argument or Nil. Whatever it
// raises is caught here and reported rather than raised on, because there is
// no statement for it to have come from.
// warnings._warn_unawaited_coroutine(coro), importing warnings first.
R unawaited_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        StrObj *imp = str_intern("__import__");
        Value fn;
        if (!imp || dict_get(dict_at(vm->builtins), obj_value(imp), fn) != R::Ok)
            return err_pending() ? R::Err : err_set("MemoryError", "out of memory");
        Value name = str_new("warnings");
        if (name.is_nil())
            return R::Err;
        return cont_call(k, fn, name);
    }
    if (k->i == 2) {
        StrObj *n = str_intern("_warn_unawaited_coroutine");
        Value fn;
        if (!n || py_getattr(in, n, fn) != R::Ok)
            return R::Err;
        return cont_call(k, fn, k->s[0]);
    }
    return cont_done(k, value_none());
}

R b_warn_unawaited(const CallArgs &a, Value &out)
{
    Root rc{ a.nargs ? a.args[0] : Value() };
    Root kv{ cont_new(unawaited_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = rc.v;
    out                 = kv.v;
    return R::Ok;
}

// A signal handler: s[0] the callable, j the signal. It is called with the
// frame the program was in.
R sig_step(ContObj *k, Value)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value::of_int(i32(k->j)), 2,
                         vm->frame.is_nil() ? value_none() : vm->frame);
    return cont_done(k, value_none());
}

R final_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        k->catching = CATCH_ANY;
        return cont_call(k, k->s[0], k->s[1], k->s[1].is_nil() ? 0 : 1);
    }
    if (k->j) {
        k->j = 0;
        vm->finalizing--;
    }
    if (in.is_nil() && !k->caught.is_nil()) {
        vm->tb.clear();
        vm->err.append("Exception ignored in a finalizer:\n");
        exc_line(k->caught, vm->err);
        vm->err.push('\n');
    }
    return cont_done(k, value_none());
}

// The next call the collector owes, as a continuation that makes it and
// reports what it raises. Nil when that one has nothing to run.
Value owed_next()
{
    Root o, fn;
    gc_take(o.v, fn.v);
    // A generator parked at a yield is closed rather than deleted, which is
    // what runs the `finally` it is sitting inside. A coroutine never started
    // says so through warnings.
    bool unawaited = fn.v.is_nil() && is_coro(o.v) && gen_of(o.v)->state == GEN_CREATED;
    if (unawaited)
        fn = native_new("_warn_unawaited_coroutine", b_warn_unawaited);
    Root m{ !fn.v.is_nil() ? fn.v
            : is_genlike(o.v)
                ? (gen_of(o.v)->state == GEN_SUSPENDED ? genrun_new(o.v, GR_CLOSE) : Value())
            : o.v.obj()->type->del ? o.v.obj()->type->del(o.v)
                                   : type_special(o.v, "__del__") };
    if (m.v.is_nil())
        return Value();
    Root kv{ cont_new(final_step) };
    if (kv.v.is_nil())
        return Value();
    cont_of(kv.v)->s[0] = m.v;
    cont_of(kv.v)->s[1] = fn.v.is_nil() ? Value() : o.v;
    return kv.v;
}

R drain_step(ContObj *k, Value)
{
    while (gc_owes()) {
        Root kv{ owed_next() };
        if (kv.v.is_nil()) {
            err_clear();
            continue;
        }
        return cont_await(k, kv.v);
    }
    return cont_done(k, value_none());
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

// seq[i] where i answers __index__ in Python: s[0] the bound __index__, s[1]
// the sequence.
R index_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    if (!is_intval(in)) {
        Buf<128> m;
        m.put("__index__ returned non-int (type ").put(type_name(in)).put(')');
        return err_set("TypeError", m.str());
    }
    Value out;
    if (py_getitem(k->s[1], in, out) != R::Ok)
        return R::Err;
    return cont_done(k, out);
}

// True when the subscript was handed to index_step, or failed doing so.
bool index_special(FrameObj *f)
{
    Value seq = f->stack()[f->sp - 2], key = f->stack()[f->sp - 1];
    const Type *t = type_of(seq);
    if (!is_inst(key) || is_inst(seq) || !t || !t->getitem || is_anydict(seq) ||
        is_mappingproxy(seq) || !type_has_py_special(key, "__index__"))
        return false;
    Root rs{ seq };
    Root m{ type_special(key, "__index__") };
    if (m.v.is_nil())
        return true;
    Root kv{ cont_new(index_step) };
    if (kv.v.is_nil())
        return true;
    cont_of(kv.v)->s[0] = m.v;
    cont_of(kv.v)->s[1] = rs.v;
    f->sp -= 2;
    run_cont(kv.v, Value());
    return true;
}

// `from m import x` through a getter: the guard answers this continuation
// a fresh marker when the name is missing. s[0] the module, s[1] the name,
// s[2] the marker.
R from_step(ContObj *k, Value in)
{
    if (in == k->s[2])
        return import_missing(k->s[0], str_of(k->s[1]));
    return cont_done(k, in);
}

// A name read from a namespace written in Python: its __getitem__, and a
// KeyError there sends the lookup on to the globals and the builtins.
R load_name_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        k->catching = CATCH_ANY;
        return cont_call(k, k->s[0], k->s[1]);
    }
    k->catching = CATCH_NONE;
    if (k->caught.is_nil())
        return cont_done(k, in);
    Root c{ k->caught };
    k->caught        = Value();
    const ExcType *t = exc_type_of(c.v);
    if (!t || !exc_is(t, exc_find("KeyError")))
        return err_set_value(c.v);
    FrameObj *f = frame_of(vm->frame);
    StrObj *n   = str_of(k->s[1]);
    Value out;
    R r = dict_get(globals_at(f->globals), obj_value(n), out);
    if (r == R::NotImpl)
        r = dict_get(dict_at(f->builtins), obj_value(n), out);
    if (r == R::Err)
        return R::Err;
    if (r == R::NotImpl)
        return name_error("NameError", n);
    return cont_done(k, out);
}

bool load_name_python(FrameObj *f, Value name)
{
    Root rn{ name };
    Root m{ type_special(f->locals, "__getitem__") };
    if (m.v.is_nil())
        return false;
    Root kv{ cont_new(load_name_step) };
    if (kv.v.is_nil())
        return false;
    cont_of(kv.v)->s[0] = m.v;
    cont_of(kv.v)->s[1] = rn.v;
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
        return cont_done(k, (k->j & 0x200) ? value_bool(!py_truth(in)) : in);
    if (phase == 1 && !k->s[2].is_nil())
        return cont_call(k, k->s[2], k->s[3]);

    bool compare = (k->j & 0x100) != 0;
    Value a = k->s[4], b = k->s[5];
    if (compare) {
        Cmp op = (k->j & 0x200) ? Cmp::Ne : Cmp(k->j & 0xff);
        if (op == Cmp::Eq || op == Cmp::Ne)
            return cont_done(k, value_bool((a == b) == (op == Cmp::Eq)));
        Buf<96> m;
        m.put("'").put(cmp_symbol(op)).put("' not supported between instances of '");
        m.put(type_name(a)).put("' and '").put(type_name(b)).put("'");
        return err_set("TypeError", m.str());
    }
    return binop_failed(a, b, Op(k->j & 0xff));
}

// A weak proxy among the top `n` operands becomes its referent, which is what
// every operator on one asks. False with ReferenceError for a dead one.
bool unproxy_operands(FrameObj *f, u32 n)
{
    Value *st = f->stack();
    for (u32 i = 1; i <= n; i++) {
        Value &v = st[f->sp - i];
        if (!is_weakproxy(v))
            continue;
        v = proxy_target(v);
        if (v.is_nil())
            return false;
    }
    return true;
}

// A binary operator where one side is a class instance. False on failure;
// `done` is false when neither side had anything and the ordinary path stands.
bool dunder_binop(FrameObj *f, Value a, Value b, u32 tag, Dunder d, bool &done)
{
    Root left{ operand_special(a, d.name) };
    Root right;
    // An operator tries the reflected call only where the other side is a
    // different class; a comparison tries it either way, which is what lets a
    // class with only a __lt__ answer `>` between two of its own.
    if ((tag & 0x100) || !(is_inst(a) && is_inst(b) && inst_of(a)->cls == inst_of(b)->cls))
        right = operand_special(b, d.refl);
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
    if (err_line()) {
        v = exc_syntax(err_kind(), err_message(), err_file(), err_line(), err_col(), err_text());
        if (!v.is_nil())
            return v;
    }
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
    // A frame raised in between two instructions is at the one it was about
    // to run; any other is at the one that raised.
    u32 at = f == vm->between ? f->pc : f->pc ? f->pc - 1 : 0;
    b.put(int_text(tmp, sizeof tmp, i64(code_line(c, at))));
    b.put(", in ").put(is_str(c->name) ? str_of(c->name)->str() : Str("?")).put('\n');
    String line;
    return line.append(b.str()) && vm->tb.push(static_cast<String &&>(line));
}

// The frame `f` is one `e` passes through: a traceback entry for it, in front
// of the ones already there.
bool tb_here(FrameObj *f, Value e)
{
    if (!is_exc(e))
        return true;
    Root re{ e };
    CodeObj *c = code_of(f->code);
    u32 at     = f == vm->between ? f->pc : f->pc ? f->pc - 1 : 0;
    Value t    = tb_new(static_cast<ExcObj *>(re.v.obj())->tb, obj_value(f), i32(at * 2),
                        i32(code_line(c, at)));
    if (t.is_nil())
        return false;
    static_cast<ExcObj *>(re.v.obj())->tb = t;
    return true;
}

// The traceback lines of `e`, outermost first, each behind `margin`. False
// when `e` has no traceback of its own.
bool tb_lines(Value e, Str margin)
{
    if (!is_exc(e) || static_cast<ExcObj *>(e.obj())->tb.is_nil())
        return false;
    for (Value t = static_cast<ExcObj *>(e.obj())->tb; is_traceback(t); t = tb_of(t)->next) {
        CodeObj *c = code_of(frame_of(tb_of(t)->frame)->code);
        char tmp[24];
        Buf<192> b;
        b.put(margin).put("  File \"");
        b.put(is_str(c->filename) ? str_of(c->filename)->str() : Str("?"));
        b.put("\", line ").put(int_text(tmp, sizeof tmp, i64(tb_of(t)->lineno)));
        b.put(", in ").put(is_str(c->name) ? str_of(c->name)->str() : Str("?")).put('\n');
        vm->err.append(b.str());
    }
    return true;
}

// Print an exception the way CPython does, its cause or context first. The
// depth is a bound: a context chain can be made to loop.
void report(Value e, u32 depth = 0)
{
    if (is_exc(e) && depth < 8) {
        ExcObj *eo  = static_cast<ExcObj *>(e.obj());
        Value under = eo->cause;
        Str joiner  = "\nThe above exception was the direct cause of the following exception:\n\n";
        if (under.is_nil() && !eo->suppress) {
            under  = static_cast<ExcObj *>(e.obj())->context;
            joiner = "\nDuring handling of the above exception, another exception occurred:\n\n";
        }
        if (!under.is_nil() && under != e) {
            report(under, depth + 1);
            vm->err.append(joiner);
        }
    }
    if (is_egroup(e)) {
        // A group is drawn as CPython draws it: its traceback and its line
        // behind a margin, and each member in a box below.
        vm->err.append("  + Exception Group Traceback (most recent call last):\n");
        if (!tb_lines(e, "  | "))
            for (usize k = vm->tb.size(); k > 0; k--) {
                vm->err.append("  | ");
                vm->err.append(vm->tb[k - 1].str());
            }
        vm->tb.clear();
        String line;
        exc_line(e, line);
        Str text = line.str();
        for (usize at = 0;;) {
            usize end = at;
            while (end < text.size() && text[end] != '\n')
                end++;
            vm->err.append("  | ");
            vm->err.append(text.substr(at, end - at));
            vm->err.push('\n');
            if (end >= text.size())
                break;
            at = end + 1;
        }
        egroup_report(e, vm->err);
        return;
    }
    if (is_exc(e) && !static_cast<ExcObj *>(e.obj())->tb.is_nil()) {
        vm->err.append("Traceback (most recent call last):\n");
        tb_lines(e, "");
    } else if (vm->tb.size()) {
        vm->err.append("Traceback (most recent call last):\n");
        for (usize k = vm->tb.size(); k > 0; k--)
            vm->err.append(vm->tb[k - 1].str());
    }
    vm->tb.clear();
    exc_where(e, vm->err);
    exc_line(e, vm->err);
    vm->err.push('\n');
}

bool run_cont(Value kv, Value in);

R exit_files_step(ContObj *k, Value)
{
    if (k->i++ == 0) {
        Value files = io_exit_runner();
        if (files.is_nil())
            return err_pending() ? R::Err : cont_done(k, value_none());
        return cont_await(k, files);
    }
    return cont_done(k, value_none());
}

R exit_seq_step(ContObj *k, Value)
{
    switch (k->i++) {
    case 0:
        return cont_await(k, k->s[0]);
    case 1: {
        Root files{ cont_new(exit_files_step) };
        if (files.v.is_nil())
            return R::Err;
        return cont_await(k, files.v);
    }
    default:
        return cont_done(k, value_none());
    }
}

// The program is over: what atexit holds runs first, once, and then what is
// still open is flushed and closed. The last frame is gone, so the loop
// finishes when the calls are done and nothing is left.
void exit_hooks()
{
    // At a prompt a command ending only ends the command: atexit runs once,
    // when the session does.
    if (vm->prompt && !vm->quitting) {
        vm->frame    = Value();
        vm->finished = true;
        return;
    }
    if (vm->exiting || (!atexit_pending() && !io_exit_pending()))
        return;
    vm->exiting  = true;
    vm->finished = false;
    vm->frame    = Value();
    Root kv{ cont_new(exit_seq_step) };
    if (!kv.v.is_nil()) {
        Root first{ atexit_pending() ? atexit_runner() : obj_value(nullptr) };
        if (atexit_pending() && first.v.is_nil()) {
            kv = Value();
        } else if (first.v.is_nil()) {
            cont_of(kv.v)->i = 1;
        } else {
            cont_of(kv.v)->s[0] = first.v;
        }
    }
    if (kv.v.is_nil()) {
        vm->finished = true;
        return;
    }
    cont_of(kv.v)->drop = true;
    if (!run_cont(kv.v, Value())) {
        report(pending_exception());
        err_clear();
        vm->finished = true;
    }
}

// Nothing caught it. SystemExit is the one that is not an error.
void uncaught(Value e)
{
    vm->finished = true;
    if (is_exc(e) && exc_is(exc_type_of(e), exc_find("SystemExit"))) {
        // exit() at a prompt leaves the session, not just the command.
        vm->quitting = true;
        TupleObj *a  = static_cast<TupleObj *>(static_cast<ExcObj *>(e.obj())->args.obj());
        i64 code     = 0;
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
        exit_hooks();
        return;
    }
    vm->failed = true;
    // CPython dies of the SIGINT itself, which a shell reports as 130.
    vm->status = is_exc(e) && exc_is(exc_type_of(e), exc_find("KeyboardInterrupt")) ? 130 : 1;
    report(e);
    exit_hooks();
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
    if (c == CATCH_ANY)
        return true; // a finalizer: whatever it raises stops here
    if (c == CATCH_EXIT || c == CATCH_SEQEND)
        return exc_is(exc_type_of(e), exc_find("StopIteration")) ||
               exc_is(exc_type_of(e), exc_find(c == CATCH_EXIT ? "GeneratorExit" : "IndexError"));
    Str want = c == CATCH_STOP    ? Str("StopIteration")
               : c == CATCH_ASTOP ? Str("StopAsyncIteration")
                                  : Str("AttributeError");
    return exc_is(exc_type_of(e), exc_find(want));
}

// The continuation in the chain from `kv` that catches `e`, or Nil. Each one
// it passes is abandoned, and told so: its call failed, so the builtin waiting
// on it failed too.
Value cont_catcher(Value kv, Value e)
{
    // A fail hook may allocate, and the chain may be held by nothing else.
    Root re{ e }, rk{ kv };
    for (; !rk.v.is_nil(); rk = cont_of(rk.v)->next) {
        if (cont_catches(rk.v, re.v))
            return rk.v;
        ContObj *c = cont_of(rk.v);
        if (c->fail) {
            c->caught = re.v;
            c->fail(c);
            cont_of(rk.v)->fail = nullptr;
        }
    }
    return Value();
}

// A continuation that caught an exception has run and failed in its turn. The
// new exception belongs to the frame now on top.
bool rethrow_pending()
{
    vm->tb.clear();
    return raise_value(pending_exception());
}

// `here` is false for a re-raise, whose frame the traceback already has.
bool dispatch(Value e, bool here = true)
{
    Root re{ e };
    err_clear();
    for (;;) {
        FrameObj *f = frame_of(vm->frame);
        if (here && !tb_here(f, re.v))
            return uncaught(re.v), false;
        here = true;
        if (f->nb) {
            Block b = f->blocks()[--f->nb];
            f->sp   = b.sp;
            f->pc   = b.handler;
            return push(f, re.v);
        }
        if (!note_frame(f))
            return uncaught(re.v), false;
        // A generator is over once something unwinds out of it. PEP 479 turns
        // an escaping StopIteration into a RuntimeError, so that the two ways
        // a generator can end are not the same exception.
        // An async generator's end is StopAsyncIteration, so that one is
        // turned into a RuntimeError there too.
        if (!f->gen.is_nil()) {
            Root gv{ f->gen };
            GenObj *g        = gen_of(gv.v);
            g->state         = GEN_DONE;
            g->frame         = Value();
            f->gen           = Value();
            const ExcType *t = exc_type_of(re.v);
            bool stop        = t && exc_is(t, exc_find("StopIteration"));
            bool astop       = t && is_agen(gv.v) && exc_is(t, exc_find("StopAsyncIteration"));
            if (stop || astop) {
                Buf<64> m;
                m.put(gen_kind(gv.v)).put(" raised ");
                m.put(stop ? Str("StopIteration") : Str("StopAsyncIteration"));
                Value sub = exc_make("RuntimeError", m.str());
                if (!sub.is_nil()) {
                    static_cast<ExcObj *>(sub.obj())->context = re.v;
                    re                                        = sub;
                }
            }
        }
        if (f->back.is_nil() && f->cont.is_nil()) {
            uncaught(re.v);
            return false;
        }
        Value kv     = f->cont;
        vm->handling = f->handling;
        vm->frame    = f->back;
        vm->depth--;
        // Abandoned rather than caught, each continuation gets to tidy up.
        Root c{ cont_catcher(kv, re.v) };
        if (!c.v.is_nil()) {
            cont_of(c.v)->caught = re.v;
            vm->tb.clear();
            return run_cont(c.v, Value()) || rethrow_pending();
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

// -------------------------------------------------------------- generators

// What `gen.throw` names. One argument is passed on as it stands. A
// delegating generator hands it to the sub-iterator's own throw, and only a
// raise decides whether it is an exception at all. Two arguments are the older
// form, and an instance is the only thing that can carry both.
Value throw_value(const CallArgs &a)
{
    if (!a.nargs)
        return err_set("TypeError", "throw() takes at least one argument"), Value();
    Root t{ a.args[0] };
    if (a.nargs < 2 || is_none(a.args[1]))
        return t.v;
    if (is_exc(t.v))
        return err_set("TypeError", "instance exception may not have a separate value"), Value();
    if (!is_exc_type(t.v))
        return err_set2("TypeError", "exceptions must derive from BaseException", type_name(t.v)),
               Value();
    // A value that is already an instance of the type is the exception.
    if (is_exc(a.args[1]) && type_isinstance(a.args[1], t.v))
        return a.args[1];
    Root args;
    if (is_tuple(a.args[1])) {
        args = a.args[1];
    } else {
        TupleObj *one = tuple_new(1);
        if (!one)
            return oom(), Value();
        one->items()[0] = a.args[1];
        args            = obj_value(one);
    }
    return exc_inst(t.v, args.v);
}

// Whether a throw was handed a GeneratorExit, as an instance or as the type.
// A delegation treats that one differently.
bool is_generator_exit(Value v)
{
    const ExcType *t = is_exc(v) ? exc_type_of(v) : is_exc_type(v) ? type_obj(v)->exc : nullptr;
    return t && exc_is(t, exc_find("GeneratorExit"));
}

// Leave a generator at a yield. The frame is parked rather than popped, so it
// keeps its value stack, its block stack and its locals. `at` is where it
// resumes. That is past the yield, or the `yield from` itself, which re-enters
// the delegation.
void gen_park(FrameObj *f, u32 at)
{
    GenObj *g    = gen_of(f->gen);
    g->state     = GEN_SUSPENDED;
    g->handling  = vm->handling;
    vm->handling = f->handling;
    f->pc        = at;
    vm->frame    = f->back;
    f->back      = Value();
    vm->depth--;
}

// The generator ran to its end. The frame retires and StopIteration carries
// what it returned, which is what the language says and what every CATCH_STOP
// continuation waits for. The frame is retired here rather than by dispatch,
// so that a normal end leaves no traceback line.
bool gen_finish(FrameObj *f, Value v)
{
    Root rv{ v };
    GenObj *g = gen_of(f->gen);
    g->state  = GEN_DONE;
    g->frame  = Value();
    Root k{ f->cont };
    f->cont      = Value();
    f->gen       = Value();
    vm->handling = f->handling;
    vm->frame    = f->back;
    f->back      = Value();
    vm->depth--;

    Root args;
    if (!is_none(rv.v)) {
        TupleObj *t = tuple_new(1);
        if (!t)
            return oom(), dispatch(pending_exception());
        t->items()[0] = rv.v;
        args          = obj_value(t);
    }
    Root e{ exc_new(exc_find("StopIteration"), args.v) };
    if (e.v.is_nil())
        return dispatch(pending_exception());
    vm->tb.clear();
    Root c{ cont_catcher(k.v, e.v) };
    if (!c.v.is_nil()) {
        cont_of(c.v)->caught = e.v;
        return run_cont(c.v, Value()) || rethrow_pending();
    }
    return dispatch(e.v);
}

// One turn of a `yield from`. What is sent or thrown into this generator goes
// to the sub-iterator. What comes back is yielded onward, or, once the
// sub-iterator stops, is the value of the yield-from expression.
//
// s[0] is the sub-iterator, s[1] the callable that reaches it, s[2] its
// argument. j is the YieldFrom instruction to come back to.
R yf_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        if (!k->s[1].is_nil())
            return cont_call(k, k->s[1], k->s[2], k->s[2].is_nil() ? 0 : 1);
        // A built-in iterator, which the native protocol answers at once.
        Value got;
        R r = py_next(k->s[0], got);
        if (r == R::Err)
            return R::Err;
        in = r == R::NotImpl ? Value() : got;
    }

    FrameObj *f = frame_of(vm->frame);
    if (!in.is_nil()) {
        // It yielded, so the delegating generator yields too. It comes back
        // to the same instruction with whatever is sent next.
        gen_park(f, k->j);
        k->next = f->cont;
        f->cont = Value();
        k->drop = false;
        return cont_done(k, in);
    }
    // It stopped. Its return value is what the yield-from expression is
    // worth, and it replaces the sub-iterator GetIter left on the stack.
    Value v = value_none();
    if (is_exc(k->caught)) {
        TupleObj *args =
            static_cast<TupleObj *>(static_cast<ExcObj *>(k->caught.obj())->args.obj());
        if (args && args->len)
            v = args->items()[0];
    }
    f->stack()[f->sp - 1] = v;
    k->drop               = true;
    return cont_done(k, value_none());
}

// Start a delegation. `at` is the YieldFrom instruction, `how` what to do to
// the sub-iterator, `arg` what to hand it. False leaves an error pending.
bool yf_start(FrameObj *f, u32 at, u8 how, Value arg)
{
    Root ra{ arg };
    Root rs{ f->stack()[f->sp - 1] };
    Root call{ resumer(rs.v, how) };
    if (err_pending())
        return false;
    if (call.v.is_nil()) {
        // A duck type, with a send and a throw of its own.
        bool bare = how == GR_NEXT || (how == GR_SEND && is_none(ra.v));
        if (how == GR_THROW)
            call = type_special(rs.v, "throw");
        else if (bare)
            call = type_special(rs.v, "__next__");
        else
            call = type_special(rs.v, "send");
        if (err_pending())
            return false;
        if (call.v.is_nil()) {
            // Nothing to delegate to, so the exception belongs at this yield.
            if (how == GR_THROW)
                return f->pc = at + 1, err_set_value(ra.v), false;
            if (!bare)
                return err_set2("AttributeError", "object has no attribute 'send'",
                                type_name(rs.v)),
                       false;
        }
        if (bare)
            ra = Value();
    }

    f->pc = at + 1;
    Root kv{ cont_new(yf_step) };
    if (kv.v.is_nil())
        return false;
    ContObj *k  = cont_of(kv.v);
    k->s[0]     = rs.v;
    k->s[1]     = call.v;
    k->s[2]     = how == GR_NEXT ? Value() : ra.v;
    k->j        = at;
    k->catching = CATCH_STOP;
    k->drop     = true;
    return run_cont(kv.v, Value());
}

// The sub-iterator has been closed. The exception now belongs at the
// yield-from itself, which the loop raises at its next turn. s[1] is the
// exception, j the instruction.
R yf_exit_step(ContObj *k, Value in)
{
    (void)in;
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    frame_of(vm->frame)->pc = k->j + 1;
    vm->thrown              = k->s[1];
    return cont_done(k, value_none());
}

// A GeneratorExit thrown into a delegating generator is not thrown through
// it. The sub-iterator is closed, and only then does the exception reach the
// yield-from. That is what lets the delegating generator's own `except
// GeneratorExit` run, and what keeps close() off a sub-iterator's throw().
bool yf_exit(FrameObj *f, u32 at, Value exc)
{
    Root re{ exc };
    Root rs{ f->stack()[f->sp - 1] };
    Root shut{ resumer(rs.v, GR_CLOSE) };
    if (shut.v.is_nil() && !err_pending())
        shut = type_special(rs.v, "close");
    if (err_pending())
        return false;
    if (shut.v.is_nil()) {
        f->pc      = at + 1;
        vm->thrown = re.v;
        return true;
    }
    Root kv{ cont_new(yf_exit_step) };
    if (kv.v.is_nil())
        return false;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = shut.v;
    k->s[1]    = re.v;
    k->j       = at;
    k->drop    = true;
    return run_cont(kv.v, Value());
}

// One turn of close(). GeneratorExit goes in at the yield, and the generator
// must not answer with a value. s[0] is the bound throw, s[1] what it throws.
R close_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->s[1]);
    if (in.is_nil()) // it stopped, which is all close asks for
        return cont_done(k, value_none());
    Buf<64> m;
    m.put(gen_kind(genrun_of(k->s[0])->gen)).put(" ignored GeneratorExit");
    return err_set("RuntimeError", m.str());
}

R gen_close(Value gv, Value &out)
{
    Root rg{ gv };
    Root e{ exc_new(exc_find("GeneratorExit"), Value()) };
    if (e.v.is_nil())
        return R::Err;
    Root m{ genrun_new(rg.v, GR_THROW) };
    if (m.v.is_nil())
        return R::Err;
    Root kv{ cont_new(close_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k  = cont_of(kv.v);
    k->s[0]     = m.v;
    k->s[1]     = e.v;
    k->catching = CATCH_EXIT;
    out         = kv.v;
    return R::Ok;
}

// The generator is over and none of its body ran. This is close or throw on
// one that has not started.
void gen_retire(GenObj *g)
{
    g->state = GEN_DONE;
    if (!g->frame.is_nil())
        frame_of(g->frame)->gen = Value();
    g->frame = Value();
}

// One turn of the drain drain_operand sets up. s[0] is the bound __next__,
// s[1] the list being filled, s[2] the operand. j is how far down the stack
// the operand sits.
R drain_operand_step(ContObj *k, Value in)
{
    // s[0] is Nil for a class instance, whose __iter__ has to run first.
    if (k->i++ == 0) {
        if (!k->s[0].is_nil())
            return cont_call(k, k->s[0], Value(), 0);
        Value it = iter_special(k->s[2]);
        if (it.is_nil())
            return not_iterable(k->s[2]);
        return cont_call(k, it, Value(), 0);
    }
    if (k->s[0].is_nil()) {
        Root rit{ in };
        k->s[0] = next_special(rit.v);
        if (k->s[0].is_nil()) {
            // __iter__ answered with a built-in iterator, which walks itself.
            ListObj *xs = py_list_of(rit.v);
            if (!xs)
                return R::Err;
            k->s[1] = obj_value(xs);
            in      = Value();
        } else {
            return cont_call(k, k->s[0], Value(), 0);
        }
    }
    if (!in.is_nil()) {
        if (!list_push(list_of(k->s[1]), in))
            return oom();
        return cont_call(k, k->s[0], Value(), 0);
    }
    FrameObj *f                  = frame_of(vm->frame);
    f->stack()[f->sp - 1 - k->j] = k->s[1];
    return cont_done(k, value_none());
}

// An instruction whose operand is a generator cannot step it, so it parks the
// way a builtin does. The generator is drained into a list, the list is put
// where the generator was, and the instruction runs again. `below` is how far
// under the top of the stack the operand sits.
bool drain_operand(FrameObj *f, u32 below)
{
    Root src{ f->stack()[f->sp - 1 - below] };
    Root m{ is_resumable(src.v) ? genrun_new(src.v, GR_NEXT) : Value() };
    if (is_resumable(src.v) && m.v.is_nil())
        return false;
    ListObj *xs = list_new();
    if (!xs)
        return oom(), false;
    Root rl{ obj_value(xs) };
    Root kv{ cont_new(drain_operand_step) };
    if (kv.v.is_nil())
        return false;
    ContObj *k  = cont_of(kv.v);
    k->s[0]     = m.v;
    k->s[1]     = rl.v;
    k->s[2]     = src.v;
    k->j        = below;
    k->catching = CATCH_STOP;
    k->drop     = true;
    f->pc--; // the instruction is entered again, over the list
    return run_cont(kv.v, Value());
}

// Push a parked frame back on the chain. What the generator yields or returns
// arrives the way a call's answer does. It goes through the frame's cont, or
// onto the stack of the frame below. So nothing here need know who resumed it.
R gen_resume(Value gv, u8 how, const CallArgs &a, Value &out, bool &entered)
{
    entered = false;
    Root rg{ gv };
    GenObj *g = gen_of(rg.v);

    if (how == GR_ITER) {
        if (!args_only(a, "__iter__", 0, 0))
            return R::Err;
        out = rg.v;
        return R::Ok;
    }
    if (a.nkw)
        return err_set("TypeError", "a generator method takes no keyword arguments");
    if (how == GR_NEXT && !args_only(a, "__next__", 0, 0))
        return R::Err;
    if (how == GR_SEND && !args_only(a, "send", 1, 1))
        return R::Err;
    if (how == GR_CLOSE && !args_only(a, "close", 0, 0))
        return R::Err;
    if (g->state == GEN_RUNNING) {
        Buf<64> m;
        m.put(gen_kind(rg.v)).put(" already executing");
        return err_set("ValueError", m.str());
    }

    Root arg{ how == GR_SEND ? a.args[0] : how == GR_NEXT ? value_none() : Value() };
    if (how == GR_THROW) {
        arg = throw_value(a);
        if (arg.v.is_nil())
            return R::Err;
    }

    // A coroutine is awaited once. An async generator's end is its own.
    if (g->state == GEN_DONE || g->frame.is_nil()) {
        if (how == GR_CLOSE) {
            out = value_none();
            return R::Ok;
        }
        if (is_coro(rg.v))
            return err_set("RuntimeError", "cannot reuse already awaited coroutine");
        if (how == GR_THROW)
            return err_set_value(arg.v);
        return err_set(is_agen(rg.v) ? Str("StopAsyncIteration") : Str("StopIteration"), "");
    }

    // Nothing has run yet, so there is no handler in the frame to reach. Both
    // of these end the generator where they stand, as CPython does.
    if (g->state == GEN_CREATED && how == GR_CLOSE) {
        gen_retire(g);
        out = value_none();
        return R::Ok;
    }
    if (g->state == GEN_CREATED && how == GR_THROW) {
        gen_retire(g);
        return err_set_value(arg.v);
    }
    if (how == GR_CLOSE)
        return gen_close(rg.v, out);
    if (how == GR_SEND && g->state == GEN_CREATED && !is_none(arg.v)) {
        Buf<96> m;
        m.put("can't send non-None value to a just-started ").put(gen_kind(rg.v));
        return err_set("TypeError", m.str());
    }

    if (vm->depth >= max_frames)
        return err_set("RecursionError", "maximum recursion depth exceeded");

    FrameObj *f  = frame_of(g->frame);
    bool started = g->state == GEN_SUSPENDED;
    f->back      = vm->frame;
    f->handling  = vm->handling;
    vm->handling = g->handling;
    g->handling  = Value();
    vm->frame    = g->frame;
    vm->depth++;
    g->state = GEN_RUNNING;
    entered  = true;

    if (!started) // the body begins at the top, with nothing to hand it
        return R::Ok;

    // The loop raises it at the resume point, or delegates it. Either way it
    // has to wait until the frame's continuation is recorded, which the caller
    // has not done yet.
    if (how == GR_THROW) {
        vm->thrown = arg.v;
        return R::Ok;
    }
    // The value sent is what the `yield` expression is worth. Parked in a
    // `yield from`, that instruction runs again and passes it on.
    return push(f, arg.v) ? R::Ok : R::Err;
}

// ------------------------------------------------- awaitables and async generators

// Something whose stepping is __next__, whoever answers it.
bool is_iterator(Value v)
{
    return is_resumable(v) || type_of(v)->next || type_has_special(v, "__next__");
}

// The TypeError for a value that cannot be awaited, by what asked.
R cant_await(Value v, u32 from)
{
    Buf<160> m;
    switch (from) {
    case AW_AENTER:
    case AW_AEXIT:
        m.put("'async with' received an object from ");
        m.put(from == AW_AENTER ? Str("__aenter__") : Str("__aexit__"));
        m.put(" that does not implement __await__: ").put(type_name(v));
        break;
    case AW_ANEXT:
        m.put("'async for' received an invalid object from __anext__: ").put(type_name(v));
        break;
    default:
        m.put("'").put(type_name(v)).put("' object can't be awaited");
        break;
    }
    return err_set("TypeError", m.str());
}

// What __await__ answered has to be an iterator, and not another coroutine.
bool await_iter_ok(Value v)
{
    if (is_coro(v) || gen_awaitable(v))
        return err_set("TypeError", "__await__() returned a coroutine"), false;
    if (!is_iterator(v)) {
        Buf<96> m;
        m.put("__await__() returned non-iterator of type '").put(type_name(v)).put("'");
        return err_set("TypeError", m.str()), false;
    }
    return true;
}

R raise_stop(Value v)
{
    Root rv{ v }, args;
    if (!rv.v.is_nil() && !is_none(rv.v)) {
        TupleObj *t = tuple_new(1);
        if (!t)
            return oom();
        t->items()[0] = rv.v;
        args          = obj_value(t);
    }
    Value e = exc_new(exc_find("StopIteration"), args.v);
    return e.is_nil() ? R::Err : err_set_value(e);
}

// The arguments of a throw(), as a tuple a continuation can keep.
Value args_tuple(const CallArgs &a)
{
    TupleObj *t = tuple_new(a.nargs);
    if (!t)
        return oom(), Value();
    for (u32 i = 0; i < a.nargs; i++)
        t->items()[i] = a.args[i];
    return obj_value(t);
}

bool is_exc_named(Value e, Str name)
{
    const ExcType *t = exc_type_of(e);
    return t && exc_is(t, exc_find(name));
}

// An exception came out of the generator while an awaitable was stepping it.
// s[0] is the AwaitObj.
void agen_abandoned(ContObj *k)
{
    AwaitObj *a = await_of(k->s[0]);
    GenObj *g   = gen_of(a->target);
    g->running  = false;
    a->state    = AS_CLOSED;
    if (is_exc_named(k->caught, "StopAsyncIteration") || is_exc_named(k->caught, "GeneratorExit"))
        g->closed = true;
}

// One step of asend(), athrow() or aclose(). s[0] is the AwaitObj, s[1] what
// the step was handed -- a value, or a tuple of throw() arguments -- and j how
// it was asked. The first turn checks the state and resumes the generator;
// the second looks at what came back.
R agen_step(ContObj *k, Value in)
{
    AwaitObj *a = await_of(k->s[0]);
    GenObj *g   = gen_of(a->target);
    bool acl    = a->kind == AK_ACLOSE;
    u8 how      = u8(k->j);

    if (k->i++ == 0) {
        Str who = a->kind == AK_ASEND ? Str("__anext__()/asend()") : Str("aclose()/athrow()");
        if (a->state == AS_CLOSED) {
            if (how == GR_CLOSE)
                return cont_done(k, value_none());
            Buf<96> m;
            m.put("cannot reuse already awaited ").put(who);
            return err_set("RuntimeError", m.str());
        }
        bool done = g->state == GEN_DONE || g->frame.is_nil();
        if (a->kind != AK_ASEND && done) {
            a->state = AS_CLOSED;
            return how == GR_CLOSE ? cont_done(k, value_none()) : raise_stop(Value());
        }
        if (how == GR_CLOSE && done) {
            a->state = AS_CLOSED;
            return cont_done(k, value_none());
        }
        Root arg{ k->s[1] };
        if (a->state == AS_INIT) {
            if (g->running) {
                a->state = AS_CLOSED;
                Buf<96> m;
                m.put(a->kind == AK_ASEND ? Str("anext") : acl ? Str("aclose") : Str("athrow"));
                m.put("(): asynchronous generator is already running");
                return err_set("RuntimeError", m.str());
            }
            if (a->kind != AK_ASEND) {
                if (g->closed) {
                    a->state = AS_CLOSED;
                    return err_set("StopAsyncIteration", "");
                }
                if (how == GR_SEND && !is_none(arg.v))
                    return err_set("RuntimeError",
                                   "can't send non-None value to a just-started coroutine");
            } else if (how == GR_SEND && is_none(arg.v)) {
                arg = a->arg;
            }
            a->state = AS_ITER;
            // The first turn of athrow() and aclose() is the throw itself.
            if (a->kind != AK_ASEND && how == GR_SEND) {
                how = GR_THROW;
                if (acl) {
                    g->closed = true;
                    arg       = exc_new(exc_find("GeneratorExit"), Value());
                    if (arg.v.is_nil())
                        return R::Err;
                } else {
                    arg = a->arg;
                }
            }
        }
        g->running = true;
        k->fail    = agen_abandoned;
        k->j       = how;
        // aclose() is done when the generator stops or lets GeneratorExit out;
        // everything else is done when it returns, which is StopIteration.
        k->catching = acl || how == GR_CLOSE ? CATCH_EXIT : CATCH_STOP;
        if (how == GR_CLOSE) {
            Value e = exc_new(exc_find("GeneratorExit"), Value());
            if (e.is_nil())
                return R::Err;
            arg = e;
            how = GR_THROW;
        }
        Root call{ genrun_new(a->target, how == GR_THROW ? GR_THROW : GR_SEND) };
        if (call.v.is_nil())
            return R::Err;
        if (how == GR_THROW) {
            if (is_tuple(arg.v))
                return cont_call_v(k, call.v, arg.v);
            return cont_call(k, call.v, arg.v);
        }
        return cont_call(k, call.v, arg.v);
    }

    k->fail  = nullptr;
    bool val = !in.is_nil() && !is_wrapval(in);
    // A plain value is one an await in the body passed up: it goes on up, and
    // the generator is still running. Only close() refuses one.
    if (val && how != GR_CLOSE)
        return cont_done(k, in);
    g->running = false;
    a->state   = AS_CLOSED;
    if (how == GR_CLOSE) {
        if (val)
            return err_set("RuntimeError", "coroutine ignored GeneratorExit");
        if (is_wrapval(in) && acl)
            return err_set("RuntimeError", "async generator ignored GeneratorExit");
        return cont_done(k, value_none());
    }
    if (acl)
        return is_wrapval(in) ? err_set("RuntimeError", "async generator ignored GeneratorExit")
                              : raise_stop(Value());
    if (is_wrapval(in))
        return raise_stop(static_cast<WrapValObj *>(in.obj())->v);
    // It returned: an async generator's end.
    g->closed = true;
    return err_set("StopAsyncIteration", "");
}

// anext(it, default): the awaitable it wraps, stepped through its iterator,
// with StopAsyncIteration turned into the default. s[0] is the AwaitObj, s[1]
// what this step was handed, j how.
R anext_step(ContObj *k, Value in)
{
    AwaitObj *a = await_of(k->s[0]);
    u8 how      = u8(k->j);
    if (k->i == 2) {
        // Only StopAsyncIteration is caught, and the default is its answer.
        if (in.is_nil())
            return raise_stop(a->dflt);
        return cont_done(k, in);
    }
    if (k->i == 1) {
        if (!await_iter_ok(in))
            return R::Err;
        a->iter = in;
    } else if (a->iter.is_nil()) {
        Value t = a->target;
        if (is_coro(t) || gen_awaitable(t) || is_corowrap(t) || is_awaitobj(t)) {
            a->iter = t;
        } else {
            Value m = type_special(t, "__await__");
            if (m.is_nil())
                return err_pending() ? R::Err : cant_await(t, AW_AWAIT);
            k->i = 1;
            return cont_call(k, m, Value(), 0);
        }
    }

    Root it{ a->iter };
    Root call{ resumer(it.v, how) };
    if (call.v.is_nil()) {
        if (err_pending())
            return R::Err;
        // A duck type, which may have no send of its own.
        if (how == GR_SEND && is_none(k->s[1]))
            how = GR_NEXT;
        Str name = how == GR_THROW   ? Str("throw")
                   : how == GR_CLOSE ? Str("close")
                   : how == GR_SEND  ? Str("send")
                                     : Str("__next__");
        call     = type_special(it.v, name);
        if (call.v.is_nil()) {
            if (how == GR_CLOSE)
                return cont_done(k, value_none());
            if (!err_pending())
                err_set2("AttributeError", "object has no attribute", name);
            return R::Err;
        }
    }
    k->i        = 2;
    k->catching = CATCH_ASTOP;
    if (how == GR_THROW)
        return cont_call_v(k, call.v, k->s[1]);
    if (how == GR_CLOSE || how == GR_NEXT)
        return cont_call(k, call.v, Value(), 0);
    return cont_call(k, call.v, k->s[1]);
}

// A resume of a coroutine's wrapper or of an awaitable: what `send`,
// `__next__`, `throw` and `close` on one of those do.
R await_resume(Value aw, u8 how, const CallArgs &a, Value &out)
{
    Root ra{ aw };
    if (a.nkw)
        return err_set("TypeError", "an awaitable's method takes no keyword arguments");
    Root arg;
    switch (how) {
    case GR_NEXT:
        if (!args_only(a, "__next__", 0, 0))
            return R::Err;
        arg = value_none();
        how = GR_SEND;
        break;
    case GR_SEND:
        if (!args_only(a, "send", 1, 1))
            return R::Err;
        arg = a.args[0];
        break;
    case GR_THROW:
        if (!args_only(a, "throw", 1, 3))
            return R::Err;
        arg = args_tuple(a);
        if (arg.v.is_nil())
            return R::Err;
        break;
    default:
        if (!args_only(a, "close", 0, 0))
            return R::Err;
        break;
    }
    Root kv{ cont_new(await_of(ra.v)->kind == AK_DEFAULT ? anext_step : agen_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = ra.v;
    k->s[1]    = arg.v;
    k->j       = how;
    out        = kv.v;
    return R::Ok;
}

// What calling a GenRunObj does, by what it is bound to.
R run_resume(Value run, const CallArgs &a, Value &out, bool &entered)
{
    entered = false;
    Root t{ genrun_of(run)->gen };
    u8 how = genrun_of(run)->how;
    if (how == GR_ITER || how == GR_AWAIT) {
        if (!args_only(a, how == GR_ITER ? Str("__iter__") : Str("__await__"), 0, 0))
            return R::Err;
        out = how == GR_AWAIT && is_coro(t.v) ? corowrap_new(t.v) : t.v;
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_corowrap(t.v))
        return gen_resume(static_cast<CoroWrapObj *>(t.v.obj())->coro, how, a, out, entered);
    if (is_awaitobj(t.v))
        return await_resume(t.v, how, a, out);
    return gen_resume(t.v, how, a, out, entered);
}

// GetIter: a class's __iter__ is a call, anything else answers at once.
bool get_iter(FrameObj *f)
{
    Value *st = f->stack();
    Value m   = iter_special(st[f->sp - 1]);
    if (!m.is_nil())
        return run_special(f, m, nullptr, 0, 1);
    if (err_pending())
        return false;
    Value it = py_iter(st[f->sp - 1]);
    if (it.is_nil())
        return false;
    st[f->sp - 1] = it;
    return true;
}

// What a special method's answer is checked for before it is used.
enum : u32 { CK_AWAIT, CK_AITER };

// s[0] the bound method, j the check.
R checked_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    if (k->j == CK_AWAIT) {
        if (!await_iter_ok(in))
            return R::Err;
    } else if (!is_agen(in) && !type_has_special(in, "__anext__")) {
        Buf<128> b;
        b.put("'async for' received an object from __aiter__ that does not implement __anext__: ");
        return err_set("TypeError", b.put(type_name(in)).str());
    }
    return cont_done(k, in);
}

// Call `m` with no arguments and put its checked answer in place of the value
// on top.
bool run_checked(FrameObj *f, Value m, u32 check)
{
    Root rm{ m };
    Root kv{ cont_new(checked_step) };
    if (kv.v.is_nil())
        return false;
    cont_of(kv.v)->s[0] = rm.v;
    cont_of(kv.v)->j    = check;
    f->sp--;
    return run_cont(kv.v, Value());
}

// ------------------------------------------------------------------ except*

// [rest, match] from the pair (match, rest), and match is what is handled.
bool eg_place(FrameObj *f, Value pair)
{
    TupleObj *t = static_cast<TupleObj *>(pair.obj());
    Value match = t->items()[0];
    if (!push(f, t->items()[1]) || !push(f, match))
        return false;
    if (!is_none(match))
        vm->handling = match;
    return true;
}

// A continuation made callable: calling this hands it back, and run_cont
// drives it with the caller waiting on the answer.
R eg_thunk(const CallArgs &a, Value &out)
{
    out = a.args[0];
    return R::Ok;
}

// s[0] the split's answer arrives here.
R eg_match_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    return eg_place(frame_of(vm->frame), in) ? cont_done(k, value_none()) : R::Err;
}

// s[0][s[1]] = list(s[2]), with s[3] the list type.
R slice_store_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[3], k->s[2]);
    if (py_setitem(k->s[0], k->s[1], in) != R::Ok)
        return R::Err;
    return cont_done(k, value_none());
}

// A lazy import read as a name: the import runs, and its answer is pushed
// and stored where the proxy was.
bool reify_into(Value lazy, Value space, Value name)
{
    Value k = lazy_reify(lazy, space, name);
    return !k.is_nil() && run_cont(k, Value());
}

// -------------------------------------------------------------- the loop

void interpret()
{
    vm->budget = BURST;
    for (;;) {
        if (vm->finished || !vm->reading.is_nil() || vm->out.size() >= FLUSH_AT ||
            vm->err.size() >= FLUSH_AT || !vm->budget--)
            return;
        // The last atexit call has returned and there is no frame left --
        // unless a step at exit is waiting on the driver's answer.
        if (vm->frame.is_nil() && vm->resume.is_nil()) {
            vm->finished = true;
            return;
        }

        // The `self` of the last bound call is a root while the call is being
        // made and not after it: leaving it there would keep an object alive
        // until the next method call, which a finalizer would then be late by.
        vm->bound.clear();

        // The file an import asked for has arrived. Hand it to the step that
        // parked, as bytes -- how they read is the source's to say -- or None
        // when there was no such file.
        if (!vm->resume.is_nil()) {
            Root k{ vm->resume };
            vm->resume = Value();
            Root text{ !vm->found  ? value_none()
                       : vm->isdir ? value_bool(true)
                                   : bytes_new(vm->text.str()) };
            if (text.v.is_nil() || !run_cont(k.v, text.v)) {
                vm->tb.clear();
                if (!raise_value(pending_exception()))
                    return;
            }
            continue;
        }

        // A generator was resumed with gen.throw. The frame is back on the
        // chain and its continuation is recorded, so this is where the
        // exception reaches it.
        if (!vm->thrown.is_nil()) {
            Root e{ vm->thrown };
            vm->thrown  = Value();
            FrameObj *g = frame_of(vm->frame);
            CodeObj *gc = code_of(g->code);
            // Parked in a `yield from`, so the sub-iterator is thrown into
            // first. That is what delegation means. A GeneratorExit is the one
            // that is not thrown through. It closes the sub-iterator instead.
            if (!g->gen.is_nil() && g->pc < gc->code.size() &&
                gc->code[g->pc].op == Bc::YieldFrom &&
                (is_generator_exit(e.v) ? yf_exit(g, g->pc, e.v)
                                        : yf_start(g, g->pc, GR_THROW, e.v)))
                continue;
            vm->tb.clear();
            if (!raise_value(err_pending() ? pending_exception() : e.v))
                return;
            continue;
        }

        // A finalizer the collector owes: a __del__, or a weak reference's
        // callback. Both are Python and the sweep is not, so they are made
        // here, between two opcodes, where a frame can be pushed. An exception
        // out of one is reported and goes no further, which is what CPython
        // means by ignoring it.
        // One at a time: the next waits for this one's frame to finish, or a
        // sweep that owes many would nest them all.
        if (gc_owes() && !vm->finalizing) {
            Root kv{ owed_next() };
            if (kv.v.is_nil()) {
                err_clear();
                continue;
            }
            cont_of(kv.v)->drop = true;
            cont_of(kv.v)->j    = 1;
            vm->finalizing++;
            if (!run_cont(kv.v, Value())) {
                if (cont_of(kv.v)->j) {
                    cont_of(kv.v)->j = 0;
                    vm->finalizing--;
                }
                report(pending_exception());
                err_clear();
                vm->tb.clear();
            }
            continue;
        }

        // A ^C the driver noticed between bursts, delivered here, which is the
        // only place the stack is in a state an exception can unwind from.
        if (vm->interrupt || vm->signals) {
            u32 sig = 2;
            if (vm->interrupt) {
                vm->interrupt = false;
            } else {
                while (!(vm->signals & (1u << sig)))
                    sig = (sig + 1) % 32;
                vm->signals &= ~(1u << sig);
            }
            Root h;
            if (sig_handler(sig, h.v) == R::Ok) {
                if (h.v.is_nil())
                    continue;
                // The program's own handler, called where the program was;
                // what it raises is raised there.
                Root kv{ cont_new(sig_step) };
                if (kv.v.is_nil())
                    goto oops_between;
                cont_of(kv.v)->s[0] = h.v;
                cont_of(kv.v)->j    = sig;
                cont_of(kv.v)->drop = true;
                if (run_cont(kv.v, Value()))
                    continue;
            }
        oops_between:
            vm->tb.clear();
            vm->between = frame_of(vm->frame);
            bool go     = raise_value(pending_exception());
            vm->between = nullptr;
            if (!go)
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
                if (!f->locals.is_nil() && !is_dict(f->locals) &&
                    type_has_py_special(f->locals, "__getitem__")) {
                    if (!load_name_python(f, co->names[arg]))
                        goto oops;
                    break;
                }
                Value out;
                R r = lookup(f, n, out);
                if (r == R::Err)
                    goto oops;
                if (r == R::NotImpl) {
                    name_error("NameError", n);
                    goto oops;
                }
                if (is_lazy(out)) {
                    if (!reify_into(out, f->globals, co->names[arg]))
                        goto oops;
                    break;
                }
                if (!push(f, out))
                    goto oops;
                break;
            }
            case Bc::StoreName:
                if (!is_dict(f->locals)) {
                    // A namespace __prepare__ made: its own __setitem__.
                    Value two[2] = { co->names[arg], st[f->sp - 1] };
                    if (type_has_py_special(f->locals, "__setitem__")) {
                        Value m = type_special(f->locals, "__setitem__");
                        if (m.is_nil() || !run_special(f, m, two, 2, 1, SP_DROP))
                            goto oops;
                        break;
                    }
                    if (py_setitem(f->locals, two[0], two[1]) != R::Ok)
                        goto oops;
                    f->sp--;
                    break;
                }
                if (dict_set(dict_at(f->locals), co->names[arg], st[f->sp - 1]) != R::Ok)
                    goto oops;
                f->sp--;
                break;
            case Bc::DeleteName: {
                if (!is_dict(f->locals) && type_has_py_special(f->locals, "__delitem__")) {
                    Value m = type_special(f->locals, "__delitem__");
                    if (m.is_nil() || !run_special(f, m, &co->names[arg], 1, 0, SP_DROP))
                        goto oops;
                    break;
                }
                Value space = names_dict(f->locals);
                if (space.is_nil()) {
                    if (py_delitem(f->locals, co->names[arg]) != R::Ok)
                        goto oops;
                    break;
                }
                R r = dict_del(dict_at(space), co->names[arg]);
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
                R r = dict_get(globals_at(f->globals), co->names[arg], out);
                // A mapping of the program's own answers for itself; the
                // builtins are not consulted behind its back.
                if (r == R::NotImpl && !is_anydict(f->globals)) {
                    Value m = type_special(f->globals, "__missing__");
                    if (!m.is_nil()) {
                        Value key = co->names[arg];
                        if (!run_special(f, m, &key, 1, 0))
                            goto oops;
                        break;
                    }
                }
                if (r == R::NotImpl)
                    r = dict_get(dict_at(f->builtins), co->names[arg], out);
                if (r == R::Err)
                    goto oops;
                if (r == R::NotImpl) {
                    name_error("NameError", str_of(co->names[arg]));
                    goto oops;
                }
                if (is_lazy(out)) {
                    if (!reify_into(out, f->globals, co->names[arg]))
                        goto oops;
                    break;
                }
                if (!push(f, out))
                    goto oops;
                break;
            }
            case Bc::StoreGlobal:
                if (dict_set(globals_at(f->globals), co->names[arg], st[f->sp - 1]) != R::Ok)
                    goto oops;
                f->sp--;
                break;
            case Bc::DeleteGlobal: {
                R r = dict_del(globals_at(f->globals), co->names[arg]);
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
                    no_attr(st[f->sp - 1], name);
                    goto oops;
                }
                f->sp--;
                if (g == Got::Ok) {
                    if (!push(f, got))
                        goto oops;
                    break;
                }
                // A getter, a __get__, a __getattribute__: all Python, and
                // parked in a continuation the VM drives.
                if (!land(got, false))
                    goto oops;
                break;
            }
            case Bc::StoreAttr: {
                StrObj *name = str_of(co->names[arg]);
                Value fn;
                if (attr_store(st[f->sp - 1], name, st[f->sp - 2], fn) != R::Ok)
                    goto oops;
                f->sp -= 2;
                if (fn.is_nil())
                    break;
                cont_of(fn)->drop = true;
                if (!land(fn, false))
                    goto oops;
                break;
            }
            case Bc::DeleteAttr: {
                StrObj *name = str_of(co->names[arg]);
                Value fn;
                if (attr_delete(st[f->sp - 1], name, fn) != R::Ok)
                    goto oops;
                f->sp--;
                if (fn.is_nil())
                    break;
                cont_of(fn)->drop = true;
                if (!land(fn, false))
                    goto oops;
                break;
            }

            case Bc::LoadSubscr: {
                // A class is subscripted through its metaclass or its own
                // __class_getitem__; anything else through __getitem__.
                Value m = is_type(st[f->sp - 2]) ? type_getitem_of(st[f->sp - 2])
                                                 : type_special(st[f->sp - 2], "__getitem__");
                if (!m.is_nil()) {
                    if (!run_special(f, m, &st[f->sp - 1], 1, 2))
                        goto oops;
                    break;
                }
                // A sequence indexed by an object whose __index__ is Python.
                if (index_special(f)) {
                    if (err_pending())
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
                // `a[i:j] = g` where g iterates in Python: a list of it first.
                if (is_slice(st[f->sp - 1]) && iter_needs_vm(st[f->sp - 3])) {
                    Root kv{ cont_new(slice_store_step) };
                    Root lt{ type_wrap(&list_type) };
                    if (kv.v.is_nil() || lt.v.is_nil())
                        goto oops;
                    ContObj *k = cont_of(kv.v);
                    k->s[0]    = st[f->sp - 2];
                    k->s[1]    = st[f->sp - 1];
                    k->s[2]    = st[f->sp - 3];
                    k->s[3]    = lt.v;
                    k->drop    = true;
                    f->sp -= 3;
                    if (!run_cont(kv.v, Value()))
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
                if (!unproxy_operands(f, 1))
                    goto oops;
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
                if (Un(arg) == Un::Not && is_inst(a)) {
                    bool done = false;
                    if (!truth_python(f, Bc::UnaryOp, 0, done))
                        goto oops;
                    if (done)
                        break;
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
                if (!unproxy_operands(f, 2))
                    goto oops;
                Value a = st[f->sp - 2], b = st[f->sp - 1];
                // `xs += gen` is the one operator that iterates.
                if (in.op == Bc::InplaceOp && Op(arg) == Op::Add && is_list(a) &&
                    iter_needs_vm(b)) {
                    if (!drain_operand(f, 0))
                        goto oops;
                    break;
                }
                if (is_inst(a) || is_inst(b) || is_meta_inst(a) || is_meta_inst(b)) {
                    // `a += b` asks for __iadd__ first and falls back to __add__.
                    if (in.op == Bc::InplaceOp) {
                        Value m = operand_special(a, inplace_dunder(Op(arg)));
                        if (!m.is_nil()) {
                            if (!run_special(f, m, &b, 1, 2))
                                goto oops;
                            break;
                        }
                    }
                    // The left operand's own operator goes first, and the
                    // reflected one only answers what it left undone. A
                    // built-in on the left has its slot tried here rather
                    // than below, or `"%s" % obj` would reach __rmod__
                    // before str's own `%`.
                    bool settled = false;
                    if (!is_inst(a) && !is_meta_inst(a) && in.op == Bc::BinaryOp) {
                        Value got;
                        R r = py_binop_try(a, b, Op(arg), got);
                        if (r == R::Err)
                            goto oops;
                        if (r == R::Ok) {
                            f->sp -= 2;
                            if (!land(got, false))
                                goto oops;
                            settled = true;
                        }
                    }
                    if (settled)
                        break;

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
                // `%` on a str may hand back a ContObj: a value in it answers
                // __str__ or __format__ in Python.
                if (!land(out, false))
                    goto oops;
                break;
            }

            case Bc::CompareOp: {
                Cmp op  = Cmp(arg);
                bool ok = false;
                if (op != Cmp::Is && op != Cmp::IsNot && !unproxy_operands(f, 2))
                    goto oops;
                if (op == Cmp::Is || op == Cmp::IsNot) {
                    ok = (st[f->sp - 2] == st[f->sp - 1]) == (op == Cmp::Is);
                } else if (op == Cmp::In || op == Cmp::NotIn) {
                    // __contains__ first; iterating is the fallback. `not in`
                    // on a class needs the answer negated, which the ordinary
                    // path does and a call cannot.
                    Value m = type_special(st[f->sp - 1], "__contains__");
                    if (!m.is_nil()) {
                        u32 w = SP_BOOL | (op == Cmp::NotIn ? SP_NOT : 0);
                        if (!run_special(f, m, &st[f->sp - 2], 1, 2, w))
                            goto oops;
                        break;
                    }
                    if (iter_needs_vm(st[f->sp - 1])) {
                        if (!drain_operand(f, 0))
                            goto oops;
                        break;
                    }
                    // An __eq__ written in Python makes the scan a call each
                    // time, so the search owns the loop and suspends.
                    Value seq = list_or_tuple_of(st[f->sp - 1]);
                    if (!seq.is_nil() && (cmp_is_python(st[f->sp - 2], false) ||
                                          cmp_any_python(list_of(seq)->items, false))) {
                        Value c = cmp_find(seq, st[f->sp - 2], op == Cmp::In ? CMP_IN : CMP_NOTIN,
                                           0, list_of(seq)->items.size());
                        f->sp -= 2;
                        if (c.is_nil() || !land(c, false))
                            goto oops;
                        break;
                    }
                    if (py_contains(st[f->sp - 1], st[f->sp - 2], ok) != R::Ok)
                        goto oops;
                    if (op == Cmp::NotIn)
                        ok = !ok;
                } else if ((op == Cmp::Eq || op == Cmp::Ne) &&
                           cmp_same_kind(st[f->sp - 2], st[f->sp - 1]) &&
                           (cmp_is_python(st[f->sp - 2], false) ||
                            cmp_is_python(st[f->sp - 1], false))) {
                    // Two sequences of things that compare in Python: item by
                    // item, and each item is a call.
                    Value c = cmp_seq(st[f->sp - 2], st[f->sp - 1], op == Cmp::Ne);
                    f->sp -= 2;
                    if (c.is_nil() || !land(c, false))
                        goto oops;
                    break;
                } else if (is_inst(st[f->sp - 2]) || is_inst(st[f->sp - 1])) {
                    bool done = false;
                    if (!dunder_binop(f, st[f->sp - 2], st[f->sp - 1], arg | 0x100, cmp_dunder(op),
                                      done))
                        goto oops;
                    if (done)
                        break;
                    // No __ne__ of its own: __eq__, and the answer turned
                    // round, which is what object.__ne__ does.
                    if (op == Cmp::Ne) {
                        if (!dunder_binop(f, st[f->sp - 2], st[f->sp - 1], arg | 0x300,
                                          cmp_dunder(Cmp::Eq), done))
                            goto oops;
                        if (done)
                            break;
                    }
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
            case Bc::PopJumpIfTrue:
            case Bc::JumpIfFalseOrPop:
            case Bc::JumpIfTrueOrPop: {
                Value top = st[f->sp - 1];
                if (is_inst(top) || is_weakproxy(top)) {
                    bool done = false;
                    if (!truth_python(f, in.op, arg, done))
                        goto oops;
                    if (done)
                        break;
                    if (is_weakproxy(top) && proxy_target(top).is_nil())
                        goto oops;
                }
                bool yes = py_truth(top);
                bool pop = in.op == Bc::PopJumpIfFalse || in.op == Bc::PopJumpIfTrue;
                bool on  = in.op == Bc::PopJumpIfTrue || in.op == Bc::JumpIfTrueOrPop;
                if (pop)
                    f->sp--;
                if (yes == on)
                    f->pc = arg;
                else if (!pop)
                    f->sp--;
                break;
            }

            case Bc::GetIter:
                if (!get_iter(f))
                    goto oops;
                break;

            case Bc::GetYieldFromIter: {
                // A coroutine may be delegated to only by another, or by a
                // generator types.coroutine has marked.
                Value v = st[f->sp - 1];
                if (is_coro(v)) {
                    if (!(co->flags & (CO_COROUTINE | CO_ITERABLE_COROUTINE))) {
                        err_set("TypeError",
                                "cannot 'yield from' a coroutine object in a "
                                "non-coroutine generator");
                        goto oops;
                    }
                    break;
                }
                if (!is_gen(v) && !get_iter(f))
                    goto oops;
                break;
            }

            case Bc::GetAwaitable: {
                Value v = st[f->sp - 1];
                if (is_coro(v)) {
                    if (!is_none(gen_awaiting(v))) {
                        err_set("RuntimeError", "coroutine is being awaited already");
                        goto oops;
                    }
                    break;
                }
                if (gen_awaitable(v) || is_corowrap(v) || is_awaitobj(v))
                    break;
                Value m = type_special(v, "__await__");
                if (m.is_nil()) {
                    if (!err_pending())
                        cant_await(v, arg);
                    goto oops;
                }
                if (!run_checked(f, m, CK_AWAIT))
                    goto oops;
                break;
            }

            case Bc::GetAIter: {
                Value v = st[f->sp - 1];
                if (is_agen(v))
                    break;
                Value m = type_special(v, "__aiter__");
                if (m.is_nil()) {
                    if (!err_pending()) {
                        Buf<96> b;
                        b.put("'async for' requires an object with __aiter__ method, got ");
                        err_set("TypeError", b.put(type_name(v)).str());
                    }
                    goto oops;
                }
                if (!run_checked(f, m, CK_AITER))
                    goto oops;
                break;
            }

            case Bc::GetANext: {
                Value v = st[f->sp - 1];
                if (is_agen(v)) {
                    Value aw = await_new(v, AK_ASEND, value_none());
                    if (!aw.is_nil())
                        aw = agen_firstiter(v, aw);
                    if (aw.is_nil())
                        goto oops;
                    if (is_cont(aw)) {
                        if (!land(aw, false))
                            goto oops;
                        break;
                    }
                    if (!push(f, aw))
                        goto oops;
                    break;
                }
                Value m = type_special(v, "__anext__");
                if (m.is_nil()) {
                    if (!err_pending()) {
                        Buf<96> b;
                        b.put("'async for' requires an iterator with __anext__ method, got ");
                        err_set("TypeError", b.put(type_name(v)).str());
                    }
                    goto oops;
                }
                if (!run_special(f, m, nullptr, 0, 0))
                    goto oops;
                break;
            }

            case Bc::EndAsyncFor: {
                // [aiter, exc]: the loop is over, or something went wrong.
                Value exc = st[--f->sp];
                if (is_exc_named(exc, "StopAsyncIteration")) {
                    f->sp--;
                    break;
                }
                if (!dispatch(exc, false))
                    return;
                continue;
            }

            case Bc::AsyncGenWrap: {
                Value w = wrapval_new(st[f->sp - 1]);
                if (w.is_nil())
                    goto oops;
                st[f->sp - 1] = w;
                break;
            }
            case Bc::ForIter: {
                // A generator and a class with __next__ are the same problem.
                // Stepping either pushes a frame, so the item comes back
                // through a continuation rather than from py_next.
                // cont_new allocates, and nothing else points at the method.
                Root m{ next_special(st[f->sp - 1]) };
                if (m.v.is_nil() && err_pending())
                    goto oops;
                if (!m.v.is_nil()) {
                    Root kv{ cont_new(next_step) };
                    if (kv.v.is_nil())
                        goto oops;
                    cont_of(kv.v)->s[0]     = m.v;
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
            case Bc::BuildString: {
                String text;
                for (u32 k = 0; k < arg; k++) {
                    Value piece = st[f->sp - arg + k];
                    if (!is_str(piece)) {
                        err_set2("SystemError", "a string piece is not a str", type_name(piece));
                        goto oops;
                    }
                    if (!text.append(str_of(piece)->str())) {
                        oom();
                        goto oops;
                    }
                }
                Value made = str_new(text.str());
                if (made.is_nil())
                    goto oops;
                f->sp -= arg;
                st[f->sp++] = made;
                break;
            }

            case Bc::FormatValue: {
                u32 pop = (arg & FV_SPEC) ? 2 : 1;
                Str spec;
                if (arg & FV_SPEC) {
                    Value s = st[f->sp - 1];
                    if (!is_str(s)) {
                        err_set2("TypeError", "format specifier must be a str", type_name(s));
                        goto oops;
                    }
                    spec = str_of(s)->str();
                }
                Value out;
                if (format_field(st[f->sp - pop], spec, arg & FV_CONV, -1, out) != R::Ok)
                    goto oops;
                f->sp -= pop;
                // A ContObj here is a __format__ or a __repr__ written in
                // Python, which lands the answer when it returns.
                if (!land(out, false))
                    goto oops;
                break;
            }

            case Bc::BuildInterpolation: {
                u32 pop   = (arg & FV_SPEC) ? 3 : 2;
                Value got = interp_build(st[f->sp - pop], st[f->sp - pop + 1], arg & FV_CONV,
                                         (arg & FV_SPEC) ? st[f->sp - 1] : Value());
                if (got.is_nil())
                    goto oops;
                f->sp -= pop;
                st[f->sp++] = got;
                break;
            }
            case Bc::BuildTemplate: {
                Value got = template_build(st[f->sp - 2], st[f->sp - 1]);
                if (got.is_nil())
                    goto oops;
                f->sp -= 2;
                st[f->sp++] = got;
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
                if (iter_needs_vm(st[f->sp - 1])) {
                    if (!drain_operand(f, 0))
                        goto oops;
                    break;
                }
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
                if (is_frame_locals(st[f->sp - 1])) {
                    Value d = frame_locals_dict(st[f->sp - 1]);
                    if (d.is_nil())
                        goto oops;
                    st[f->sp - 1] = d;
                }
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
                if (iter_needs_vm(st[f->sp - 1])) {
                    if (!drain_operand(f, 0))
                        goto oops;
                    break;
                }
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
                        if (is_frame_locals(st[f->sp - 1])) {
                            Value d = frame_locals_dict(st[f->sp - 1]);
                            if (d.is_nil())
                                goto oops;
                            st[f->sp - 1] = d;
                        }
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
                if (arg & MF_ANNOTATE)
                    fo->annotate = st[--at];
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

            case Bc::PrintExpr: {
                Value got;
                if (py_display(st[--f->sp], got) != R::Ok)
                    goto oops;
                if (is_cont(got) && !run_cont(got, Value()))
                    goto oops;
                break;
            }

            case Bc::YieldValue: {
                if (f->gen.is_nil()) {
                    err_set("SystemError", "yield outside a generator");
                    goto oops;
                }
                Value v = st[--f->sp];
                Value k = f->cont;
                f->cont = Value();
                gen_park(f, f->pc);
                // Nothing allocates before run_cont pins `v`.
                if (!k.is_nil() ? !run_cont(k, v) : !push(frame_of(vm->frame), v))
                    goto oops;
                break;
            }

            case Bc::YieldFrom: {
                if (f->gen.is_nil()) {
                    err_set("SystemError", "yield outside a generator");
                    goto oops;
                }
                Value sent = st[--f->sp];
                if (!yf_start(f, f->pc - 1, GR_SEND, sent))
                    goto oops;
                break;
            }

            case Bc::Return: {
                Value v = st[f->sp - 1];
                if (!f->gen.is_nil()) {
                    if (!gen_finish(f, v))
                        return;
                    continue;
                }
                if (f->back.is_nil() && f->cont.is_nil()) {
                    vm->finished = true;
                    exit_hooks();
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

            case Bc::ImportName:
            case Bc::ImportNameEager:
            case Bc::LazyImportName: {
                // The compiler left [level, fromlist] here, and the loader
                // suspends, so what comes back is a continuation.
                Root nm{ co->names[arg] }, lv{ st[f->sp - 2] }, fl{ st[f->sp - 1] };
                f->sp -= 2;
                bool top  = f->locals == f->globals;
                bool lazy = in.op == Bc::LazyImportName ||
                            (in.op == Bc::ImportName && top && lazy_wanted(nm.v, lv.v, f->globals));
                if (lazy) {
                    if (!top) {
                        err_set("SyntaxError", "'lazy import' is only allowed at module level");
                        goto oops;
                    }
                    Value out;
                    if (lazy_import(nm.v, lv.v, fl.v, f->globals, out) != R::Ok ||
                        !land(out, false))
                        goto oops;
                    break;
                }
                Value a4[4] = { nm.v, lv.v, fl.v, f->globals };
                CallArgs a;
                a.args  = a4;
                a.nargs = 4;
                Value out;
                if (py_import(a, out) != R::Ok || !land(out, false))
                    goto oops;
                break;
            }
            case Bc::ImportFrom: {
                Value out;
                StrObj *what = str_of(co->names[arg]);
                if (is_lazy(st[f->sp - 1])) {
                    if (lazy_from(st[f->sp - 1], what, out) != R::Ok || !push(f, out))
                        goto oops;
                    break;
                }
                Got g = py_attr(st[f->sp - 1], what, out);
                if (g == Got::Call) {
                    // A module's __getattr__, or another getter in Python: an
                    // AttributeError from it is the name being missing.
                    Root got{ out };
                    Root kv{ cont_new(from_step) };
                    if (kv.v.is_nil())
                        goto oops;
                    cont_of(kv.v)->s[0] = st[f->sp - 1];
                    cont_of(kv.v)->s[1] = co->names[arg];
                    ListObj *marker     = list_new();
                    if (!marker)
                        goto oops;
                    cont_of(kv.v)->s[2] = obj_value(marker);
                    attr_cont_guard(got.v, obj_value(marker));
                    cont_of(got.v)->next = kv.v;
                    if (!land(got.v, false))
                        goto oops;
                    break;
                }
                if (g != Got::Ok) {
                    err_clear();
                    // A circular relative import: the submodule is being
                    // loaded and is in sys.modules, but the package has no
                    // attribute for it yet.
                    Value sub = import_submodule(st[f->sp - 1], what);
                    if (!sub.is_nil()) {
                        if (!push(f, sub))
                            goto oops;
                        break;
                    }
                    // The name is missing, not the object: say so as an import.
                    import_missing(st[f->sp - 1], what);
                    goto oops;
                }
                if (!push(f, out))
                    goto oops;
                break;
            }
            case Bc::ImportStar: {
                Value into = f->locals.is_nil() ? f->globals : names_dict(f->locals);
                if (into.is_nil()) {
                    err_set("TypeError", "import * needs a dict namespace here");
                    goto oops;
                }
                if (import_star(st[f->sp - 1], dict_at(into)) != R::Ok)
                    goto oops;
                f->sp--;
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
                if (!dispatch(exc, false))
                    return;
                continue;
            }

            case Bc::Raise: {
                if (!arg) {
                    if (vm->handling.is_nil()) {
                        err_set("RuntimeError", "No active exception to re-raise");
                        goto oops;
                    }
                    if (!dispatch(vm->handling, false))
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
                    if (!is_none(rc.v) && !is_exc(rc.v)) {
                        Root made;
                        if (is_exc_type(rc.v))
                            made = exc_inst(rc.v, Value());
                        if (made.v.is_nil() || !is_exc(made.v)) {
                            if (!err_pending())
                                err_set("TypeError",
                                        "exception causes must derive from "
                                        "BaseException");
                            goto oops;
                        }
                        rc = made.v;
                    }
                    ExcObj *eo   = static_cast<ExcObj *>(re.v.obj());
                    eo->cause    = is_none(rc.v) ? Value() : rc.v;
                    eo->suppress = true;
                    exc          = re.v;
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

            case Bc::BeforeAsyncWith: {
                // __aexit__ first, as CPython looks them up, and pinned
                // before __aenter__ is.
                Root exit, enter;
                bool ok = true;
                for (u32 k = 0; k < 2 && ok; k++) {
                    Str name    = k == 0 ? Str("__aexit__") : Str("__aenter__");
                    Value &into = k == 0 ? exit.v : enter.v;
                    switch (py_attr(st[f->sp - 1], str_intern(name), into)) {
                    case Got::Ok:
                        break;
                    case Got::Missing: {
                        Buf<160> b;
                        b.put("'").put(type_name(st[f->sp - 1]));
                        b.put(
                            "' object does not support the asynchronous context manager protocol");
                        if (k == 0)
                            b.put(" (missed __aexit__ method)");
                        err_set("TypeError", b.str());
                        ok = false;
                        break;
                    }
                    case Got::Call:
                        err_set2("TypeError", "this attribute needs the interpreter", name);
                        [[fallthrough]];
                    case Got::Error:
                        ok = false;
                        break;
                    }
                }
                if (!ok)
                    goto oops;
                st[f->sp - 1] = exit.v;
                if (!push(f, enter.v))
                    goto oops;
                CallArgs a;
                Value got;
                bool entered = false;
                if (do_call(st[f->sp - 1], a, got, entered) != R::Ok)
                    goto oops;
                f->sp--;
                if (!land(got, entered))
                    goto oops;
                break;
            }

            case Bc::WithExceptStart: {
                Value exc = st[f->sp - 1];
                if (!is_exc(exc)) {
                    err_set("SystemError", "a with handler without an exception");
                    goto oops;
                }
                // The instance's own class, not the built-in it derives from:
                // a class of one's own is what __exit__ is handed.
                Value t = type_of_value(exc);
                if (t.is_nil())
                    goto oops;
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

                // ---------------------------------------------------- match

            case Bc::Copy:
                if (!push(f, st[f->sp - arg]))
                    goto oops;
                break;
            case Bc::Swap: {
                Value t         = st[f->sp - 1];
                st[f->sp - 1]   = st[f->sp - arg];
                st[f->sp - arg] = t;
                break;
            }
            case Bc::GetLen: {
                Value m = type_special(st[f->sp - 1], "__len__");
                if (!m.is_nil()) {
                    if (!run_special(f, m, nullptr, 0, 0))
                        goto oops;
                    break;
                }
                if (err_pending())
                    goto oops;
                usize n = 0;
                if (py_len(st[f->sp - 1], n) != R::Ok)
                    goto oops;
                if (!push(f, int_from_i64(i64(n))))
                    goto oops;
                break;
            }
            case Bc::MatchSequence:
            case Bc::MatchMapping: {
                u8 want = in.op == Bc::MatchSequence ? PATMA_SEQ : PATMA_MAP;
                if (!push(f, value_bool((patma_kind(st[f->sp - 1]) & want) != 0)))
                    goto oops;
                break;
            }
            case Bc::MatchKeys: {
                Value out;
                if (patma_keys(st[f->sp - 2], st[f->sp - 1], out) != R::Ok || !land(out, false))
                    goto oops;
                break;
            }
            case Bc::MatchClass: {
                Value out;
                if (patma_class(st[f->sp - 3], st[f->sp - 2], arg, st[f->sp - 1], out) != R::Ok)
                    goto oops;
                f->sp -= 3;
                if (!land(out, false))
                    goto oops;
                break;
            }
            case Bc::CopyDict: {
                Value out;
                if (patma_copy(st[f->sp - 1], out) != R::Ok)
                    goto oops;
                f->sp--;
                if (!land(out, false))
                    goto oops;
                break;
            }

            case Bc::CheckEgMatch: {
                Value out;
                if (egroup_match(st[f->sp - 2], st[f->sp - 1], out) != R::Ok)
                    goto oops;
                f->sp -= 2;
                if (!is_cont(out)) {
                    if (!eg_place(f, out))
                        goto oops;
                    break;
                }
                // The group splits itself, which is Python.
                Root split{ out };
                Root kv{ cont_new(eg_match_step) };
                if (kv.v.is_nil())
                    goto oops;
                Root thunk{ native_new("_split", eg_thunk) };
                if (thunk.v.is_nil())
                    goto oops;
                cont_of(kv.v)->s[0] = method_new(thunk.v, split.v);
                cont_of(kv.v)->drop = true;
                if (cont_of(kv.v)->s[0].is_nil() || !run_cont(kv.v, Value()))
                    goto oops;
                break;
            }
            case Bc::Intrinsic: {
                u32 n = intrinsic_arity(arg);
                Value module;
                StrObj *mk = str_intern("__name__");
                if (!mk || dict_get(globals_at(f->globals), obj_value(mk), module) != R::Ok)
                    module = value_none();
                err_clear();
                Value out;
                if (typing_intrinsic(arg, &st[f->sp - n], module, out) != R::Ok)
                    goto oops;
                f->sp -= n;
                st[f->sp++] = out;
                break;
            }
            case Bc::LoadLocals:
                if (f->locals.is_nil()) {
                    err_set("SystemError", "no locals found");
                    goto oops;
                }
                if (!push(f, f->locals))
                    goto oops;
                break;
            case Bc::LoadFromDictOrGlobals:
            case Bc::LoadFromDictOrDeref: {
                // The class namespace first, then where the name really is.
                Value space = st[f->sp - 1];
                StrObj *n   = in.op == Bc::LoadFromDictOrGlobals
                                  ? str_of(co->names[arg])
                                  : str_of(arg < co->cellvars.size()
                                               ? co->cellvars[arg]
                                               : co->freevars[arg - co->cellvars.size()]);
                Value out;
                Value sd = names_dict(space);
                R r      = sd.is_nil() ? R::NotImpl : dict_get(dict_at(sd), obj_value(n), out);
                if (r == R::Err)
                    goto oops;
                if (r == R::NotImpl && in.op == Bc::LoadFromDictOrGlobals) {
                    r = dict_get(globals_at(f->globals), obj_value(n), out);
                    if (r == R::NotImpl)
                        r = dict_get(dict_at(f->builtins), obj_value(n), out);
                    if (r == R::Err)
                        goto oops;
                    if (r == R::NotImpl) {
                        name_error("NameError", n);
                        goto oops;
                    }
                } else if (r == R::NotImpl) {
                    CellObj *c = static_cast<CellObj *>(
                        static_cast<TupleObj *>(f->cells.obj())->items()[arg].obj());
                    if (c->v.is_nil()) {
                        unbound_cell(co, arg);
                        goto oops;
                    }
                    out = c->v;
                }
                if (is_lazy(out)) {
                    f->sp--;
                    if (!reify_into(out, f->globals, obj_value(n)))
                        goto oops;
                    break;
                }
                st[f->sp - 1] = out;
                break;
            }
            case Bc::PrepReraiseStar: {
                Value out = egroup_reraise(st[f->sp - 2], st[f->sp - 1]);
                if (out.is_nil())
                    goto oops;
                f->sp -= 2;
                if (!land(out, false))
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

void atexit_report(Str text)
{
    if (vm)
        vm->err.append(text);
}

// What makes a truth test Python: an instance whose class wrote __bool__, or
// failing that __len__, and a weak proxy to one.
Value truth_special(Value v, bool &len)
{
    len = false;
    if (is_weakproxy(v)) {
        Value t = proxy_target(v);
        if (t.is_nil())
            return Value();
        v = t;
    }
    if (!is_inst(v))
        return Value();
    if (type_has_py_special(v, "__bool__"))
        return type_special(v, "__bool__");
    if (type_has_py_special(v, "__len__")) {
        len = true;
        return type_special(v, "__len__");
    }
    return Value();
}

// What such a method returned, as a truth.
R truth_answer(Value in, bool len, bool &yes)
{
    if (len) {
        i64 n = 0;
        if (!is_intval(in) || !as_index(in, n)) {
            Buf<96> m;
            m.put('\'').put(type_name(in)).put("' object cannot be interpreted as an integer");
            return err_set("TypeError", m.str());
        }
        if (n < 0)
            return err_set("ValueError", "__len__() should return >= 0");
        yes = n != 0;
        return R::Ok;
    }
    if (!is_bool(in)) {
        Buf<96> m;
        m.put("__bool__ should return bool, returned ").put(type_name(in));
        return err_set("TypeError", m.str());
    }
    yes = is_true(in);
    return R::Ok;
}

bool vm_start(Value code, Args argv, Str file)
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
    if (!put_builtins(dict_at(vm->globals)))
        return false;
    if (!file.empty()) {
        StrObj *key = str_intern("__file__");
        Value path  = str_new(file);
        if (!key || path.is_nil() || dict_set(dict_at(vm->globals), obj_value(key), path) != R::Ok)
            return false;
    }

    // The program is a module too, so `import __main__` and sys.modules both
    // find it. Its namespace is the one already made, not a second one.
    Root m{ module_new("__main__") };
    if (m.v.is_nil())
        return false;
    static_cast<ModuleObj *>(m.v.obj())->dict = vm->globals;
    if (!module_defaults(static_cast<DictObj *>(vm->globals.obj())) ||
        !module_register("__main__", m.v))
        return false;

    FrameObj *f = frame_push(code_of(rc.v), vm->globals, vm->globals, Value());
    return f != nullptr;
}

bool vm_again(Value code)
{
    if (!vm || vm->quitting)
        return false;
    vm->finished = false;
    vm->failed   = false;
    vm->status   = 0;
    vm->tb.clear();
    err_clear();
    return frame_push(code_of(code), vm->globals, vm->globals, Value()) != nullptr;
}

void vm_finish()
{
    if (!vm)
        return;
    vm->prompt = false;
    exit_hooks();
}

void vm_set_prompt(bool on)
{
    if (vm)
        vm->prompt = on;
}

bool vm_quitting()
{
    return vm && vm->quitting;
}

Req vm_burst()
{
    for (;;) {
        bool spent = false;
        if (!vm->finished && vm->reading.is_nil()) {
            interpret();
            spent = !vm->finished && vm->reading.is_nil() && vm->out.size() < FLUSH_AT &&
                    vm->err.size() < FLUSH_AT;
        }

        if (!vm->out.empty()) {
            vm->sent = &vm->out;
            return Req{ ReqKind::Write, SYS_STDOUT, vm->out.str(), Str(), 0 };
        }
        // stderr after stdout, so a diagnostic lands after what it is about.
        // A program writing through sys.stderr flushes here too, not only the
        // traceback on the way out.
        if (!vm->err.empty()) {
            // stderr is backslashreplace: a lone surrogate is written as its
            // escape, and that cannot fail.
            if (has_surrogate(vm->err.str())) {
                String clean;
                if (std_encode(vm->err.str(), false, clean))
                    vm->err = static_cast<String &&>(clean);
                err_clear();
            }
            vm->sent = &vm->err;
            return Req{ ReqKind::Write, SYS_STDERR, vm->err.str(), Str(), 0 };
        }
        // Output first, so anything already printed is out before the driver
        // goes to the file system.
        if (!vm->reading.is_nil()) {
            if (vm->napping)
                return Req{ ReqKind::Sleep, 0, Str(), Str(), 0, vm->nap_ms };
            if (vm->calling) {
                Req r;
                r.kind = ReqKind::Sys;
                r.sys  = &vm->sys;
                return r;
            }
            return Req{ ReqKind::Read, 0, Str(), vm->want.str(), 0 };
        }
        if (!vm->finished) {
            // The budget ran out rather than the work: give the driver its
            // turn, which is the only way a signal reaches this process.
            if (spent)
                return Req{ ReqKind::Tick, 0, Str(), Str(), 0 };
            continue;
        }
        return Req{ ReqKind::Exit, 0, Str(), Str(), vm->status };
    }
}

Value vm_frame()
{
    return vm ? vm->frame : Value();
}

String *vm_out()
{
    return vm ? &vm->out : nullptr;
}

String *vm_errout()
{
    return vm ? &vm->err : nullptr;
}

void vm_report(Value e)
{
    Root re{ e };
    vm->tb.clear();
    report(re.v);
}

bool vm_frame_running(const Obj *f)
{
    if (!vm)
        return false;
    for (Value v = vm->frame; !v.is_nil(); v = frame_of(v)->back)
        if (v.obj() == f)
            return true;
    return false;
}

Value vm_handling()
{
    return vm ? vm->handling : Value();
}

u32 vm_recursion_limit()
{
    return max_frames;
}

void vm_set_recursion_limit(u32 n)
{
    max_frames = n;
}

ListObj *vm_frames()
{
    ListObj *l = list_new();
    if (!l)
        return err_set("MemoryError", "out of memory"), nullptr;
    Root rl{ obj_value(l) };
    for (Value f = vm ? vm->frame : Value(); !f.is_nil(); f = frame_of(f)->back)
        if (!list_push(list_of(rl.v), f))
            return err_set("MemoryError", "out of memory"), nullptr;
    return list_of(rl.v);
}

Value exc_pending()
{
    return err_pending() ? pending_exception() : Value();
}

Value vm_collect_and_finalize()
{
    gc_collect();
    if (!gc_owes())
        return Value();
    return cont_new(drain_step);
}

void vm_hard_exit(i32 status)
{
    vm->status   = status;
    vm->finished = true;
    vm->exiting  = true;
}

void vm_signal(u32 sig)
{
    if (sig == 2)
        vm->interrupt = true;
    else if (sig < 32)
        vm->signals |= 1u << sig;
}

void vm_interrupt()
{
    vm->interrupt = true;
}

bool vm_take_interrupt()
{
    bool was = vm && vm->interrupt;
    if (was)
        vm->interrupt = false;
    return was;
}

u32 vm_take_signal()
{
    if (vm_take_interrupt())
        return 2;
    if (!vm || !vm->signals)
        return 0;
    u32 sig = 1;
    while (!(vm->signals & (1u << sig)))
        sig++;
    vm->signals &= ~(1u << sig);
    return sig;
}

R cont_read(ContObj *k, Str path)
{
    if (!vm->want.assign(path))
        return err_set("MemoryError", "out of memory");
    k->fn      = Value();
    k->reading = true;
    return R::Ok;
}

R cont_sleep(ContObj *k, u32 ms)
{
    vm->nap_ms  = ms;
    vm->napping = true;
    k->fn       = Value();
    k->reading  = true;
    return R::Ok;
}

R cont_sys(ContObj *k, const SysReq &r)
{
    vm->sys = r;
    if (!vm->sys_path.assign(r.path) || !vm->sys_path2.assign(r.path2) ||
        !vm->sys_data.assign(r.data))
        return err_set("MemoryError", "out of memory");
    vm->sys.path  = vm->sys_path.str();
    vm->sys.path2 = vm->sys_path2.str();
    vm->sys.data  = vm->sys_data.str();
    vm->calling   = true;
    k->fn         = Value();
    k->reading    = true;
    return R::Ok;
}

void vm_sys_done(SysAns &a)
{
    vm->calling = false;
    vm->answer  = static_cast<SysAns &&>(a);
    vm->sys_data.clear();
    vm_sleep_done();
}

SysAns &vm_sys_answer()
{
    return vm->answer;
}

void vm_sleep_done()
{
    vm->napping = false;
    vm->found   = false;
    vm->isdir   = false;
    vm->text.clear();
    vm->resume  = vm->reading;
    vm->reading = Value();
}

void vm_read_done(bool found, bool dir, Str text)
{
    vm->found = found;
    vm->isdir = dir;
    vm->text.clear();
    if (found && !dir && !vm->text.append(text)) {
        vm->found = false;
        err_set("MemoryError", "out of memory");
    }
    vm->resume  = vm->reading;
    vm->reading = Value();
}

void vm_write_done(bool ok)
{
    if (vm->sent)
        vm->sent->clear();
    vm->sent = nullptr;
    if (!ok) {
        vm->finished = true;
        vm->failed   = true;
    }
}
