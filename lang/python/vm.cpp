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

R name_error(Str kind, StrObj *name)
{
    Buf<96> b;
    b.put("name '").put(name->str()).put("' is not defined");
    return err_set(kind, b.str());
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

// ----------------------------------------------------------- binding a call

bool same_name(Value a, Value b)
{
    if (a == b)
        return true;
    bool eq = false;
    return is_str(a) && is_str(b) && py_eq(a, b, eq) == R::Ok && eq;
}

R too_many(CodeObj *co, u32 given)
{
    char tmp[24];
    Buf<128> m;
    m.put(is_str(co->name) ? str_of(co->name)->str() : Str("?"));
    m.put("() takes ").put(int_text(tmp, sizeof tmp, i64(co->argcount)));
    m.put(" positional arguments but ").put(int_text(tmp, sizeof tmp, i64(given)));
    m.put(" were given");
    return err_set("TypeError", m.str());
}

R missing(CodeObj *co, Value name, Str what)
{
    Buf<128> m;
    m.put(is_str(co->name) ? str_of(co->name)->str() : Str("?"));
    m.put("() missing a required ").put(what).put(" argument: '");
    m.put(is_str(name) ? str_of(name)->str() : Str("?")).put("'");
    return err_set("TypeError", m.str());
}

// Fill the new frame's locals. `fn` and the arguments are rooted by the
// caller; `nf` is rooted by the Root in do_call.
R bind_args(FuncObj *fn, CodeObj *co, FrameObj *nf, const CallArgs &a)
{
    Value *lo   = nf->slots();
    u32 argc    = co->argcount;
    u32 named   = argc + co->kwonly;
    u32 at_star = named;
    u32 at_kw   = named + ((co->flags & CO_VARARGS) ? 1 : 0);

    if (a.nargs > argc && !(co->flags & CO_VARARGS))
        return too_many(co, a.nargs);

    u32 direct = a.nargs < argc ? a.nargs : argc;
    for (u32 i = 0; i < direct; i++)
        lo[i] = a.args[i];

    if (co->flags & CO_VARARGS) {
        u32 extra   = a.nargs > argc ? a.nargs - argc : 0;
        TupleObj *t = tuple_new(extra);
        if (!t)
            return oom();
        for (u32 i = 0; i < extra; i++)
            t->items()[i] = a.args[argc + i];
        lo[at_star] = obj_value(t);
    }

    DictObj *rest = nullptr;
    if (co->flags & CO_VARKW) {
        rest = dict_new();
        if (!rest)
            return oom();
        lo[at_kw] = obj_value(rest);
    }

    for (u32 k = 0; k < a.nkw; k++) {
        Value key = a.kwnames[k], val = a.kwvals[k];
        u32 slot = named;
        for (u32 i = co->posonly; i < named; i++)
            if (same_name(co->varnames[i], key)) {
                slot = i;
                break;
            }
        if (slot < named) {
            if (!lo[slot].is_nil()) {
                Buf<128> m;
                m.put(is_str(co->name) ? str_of(co->name)->str() : Str("?"));
                m.put("() got multiple values for argument '");
                m.put(is_str(key) ? str_of(key)->str() : Str("?")).put("'");
                return err_set("TypeError", m.str());
            }
            lo[slot] = val;
            continue;
        }
        if (!rest) {
            Buf<128> m;
            m.put(is_str(co->name) ? str_of(co->name)->str() : Str("?"));
            m.put("() got an unexpected keyword argument '");
            m.put(is_str(key) ? str_of(key)->str() : Str("?")).put("'");
            return err_set("TypeError", m.str());
        }
        if (dict_set(rest, key, val) != R::Ok)
            return R::Err;
    }

    // Defaults line up with the last positional parameters.
    if (!fn->defaults.is_nil()) {
        TupleObj *d = static_cast<TupleObj *>(fn->defaults.obj());
        u32 first   = argc - (d->len < argc ? u32(d->len) : argc);
        for (u32 i = first; i < argc; i++)
            if (lo[i].is_nil())
                lo[i] = d->items()[i - first];
    }
    for (u32 i = 0; i < argc; i++)
        if (lo[i].is_nil())
            return missing(co, co->varnames[i], "positional");

    if (!fn->kwdefaults.is_nil())
        for (u32 i = argc; i < named; i++) {
            if (!lo[i].is_nil())
                continue;
            Value got;
            R r = dict_get(dict_at(fn->kwdefaults), co->varnames[i], got);
            if (r == R::Err)
                return R::Err;
            if (r == R::Ok)
                lo[i] = got;
        }
    for (u32 i = argc; i < named; i++)
        if (lo[i].is_nil())
            return missing(co, co->varnames[i], "keyword-only");

    // A parameter some nested scope captures is copied into its cell by the
    // first instructions of the body; nothing to do here.
    return R::Ok;
}

// A call, whatever it lands on. `entered` says a Python frame was pushed and
// the loop must not store a result; otherwise `out` is the answer.
R do_call(Value callable, const CallArgs &a, Value &out, bool &entered)
{
    entered = false;
    if (is_native(callable))
        return static_cast<NativeObj *>(callable.obj())->fn(a, out);
    if (is_exc_type(callable))
        return exc_type_invoke(callable, a, out);

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

// ---------------------------------------------------------------- unwinding

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
        vm->handling = f->handling;
        vm->frame    = f->back;
        vm->depth--;
    }
}

// Start an exception on its way. `e` may be a type, which is instantiated.
bool raise_value(Value e)
{
    Root re{ e };
    if (is_exc_type(re.v)) {
        Value made = exc_new(static_cast<ExcTypeObj *>(re.v.obj())->t, Value());
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
                    name_error("UnboundLocalError", str_of(co->varnames[arg]));
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
                    name_error("UnboundLocalError", str_of(co->varnames[arg]));
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
                    err_set("NameError", "a free variable is not bound yet");
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
                c->v = Value();
                break;
            }
            case Bc::LoadClosure:
                if (!push(f, static_cast<TupleObj *>(f->cells.obj())->items()[arg]))
                    goto oops;
                break;

            case Bc::LoadAttr: {
                Value out;
                if (py_getattr(st[f->sp - 1], str_of(co->names[arg]), out) != R::Ok)
                    goto oops;
                st[f->sp - 1] = out;
                break;
            }
            case Bc::StoreAttr:
            case Bc::DeleteAttr:
                err_set2("AttributeError", "attributes cannot be set on this type",
                         type_name(st[f->sp - 1]));
                goto oops;

            case Bc::LoadSubscr: {
                Value out;
                if (py_getitem(st[f->sp - 2], st[f->sp - 1], out) != R::Ok)
                    goto oops;
                f->sp -= 2;
                st[f->sp++] = out;
                break;
            }
            case Bc::StoreSubscr:
                if (py_setitem(st[f->sp - 2], st[f->sp - 1], st[f->sp - 3]) != R::Ok)
                    goto oops;
                f->sp -= 3;
                break;
            case Bc::DeleteSubscr:
                if (py_delitem(st[f->sp - 2], st[f->sp - 1]) != R::Ok)
                    goto oops;
                f->sp -= 2;
                break;

            case Bc::UnaryOp: {
                Value out;
                Value a = st[f->sp - 1];
                R r     = R::Ok;
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
                Value out;
                R r = in.op == Bc::BinaryOp
                          ? py_binop(st[f->sp - 2], st[f->sp - 1], Op(arg), out)
                          : py_inplace(st[f->sp - 2], st[f->sp - 1], Op(arg), out);
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
                    if (py_contains(st[f->sp - 1], st[f->sp - 2], ok) != R::Ok)
                        goto oops;
                    if (op == Cmp::NotIn)
                        ok = !ok;
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
                Value it = py_iter(st[f->sp - 1]);
                if (it.is_nil())
                    goto oops;
                st[f->sp - 1] = it;
                break;
            }
            case Bc::ForIter: {
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
            case Bc::DictUpdate: {
                Value from = st[f->sp - 1];
                if (!is_dict(from)) {
                    err_set2("TypeError", "argument after ** must be a mapping", type_name(from));
                    goto oops;
                }
                Value into = st[f->sp - 1 - arg];
                usize at   = 0;
                Value k, v;
                while (table_next(dict_at(from)->t, at, k, v))
                    if (dict_set(dict_at(into), k, v) != R::Ok)
                        goto oops;
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
                if (entered) {
                    f->sp -= consumed;
                } else {
                    f->sp -= consumed;
                    st[f->sp++] = out;
                }
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
                vm->frame = f->back;
                vm->depth--;
                FrameObj *c = frame_of(vm->frame);
                if (!push(c, v))
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
                        hit = exc_is(exc_type_of(exc),
                                     static_cast<ExcTypeObj *>(t->items()[k].obj())->t);
                    }
                } else if (is_exc_type(want)) {
                    hit = exc_is(exc_type_of(exc), static_cast<ExcTypeObj *>(want.obj())->t);
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
                        Value made = exc_new(static_cast<ExcTypeObj *>(re.v.obj())->t, Value());
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
                Value enter, exit;
                if (py_getattr(st[f->sp - 1], str_intern("__exit__"), exit) != R::Ok ||
                    py_getattr(st[f->sp - 1], str_intern("__enter__"), enter) != R::Ok)
                    goto oops;
                st[f->sp - 1] = exit;
                if (!push(f, enter)) // the callable stays rooted while it runs
                    goto oops;
                CallArgs a;
                Value got;
                bool entered = false;
                if (do_call(st[f->sp - 1], a, got, entered) != R::Ok)
                    goto oops;
                f->sp--;
                // A Python __enter__ pushes its own answer when it returns.
                if (!entered)
                    st[f->sp++] = got;
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
                if (!entered)
                    st[f->sp++] = got;
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
