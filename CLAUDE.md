# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

A collection of third-party software ported to **Braam**, an operating system
that runs in a browser tab. Each program is a freestanding C++20 wasm32 binary,
compiled against the Braam SDK and shipped as a ZIP package that `/bin/pkg`
installs.

**Nine programs are ported so far**:
[benchmarks/dhrystone](benchmarks/dhrystone/), which established the build and
is the worked example a new port copies;
[benchmarks/duremark](benchmarks/duremark/), which shows the other shape — an
upstream that already has a porting layer, where the port is a third
implementation of it rather than a rewrite; and
[games/adventure](games/adventure/), which shows what a program that reads,
writes files and carries its own data has to do here; and
[archivers/zip](archivers/zip/), the largest by far — Info-ZIP's zip 3.0, four
commands out of one set of sources, where nearly every function became a
coroutine because nearly every one of them reaches a stream; and
[converters/iconv](converters/iconv/), Citrus iconv with two hundred character
sets, which is the opposite shape — it reads files only while opening a
conversion, so the coroutines stop at the door and the whole conversion path is
untouched C. Its 44 MB of tables are compiled from upstream's own sources by
Python replacements for `mkcsmapper` and `mkesdb`, byte for byte; and
[editors/vi](editors/vi/), UCB ex/vi 3.6 — a third shape again, where the
hard part is neither streams nor data but *control flow*: `error()` was a
`longjmp` from arbitrary depth and there is no `setjmp` to be had, so it
records and unwinds a frame at a time. Its other half is the screen — vi kept
an exact image of the terminal in `vtube` to work out the fewest bytes to
send, and that image is now the back buffer for a damage-tracked Grid; and
[editors/uemacs](editors/uemacs/), uEmacs/PK 4.0.15, whose hard part is
*breadth*: the one blocking read sits under `ask_string()` and `getcmd()`,
which nearly every command calls, and the commands are reached through two
function-pointer tables — so changing `fn_t` converted 244 of 377 functions
at once. It is also where the tree learned that **a `co_await` is a call and
not a tail call**, so a loop that awaits without ever suspending grows the
native stack until the process traps; see its README; and
[editors/le](editors/le/), LE 1.16.8, the block editor — the largest surface of
all of them, since it brings its own curses, its own regex and its own wide
half; and
[emulators/simbesm](emulators/simbesm/), the BESM-6 simulator, which boots
Unix — a fourth shape, where the hard part is that **the program is a machine**
and cannot stop to wait. A `co_await` cannot appear in an instruction loop, so
the port is a *driver*: `cpu_burst()` runs plain C++ until it has something for
its caller to do — a transfer, the burst being up, or a stop — and everything
that blocks is in the one file the caller lives in, `braam.cpp` or `host.cpp`.
A disk transfer is deferred out of the instruction that starts it and becomes
*data* the driver performs, which is also the more faithful model; the console
is a buffer the driver drains and a ring a key task fills; and upstream's
telnet line is `Sys::TermOpen`, so the machine's second terminal is a second
Braam screen. Its 43 `longjmp`s to `cpu_halt` became returned stop codes, and
the SIMH framework it came with is gone: 2.6k lines of it are 620 of
`machine.cpp`. It keeps a **native build** as well, which `tests/unix.exp`
drives, and that is how every step of the port was checked — including
byte-for-byte that the deferred transfers write the same packs.
The rest of the tree is category directories, a few
holding a one-line `TODO.md` naming the upstream to port:
[games/tetris](games/tetris/TODO.md), [misc/stat](misc/stat/TODO.md).

Layout is `<category>/<program>/`, categories borrowed from pkgsrc (`archivers`,
`benchmarks`, `editors`, `games`, `misc`, …). A directory with only a `TODO.md`
is a stated intention, not work in progress.

A port is a rewrite, and it keeps upstream's identifiers, structure and output
text: the value of porting a historic program is that it is still the same
program. Replace what touches the OS, take the C library from the port kit, and
leave the rest — including the comments — alone. Say in the program's `README.md` what had to
change and why.

## Related trees

Braam itself is a sibling checkout at `../braam-core` (also on GitHub as
`braamix/core`). It is the authoritative reference and worth reading before
writing any code here:

- `../braam-core/doc/Programming_Manual.md` — the build and the API for an
  out-of-tree program. **Read this first.**
- `../braam-core/doc/Package_Formats.md` §5 — what a package zip must contain.
- `../braam-core/doc/System_Calls.md`, `Concept.md`, `Shell.md` — the mechanism,
  the specification, the shell language.
- `../braam-core/src/cmd/` — forty-four worked examples of the program shape.
- `../braam-core/examples/hello/` — the minimal program and its `CMakeLists.txt`.
- `../braam-core/tools/mkpkg.py` — the package builder, and `mkrepo.py` beside
  it for a whole signed repository. Both install with the SDK as of 0.5.172, so
  nothing here reaches into the core tree for them any more; only the tests do.

## Building

`make` at the top. It fetches the SDK named at the head of the
[Makefile](Makefile) into `build/`, unpacks it, configures against its
toolchain file and builds everything. `make SDK=<prefix>` uses an SDK that is
already unpacked and skips the download; `make clean` removes `build/` and the
next `make` fetches again. `make test` runs the headless tests, for which you
also need node and a built `../braam-core` — see Testing a program below. You
need clang with the wasm32 target, `wasm-ld`, CMake 3.24, Python 3, `curl` and
`unzip`.

**The SDK version lives in the Makefile**, and [README.md](README.md) repeats
it. Move both with each Braam release: a binary stamped for another process ABI
is refused at exec.

Three levels of CMake, and a new program touches all three: the top
[CMakeLists.txt](CMakeLists.txt) adds each category, a category's adds each
program, and the program's own calls `braam_add_program`. Copy
[benchmarks/dhrystone/CMakeLists.txt](benchmarks/dhrystone/CMakeLists.txt),
which is guarded so the directory also configures standalone:

```cmake
cmake_minimum_required(VERSION 3.24)
project(<name> LANGUAGES CXX)
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()
braam_add_program(NAME <name> SOURCES <name>.cpp)   # optional LIBS <...>, PORT
```

The default build type is `MinSizeRel` (`-Os`), which the toolchain file sets.
A program that should be built otherwise says so itself, as dhrystone does with
`target_compile_options(bin_<name> PRIVATE -O3)`.

**The toolchain file must be on the first configure and only there.** CMake
fixes the compiler when a project is configured; a build directory configured
without it holds a host compiler and cannot be repaired by adding the flag —
delete the directory and configure again. Symptoms are `sizeof(usize) == 4,
"wasm32"`, `unknown type name '__externref_t'`, or `find_package(braam)`
refusing the compiler outright.

`braam_add_program` defines target `bin_<name>` producing `<name>.wasm`; it
links `braam::proc` and `braam::flags`, links `--import-memory`, and runs
`stamp.py`, which reads the process-ABI number out of the SDK's own
`kernel/sysabi.h`. A binary stamped for the wrong ABI is refused at exec with
`built for another process ABI` — rebuild against the matching SDK.

## The program shape

A port is a **rewrite, not a recompile.** There are no exceptions and no RTTI,
and `-nostdlib -nostdinc++` is not negotiable. The whole C++ standard library is
`include/braam/kernel/` — `str.h`, `string.h`, `vec.h`, `span.h`, `result.h`,
`fmt.h`, `text.h`, `path.h`. Expect to keep upstream's algorithm and structure
and to replace every line that touches the OS.

**A ported program may ask for a C library; nothing else here has one.**
`braam_add_program(... PORT)` links `braam::compat`, the port kit
(`../braam-core/doc/Compat.md`), puts `<string.h>` and the rest on **that
target's** include path and no other's, and applies `-fno-builtin` once — in
place of the per-name lists five packages used to carry. Without `PORT`,
`#include <string.h>` is still "file not found", and that is the guard.

Three groups, and the difference decides how much of a port changes:

- **Group A is drop-in**: `mem*`, `str*`, `ctype`, `malloc`/`calloc`/`realloc`/
  `free`, the `strtol` family, `qsort`/`mergesort`/`bsearch`, `snprintf` and
  friends, `errno`, `strerror`, `getenv`. Exact C signatures.
- **Group B blocks, so it does not exist as C.** Streams, descriptors and
  directories: a C signature cannot block here. `<stdio.h>` declares `fopen`,
  `fgetc` and the rest `unavailable` so one build names every call site, but
  the `b_*` replacements are **not in this SDK** — every port keeps the stream
  layer it wrote (`zip`'s `zfopen`, `le`'s `lefile.cpp`, `uemacs`'s
  `fileio.cpp`).
- **Group C is absent**: `setjmp`/`longjmp`, `fork`, `setenv`, `dlopen`, `mmap`,
  signal handlers, `<time.h>`. Also not yet in the kit: `wchar`/`wctype`,
  `fnmatch`, `sscanf`, `strtod`.

Watch for: `qsort` is heapsort and **not stable** — `mergesort` is the stable
one, and `zip` uses it at two sites; `strerror` returns `"ENOENT"`, not prose;
`PATH_MAX` is 512, which `iconv` overrides back to 256 because its paths live
in coroutine frames; and a port that spells a global `errno` while storing an
`Error` in it must rename it, as `vi` (`ex_errno`) and `le` (`le_errno`) did,
or the kit's `strtol` will write `ERANGE` into it.

`benchmarks/dhrystone` **never** links the kit: its `strcpy` and `strcmp` are
what it measures.

```cpp
#include "proc/io.h"

Task<i32> proc_main(Args args)   // no main, no argc/argv; return value is the status
{
    Input in(args.tail(), SYS_STDIN, "<progname>");   // named files, or stdin
    ...
    co_return 0;
}
```

Everything that would block is a `co_await` returning `Task<Result<T>>`,
unpacked with `.is_err()` / `.error()` / `.value()`, or propagated with
`CO_TRY`. Conventions every program in `src/cmd/` follows, and ports should
too:

- `Error::Closed` is a normal end of input, not a failure.
- `Error::Cancelled` is `^C`; exit 130 for it.
- Format into a stack `Buf<N>` and write once — a write per field is a syscall
  per field, and a syscall is two `postMessage` hops (34–45 µs) because every
  program runs in its own Web Worker.
- `OptParse` with a `constexpr Opts SPEC` (`proc/opt.h`) parses the command line
  without allocating.
- `tty_of(SYS_STDOUT)` reports whether output is a terminal and how wide;
  geometry is zero for a pipe or file.

**A libm is available and is not linked by default.** `braam_add_program(...
LIBS braam::math)` brings in `math/math.h`, C99 §7.12 answered by vendored
musl, and `math/ftoa.h`, which is printf's float conversions — `put_f64(buf, v,
prec, 'f')` into an ordinary `Buf<N>`, plus `parse_f64`/`scan_f64` for strtod.
A program pays only for what it calls: `--gc-sections` extracts nothing else,
and the float printf engine alone is 6–7 KB. Link it where a port formats a
non-integer number by hand, and nowhere else.

**A buffered stream is available and none of these three ports uses it.**
`proc/file.h`'s `File` is stdio's shape — `get()` a rune, `put()`, `write()`,
`getline()`, a sticky error checked once rather than per character, and an
exit-time flush. It costs ~7 KB and `--gc-sections` keeps every byte of it out
of a binary that does not name the header; `braam_add_program` links nothing
extra for it. It pays where a port's inner loop is `while ((c = getchar()) !=
EOF)`, which has no other expressible form here, and nowhere else: a program
that already formats into a `Buf<N>` and writes once has no syscall left for it
to save. Two rules it states and does not enforce — a buffered `File` owns its
stream until `close()` or `detach()`, and `~File` does not flush. `get()` and
`put()` are awaiters, not `Task`s, so a plain non-coroutine function cannot
reach them at all; that is what keeps adventure's `adv_printf` on its own
buffer.

The rest of what 0.7 put within a port's reach: `stat_fd` (`proc/io.h`) sizes an
open descriptor, for a format whose directory is at the end of the file;
`SYS_O_EXCL` beside `SYS_O_CREATE` makes a create fail on a name already taken;
`proc_random()` (`proc/rt.h`) is 32 bits out of `crypto.getRandomValues`, with
no error, where a seed used to be guessed from the clock and the pid; and
`/dev/null`, `/dev/zero`, `/dev/random` and `/dev/urandom` are open-able paths.
`rune_lower`/`rune_upper` in `kernel/text.h` are ASCII through Cyrillic by
range.

**Signals are asked for, or the default action stands.** `sig_catch(SIG_INT)`
(`proc/io.h`) says a signal should be delivered rather than acted on; the mask
starts empty, so a program that asks for nothing behaves as every program did
before signals existed. `SIG_INT`, `SIG_TERM` and `SIG_WINCH` are the whole
catchable set. A delivered signal abandons the call the process is parked on
with `Err(Intr)` — only `Read`, `KeyRead`, `Sleep`, `Wait` and `ClipRead` can
answer it — and `sig_take(SIG)` (`proc/rt.h`) then says which one it was:

```cpp
if (r.is_err() && r.error() == Error::Intr && sig_take(SIG_INT))
    ...                             // interrupted, and still alive
```

`Err(Intr)` is not `Err(Cancelled)`: the first means the call was abandoned and
nothing happened, the second that the process is being destroyed. A signal is
delivered where a process *parks*, so a compute loop with no `co_await` in it
cannot be interrupted at all — a long one has to be entered in stretches, with
something awaited between two of them, for a `^C` to reach it. The SDK's manual
documents `braam::math` but not signals; `../braam-core/doc/Concept.md` §3.5 is
where they are written down.

Rules that are a compile error, link error or trap rather than a warning:

- Never `new` — `operator new` returns null and there are no exceptions. Use
  `heap_new` / `heap_delete` from `kernel/alloc.h`.
- A namespace-scope global must be trivially destructible (`__cxa_atexit` does
  not exist). Make it POD or hide it behind a pointer built on first use.
- Keep coroutine frames under 512 bytes; a frame past that costs a whole 64 KiB
  span. Long-lived state goes in a heap block the frame points at.
- The memory cap is 16 MB and it belongs to the kernel, not the binary.
- A construct needing a compiler-rt builtin — 128-bit division, an outlined
  `memcpy` — will not link. **`long double` is the same**: it is 113-bit quad
  here, so the `l`-suffixed half of `<math.h>` does not exist. Ordinary
  `float`/`double` arithmetic and `sqrt` are native and fine, and everything
  else C99 declares is `braam::math` (below).
- A libc routine a port still supplies for itself — one the kit has not got —
  must be **`extern "C"`**. The compiler emits calls of its own and they name
  the C symbol, which a C++-mangled definition does not satisfy. `PORT` carries
  `-fno-builtin` for the whole target, so clang neither folds a call into
  inline code nor turns the implementation into a call to itself. **Never
  define a name the kit already defines**: an archive member is pulled only
  when something needs it, so a duplicate links today and breaks the day an
  unrelated call reaches the member that also defines it.
- A coroutine must not contain a hot loop by accident: inlining an ordinary
  function into one moves its locals into the heap-allocated frame. Mark the
  callee `__attribute__((noinline))` where that matters.
- **A `co_await` is a call, not a tail call.** The wasm tail-call feature is
  off, so entering a task and returning from it each leave a frame on the
  native stack, and it is only given back where something *suspends* — a
  syscall unwinds the whole stack and the kernel re-enters at `_resume`. So a
  loop that awaits N times without ever suspending costs O(N) stack and traps
  at a few hundred turns. Keep the callee a plain function where it cannot
  block, and where the loop is unbounded, park on something every so often;
  `exec_yield()` in [editors/uemacs/exec.cpp](editors/uemacs/exec.cpp) is the
  worked example.

A command whose work is running other commands should be a `/bin/sh` script
instead, with an absolute `#!` interpreter; that needs no toolchain at all.

## Packaging

`make package` builds every program's zip. A program declares one beside its
`braam_add_program`:

```cmake
braam_add_package(NAME <name> VERSION <v>
                  FIELD "T=<description>" [FIELD "D=<depends>"]...
                  FILES $<TARGET_FILE:bin_<name>>=bin/<name>)
add_dependencies(packages pkg_<name>)
```

**Quote a `FIELD` whose value has a space**, or CMake splits it and the value
is silently truncated — `braam_add_package` refuses what does not look like
`<letter>=<value>` to catch that.

**`bin/` is what reaches `PATH`**, and the entry's leaf is the command's name.
Every flat entry becomes a link in the installed generation's `bin/`, which
`/pkg/bin` points at and the default `PATH` (`/bin:/pkg/bin`) already names,
plus a generated `cmd:<entry>` provide. So `bin/<name>`, never
`bin/<name>.wasm`, and never nested: `bin/sub/tool` yields no command.

Per `Package_Formats.md` §5: a zip whose top-level dot-entries are metadata and
whose everything else is payload, unpacked into `/pkg/store/<name>-<version>/`.
`.PKGINFO` is written by the tool, carries the §3.2 letters less `C` and `S` in
the order `P V I T o t k g D p i`, and only `P` and `V` are required — `p:`
`cmd:` names are the publisher's, generated into the index. Optional `/bin/sh`
hooks: `.pre-install`, `.post-install`, `.pre-deinstall`, `.post-deinstall`,
`.pre-upgrade`, `.post-upgrade`, `.trigger`. **An unknown top-level dot-entry
makes the package uninstallable.** A package must stay under 4 MiB compressed
and unpack to no more than 50 MiB.

Versions follow apk's grammar (`1.2-r0`), and nothing checks the spelling —
read it back.

**A zip on its own cannot be installed.** `pkg` has twelve subcommands and none
takes a path: every install resolves a name in a signed index and checks the
package's size and digest against it, with no `--force` in any form.

## Publishing

`make index` writes `build/repo/` — the signed index and the zips it vouches
for, in one directory because a package's URL is derived from the index's own
`N`. That directory is what gets uploaded to `https://braamix.github.io`,
which is the URL `rootfs/etc/repositories` ships and which `N` must equal byte
for byte.

Three variables at the head of the [Makefile](Makefile) govern it:

- **`INDEX_VERSION`** — `G`. **It must rise at every publication**; a client
  refuses an index below the one it holds, and equal means "unchanged" and no
  rewrite. It cannot be derived — only the publisher knows what was uploaded
  last.
- **`INDEX_EXPIRY`** — `E`, milliseconds. A promise to re-sign by then.
- **`INDEX_KEY`** — the publisher's private key, `~/.ssh/braam/index.key`,
  outside this tree and never copied into it. Its public half must be the
  `K:index` of the anchor the client boots with, or the index is refused at the
  `signature` step. Re-signing needs only this key; the root keys are for
  anchors alone.

Re-publishing is the whole set every time: `mkindex.py` reads `C`, `S` and the
`cmd:` provides out of the zips, so a package rebuilt without a re-signed index
fails as `not in the index` or `digest does not match`.

A zip is reproducible only with **`SOURCE_DATE_EPOCH`** set — `mkpkg.py` stamps
entries with the pack time otherwise, so two builds of identical sources differ
in their bytes and therefore in `C`. Set it when a rebuild should produce the
package that is already published.

To check a repository end to end without uploading, drive
`../braam-core/test/system/harness.mjs` with the unmodified rootfs and serve
`index` and the zips from `net.routes` — the shipped anchor is the real one, so
`pkg update` and `pkg install` exercise §7 in full.

## Verifying a binary

The module surface is exact, and a link that accidentally pulled in kernel code
shows up here first — imports `env.memory`, `kernel.sys`, `kernel.sys_async`
and nothing else; exports exactly `_alloc`, `_free`, `_sig`, `_start`, `_resume`
(`_sig` is the word the kernel writes a delivered signal into, new in ABI 17);
one custom section `braam` of five little-endian `u32`s.

The section's second word is the process ABI, and it must match the running
kernel's `PROC_ABI`.

```
node -e 'const m=new WebAssembly.Module(require("fs").readFileSync(
    "build/<category>/<name>/<name>.wasm"));
  console.log(WebAssembly.Module.imports(m).map(i=>i.module+"."+i.name));
  console.log(WebAssembly.Module.exports(m).map(e=>e.name));
  console.log(new Uint32Array(WebAssembly.Module.customSections(m,"braam")[0]))'
```

From a built core tree, `test/system/abi.mjs` asserts all of it for any binary
named after the rootfs — silently, so a clean `system ok` is the pass:

```
cd ../braam-core && node test/run.mjs --kernel build/kernel.wasm \
    build/web/rootfs.zip <path>/<name>.wasm
```

`../braam-core/test/system/` is an additional working directory for this
session; `harness.mjs` there owns the kernel instance, the screen and the shell,
and each case is one file exporting `check()`.

**To actually run a program under that harness**, plant it: after `init()` and
the first tick, `store.files.set("/bin/<name>", bytes)` puts it where `PATH`
looks, because `exec` takes any path carrying a well-formed stamp and `/bin` is
ordinary store content once boot has unpacked the archive. Then drive the shell
with `submit()`. Repacking `../braam-core/build/rootfs` with
`../braam-core/tools/pack.py` is the other way and is not needed for this.
Note that **the harness clock is frozen on purpose** — `fakeworker.mjs` says
so — so `proc_now()` never advances there and anything that measures elapsed
time reads zero. Check that in a browser instead.

## Testing a program

`make test` runs every program's headless tests — adventure's four and one for
each benchmark. [games/adventure/test/](games/adventure/test/) is the worked
example, the way dhrystone is the worked example for the build: `play.mjs`
imports `../braam-core/test/system/harness.mjs` directly — `test/run.mjs` is not
reusable, its case list is a literal and it never injects an out-of-tree
binary — boots the kernel, plants the `.wasm`, and drives one run. It asserts
what the run meant (landmarks in order, the score, no parser refusal) and then
the transcript byte for byte against a golden file beside it, which is exact
because the run is deterministic.

A program that reads and writes should be tested through **stdin and stdout
redirected to files**, not through the grid: `store.files.set("/tmp/w", …)`,
`submit("<name> </tmp/w >/tmp/g")`, then read `/tmp/g` back out of the store.
A long transcript does not fit on 24 rows, and a program reading a file behaves
differently from one reading the console — adventure prints no prompt and
echoes nothing over a pipe, which is what makes its transcript assertable.

Two things the harness imposes: the keyboard is `Channel<Key, 64>` and `type()`
posts a whole line without checking, so keep a submitted command line under
sixty characters; and `run(now)` returns `-1` when the kernel is idle, which is
how a test waits — there is no sleep and no timeout anywhere in the suite.

**A signal has to be aimed at a process that parks.** `run(now)` pumps until
the kernel is idle, so a foreground program that computes without parking runs
to completion inside one call and leaves no window to press `^C` in — which is
what `press("c".codePointAt(0), CTRL)` between two `run`s relies on
(`games/adventure/test/interrupt.mjs`, and `signal.mjs` in the core suite). For
a compute-bound one, queue two command lines before the first tick — the job in
the background and a `kill -INT %1` behind it — and the shell takes its turn
where the program parks, which is the moment under test (both benchmarks'
`test/interrupt.mjs`).

**The clock is frozen, and that decides what can be tested at all.** Anything
whose control flow reads `proc_now()` behaves differently here: dhrystone
always takes its "measured time too small" branch, and duremark's ladder never
converges — every step measures zero, so the iteration count climbs until the
twenty-step limit, and only a signal ends the run. What a benchmark *measures*
has to be checked in a browser; what it does around the measurement can be
tested here.

**Anything random has to be pinned.** The service clock is a fixed epoch and
pids are deterministic, so a seed taken from them is reproducible by accident
and moves the moment process ordering does. adventure takes `ADVENTURE_SEED`
from `proc_env` for this; the shell's `VAR=x cmd` prefix puts it in the child's
environment alone.

## Running without rebuilding the system

`exec` accepts any path carrying a well-formed stamp, so a `.wasm` that arrives
at runtime is a command: `fimport` and pick the file (it lands in `/import/`),
or `curl … > /home/<name>`. `PATH` is `/bin` at boot, which is a read-only view
of the archive beside the kernel — putting a program *there* means rebuilding
the image; anywhere else needs only a `PATH` that names it. While iterating,
note the host caches a compiled module by path: replacing a program at a path
already run in this page needs a reload, or write the new one beside the old.

## Conventions

- Keep comments terse; say what, not why.
- Markdown wraps at 80 columns.
- C++ follows [.clang-format](.clang-format) at the root (the same file as
  braam-core's): 4-space indent, 100 columns, types `PascalCase`, functions and
  variables `snake_case`, constants `SCREAMING_SNAKE`. **Ported code is
  clang-formatted too** — upstream's identifiers, statements, comments and
  output are what a port keeps; its whitespace is not.
- Update the SDK version in [Makefile](Makefile) and [README.md](README.md)
  with each Braam release.
- Commit only when asked.
