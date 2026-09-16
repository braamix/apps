// The generator: a frame that is parked rather than popped.
//
// A call to a CO_GENERATOR function binds its arguments into a frame and hands
// that frame to one of these instead of entering it. Resuming pushes the frame
// back on the chain, so a generator costs no new kind of activation.
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
    Value name;     // the code object's, for repr
    Value handling; // the resumer's exception, held across the suspension
    u8 state;
};

extern const Type gen_type;

// The generator owning `frame`. Nil with the error pending.
Value gen_new(Value frame);

inline bool is_gen(Value v)
{
    return v.is_obj() && v.obj()->type == &gen_type;
}

inline GenObj *gen_of(Value v)
{
    return static_cast<GenObj *>(v.obj());
}

// What a resume does with the value it is given.
enum : u8 {
    GR_NEXT,  // send None; a bare __next__
    GR_SEND,  // send a value
    GR_THROW, // raise it at the yield
    GR_CLOSE, // throw GeneratorExit and insist the generator ends
    GR_ITER,  // __iter__: the generator itself, no resume at all
};

// A bound `gen.send`. It is callable, and do_call resumes on it rather than
// entering a native, because a native cannot push a frame.
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
