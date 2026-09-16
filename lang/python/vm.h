// The interpreter, as a driver rather than a coroutine. `vm_burst` runs plain
// C++ until it has something for its caller to do and says what; braam.cpp
// performs it and comes back. Ground rule 1, and emulators/simbesm's shape.
#pragma once

#include "code.h"
#include "kernel/args.h"

enum class ReqKind : u8 {
    Write, // `data` to `fd`, then vm_write_done
    Read,  // the whole of `path`, then vm_read_done
    Tick,  // the burst is up: park for a moment so a signal can arrive
    Sleep, // park for `ms`, then vm_sleep_done
    Exit,  // the program is over
};

struct Req {
    ReqKind kind = ReqKind::Exit;
    i32 fd       = 0;
    Str data; // Write: the bytes. Valid until vm_write_done
    Str path; // Read: the file wanted. Valid until vm_read_done
    i32 status = 0;
    u32 ms     = 0; // Sleep: how long
};

// Make `code` the __main__ module and take the command line. False leaves the
// error pending.
bool vm_start(Value code, Args argv);

// Run until the driver is needed.
Req vm_burst();

// The write the last burst asked for is done.
void vm_write_done(bool ok);

// The file the last burst asked for. `found` false means no such name, which
// an import takes as "try the next candidate" and not as an error. A path
// ending in `/` is asking whether that is a directory, and `dir` answers it.
void vm_read_done(bool found, bool dir, Str text);

// The sleep the last burst asked for is over.
void vm_sleep_done();

// A ^C arrived. The next instruction boundary raises KeyboardInterrupt.
void vm_interrupt();

// The frame a builtin was called from: a native pushes none, so this is the
// caller's. Nil before the first. Zero-argument super() is why it exists.
Value vm_frame();

// The two buffers sys.stdout and sys.stderr write into. The VM owns both and
// decides when to flush them; null before vm_start.
String *vm_out();
String *vm_errout();

// The exception an `except` clause is working on, or Nil. sys.exc_info().
Value vm_handling();

// The Python frame stack, innermost first, as a fresh list of FrameObj.
// sys._getframe reads it.
ListObj *vm_frames();

// How deep a Python call may go. sys.getrecursionlimit and its setter; the
// frames are heap, so this is a stated limit rather than a stack measurement.
u32 vm_recursion_limit();
void vm_set_recursion_limit(u32 n);
