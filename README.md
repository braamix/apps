# Braam applications

Software ported to [Braam](https://github.com/braamix/core), an operating system
that runs in a browser tab.

## What is here

| | |
| --- | --- |
| [archivers/zip](archivers/zip/) | Info-ZIP zip 3.0, and zipnote, zipsplit and zipcloak with it |
| [converters/iconv](converters/iconv/) | Citrus iconv, and the two hundred character sets it converts between |
| [editors/le](editors/le/) | LE 1.16.8, the block editor, with its own curses |
| [editors/uemacs](editors/uemacs/) | uEmacs/PK 4.0.15, MicroEMACS as Linus Torvalds keeps it |
| [editors/vi](editors/vi/) | UCB vi 3.6, and ex under it — the editor Bill Joy wrote |
| [benchmarks/dhrystone](benchmarks/dhrystone/) | Dhrystone 2.1, the 1984 integer benchmark |
| [benchmarks/duremark](benchmarks/duremark/) | DureMark 1.1, a small CoreMark-inspired benchmark |
| [games/adventure](games/adventure/) | Colossal Cave Adventure, the 1977 C re-coding |
| [games/asciifluid](games/asciifluid/) | IOCCC 2012/endoh1, a fluid simulator that fits on a screen |
| [games/asciiquarium](games/asciiquarium/) | goquarium, the ASCII aquarium — fish, sharks, whales and a castle |
| [emulators/simbesm](emulators/simbesm/) | The BESM-6, the Soviet mainframe — it boots Unix |

The rest of the tree is category directories, a few of them holding a `TODO.md`
naming an upstream worth porting.

## Layout

Categories, as in pkgsrc — `archivers`, `benchmarks`, `converters`, `editors`,
`games`, `misc` — each holding one sub-directory per program. Every program
builds into a ZIP package that `/bin/pkg` installs.

## Building

    make

That fetches the SDK into `build/`, unpacks it, and builds every program —
`build/<category>/<program>/<program>.wasm`. You need clang with the wasm32
target, `wasm-ld`, CMake 3.24, Python 3, `curl` and `unzip`.

    make package

builds the packages beside them, `<program>-<version>.zip`.

    make test

runs what headless tests there are — `archivers/zip`, which writes archives and
reads them back with Braam's own `/bin/unzip`; `games/adventure`, which plays a
whole game of Colossal Cave and interrupts a second one with `^C`; the two
editors, driven a keystroke at a time and asserted cell by cell;
`converters/iconv`, which checks 137,385 mappings against GNU libiconv's own
answers; `games/asciifluid`, whose frames are compared with the ones upstream's
own binary paints; `games/asciiquarium`, a seeded aquarium asserted frame by
frame and driven by its own keys; and the two benchmarks, each stopped partway
by a signal. All of them need node and a sibling `../braam-core` built.

    make index

signs a repository into `build/repo/` — the packages and an index over them,
ready to upload to <https://braamix.github.io>, which is the repository
Braam ships configured. `pkg` installs nothing that a signed index does not vouch
for, so the zips and the index travel together.

Signing needs the publisher's index key, which lives outside this tree; the
index version must rise at every publication. The whole procedure is
[Package Formats](https://github.com/braamix/core/blob/main/doc/Package_Formats.md)
§10.

The SDK is currently

    https://github.com/braamix/core/releases/download/v0.9/braam-sdk-0.9.253-45315e4.zip

named at the head of the [Makefile](Makefile). Please move to the newest with
each Braam release: a binary carries the process ABI it was built for, and the
kernel refuses one built for another. `make SDK=<prefix>` builds against an SDK
already unpacked somewhere and skips the download.

A program is freestanding C++20 compiled to wasm32, and a port is a rewrite
rather than a recompile — one that keeps the original's structure, names and
output, replacing only what reached the OS.

The C library a port calls is the SDK's opt-in **port kit**, asked for with
`PORT` on `braam_add_program`: `mem*`, `str*`, `ctype`, the allocator, `strtol`,
`snprintf`, `qsort` and `getenv`, with exact C signatures. Nothing that blocks
is in it — a C signature cannot block here — so each port still writes its own
streams. `benchmarks/dhrystone` does not link it at all, because `strcpy` and
`strcmp` are what it measures.

## Documents

* [Programming Manual](https://github.com/braamix/core/blob/main/doc/Programming_Manual.md)
  — writing a C++ program for Braam: the build, the API, the rules that bite.
* [Package Formats](https://github.com/braamix/core/blob/main/doc/Package_Formats.md)
  — what a package ZIP must contain.
* [Concept](https://github.com/braamix/core/blob/main/doc/Concept.md)
  — the specification.

## License

MIT, for what is in this repository. A ported program keeps its own.
