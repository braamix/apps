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

// FrameObj::tflags. LINES and OPCODES are f_trace_lines and f_trace_opcodes;
// the two CALL bits say a `call` event is owed to the tracer or the profiler,
// and are set where the frame is entered rather than where it is made, so a
// generator owes one at every resume.
enum : u8 {
    FT_LINES   = 1 << 0,
    FT_OPCODES = 1 << 1,
    FT_CALL_T  = 1 << 2,
    FT_CALL_P  = 1 << 3,
    FT_CALL_M  = 1 << 4, // sys.monitoring is owed a PY_START or PY_RESUME
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
    Value gen;      // the GenObj this frame belongs to, or Nil
    Value extra;    // DictObj: what f_locals stored that is not a local, or Nil
    Value trace;    // f_trace: the local trace function sys.settrace left, or Nil
    u32 pc;
    u32 sp;      // values on the stack
    u32 nb;      // handlers on the block stack
    u32 nlocals; // slots before the stack begins
    u32 nslots;
    u32 nblocks;

    // sys.settrace's bookkeeping, all of it idle while no tracer is
    // installed. `tracepc` is the pc `fired` applies to, so an event that
    // suspends to call Python does not fire again when the loop comes back;
    // `prevpc` is where the previous instruction was, which is how a backward
    // edge is seen; `lastline` is the line a line event last went out for.
    u32 tracepc;
    u32 prevpc;
    u32 lastline;
    u16 fired; // FIRED_* bits already fired at tracepc
    u8 tflags; // FT_*

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

inline bool is_frame(Value v)
{
    return v.is_obj() && v.obj()->type == &frame_type;
}

// f_locals of a function's frame: a mapping over its slots and cells.
Value frame_locals_proxy(Value frame);

extern const Type frame_locals_type;

inline bool is_frame_locals(Value v)
{
    return v.is_obj() && v.obj()->type == &frame_locals_type;
}

// A FrameLocalsProxy as a fresh dict, for what reads a mapping as one; `v`
// itself otherwise.
Value frame_locals_dict(Value v);

// FrameLocalsProxy's methods and constructor. Called from methods_install.
bool frame_locals_methods();

// frame.clear(). Called from methods_install.
bool frame_methods();
