// One activation. Frames are heap objects chained through `back`, never C++
// stack frames: a Python call pushes one and the dispatch loop carries on,
// which is ground rule 2.
//
// The fast locals and the value stack are one run of slots after the header,
// so an activation is a single allocation.
#pragma once

#include "code.h"

struct FrameObj : Obj {
    Value code;     // CodeObj
    Value globals;  // DictObj
    Value locals;   // DictObj: the namespace LoadName reads, or Nil
    Value builtins; // DictObj
    Value cells;    // TupleObj of CellObj, cellvars then freevars, or Nil
    Value back;     // the caller's frame, or Nil
    u32 pc;
    u32 sp;      // values on the stack
    u32 nlocals; // slots before the stack begins
    u32 nslots;

    Value *slots() { return reinterpret_cast<Value *>(this + 1); }

    Value *stack() { return slots() + nlocals; }
};

extern const Type frame_type;

// Every slot Nil. Null with the error pending.
FrameObj *frame_new(CodeObj *c);

inline FrameObj *frame_of(Value v)
{
    return static_cast<FrameObj *>(v.obj());
}
