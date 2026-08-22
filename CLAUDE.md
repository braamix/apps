# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

A collection of third-party software ported to **Braam**, an operating system
that runs in a browser tab. Each program is a freestanding C++20 wasm32 binary,
compiled against the Braam SDK and shipped as a ZIP package that `/bin/pkg`
installs.

**One program is ported so far**, [benchmarks/dhrystone](benchmarks/dhrystone/),
and it is the worked example: it established the build, and the next port
copies its shape. The rest of the tree is category directories, a few holding a
one-line `TODO.md` naming the upstream to port:
[archivers/zip](archivers/zip/TODO.md),
[benchmarks/duremark](benchmarks/duremark/TODO.md), [editors/vi](editors/vi/TODO.md),
[emulators/simbesm](emulators/simbesm/TODO.md), [games/adventure](games/adventure/TODO.md),
[games/tetris](games/tetris/TODO.md), [misc/stat](misc/stat/TODO.md).

Layout is `<category>/<program>/`, categories borrowed from pkgsrc (`archivers`,
`benchmarks`, `editors`, `games`, `misc`, …). A directory with only a `TODO.md`
is a stated intention, not work in progress.

A port is a rewrite, and it keeps upstream's identifiers, structure and output
text: the value of porting a historic program is that it is still the same
program. Replace what touches the C library or the OS, and leave the rest —
including the comments — alone. Say in the program's `README.md` what had to
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
- `../braam-core/src/cmd/` — thirty-six worked examples of the program shape.
- `../braam-core/examples/hello/` — the minimal program and its `CMakeLists.txt`.
- `../braam-core/tools/mkpkg.py` — the package builder, and `mkrepo.py` beside
  it for a whole signed repository. `mkpkg.py` now installs with the SDK, but
  not in 0.4.162, the release this tree pins — so `make package` still reaches
  into the core tree for it. See Packaging below.

## Building

`make` at the top. It fetches the SDK named at the head of the
[Makefile](Makefile) into `build/`, unpacks it, configures against its
toolchain file and builds everything. `make SDK=<prefix>` uses an SDK that is
already unpacked and skips the download; `make clean` removes `build/` and the
next `make` fetches again. You need clang with the wasm32 target, `wasm-ld`,
CMake 3.24, Python 3, `curl` and `unzip`.

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
braam_add_program(NAME <name> SOURCES <name>.cpp)   # optional LIBS <...>
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

A port is a **rewrite, not a recompile.** There is no libc: no `malloc`, no
`printf`, no `errno`, no `<cstring>`, no exceptions and no RTTI. `-nostdlib
-nostdinc++` is not negotiable. The whole standard library is
`include/braam/kernel/` — `str.h`, `string.h`, `vec.h`, `span.h`, `result.h`,
`fmt.h`, `text.h`, `path.h`. Expect to keep upstream's algorithm and structure
and to replace every line that touches the C library or the OS.

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

Rules that are a compile error, link error or trap rather than a warning:

- Never `new` — `operator new` returns null and there are no exceptions. Use
  `heap_new` / `heap_delete` from `kernel/alloc.h`.
- A namespace-scope global must be trivially destructible (`__cxa_atexit` does
  not exist). Make it POD or hide it behind a pointer built on first use.
- Keep coroutine frames under 512 bytes; a frame past that costs a whole 64 KiB
  span. Long-lived state goes in a heap block the frame points at.
- The memory cap is 16 MB and it belongs to the kernel, not the binary.
- A construct needing a compiler-rt builtin — 128-bit division, an outlined
  `memcpy` — will not link. `fmod`, `pow` and `long double` are the same;
  ordinary `float`/`double` arithmetic and `sqrt` are native and fine.
- A libc routine a port supplies for itself must be **`extern "C"`**. The
  compiler emits calls of its own — `strlen` behind `Str`'s `const char *`
  constructor, for one — and they name the C symbol, which a C++-mangled
  definition does not satisfy. Add `-fno-builtin-<name>` for each so clang
  neither folds a call into inline code nor turns the implementation into a
  call to itself.
- A coroutine must not contain a hot loop by accident: inlining an ordinary
  function into one moves its locals into the heap-allocated frame. Mark the
  callee `__attribute__((noinline))` where that matters.

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
makes the package uninstallable.** A package must stay under 4 MiB.

Versions follow apk's grammar (`1.2-r0`), and nothing checks the spelling —
read it back.

**A zip on its own cannot be installed.** `pkg` has twelve subcommands and none
takes a path: every install resolves a name in a signed index and checks the
package's size and digest against it, with no `--force` in any form. Publishing
is `Package_Formats.md` §10 — keys, an anchor in `rootfs.zip`, a signed index —
and `../braam-core/tools/mkrepo.py` does the whole of it in forty lines.

`braam_add_package` and `mkpkg.py` reached the SDK after 0.4.162, the release
this tree pins, so [cmake/BraamPackage.cmake](cmake/BraamPackage.cmake) stands
in by driving `../braam-core/tools/mkpkg.py`. **Delete it and the block that
includes it in [CMakeLists.txt](CMakeLists.txt) when the pinned SDK carries its
own** — packaging is the one thing here that wants the core tree beside this
one.

## Verifying a binary

The module surface is exact, and a link that accidentally pulled in kernel code
shows up here first — imports `env.memory`, `kernel.sys`, `kernel.sys_async`
and nothing else; exports exactly `_alloc`, `_free`, `_resume`, `_start`;
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

From a built core tree, `test/smoke/abi.mjs` asserts all of it for any binary
named after the rootfs — silently, so a clean `smoke ok` is the pass:

```
cd ../braam-core && node test/run.mjs --kernel build/kernel.wasm \
    build/web/rootfs.zip <path>/<name>.wasm
```

`../braam-core/test/smoke/` is an additional working directory for this session;
`harness.mjs` there owns the kernel instance, the screen and the shell, and each
case is one file exporting `check()`.

**To actually run a program under that harness**, it has to be in the image:
copy `../braam-core/build/rootfs` aside, drop the `.wasm` in as `bin/<name>`,
repack with `../braam-core/tools/pack.py <dir> <out.zip>`, and drive the shell
with `init()` and `submit()` from `harness.mjs`. Note that **the harness clock
is frozen on purpose** — `fakeworker.mjs` says so — so `proc_now()` never
advances there and anything that measures elapsed time reads zero. Check that
in a browser instead.

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
