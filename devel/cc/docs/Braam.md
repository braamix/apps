# C programs for Braam

Braam is an operating system that runs in a
browser tab. Its programs are WebAssembly modules, and none of them may wait inside a
call: a program sends a request to the kernel, returns, and is started again later
with the answer. `vcc -t wasm32-braam` builds such programs from C, using vcc's
coroutines ([Coroutines_in_C.md](Coroutines_in_C.md)) for every call that waits.

This document is the reference for the target: what the compiler and the driver do,
how the runtime runs a program, what the library offers, how to port a program, and
how to run one by hand. [Braam_Example.md](Braam_Example.md) works one program
through from source to a session on Braam.
§7 describes how the target is built.

## 1. The target

`wasm32-braam` is `wasm32` (see [Wasm_Backend.md](Wasm_Backend.md)) with Braam's process
model on top: the same data model (ILP32, signed `char`, binary128 `long double`), the
same code generator `vgenwasm`, and the same assembler and linker, clang and `wasm-ld`.
What differs:

- **`main` is a coroutine.** It must be `coro(braam_call *) int main(int argc, char
  **argv)`. A plain `main` is a compile-time error on this target, and `main` cannot be
  a coroutine on any other.
- **The preprocessor** (`vcpp -t wasm32-braam`) defines wasm32's macros,
  `__vcc_coroutines__` and `__braam__`.
- **Headers and libraries** come from `share/vcc/wasm32-braam/`. Its `include/` puts
  the Braam headers (`braam.h`, `stdio.h`, `unistd.h`, `fcntl.h`, `errno.h`,
  `signal.h`, `poll.h`, `stdlib.h`, `sys/stat.h`, `sys/types.h`) ahead of wasm32's.
  Its `lib/` holds `crt0.o`, `libc.a` and the fake kernel `run.mjs`.
- **The link** imports the memory and has no entry point:

  ```
  wasm-ld --no-entry --import-memory --stack-first -z stack-size=131072 --gc-sections
          --initial-memory=<pages * 65536> crt0.o objects -lc
  ```

  The driver then appends a custom section `braam`, which Braam's `exec` reads: the
  magic `bram`, the process ABI version (21), flags, the initial pages and the maximum,
  1600. `--initial-pages=N` sets the initial pages, 1 to 1600; the default is 4.
- **The module** imports `env.memory`, `kernel.sys` and `kernel.sys_async`, and exports
  exactly `_start`, `_resume`, `_alloc`, `_free` and `_sig`. The shadow stack is 128 KiB
  at the bottom of memory.

The ABI numbers (`BRAAM_PROC_ABI`, the call numbers, the open flags, the error codes)
are copied from braam-core's `src/kernel/sysabi.h` and `result.h` into `braam.h` and
`errno.h`. The ctest `braam-abi` compares them with braam-core's when that tree is
beside this one, so a change in Braam fails a test rather than producing a binary
Braam refuses.

## 2. How a program runs

The kernel *steps* a process: it calls `_start` once, then `_resume` each time a
request is answered. A step returns 1 while a request is outstanding and 0 once the
process has exited. The runtime (`libc/wasm32/braam/rt.c`) turns this into coroutines:

1. `_start` copies the arguments out of the block the kernel wrote, allocates the
   **root task's block** with `malloc`, builds in it the frame of a coroutine that
   awaits `main` and then flushes every stream, and resumes that frame.
2. The program runs until some call has to wait. That call, `braam_sys` at the bottom
   of every library coroutine, yields a `braam_call` describing the request. The
   `await`s above it pass it up to the runtime, which hands it to the kernel with
   `sys_async` and returns from the step.
3. When the answer comes, `_resume` gives it to the task that asked, and resumes that
   task. `braam_sys` reads the status and the data, frees the reply block, and returns
   to its caller as an ordinary function would.
4. When `main` returns, the runtime writes out what the streams still hold, then tells
   the kernel the exit status.

The root block holds `main`'s frame and, after it, the **arena** that every `await`
in the program takes its callee's frame from (Coroutines_in_C.md §5). It is 64 KiB. A
program whose `await` chains go deep, recursion for instance, can make it larger by
defining

```c
const unsigned __braam_task_bytes = 256 * 1024;
```

When the arena runs out, the program stops with `coroutine trap: CO_TRAP_NO_SPACE:`
and the name of the coroutine that did not fit.

### Tasks

A process has up to `BRAAM_TASKS` (8) tasks, `main`'s included, each with one request
outstanding. `braam_spawn(frame)` adds one: `frame` is a `braam_task *`, that is a
`co_frame(braam_call *, int) *`, usually made by `co_init` on storage that outlives
the task. The storage after the frame is that task's arena. `braam_spawn` runs the
task to its first wait and returns its number, or 0 when the table is full. From then
on each task is resumed when its own request is answered, so the tasks run in turns
and need no locks. A task that returns leaves the table; `co_done` and `co_result`
on its frame tell how it ended. `co_destroy` stops a waiting task, and its late answer
is thrown away. The process ends when `main` returns, whatever the other tasks are
doing.

### Signals

There are no signal handlers. A signal the program did not ask for ends it with
status 130: `^C` at the terminal, or `kill`. `await sig_catch(SIGINT, 1)` asks for one
instead (`SIGINT`, `SIGTERM` or `SIGWINCH`). A signal that was asked for is recorded,
and every call the process is waiting in that could wait for ever gives up with -1
and `errno` set to `EINTR`: a read of the terminal or of a pipe, `sleep_ms`,
`braam_yield` and `poll`. `sig_take(sig)` says whether `sig` came and forgets it;
`sig_pending()` gives the bits `1 << sig` not yet taken.

A signal reaches a process only while it waits. A loop that computes for long and
`await`s nothing cannot be interrupted, so it should call `await braam_yield()` now
and then, as Braam's interpreters do.

### Ending

`main`'s return value is the exit status. `exit(n)` reports `n` and stops the process
at once, as Braam's own C library does: nothing unwinds, no `defer` runs and buffered
output is lost, and the kernel counts it as a crash. Return from `main` instead.

A coroutine trap (Coroutines_in_C.md §7) and a WebAssembly trap end the process the
same way.

## 3. The library

| Header | Calls that wait (`await` them) | Calls that do not |
|---|---|---|
| `<stdio.h>` | `fflush`, `fgetc`, `getc`, `getchar`, `fgets`, `fread`, `fopen`, `fclose`, `fseek`, `ftell`, `rewind`, `remove`, `rename` | `printf`, `fprintf`, `vfprintf`, `vprintf`, `sprintf`, `snprintf`, `puts`, `putchar`, `fputs`, `fputc`, `putc`, `fwrite`, `ungetc`, `feof`, `ferror`, `clearerr`, `fileno`, `perror` |
| `<unistd.h>` | `read`, `write`, `close`, `lseek`, `unlink`, `rmdir`, `chdir`, `getcwd` | `getpid` |
| `<fcntl.h>` | `open` | |
| `<sys/stat.h>` | `stat`, `lstat`, `fstat`, `mkdir` | `S_ISDIR`, `S_ISREG`, `S_ISLNK` |
| `<poll.h>` | `poll` | |
| `<braam.h>` | `braam_sys` (any request), `sleep_ms`, `braam_yield`, `sig_catch` | `braam_now`, `braam_sys_sync`, `braam_spawn`, `sig_take`, `sig_pending` |

Every call that waits is declared `coro(braam_call *)` with C's usual name and
parameters, so `n = read(fd, buf, len)` becomes `n = await read(fd, buf, len)`. The
rest of the C library (`<string.h>`, `<ctype.h>`, `<math.h>`, and `<stdlib.h>`'s
`atoi`, `malloc`, `calloc`, `realloc` and `free`) is wasm32's, unchanged. `malloc` here
reuses freed memory, since Braam frees every answer block; plain wasm32 has a bump
allocator.

Things that work differently from Unix:

- **Streams.** Output never waits: `printf` and the other formatting calls only
  append to the stream's buffer, which grows when full. The buffer is written out by
  `await fflush(f)`, by `await fclose(f)`, when `main` returns, and, for standard
  output, before every read of standard input, so a prompt is shown before the
  program waits for the answer. Input is buffered in 512-byte chunks. Between reading
  and writing one stream, `fflush` or `fseek` it, as C requires.
- **`open`'s flags** are Braam's own bits: `O_RDONLY` is 1, `O_WRONLY` 2, `O_RDWR` 3.
  Use the names.
- **`errno`.** C's `EDOM`, `ERANGE` and `EILSEQ` keep 1 to 3; Braam's errors are
  numbered from 33 up (`EINVAL` 33, `ENOENT` 35, `EEXIST` 36, … `EINTR` 47), and
  `strerror` and `perror` know them.
- **`stat`.** Braam keeps a kind, a size and a modification time per file. The mode
  is 0644 for a file and 0755 for a directory, and `st_ino` is a hash of the path.
- **`sleep_ms(ms)`** in place of `sleep`; `braam_now()` is milliseconds since boot.
- **`poll`** waits for several descriptors at once, up to 64; tasks are the other way.

Not available: `scanf` and its family, `tmpfile`, `setvbuf`, `signal()` and `raise()`,
`setjmp`/`longjmp`, `fork` and `exec`, and reading directories.

## 4. Porting a program

The compiler does most of the work: it rejects a call of a coroutine without
`await`, and an `await` in a function that is not a coroutine. So:

1. Change `main` to `coro(braam_call *) int main(int argc, char **argv)`.
2. Compile. Put `await` in front of each call the compiler names.
3. Make each function that now contains an `await` a `coro(braam_call *)` coroutine,
   with the same parameters and return type, and compile again. Its callers are
   named next. Repeat until it compiles.

Then look for the things the compiler cannot tell you about:

- **`exit()`** in the middle of the program loses buffered output. Return a status
  up to `main` instead, or `await fflush(NULL)` before the `exit`.
- **Cleanup that waits.** A deferred statement may not `await`, so `defer await
  fclose(f)` is an error. Close files and descriptors explicitly; `defer` is still
  right for memory.
- **Function pointers to waiting functions.** A coroutine has no function pointer.
  A table of handlers that wait becomes a table of `coro_ptr(braam_call *, int)`
  (Coroutines_in_C.md §4), whose coroutines take `(void)` or one `void *`, or a
  `switch` over the handlers.
- **Callbacks.** A function called through an ordinary function pointer cannot
  wait. Do the waiting before or after the call that takes it.
- **Long computations** should `await braam_yield()` now and then, so that `^C`
  reaches them and other processes get a turn.
- **`alloca`** is an error in a coroutine, `main` included, since its frame outlives
  the stack. Move the code that allocates into an ordinary function, or use `malloc`.
- **Deep recursion through `await`** needs a larger `__braam_task_bytes` (§2).
- **Variadic functions** cannot be coroutines. One that waits becomes a coroutine
  taking a `va_list`, or formats into its stream with `vfprintf`, which does not wait,
  and leaves the writing to a later `await fflush`.

## 5. Running a program by hand

### Under node

`run.mjs` in the runtime directory is a stand-in kernel for node. It checks the module
as Braam's `exec` would (the imports, the five exports, the `braam` section), then
serves the program's requests from the host: files and paths relative to the current
directory, standard input and output, the clock, signals and `poll`. Each answer
comes back in a later step, as Braam's would.

```sh
vcc -t wasm32-braam prog.c -o prog.wasm
node ~/.local/share/vcc/wasm32-braam/lib/run.mjs prog.wasm arg1 arg2 < input
```

In the build tree the driver is `build/cc/cc` and the kernel
`build/share/vcc/wasm32-braam/lib/run.mjs`.

It prints `[exit N]` on standard error and exits with `N`. Other endings:

| Status | Meaning |
|---|---|
| 255 | a trap: a coroutine trap, a WebAssembly trap, or `exit()` (what `exit` said is printed) |
| 254 | the process waits while nothing will answer it |
| 130 | a signal the program did not ask for |

A byte 0x03 on standard input is a `^C`, as at a terminal, and node's own SIGINT and
SIGTERM are passed on. So `printf 'abc\003' | node run.mjs prog.wasm` tests a program's
handling of `^C`. Requests the fake kernel does not serve answer `ENOTSUP`.

### On Braam

The same file runs on Braam unchanged. In Braam's shell, `fimport` opens the
browser's file picker and puts the chosen file in `/import`. Run it as
`/import/prog.wasm`, or copy it to `/bin/prog` to run it by name; `/bin` is replaced
when a new version of Braam is loaded.

### Inspecting a module

`wasm-objdump -x prog.wasm` shows the three imports, the five exports, no `memory`
export, and one custom section `braam`; `wasm-validate prog.wasm` checks the rest.
braam-core's `test/system/abi.mjs` makes the checks `exec` makes.

## 6. Tests

- `wasm32-tests`' `BraamTest.*` (`backend/wasm/test/braam_tests.cpp`) builds programs
  with `build/cc/cc` and runs them on `run.mjs`: the streams, files and paths, signals,
  tasks, `poll`, the allocator, the `braam` section, and the worked example.
- The ctest `braam-system` (`backend/wasm/test/braam_system.mjs`) runs programs on
  Braam itself, in a session of braam-core's system harness, typing at its terminal.
  It is defined when a built braam-core (with `build/kernel.wasm` and
  `build/web/rootfs.zip`) is at `../../Braam/braam-core` or where `-DBRAAM_CORE=`
  points.
- The ctest `braam-abi` compares the ABI numbers with braam-core's.

## 7. How it is built

This section is for whoever works on the target. The design of the coroutines
underneath is in [Coroutines_Internals.md](Coroutines_Internals.md).

### 7.1 The process ABI

A Braam process is a wasm module that the kernel steps. The facts it must meet, all
from braam-core (`doc/Concept.md` §2.1 and §4.3, `doc/System_Calls.md` §6, §7.3 and
§11, `src/proc/rt.cpp`, `src/kernel/sysabi.h`):

- **Imports:** `env.memory`; `kernel.sys(op, a0, a1, a2) -> i32`, the five synchronous
  calls (Exit, GetPid, Now, Stage, Random); `kernel.sys_async(op, token, ptr, len)`,
  everything else, the answer coming later. The op word is `op | arg << 8`.
- **Exports, exactly:** `_start(argv_ptr, len) -> i32`, `_resume(token, reply_ptr,
  len) -> i32`, `_alloc(n) -> ptr`, `_free(ptr, n)`, `_sig(n)`. A step returns 0 when
  the process has exited (after `sys(Exit, status)`) and 1 while a call is outstanding.
  An answer is a block of `i32` status and then the data, placed by the host through
  `_alloc` and freed by the process. The host writes argv (then the environment)
  through `_alloc` before `_start`: a `u32` count, then each word's `u32` length and
  bytes.
- **The custom section `braam`:** five `u32`, the magic `0x6d617262`, `PROC_ABI` (21),
  flags, the initial pages and the maximum (1600). braam-core's `tools/stamp.py`
  appends it to its own programs; `exec` refuses a binary whose ABI is not the
  kernel's.
- **The link:** `--import-memory --initial-memory=N --no-entry --stack-first -z
  stack-size=131072 --gc-sections`, and no `memory` export. braam-core's
  `test/system/abi.mjs` checks all of this for every binary.
- **Several calls** may be outstanding, one per task (`PROC_TASKS`, 8); an answer names
  its token. There is no preemption, no Asyncify, no JSPI, no stack switching and no
  tail calls.

In vcc:

- **`lower -t wasm32-braam`** is wasm32's descriptor with `braam = 1`
  (`semantic/target.c`); on it `main` must be the coroutine of §1
  (`semantic/coroutines.c`).
- **The driver** (`cc/cc.c`) links as above, with `crt0.o` first and `-lc` last, and
  then appends the `braam` section itself (`braam_stamp`, the work of `stamp.py`),
  dropping an earlier one. `--initial-pages=N` sets the page count, 4 by default as
  `BRAAM_BIN_INITIAL_PAGES` is in braam-core's `cmake/BraamProgram.cmake`.
- **The exports and imports** come from `.export_name`, `.import_module` and
  `.import_name` directives in `libc/wasm32/braam/exports.s`, which is `crt0.o`: vcc's
  C has no way to spell them.
- **The numbers** in `braam.h` (the ABI version, the call numbers, `BRAAM_O_*`,
  `BRAAM_CHUNK`) and in `errno.h` (each Braam `Error` plus 32) are transcribed from
  braam-core. The ctest `braam-abi` (`scripts/check_braam_abi.py`) compares them with
  `src/kernel/sysabi.h` and `result.h`, counting the enums' implicit values, when that
  tree is at `../../Braam/braam-core` or `-DBRAAM_CORE` names it.
- **The headers and libraries** are staged by CMake in `build/share/vcc/wasm32-braam`
  (`libc/wasm32/CMakeLists.txt`), where `build/cc/cc` finds them, so the tests run the
  driver end to end.

### 7.2 The runtime: `rt.c`

Written in C with coroutines, as braam-core's `src/proc/rt.cpp` is written in C++ with
them:

```c
static coro(braam_call *) int braam_root(int argc, char **argv)
{
    int status = await main(argc, argv);
    await fflush(NULL);             /* what the program left buffered, then exit */
    return status;
}

static void run(int k)              /* task k to its next suspension */
{
    if (co_resume(tasks[k].frame) == CO_DONE) {
        if (k == 0) {                   /* the root: the process exits */
            exited = 1;
            braam_sys_sync(BRAAM_SYS_EXIT, co_result(tasks[0].frame), 0, 0);
        } else {
            tasks[k].frame = NULL;      /* the slot is free again */
        }
        return;
    }
    braam_call *c = co_value(tasks[k].frame);
    c->token = ++token;  tasks[k].call = c;
    __braam_sys_async(c->op, c->token, c->ptr, c->len);
}
```

- **The table** has `BRAAM_TASKS` (8, braam-core's `PROC_TASKS`) entries, each a frame
  and the call it waits on; task 0 is the root.
- **`_start`** (`__braam_start`) copies argv out of the host's block, each word with a
  NUL, makes the root with `co_init` on a block of `__braam_task_bytes` from `malloc`,
  and runs it. The default size is a `libc.a` member of its own (`taskbytes.c`), which
  the linker leaves out when the program defines the name: vcc has no weak symbols.
- **`_resume(token, reply, len)`** gives the block to the task whose call has that
  token and runs it, or frees the block (an answer to a call whose frame was
  destroyed). Either returns 0 once the root has returned, 1 otherwise.
- **`braam_sys`** (`sys.c`) yields a `braam_call` that is a local of its frame, so the
  pointer stays good while it waits. Resumed, it reads the status, copies the data and
  frees the block. A `defer` in it (`__braam_forget`) tells the runtime when the frame
  goes before the answer comes: the task's slot is freed, so the table never points
  into a dead frame. That is what makes `co_destroy` of a waiting task safe, and why a
  task may suspend only in `braam_sys`.
- **`braam_spawn`** puts a frame in a free slot and runs it to its first suspension, as
  braam-core's `proc_spawn` does. Several tasks may park in one step, each with its
  call outstanding.
- **Signals.** `_sig(n)` records a bit, as braam-core's does. `sig_catch` is
  Sys::SigAct with a shadow copy of the mask, as braam-core's `io.cpp` keeps.
  `sig_take` and `sig_pending` read and clear the bits. The kernel does the rest: a
  caught signal abandons the interruptible calls (Read, KeyRead, Sleep, Wait, ClipRead,
  Poll), which answer `Err(Intr)`; an uncaught one cancels the process, status 130.
  `braam_yield` is `await sleep_ms(0)`.
- **`_alloc` and `_free`** are `malloc` and `free`. `malloc.c` is a first fit over an
  address-ordered free list, merging neighbours and calling `memory.grow` when nothing
  fits.
- **`exit(n)`** (`exports.s`) calls `sys(Exit, n)` and traps, as Braam's own compat
  layer does (`doc/Compat.md`: "C `exit()` and `abort()` trap"). A coroutine trap goes
  through `exit` too, its message left in the stdout buffer.

### 7.3 The library

The C library splits as Braam's own does (`braam-apps/CLAUDE.md`, "Three groups"):
what never waits is `libc/common`'s, and what waits keeps C's names and parameters
with a coroutine specifier, so a port adds `await` and `coro(braam_call *)` and
changes nothing else. Every call that waits is `await braam_sys(op, payload, len, …)`
plus a copy into the caller's buffer. On an error they return -1 and set `errno`.

- **Output** (`stdio.c`). `printf` and its kin are variadic, so they cannot be
  coroutines: they format into the stream's buffer, which grows by `realloc` rather
  than wait. `putbyte` writes to the stream of the `fprintf` running, stdout
  otherwise. `write` writes all of its bytes, in chunks.
- **Input.** Each stream has an input buffer of `BRAAM_CHUNK` bytes, with one byte in
  front for `ungetc`; `fgetc`, `fgets` and `fread` take from it and refill it with
  `read`. A `read` of standard input writes out standard output first, as a
  line-buffered terminal does.
- **Opening and closing.** `fopen` reads the mode as Braam's compat layer does; the
  streams it makes join the list that `fflush(NULL)` and the root's last flush walk.
  `fclose` writes the buffer out, closes the descriptor and frees the buffers, and for
  a stream from `fopen` the `FILE`.
- **Positioning.** `fseek` writes out pending output, moves the descriptor back past
  what was read ahead and not taken, and drops the input buffer; `ftell` and `rewind`
  go through it.
- **Files** (`sys.c`). `stat`, `lstat` and `fstat` fill a `struct stat` as Braam's
  `cstat.cpp` does: the mode from the kind, `st_ino` an FNV-1a hash of the path, the
  time in seconds. `unlink`, `rmdir` and `remove` are all Sys::Remove. `<sys/types.h>`
  makes `off_t` 64 bits, as Braam's answers carry it.

### 7.4 The fake kernel: `run.mjs`

`libc/wasm32/braam/run.mjs` lets the tests run without a Braam checkout.

- It checks the module as braam-core's `test/system/abi.mjs` does: the imports a
  subset of `env.memory`, `kernel.sys` and `kernel.sys_async`, exactly the five
  exports, one `braam` section with the magic, the ABI version and 1600 pages.
- It makes `env.memory` of the section's page counts, writes argv (the program's name
  first) through `_alloc`, and calls `_start`.
- It serves `kernel.sys` (Exit, GetPid, Now, Random) and, through `kernel.sys_async`:
  Write (fd 1 and 2 to stdout and stderr), Read (fd 0 from stdin, in 512-byte
  chunks), Open, Close, Read, Write, Seek and FStat on files, keeping each
  descriptor's position itself since node has no `lseek`; Stat, Remove, MkDir, Chdir
  and Rename on paths; Sleep by `setTimeout`; SigAct and Poll. Anything else is
  `Err(Unsupported)`.
- Each answer goes through `_resume` from a later macrotask, never from inside
  `sys_async`, so a program that does not return from the step is caught.
- **Signals.** A byte 0x03 on standard input is a `^C`: the read that reaches it parks,
  and the signal finds it parked. Node's own SIGINT and SIGTERM are passed on. A caught
  signal goes to `_sig`, and the parked calls (a Read of stdin, a Sleep, a Poll)
  answer `Err(Intr)`; an uncaught one ends the run with status 130. Poll answers a file
  and a redirected stdin as ready, and a terminal by what is buffered.

### 7.5 The system test

The ctest `braam-system` (`backend/wasm/test/braam_system.mjs`) is defined when
`BRAAM_CORE` is a built checkout, with `build/kernel.wasm` and `build/web/rootfs.zip`.
It:

- builds `hello`, `cat`, `wc` and `docs/examples/notes.c` with the in-tree driver;
- runs braam-core's `test/system/abi.mjs` over them;
- plants them in `/bin` of a session of braam-core's harness (`test/system/harness.mjs`),
  as `braam-apps/devel/c4/test/run.mjs` does;
- types command lines at the shell, the output redirected to a file and the status
  echoed to another: hello's arguments and status, `cat` and `wc` on a file and
  through `<`, a missing file, the whole `notes` session with `notes ask` reading the
  terminal up to `^D`, and `^C` three ways (caught by a read of the terminal while a
  second task sleeps, both interrupted; not caught, status 130; caught by a loop that
  parks in `braam_yield`). That last loop keeps the kernel busy, so the test runs a
  fixed number of ticks instead of the harness's `run()`, which waits until the
  kernel is idle.

The fake kernel and Braam give the same output for the same programs, but for the
pid.

### 7.6 Not done

- `scanf`, `tmpfile`, `setvbuf`, and Sys::List (reading directories).
- A recipe like braam-core's `braam_add_package` for packing programs into a `.zip`.
- [Coroutines_Internals.md](Coroutines_Internals.md) §8 has the open questions that
  touch Braam: `exit()` as a trap, `main(void)`, and a `defer` that could `await`.
