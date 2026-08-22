# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

A collection of third-party software ported to **Braam**, an operating system
that runs in a browser tab. Each program is a freestanding C++20 wasm32 binary,
compiled against the Braam SDK and shipped as a ZIP package that `/bin/pkg`
installs.

**Almost nothing is written yet.** The tree is category directories, and a
handful of them hold a one-line `TODO.md` naming the upstream to port:
[archivers/zip](archivers/zip/TODO.md), [benchmarks/dhrystone](benchmarks/dhrystone/TODO.md),
[benchmarks/duremark](benchmarks/duremark/TODO.md), [editors/vi](editors/vi/TODO.md),
[emulators/simbesm](emulators/simbesm/TODO.md), [games/adventure](games/adventure/TODO.md),
[games/tetris](games/tetris/TODO.md), [misc/stat](misc/stat/TODO.md). There is no
build system at the top level and no shared machinery yet — the first port
establishes both, so choose its layout deliberately.

Layout is `<category>/<program>/`, categories borrowed from pkgsrc (`archivers`,
`benchmarks`, `editors`, `games`, `misc`, …). A directory with only a `TODO.md`
is a stated intention, not work in progress.

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
- `../braam-core/tools/mkpkg.py` — the package builder. **It is not in the SDK**;
  the SDK ships only `stamp.py`. A package built here needs it from the core
  tree, or a local equivalent.

## Building a program

Get an SDK from `https://github.com/braamix/core/releases/download/v*/braam-sdk-*.zip`
(the version [README.md](README.md) names, currently 0.4.162) and unpack it
anywhere — the tree is relocatable, needs no install and no environment
variable. Or `make install` in `../braam-core`. You also need clang with the
wasm32 target, `wasm-ld`, CMake 3.24 and Python 3.

```cmake
cmake_minimum_required(VERSION 3.24)
project(<name> LANGUAGES CXX)
find_package(braam REQUIRED)
braam_add_program(NAME <name> SOURCES <name>.cpp)   # optional LIBS <...>
```

```
cmake -B build --toolchain <sdk-prefix>/lib/cmake/braam/wasm32-unknown-unknown.cmake
cmake --build build
```

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
  `memcpy` — will not link.

A command whose work is running other commands should be a `/bin/sh` script
instead, with an absolute `#!` interpreter; that needs no toolchain at all.

## Packaging

Per `Package_Formats.md` §5: a zip whose top-level dot-entries are metadata and
whose everything else is payload, unpacked into `/pkg/store/<name>-<version>/`.
`.PKGINFO` is required and carries the §3.2 letters less `C` and `S`, in the
order `P V I T o t k g D p i`. Optional `/bin/sh` hooks: `.pre-install`,
`.post-install`, `.pre-deinstall`, `.post-deinstall`, `.pre-upgrade`,
`.post-upgrade`, `.trigger`. **An unknown top-level dot-entry makes the package
uninstallable.**

```
../braam-core/tools/mkpkg.py --out <name>-<version>.zip --name <name> --version <v> \
    [--field T=<description>] [--field D=<depends>] build/<name>.wasm=bin/<name>
```

Versions follow apk's grammar (`1.2-r0`).

## Verifying a binary

The module surface is exact, and a link that accidentally pulled in kernel code
shows up here first — imports `env.memory`, `kernel.sys`, `kernel.sys_async`
and nothing else; exports exactly `_alloc`, `_free`, `_resume`, `_start`;
one custom section `braam` of five little-endian `u32`s.

```
node -e 'const m=new WebAssembly.Module(require("fs").readFileSync("build/<name>.wasm"));
  console.log(WebAssembly.Module.imports(m).map(i=>i.module+"."+i.name));
  console.log(WebAssembly.Module.exports(m).map(e=>e.name));
  console.log(new Uint32Array(WebAssembly.Module.customSections(m,"braam")[0]))'
```

From a built core tree, `test/smoke/abi.mjs` asserts all of it for a binary you
hand it, and the smoke harness will boot a kernel and run it:

```
cd ../braam-core && node test/run.mjs --kernel build/kernel.wasm build/web/rootfs.zip <path>/<name>.wasm
```

`../braam-core/test/smoke/` is an additional working directory for this session;
`harness.mjs` there owns the kernel instance, the screen and the shell, and each
case is one file exporting `check()`.

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
- C++ follows `../braam-core/.clang-format`: 4-space indent, 100 columns, types
  `PascalCase`, functions and variables `snake_case`, constants
  `SCREAMING_SNAKE`.
- Update the SDK version in [README.md](README.md) with each Braam release.
- Commit only when asked.
