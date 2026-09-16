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
// `caught` holds the exception when it is called.
using ContFail = void (*)(ContObj *k);

struct ContObj : Obj {
    ContStep step;
    ContFail fail; // null when there is nothing to undo
    // The builtin to enter again once the drain is done; see iter_park.
    R (*redo)(const CallArgs &, Value &out);
    Value s[8];    // the builtin's own state
    Value fn;      // what to call next, Nil when there is nothing left to call
    Value a[2];    // its arguments
    Value argv;    // or a tuple of them, when there are more than two
    Value kwnames; // TupleObj of StrObj, or Nil: keywords for the next call
    Value kwvals;  // TupleObj beside it
    Value out;     // the answer, once fn is Nil
    Value next;    // the ContObj waiting on this one, or Nil
    Value locals;  // the namespace the next call's frame runs in, or Nil
    Value caught;  // the exception `catching` swallowed
    u32 nargs;
    u32 i, j;     // counters a step keeps across its requests
    u32 catching; // a CATCH_*: the step is resumed with Nil rather than unwound
    bool drop;    // the answer is not wanted: push nothing
    bool reading; // parked on a file read; see cont_read
};

// What a continuation is willing to catch out of the call it asked for. The
// step is re-entered with Nil instead, and `caught` holds the exception.
// CATCH_SEQEND is IndexError or StopIteration: a sequence iterator's end.
enum : u32 { CATCH_NONE, CATCH_STOP, CATCH_ATTR, CATCH_EXIT, CATCH_ANY, CATCH_ASTOP, CATCH_SEQEND };

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

// The same with keywords: `names` and `vals` are two tuples of equal length.
inline R cont_call_kw(ContObj *k, Value fn, Value args, Value names, Value vals)
{
    k->fn      = fn;
    k->argv    = args;
    k->kwnames = names;
    k->kwvals  = vals;
    return R::Ok;
}

// Inside a step: this is the answer, and there is nothing more to call.
inline R cont_done(ContObj *k, Value v)
{
    k->fn  = Value();
    k->out = v;
    return R::Ok;
}

// True for a generator. Stepping one pushes a frame, so only the dispatch
// loop can do it and a builtin cannot walk it at all.
bool iter_needs_vm(Value v);

// What iter(v) calls when that is Python: a bound __iter__, or, for a class
// with only __getitem__, a native making the iterator that walks it from 0
// until IndexError. Nil when the native protocol answers.
Value iter_special(Value v);

// What steps an iterator when that needs the VM: a generator's resumer, a
// bound __next__, or the step of that sequence iterator. Nil otherwise.
Value next_special(Value v);

// The sequence iterator's methods, from methods_install().
bool seqiter_methods();

// So the builtin parks here. The VM drains the generator into a list, then
// enters `again` with that list in place of argument `at`. This is eager where
// CPython is lazy, the same trade the eager map() and filter() make; see
// README.md.
R iter_park(const CallArgs &a, u32 at, R (*again)(const CallArgs &, Value &out), Value &out);

// Call `fn(a0[, a1])` first, then enter `again` with what it answered in
// place of argument `at`. How a builtin waits on a call it needs before it can
// start: compile() on bytes whose cookie names a codec written in Python.
R redo_with(const CallArgs &a, u32 at, Value fn, Value a0, Value a1, u32 n,
            R (*again)(const CallArgs &, Value &out), Value &out);

// Inside a step: read `path`, and come back with its text as a str. None means
// there is no such file. The driver performs it, so the VM parks here; only
// `import` needs this, and vm.h says what the driver sees.
R cont_read(ContObj *k, Str path);

// Inside a step: park for `ms`, and come back with None. time.sleep is the
// only caller, and the driver is what actually waits.
R cont_sleep(ContObj *k, u32 ms);
