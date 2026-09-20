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
    gc_mark(f->trace);
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
        // Nothing has run yet at pc 0, so the line is the one the `def` or
        // the module began on -- which is what f_lasti answering -1 means.
        out = Value::of_int(i32(f->pc ? code_line(code_of(f->code), f->pc - 1)
                                      : code_of(f->code)->firstline));
    else if (n == "f_builtins")
        out = f->builtins.is_nil() ? value_none() : f->builtins;
    else if (n == "f_trace")
        out = f->trace.is_nil() ? value_none() : f->trace;
    else if (n == "f_trace_lines")
        out = value_bool((f->tflags & FT_LINES) != 0);
    else if (n == "f_trace_opcodes")
        out = value_bool((f->tflags & FT_OPCODES) != 0);
    else if (n == "f_generator")
        out = f->gen.is_nil() ? value_none() : f->gen;
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

// The three a debugger writes. Everything else on a frame is the activation
// itself and stays read-only.
R frame_setattr(Value v, StrObj *name, Value val)
{
    FrameObj *f = frame_of(v);
    Str n       = name->str();
    if (n != "f_trace" && n != "f_trace_lines" && n != "f_trace_opcodes")
        return R::NotImpl;
    // `del frame.f_trace` is how a debugger stops tracing one frame, and
    // CPython takes it; the two flags cannot be deleted.
    if (val.is_nil() && n != "f_trace")
        return err_set2("AttributeError", "cannot delete", n);
    if (n == "f_trace") {
        f->trace = val.is_nil() || is_none(val) ? Value() : val;
        return R::Ok;
    }
    bool yes  = py_truth(val);
    u8 bit    = n == "f_trace_lines" ? u8(FT_LINES) : u8(FT_OPCODES);
    f->tflags = u8(yes ? (f->tflags | bit) : (f->tflags & ~bit));
    return R::Ok;
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
                           .getattr = frame_getattr,
                           .setattr = frame_setattr };

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
    f->trace    = Value();
    f->pc       = 0;
    f->sp       = 0;
    f->nb       = 0;
    f->nlocals  = nlocals;
    f->nslots   = nslots;
    f->nblocks  = nblocks;
    f->tracepc  = ~0u;
    f->prevpc   = ~0u;
    f->lastline = 0;
    f->fired    = 0;
    f->tflags   = FT_LINES;
    for (u32 i = 0; i < nslots; i++)
        f->slots()[i] = Value();
    return f;
}
