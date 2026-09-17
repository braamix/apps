// What `posix` and `_io` share: a path as the bytes a system call takes, a
// name from the system as a str, and the turn a continuation takes to make
// one system call and read its answer.
#pragma once

#include "call.h"
#include "vm.h"

// The bytes of a path argument: a str encoded with surrogateescape, or bytes.
// False with a TypeError pending for anything else, and a ValueError for an
// embedded NUL. A PathLike has to be converted first; see fs_convert.
bool fs_bytes(Value v, Str who, String &out);

// A str whose class writes __fspath__ in Python, or anything else that has
// one: fs_convert calls it.
bool fs_needs_call(Value v);

// os.fspath(v) for everything that answers without Python: str and bytes as
// they are, and a native with __fspath__. Nil with TypeError pending, or with
// nothing pending when `v` has to be converted by a call.
Value fs_path_of(Value v);

// Some of `vals` (the bits of `which`) have __fspath__ written in Python.
// Call it for each, then enter `again` with `vals` as positional arguments,
// Nil where a call left one out. False when none needs it.
bool fs_convert(const Value *vals, u32 n, u32 which, R (*again)(const CallArgs &, Value &out),
                Value &out, R &r);

// A name the system handed back, as a str: UTF-8, with surrogateescape for
// what is not. Nil only when out of memory.
Value fs_decode(Str bytes);

// A call's arguments laid out by name, positional first: `out` takes one
// value each, Nil where the call left it out. Unlike meth_take there is no
// self.
bool fn_take(const CallArgs &a, Str who, const Str *names, u32 n, u32 least, Value *out);

template <usize N>
inline bool fn_take(const CallArgs &a, Str who, const Str (&names)[N], u32 least, Value (&out)[N])
{
    return fn_take(a, who, names, N, least, out);
}

// One system call from inside a step, as two turns of it. The first asks the
// driver for `q` and answers false with `r` set to what the step returns. The
// second finds the answer in: true when it succeeded, and vm_sys_answer() is
// it. A failure is false with an OSError naming `f1` and `f2` pending, where
// either may be Nil. An interrupted call is made again once the program's
// handler has run, unless the ^C becomes the exception. The step calls this
// with the same `q` every time it is entered until it answers true; the top
// two bits of `k->i` are this function's, and the rest the step's own.
bool sys_turn(ContObj *k, const SysReq &q, R &r, Value f1 = Value(), Value f2 = Value());

constexpr u32 SYS_TURN_BITS = 3u << 30;

// The store gives a file one writer or any number of readers, so an open can
// be refused because an object the program dropped still holds the file.
// When sys_turn has just failed that way, and `*tried` is false: collect, and
// make the calls that closes -- `r` is what the step returns, and it is
// entered again with k->i set to `again`. True when that is under way, and
// the pending error cleared.
bool sys_open_retry(ContObj *k, R &r, u32 again, i64 *tried);

// What a delivered signal runs: Err with KeyboardInterrupt pending for ^C
// under the default handler, Ok with Nil for one that is ignored, and Ok with
// the program's handler in `out`. signalmod.cpp.
R sig_handler(u32 sig, Value &out);

// Where a ^C that interrupted a call goes: the handler the program installed,
// or KeyboardInterrupt. Always R::Err, or R::Ok with a handler to call in
// `out`. signalmod.cpp.
R sig_interrupted(Value &out);

// The working directory. The driver reads it once before the program starts,
// and nothing but chdir moves it, so getcwd needs no system call.
void sys_set_cwd(Str path);
Str sys_cwd();

// Whether a descriptor is the terminal: the three the driver asked about, and
// false for anything else, which os.isatty asks the system about instead.
bool sys_tty(i32 fd, bool &known);

// OSError(EBADF) for a descriptor that is not open, or a negative one.
R err_badfd();
