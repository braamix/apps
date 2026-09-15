// The frame object. Locals and the value stack are one run of slots after the
// header, so an activation costs a single allocation.
#include "frame.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "ops.h"

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

} // namespace

constexpr Type frame_type{ .name = "frame", .trace = frame_trace, .repr = frame_repr };

FrameObj *frame_new(CodeObj *c)
{
    Root rc{ obj_value(c) };
    u32 nlocals = u32(c->varnames.size());
    u32 nslots  = nlocals + c->stacksize + STACK_MARGIN;

    FrameObj *f =
        static_cast<FrameObj *>(obj_alloc(&frame_type, sizeof(FrameObj) + nslots * sizeof(Value)));
    if (!f)
        return err_set("MemoryError", "out of memory"), nullptr;
    f->code     = rc.v;
    f->globals  = Value();
    f->locals   = Value();
    f->builtins = Value();
    f->cells    = Value();
    f->back     = Value();
    f->pc       = 0;
    f->sp       = 0;
    f->nlocals  = nlocals;
    f->nslots   = nslots;
    for (u32 i = 0; i < nslots; i++)
        f->slots()[i] = Value();
    return f;
}
