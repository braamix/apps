// The generator: a frame that is parked rather than popped.
//
// A call to a CO_GENERATOR function binds its arguments into a frame and hands
// that frame to one of these instead of entering it. Resuming pushes the frame
// back on the chain, so a generator costs no new kind of activation.
//
// A coroutine and an async generator are the same object under another type:
// what differs is who may resume them and what their end means, not how.
//
// Only the dispatch loop may push a frame, which is ground rule 2. So
// `gen.send` and friends are objects rather than natives: do_call recognises a
// GenRunObj and resumes. That is what lets a continuation ask a generator for
// its next item, and therefore what makes list(gen) work.
#pragma once

#include "frame.h"

enum : u8 {
    GEN_CREATED,   // never resumed: the frame is at pc 0
    GEN_SUSPENDED, // parked at a yield
    GEN_RUNNING,   // on the frame chain now
    GEN_DONE,      // returned, raised, or closed
};

struct GenObj : Obj {
    Value frame;    // FrameObj, or Nil once done
    Value name;     // __name__
    Value qualname; // __qualname__, which repr prints
    Value code;     // CodeObj, kept once the frame has gone
    Value handling; // the resumer's exception, held across the suspension
    u8 state;
    bool running; // async generator: an asend or athrow is under way
    bool closed;  // async generator: finished, or aclose() has begun
};

extern const Type gen_type;
extern const Type coro_type;
extern const Type agen_type;

// The generator, coroutine or async generator owning `frame`, by its code's
// flags. Nil with the error pending.
Value gen_new(Value frame);

inline bool is_gen(Value v)
{
    return v.is_obj() && v.obj()->type == &gen_type;
}

inline bool is_coro(Value v)
{
    return v.is_obj() && v.obj()->type == &coro_type;
}

inline bool is_agen(Value v)
{
    return v.is_obj() && v.obj()->type == &agen_type;
}

// Any of the three: a GenObj.
inline bool is_genlike(Value v)
{
    return is_gen(v) || is_coro(v) || is_agen(v);
}

inline GenObj *gen_of(Value v)
{
    return static_cast<GenObj *>(v.obj());
}

// The word a message calls it by: "generator", "coroutine", "async generator".
Str gen_kind(Value v);

// What a parked one is delegating to -- the value under its `yield from` or
// its await -- or None.
Value gen_awaiting(Value v);

// A generator that types.coroutine has marked, which may be awaited.
bool gen_awaitable(Value v);

// coro.__await__(): the iterator that drives a coroutine for a plain caller.
struct CoroWrapObj : Obj {
    Value coro;
};

extern const Type corowrap_type;

Value corowrap_new(Value coro);

inline bool is_corowrap(Value v)
{
    return v.is_obj() && v.obj()->type == &corowrap_type;
}

// What an async generator's asend(), athrow() and aclose() answer, and what
// anext() answers when it has a default. Each is an awaitable that is its own
// iterator; stepping one steps the generator, and a value the generator
// yielded -- rather than one an await inside it passed up -- ends the step
// with StopIteration.
enum : u8 {
    AK_ASEND,   // asend(v) and __anext__()
    AK_ATHROW,  // athrow(exc)
    AK_ACLOSE,  // aclose()
    AK_DEFAULT, // anext(it, default)
};

enum : u8 { AS_INIT, AS_ITER, AS_CLOSED };

struct AwaitObj : Obj {
    Value target; // the async generator, or AK_DEFAULT's awaitable
    Value arg;    // what asend sends, or what athrow throws
    Value dflt;   // AK_DEFAULT: what StopAsyncIteration turns into
    Value iter;   // AK_DEFAULT: the iterator the awaitable gave, once asked
    u8 kind;
    u8 state;
};

extern const Type asend_type;
extern const Type athrow_type;
extern const Type anext_type;

inline bool is_awaitobj(Value v)
{
    return v.is_obj() && (v.obj()->type == &asend_type || v.obj()->type == &athrow_type ||
                          v.obj()->type == &anext_type);
}

inline AwaitObj *await_of(Value v)
{
    return static_cast<AwaitObj *>(v.obj());
}

// agen.asend(v); anext(it, default). Nil with the error pending.
Value await_new(Value target, u8 kind, Value arg);
Value anext_default(Value awaitable, Value dflt);

// What a resume does with the value it is given.
enum : u8 {
    GR_NEXT,  // send None; a bare __next__
    GR_SEND,  // send a value
    GR_THROW, // raise it at the yield
    GR_CLOSE, // throw GeneratorExit and insist the generator ends
    GR_ITER,  // __iter__: the object itself, no resume at all
    GR_AWAIT, // __await__: the iterator an await walks, no resume either
};

// A bound `gen.send`. It is callable, and do_call resumes on it rather than
// entering a native, because a native cannot push a frame. `gen` is a GenObj,
// a CoroWrapObj or an AwaitObj.
struct GenRunObj : Obj {
    Value gen;
    u8 how;
};

extern const Type genrun_type;

Value genrun_new(Value gen, u8 how);

inline bool is_genrun(Value v)
{
    return v.is_obj() && v.obj()->type == &genrun_type;
}

inline GenRunObj *genrun_of(Value v)
{
    return static_cast<GenRunObj *>(v.obj());
}

// An iterator whose stepping pushes a frame: a generator, a coroutine's
// wrapper, an async generator's awaitable.
inline bool is_resumable(Value v)
{
    return is_gen(v) || is_corowrap(v) || is_awaitobj(v);
}

// The callable that does `how` to `v`, when `v` is resumable, a coroutine or
// an async generator; Nil, with no error, for anything else.
Value resumer(Value v, u8 how);

// The value an async generator yields, as opposed to one an await in its body
// passes up to whoever is driving it.
struct WrapValObj : Obj {
    Value v;
};

extern const Type wrapval_type;

Value wrapval_new(Value v);

inline bool is_wrapval(Value v)
{
    return v.is_obj() && v.obj()->type == &wrapval_type;
}

// The generator types' own methods. Called once, from methods_install().
bool gen_methods();
