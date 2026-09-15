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
    Exit,  // the program is over
};

struct Req {
    ReqKind kind = ReqKind::Exit;
    i32 fd       = 0;
    Str data; // Write: the bytes. Valid until vm_write_done
    Str path; // Read: the file wanted. Valid until vm_read_done
    i32 status = 0;
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

// A ^C arrived. The next instruction boundary raises KeyboardInterrupt.
void vm_interrupt();

// The frame a builtin was called from: a native pushes none, so this is the
// caller's. Nil before the first. Zero-argument super() is why it exists.
Value vm_frame();
