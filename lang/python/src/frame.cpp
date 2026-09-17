// The frame object. Locals and the value stack are one run of slots after the
// header, so an activation costs a single allocation.
#include "frame.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "vm.h"

namespace {

// The stack size is an upper bound the compiler walked out; the margin is for
// the places the walk rounds up rather than down.
constexpr u32 STACK_MARGIN = 8;

void frame_trace(Obj *o)
{
    FrameObj *f = static_cast<FrameObj *>(o);
    gc_mark(f->code);
    gc_mark(f->globals);
    gc_mark(f->locals);
    gc_mark(f->builtins);
    gc_mark(f->cells);
    gc_mark(f->back);
    gc_mark(f->handling);
    gc_mark(f->cont);
    gc_mark(f->gen);
    gc_mark(f->extra);
    for (u32 i = 0; i < f->nlocals; i++)
        gc_mark(f->slots()[i]);
    for (u32 i = 0; i < f->sp; i++)
        gc_mark(f->stack()[i]);
}

R frame_repr(Value v, String &out)
{
    CodeObj *c = code_of(frame_of(v)->code);
    Buf<96> b;
    b.put("<frame of ").put(is_str(c->name) ? str_of(c->name)->str() : Str("?")).put('>');
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

// What a traceback and sys._getframe read off a frame. Not writable: this is
// the activation itself, not a copy of it.
R frame_getattr(Value v, StrObj *name, Value &out)
{
    FrameObj *f = frame_of(v);
    Str n       = name->str();
    if (n == "f_back")
        out = f->back.is_nil() ? value_none() : f->back;
    else if (n == "f_globals")
        out = f->globals.is_nil() ? value_none() : f->globals;
    else if (n == "f_locals" && f->locals.is_nil() && (code_of(f->code)->flags & CO_OPTIMIZED))
        out = frame_locals_proxy(v);
    else if (n == "f_locals")
        out = f->locals.is_nil() ? f->globals : f->locals;
    else if (n == "f_code")
        out = f->code;
    else if (n == "f_lasti")
        out = Value::of_int(i32(f->pc ? (f->pc - 1) * 2 : -1));
    else if (n == "f_lineno")
        out = Value::of_int(i32(code_line(code_of(f->code), f->pc ? f->pc - 1 : 0)));
    else if (n == "f_builtins")
        out = f->builtins.is_nil() ? value_none() : f->builtins;
    else if (n == "f_trace")
        out = value_none();
    else if (n == "f_generator")
        out = f->gen.is_nil() ? value_none() : f->gen;
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

// frame.clear(): drop the locals of a frame that is over.
R m_clear(const CallArgs &a, Value &out)
{
    if (a.nargs != 1 || a.nkw || !is_frame(a.args[0]))
        return err_set("TypeError", "clear() takes no arguments");
    FrameObj *f = frame_of(a.args[0]);
    if (vm_frame_running(f))
        return err_set("RuntimeError", "cannot clear an executing frame");
    if (!f->gen.is_nil())
        return err_set("RuntimeError", "cannot clear a suspended frame");
    for (u32 i = 0; i < f->nslots; i++)
        f->slots()[i] = Value();
    f->sp     = 0;
    f->locals = Value();
    f->extra  = Value();
    out       = value_none();
    return R::Ok;
}

constexpr Method FRAME_METHODS[] = { { "clear", m_clear } };

} // namespace

bool frame_methods()
{
    return method_install(&frame_type, FRAME_METHODS);
}

constexpr Type frame_type{ .name    = "frame",
                           .trace   = frame_trace,
                           .repr    = frame_repr,
                           .getattr = frame_getattr };

FrameObj *frame_new(CodeObj *c)
{
    Root rc{ obj_value(c) };
    u32 nlocals = u32(c->varnames.size());
    u32 nslots  = nlocals + c->stacksize + STACK_MARGIN;
    u32 nblocks = c->nblocks;

    FrameObj *f = static_cast<FrameObj *>(obj_alloc(
        &frame_type, sizeof(FrameObj) + nslots * sizeof(Value) + nblocks * sizeof(Block)));
    if (!f)
        return err_set("MemoryError", "out of memory"), nullptr;
    f->code     = rc.v;
    f->globals  = Value();
    f->locals   = Value();
    f->builtins = Value();
    f->cells    = Value();
    f->back     = Value();
    f->handling = Value();
    f->cont     = Value();
    f->gen      = Value();
    f->extra    = Value();
    f->pc       = 0;
    f->sp       = 0;
    f->nb       = 0;
    f->nlocals  = nlocals;
    f->nslots   = nslots;
    f->nblocks  = nblocks;
    for (u32 i = 0; i < nslots; i++)
        f->slots()[i] = Value();
    return f;
}
