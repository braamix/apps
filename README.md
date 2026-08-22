# Braam applications

Software ported to [Braam](https://github.com/braamix/core), an operating system
that runs in a browser tab.

## What is here

| | |
| --- | --- |
| [benchmarks/dhrystone](benchmarks/dhrystone/) | Dhrystone 2.1, the 1984 integer benchmark |
| [benchmarks/duremark](benchmarks/duremark/) | DureMark 1.1, a small CoreMark-inspired benchmark |

The rest of the tree is category directories, a few of them holding a `TODO.md`
naming an upstream worth porting.

## Layout

Categories, as in pkgsrc — `archivers`, `benchmarks`, `editors`, `games`,
`misc` — each holding one sub-directory per program. Every program builds into
a ZIP package that `/bin/pkg` installs.

## Building

    make

That fetches the SDK into `build/`, unpacks it, and builds every program —
`build/<category>/<program>/<program>.wasm`. You need clang with the wasm32
target, `wasm-ld`, CMake 3.24, Python 3, `curl` and `unzip`.

    make package

builds the packages beside them, `<program>-<version>.zip`.

    make index

signs a repository into `build/repo/` — the packages and an index over them,
ready to upload to <https://pub.sergev.org/braam>, which is the repository
Braam ships configured. `pkg` installs nothing that a signed index does not vouch
for, so the zips and the index travel together.

Signing needs the publisher's index key, which lives outside this tree; the
index version must rise at every publication. The whole procedure is
[Package Formats](https://github.com/braamix/core/blob/main/doc/Package_Formats.md)
§10.

Packaging is also the one part that wants the core checkout next to this one,
until a Braam release later than 0.4.162 ships `mkpkg.py` in the SDK.

The SDK is currently

    https://github.com/braamix/core/releases/download/v0.4/braam-sdk-0.4.162-6b94bea.zip

named at the head of the [Makefile](Makefile). Please move to the newest with
each Braam release: a binary carries the process ABI it was built for, and the
kernel refuses one built for another. `make SDK=<prefix>` builds against an SDK
already unpacked somewhere and skips the download.

A program is freestanding C++20 compiled to wasm32. There is no libc, so a port
is a rewrite rather than a recompile — and one that keeps the original's
structure, names and output, replacing only what reached the C library or the
OS.

## Documents

* [Programming Manual](https://github.com/braamix/core/blob/main/doc/Programming_Manual.md)
  — writing a C++ program for Braam: the build, the API, the rules that bite.
* [Package Formats](https://github.com/braamix/core/blob/main/doc/Package_Formats.md)
  — what a package ZIP must contain.
* [Concept](https://github.com/braamix/core/blob/main/doc/Concept.md)
  — the specification.

## License

MIT, for what is in this repository. A ported program keeps its own.
