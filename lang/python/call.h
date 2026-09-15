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

// Called when an exception unwinds past a parked continuation. The step may
// undo what it half-did; the exception carries on either way.
using ContFail = void (*)(ContObj *k);

struct ContObj : Obj {
    ContStep step;
    ContFail fail; // null when there is nothing to undo
    Value s[6];    // the builtin's own state
    Value fn;      // what to call next, Nil when there is nothing left to call
    Value a[2];    // its arguments
    Value argv;    // or a tuple of them, when there are more than two
    Value out;     // the answer, once fn is Nil
    Value next;    // the ContObj waiting on this one, or Nil
    Value locals;  // the namespace the next call's frame runs in, or Nil
    u32 nargs;
    u32 i, j;     // counters a step keeps across its requests
    u32 catching; // a CATCH_*: the step is resumed with Nil rather than unwound
    bool drop;    // the answer is not wanted: push nothing
    bool reading; // parked on a file read; see cont_read
};

// What a continuation is willing to catch out of the call it asked for. The
// step is re-entered with Nil instead of the exception unwinding past it.
enum : u32 { CATCH_NONE, CATCH_STOP, CATCH_ATTR };

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
inline R cont_call(ContObj *k, Value fn, Value a0, u32 n = 1, Value a1 = Value())
{
    k->fn    = fn;
    k->a[0]  = a0;
    k->a[1]  = a1;
    k->nargs = n;
    k->argv  = Value();
    return R::Ok;
}

// Inside a step: call `fn(*args)`, for an arity this code does not fix.
inline R cont_call_v(ContObj *k, Value fn, Value args)
{
    k->fn   = fn;
    k->argv = args;
    return R::Ok;
}

// Inside a step: this is the answer, and there is nothing more to call.
inline R cont_done(ContObj *k, Value v)
{
    k->fn  = Value();
    k->out = v;
    return R::Ok;
}

// Inside a step: read `path`, and come back with its text as a str. None means
// there is no such file. The driver performs it, so the VM parks here; only
// `import` needs this, and vm.h says what the driver sees.
R cont_read(ContObj *k, Str path);
