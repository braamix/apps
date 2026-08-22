# Braam applications

Software ported to [Braam](https://github.com/braamix/core), an operating system
that runs in a browser tab.

**Status: nothing is ported yet.** The tree is empty category directories, and a
few of them hold a `TODO.md` naming an upstream worth porting. There are no
sources and no build scripts.

## Layout

Categories, as in pkgsrc — `archivers`, `benchmarks`, `editors`, `games`,
`misc` — each holding one sub-directory per program. Every program builds into
a ZIP package that `/bin/pkg` installs.

## Building

A program is a freestanding C++20 file compiled to wasm32 against the Braam SDK.
There is no libc, so a port is a rewrite rather than a recompile.

Download and unpack the SDK — it is relocatable and needs no install:

    https://github.com/braamix/core/releases/download/v0.4/braam-sdk-0.4.162-6b94bea.zip

You also need clang with the wasm32 target, `wasm-ld`, CMake 3.24 and Python 3.

    cmake -B build --toolchain <sdk>/lib/cmake/braam/wasm32-unknown-unknown.cmake
    cmake --build build

The toolchain file must be on that first command; CMake picks the compiler when
a project is configured, and a build directory configured without it cannot be
repaired by adding the flag.

Please move to the newest SDK with each Braam release.

## Documents

* [Programming Manual](https://github.com/braamix/core/blob/main/doc/Programming_Manual.md)
  — writing a C++ program for Braam: the build, the API, the rules that bite.
* [Package Formats](https://github.com/braamix/core/blob/main/doc/Package_Formats.md)
  — what a package ZIP must contain.
* [Concept](https://github.com/braamix/core/blob/main/doc/Concept.md)
  — the specification.

## License

MIT, for what is in this repository. A ported program keeps its own.
