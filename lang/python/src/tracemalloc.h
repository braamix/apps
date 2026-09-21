// Where every live object was made, which is `tracemalloc`'s floor.
//
// CPython hooks PyMem and keeps a traceback per raw block. There are no
// allocator hooks here and no raw block under an object, so what is traced is
// the object itself: obj_alloc records a traceback and the sweep drops it. So
// get_traced_memory counts what the live objects hold, header and payload,
// and not what the heap reserves for them.
//
// Nothing here makes a Python object. The tables are plain heap and the
// filenames are copies, so a trace is invisible to the collector and
// recording one cannot re-enter it.
#pragma once

#include "kernel/types.h"

struct Obj;

// Tracing is on: what is_tracing answers, and what tells the sweep it must
// drop an object's trace.
extern bool tm_on;

// And this allocation is one to record. Off while _get_traces builds its
// answer, which allocates -- CPython's reentrancy flag.
extern bool tm_rec;

// `o`, `bytes` of heap, was just made: record the frames it was made from.
void tm_track(Obj *o, usize bytes);

// The sweep is about to free `o`.
void tm_untrack(Obj *o);

// start(nframe), for -X tracemalloc and PYTHONTRACEMALLOC, which are settled
// before the program runs. False when there is no room, nothing pending.
bool tm_start(u32 nframe);

// The deepest traceback start() takes, CPython's MAX_NFRAME.
constexpr u32 TM_MAX_NFRAME = 65535;
