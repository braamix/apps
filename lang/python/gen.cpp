// The generator object and the bound resume. What a resume does is in vm.cpp,
// beside the dispatch loop whose frame chain it pushes onto.
#include "gen.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "ops.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

void gen_trace(Obj *o)
{
    GenObj *g = static_cast<GenObj *>(o);
    gc_mark(g->frame);
    gc_mark(g->name);
    gc_mark(g->handling);
}

R gen_repr(Value v, String &out)
{
    GenObj *g = gen_of(v);
    char tmp[24];
    Buf<96> b;
    b.put("<generator object ");
    b.put(is_str(g->name) ? str_of(g->name)->str() : Str("?"));
    b.put(" at 0x").put(int_text(tmp, sizeof tmp, i64(usize(v.obj())))).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

// A generator is its own iterator. There is no `next` slot, because stepping
// one pushes a frame; ForIter and the drain in call.cpp use a GenRunObj.
Value gen_iter(Value v)
{
    return v;
}

R gen_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    u8 how;
    if (n == "__next__")
        how = GR_NEXT;
    else if (n == "send")
        how = GR_SEND;
    else if (n == "throw")
        how = GR_THROW;
    else if (n == "close")
        how = GR_CLOSE;
    else if (n == "__iter__")
        how = GR_ITER;
    else if (n == "__name__") {
        out = gen_of(v)->name;
        return R::Ok;
    } else if (n == "gi_frame") {
        out = gen_of(v)->frame.is_nil() ? value_none() : gen_of(v)->frame;
        return R::Ok;
    } else if (n == "gi_running") {
        out = value_bool(gen_of(v)->state == GEN_RUNNING);
        return R::Ok;
    } else {
        return R::NotImpl;
    }
    out = genrun_new(v, how);
    return out.is_nil() ? R::Err : R::Ok;
}

void genrun_trace(Obj *o)
{
    gc_mark(static_cast<GenRunObj *>(o)->gen);
}

Str genrun_name(u8 how)
{
    switch (how) {
    case GR_NEXT:
        return "__next__";
    case GR_SEND:
        return "send";
    case GR_THROW:
        return "throw";
    case GR_CLOSE:
        return "close";
    default:
        return "__iter__";
    }
}

R genrun_repr(Value v, String &out)
{
    Buf<96> b;
    b.put("<method '").put(genrun_name(genrun_of(v)->how)).put("' of 'generator' objects>");
    return out.append(b.str()) ? R::Ok : oom();
}

} // namespace

constexpr Type gen_type{ .name    = "generator",
                         .trace   = gen_trace,
                         .repr    = gen_repr,
                         .iter    = gen_iter,
                         .getattr = gen_getattr };

constexpr Type genrun_type{ .name = "method", .trace = genrun_trace, .repr = genrun_repr };

Value gen_new(Value frame)
{
    Root rf{ frame };
    GenObj *g = static_cast<GenObj *>(obj_alloc(&gen_type, sizeof(GenObj)));
    if (!g)
        return oom(), Value();
    g->frame    = rf.v;
    g->name     = code_of(frame_of(rf.v)->code)->name;
    g->handling = Value();
    g->state    = GEN_CREATED;
    // A generator dropped at a yield owes its `finally` a run, and that is
    // what a finalizer is: the collector owes it one close.
    g->flags |= OBJ_FINAL;
    return obj_value(g);
}

Value genrun_new(Value gen, u8 how)
{
    Root rg{ gen };
    GenRunObj *m = static_cast<GenRunObj *>(obj_alloc(&genrun_type, sizeof(GenRunObj)));
    if (!m)
        return oom(), Value();
    m->gen = rg.v;
    m->how = how;
    return obj_value(m);
}
