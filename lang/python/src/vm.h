// The interpreter, as a driver rather than a coroutine. `vm_burst` runs plain
// C++ until it has something for its caller to do and says what; braam.cpp
// performs it and comes back. Ground rule 1, and emulators/simbesm's shape.
#pragma once

#include "code.h"
#include "kernel/args.h"
#include "kernel/result.h"

enum class ReqKind : u8 {
    Write, // `data` to `fd`, then vm_write_done
    Read,  // the whole of `path`, then vm_read_done
    Tick,  // the burst is up: park for a moment so a signal can arrive
    Sleep, // park for `ms`, then vm_sleep_done
    Sys,   // one system call, `sys`, then vm_sys_done
    Exit,  // the program is over
};

// What io and os ask the driver for: one syscall each, named after proc/io.h's
// helper that performs it.
enum class SysOp : u8 {
    Open,     // path, flags -> n the fd
    Read,     // fd, max -> data; empty at end of input
    Write,    // fd, data -> n written
    Close,    // fd
    Seek,     // fd, off, whence -> n the position
    Truncate, // fd, off the length
    Stat,     // path, follow -> st
    FStat,    // fd -> st
    List,     // path -> names, kinds, sizes, mtimes
    MkDir,    // path
    Remove,   // path, flags 1 for the whole tree
    Rename,   // path to path2
    Symlink,  // data the target, path the link
    ReadLink, // path -> data
    Touch,    // path
    Cwd,      // -> data
    Chdir,    // path -> data
    Dup,      // fd -> n
    Tty,      // fd -> n 1 for the console, cols and rows in st
    Pipe,     // -> n the read end, off the write end
    Kill,     // fd the pid, flags the signal
    SigCatch, // fd the signal, flags 1 to be told of it and 0 to stop
    Spawn,    // data argv, path2 env, path cwd, io the slots -> n the pid
    Wait,     // fd the pid or 0 for any, flags SYS_WAIT_NOHANG -> n the pid, off the status
    Poll,     // data the fd/events pairs, max the timeout -> n ready, data the revents
};

// Open: the path is a directory, and the driver makes a file in it that is
// removed again when its descriptor closes. O_TMPFILE.
constexpr u32 SYS_O_HIDDEN = 1u << 30;

// Spawn: path2 is the child's environment. Without it the child inherits.
constexpr u32 SYS_SPAWN_WITH_ENV = 1;

// Wait: a child still running answers n = 0 rather than being waited for.
constexpr u32 SYS_WAIT_NOHANG = 1;

// Poll: a pair of u32s per descriptor, and a u32 of revents per descriptor
// back, because a SysReq carries bytes and not a vector. The event bits in
// them are the kernel's own SYS_POLL_*, which selectmod.cpp names directly.
constexpr usize SYS_POLL_PAIR = 8;

struct SysReq {
    SysOp op    = SysOp::Close;
    i32 fd      = -1;
    u32 flags   = 0;
    i64 off     = 0;
    u32 whence  = 0;
    u32 max     = 0;
    bool follow = true;
    Str path, path2, data;      // valid until vm_sys_done
    i32 io[3] = { -1, -1, -1 }; // Spawn: stdin, stdout, stderr; -1 shares the parent's
};

// One entry of a listing.
struct SysEnt {
    String name;
    u32 kind  = 0;
    u64 size  = 0;
    u64 mtime = 0;
};

struct SysAns {
    bool ok   = false;
    Error err = Error::Io;
    i64 n     = 0;
    i64 off   = 0;
    String data;
    u32 kind  = 0; // Stat: SYS_KIND_*; Tty: cols
    u64 size  = 0; //       and rows
    u64 mtime = 0;
    Vec<SysEnt> ents;
};

struct Req {
    ReqKind kind = ReqKind::Exit;
    i32 fd       = 0;
    Str data; // Write: the bytes. Valid until vm_write_done
    Str path; // Read: the file wanted. Valid until vm_read_done
    i32 status        = 0;
    u32 ms            = 0;       // Sleep: how long
    const SysReq *sys = nullptr; // Sys: the call. Valid until vm_sys_done
};

// Make `code` the __main__ module and take the command line; `file`, when
// the program came from one, is its __file__. False leaves the error pending.
bool vm_start(Value code, Args argv, Str file = Str());

// A second command over the same __main__, after the last one ended. False
// leaves the error pending, or says the session is over.
bool vm_again(Value code);

// Whether a command is being read at a prompt: one ending is then not the
// program ending, so atexit waits for the session to end instead.
void vm_set_prompt(bool on);

// A warning from where warnings.warn cannot be called, issued before the next
// instruction runs. Both are literals.
void vm_warn_later(Str category, Str message);

// A native profiler, told about every call and return with no Python call
// per event: _lsprof's, which cProfile stands on. `what` is the code object
// of the frame entered or left, or the builtin called. Null turns it off.
enum : u32 { PROF_CALL, PROF_RETURN, PROF_C_CALL, PROF_C_RETURN };
void vm_set_native_profile(void (*fn)(u32 event, Value what));
bool vm_native_profiling();

// sys.settrace and sys.setprofile: the function installed, or Nil for none.
// A frame entered from here owes it a `call` event; see FT_CALL_T.
void vm_set_trace(Value fn);
Value vm_trace();
void vm_set_profile(Value fn);
Value vm_profile();

// -u: every write reaches the descriptor before the next instruction runs.
void vm_set_unbuffered(bool on);

// The session is over: what atexit holds runs, and the next burst exits.
void vm_finish();

// Whether a SystemExit has asked for the session to end.
bool vm_quitting();

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

// The system call the last burst asked for has answered. `a` is moved from.
void vm_sys_done(SysAns &a);

// What it answered, for the step that asked. Valid until the next request.
SysAns &vm_sys_answer();

// The pending error as an exception object, made now if it was only a kind
// and a message. Nil when nothing is pending.
Value exc_pending();

// A collection, and a ContObj that makes the calls it owes -- the finalizers
// of what it found dropped. Nil when it owes none. How a store refusing to
// open a file a dropped object still holds gets it closed first.
Value vm_collect_and_finalize();

// os._exit: the program ends now, with `status`, and nothing else runs.
void vm_hard_exit(i32 status);

// A signal the program has a handler for arrived, or raise_signal sent one.
// The next instruction boundary runs the handler.
void vm_signal(u32 sig);

// A ^C arrived. The next instruction boundary raises KeyboardInterrupt.
void vm_interrupt();

// A continuation resumed from a park takes the ^C itself: true once for each
// that arrived, and the instruction boundary no longer sees it.
bool vm_take_interrupt();

// The same for any signal: the one to handle now, ^C first, or 0.
u32 vm_take_signal();

// The frame a builtin was called from: a native pushes none, so this is the
// caller's. Nil before the first. Zero-argument super() is why it exists.
Value vm_frame();

// The two buffers sys.stdout and sys.stderr write into. The VM owns both and
// decides when to flush them; null before vm_start.
String *vm_out();
String *vm_errout();

// The exception an `except` clause is working on, or Nil. sys.exc_info().
Value vm_handling();

// Print `e` as an uncaught exception is printed, to stderr.
void vm_report(Value e);

// The same report, as text rather than onto stderr. False when out of memory.
bool vm_report_text(Value e, String &out);

// Whether frame `f` is on the chain now.
bool vm_frame_running(const Obj *f);

// The Python frame stack, innermost first, as a fresh list of FrameObj.
// sys._getframe reads it.
ListObj *vm_frames();

// How deep a Python call may go. sys.getrecursionlimit and its setter; the
// frames are heap, so this is a stated limit rather than a stack measurement.
u32 vm_recursion_limit();
void vm_set_recursion_limit(u32 n);

// The bound __bool__, or __len__ with `len` set, a class instance answers its
// truth with; Nil for the native test, with an error pending for a dead proxy.
Value truth_special(Value v, bool &len);

// What that method returned, as a truth.
R truth_answer(Value in, bool len, bool &yes);
