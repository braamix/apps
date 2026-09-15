// One activation. Frames are heap objects chained through `back`, never C++
// stack frames: a Python call pushes one and the dispatch loop carries on,
// which is ground rule 2.
//
// The fast locals and the value stack are one run of slots after the header,
// so an activation is a single allocation.
#pragma once

#include "code.h"

// One exception handler, pushed by SetupFinally and popped by PopBlock or by
// the exception itself. `sp` is the depth the value stack is cut back to.
struct Block {
    u32 handler;
    u32 sp;
};

struct FrameObj : Obj {
    Value code;     // CodeObj
    Value globals;  // DictObj
    Value locals;   // DictObj: the namespace LoadName reads, or Nil
    Value builtins; // DictObj
    Value cells;    // TupleObj of CellObj, cellvars then freevars, or Nil
    Value back;     // the caller's frame, or Nil
    Value handling; // what the VM was handling when this frame was entered
    Value cont;     // a ContObj this frame's return value belongs to, or Nil
    u32 pc;
    u32 sp;      // values on the stack
    u32 nb;      // handlers on the block stack
    u32 nlocals; // slots before the stack begins
    u32 nslots;
    u32 nblocks;

    Value *slots() { return reinterpret_cast<Value *>(this + 1); }

    Value *stack() { return slots() + nlocals; }

    // After the slots, so an activation is still one allocation.
    Block *blocks() { return reinterpret_cast<Block *>(slots() + nslots); }
};

extern const Type frame_type;

// Every slot Nil. Null with the error pending.
FrameObj *frame_new(CodeObj *c);

inline FrameObj *frame_of(Value v)
{
    return static_cast<FrameObj *>(v.obj());
}
