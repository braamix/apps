// Binding one call's arguments, and the rule a builtin follows when it has to
// call back into Python.
//
// A builtin must not re-enter the dispatch loop -- ground rule 2 -- so one that
// needs a Python call parks its state in a ContObj and hands it back as its
// result. The VM notices, runs `step` with the answer to each request, and
// pushes what the last one leaves in `out`.
#pragma once

#include "frame.h"
#include "func.h"

// Fill `nf`'s locals from `a`. Everything is rooted by the caller.
R bind_args(FuncObj *fn, CodeObj *co, FrameObj *nf, const CallArgs &a);

struct ContObj;

// Re-entered with the answer to the last request; `in` is Nil the first time.
// Ask for another call with cont_call, or stop with cont_done.
using ContStep = R (*)(ContObj *k, Value in);

struct ContObj : Obj {
    ContStep step;
    Value s[4]; // the builtin's own state
    Value fn;   // what to call next, Nil when there is nothing left to call
    Value a[2]; // its arguments
    Value out;  // the answer, once fn is Nil
    Value next; // the ContObj waiting on this one, or Nil
    u32 nargs;
    u32 i, j; // counters a step keeps across its requests
};

extern const Type cont_type;

// Nil with the error pending.
Value cont_new(ContStep step);

inline bool is_cont(Value v)
{
    return v.is_obj() && v.obj()->type == &cont_type;
}

inline ContObj *cont_of(Value v)
{
    return static_cast<ContObj *>(v.obj());
}

// Inside a step: call `fn(args...)` and come back with what it returned.
inline R cont_call(ContObj *k, Value fn, Value a0)
{
    k->fn    = fn;
    k->a[0]  = a0;
    k->nargs = 1;
    return R::Ok;
}

// Inside a step: this is the answer, and there is nothing more to call.
inline R cont_done(ContObj *k, Value v)
{
    k->fn  = Value();
    k->out = v;
    return R::Ok;
}
