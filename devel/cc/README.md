# cc — vcc, the C compiler with coroutines, for Braam

This is vcc, Serge Vakulenko's C11 compiler with `defer` and coroutines, cut
down to its `wasm32-braam` target. It runs on Braam as the package `cc`,
and is on its way to compiling itself there. [Plan.md](Plan.md) is the
plan; this file says where it stands.

## Upstream

vcc at commit `937a00c4` (`~/Project/Claude/vcc`), MIT ([LICENSE](LICENSE)).
The directory layout is upstream's, so a file here is at the same path as
there:

| Directory | What it is |
| --- | --- |
| `cc/` | the driver |
| `cpp/` | the preprocessor |
| `scanner/`, `parser/`, `ast/` | `parse`: C to a syntax tree |
| `semantic/`, `translator/`, `optimize/`, `tac/` | `lower`: the tree to three-address code |
| `backend/common/`, `backend/wasm/` | `genwasm`: three-address code to wasm assembly |
| `libutil/` | memory, strings, word streams, diagnostics |
| `libc/` | the runtime of `wasm32-braam`: the parts of upstream's `common/`, `ilp32/` and `wasm32/` it links, and `wasm32/braam/` |
| `docs/` | upstream's documents for this target and for coroutines |
| `scripts/check_braam_abi.py` | the ABI numbers against braam-core's |

Not taken: the other eight backends and their runtimes, the hosted targets,
`grammar/`, the benchmarks and the GoogleTest suites.

## What changed from upstream

Only what serves another target went. The shape of every pass, the
syntax tree and the three-address code are upstream's, and so is every
byte of assembly the compiler writes.

- **Targets.** `cpp` and `lower` know `wasm32` and `wasm32-braam`, the
  latter by default; the driver knows `wasm32-braam` alone.
  `wasm32` stays in the passes because it is the same descriptor less
  Braam's process model, so a program whose `main` is not a coroutine can
  still be compiled to assembly.
- **The target descriptor** keeps the fields wasm32 sets: type sizes,
  value widths, plain `char`'s sign, `struct_return_max` and `va_class`.
  The rest were folded into the code at the value wasm32 gives them, and
  what they guarded is gone: word addressing and struct arguments split
  into words (BESM-6), logical right shifts and the intrinsics with an
  immediate argument (BESM-6), big-endian layout and packed bit-fields
  (MMIX), AAPCS bit-fields, clang's bit-field access units (AVR), a
  `double` that is binary32 (AVR), the x87 `long double`, and the switch
  that turned the loop optimizations off.
- **The three-address code is untouched**, `PTR_DIFF` and the fat-pointer
  initializers included, since it is the file format between `lower` and
  `genwasm`, and `tac/tacky.asdl` is its specification.
- **Gone outright**: the other targets' ABI classifiers, the register
  allocator (wasm is a stack machine), Graphviz output (`--dot`), the x87
  conversions in `float128`, cpp's BESM-6 build profile, and the driver's
  hosted and bare-metal links.
- **The driver** assembles with clang and links with `wasm-ld`; `-Sbemsh`,
  `-Smadlen` and the other targets' assemblers, linkers and scripts are
  gone. [cc/README.md](cc/README.md) is its manual.
- **Formatting** is the root `.clang-format`, which for C is vcc's own.

## Building

`make` at the top builds the compiler twice over. First natively: the top
[Makefile](../../Makefile) configures this directory with the host's C
compiler into `build/host-cc/`, without the SDK's toolchain file, and
installs it into `build/host-cc/stage/` as upstream lays an installation
out — `bin/vcc` and its passes, and the headers of `wasm32-braam` in
`share/vcc/wasm32-braam/include/`. That is the cross compiler.

Then, in the tree's own wasm32 configure, [braam.cmake](braam.cmake) uses
it (`VCC_HOST`, which defaults to that stage):

- **The runtime.** `crt0.o` is `libc/wasm32/braam/exports.s`; `libc.a` is
  upstream's `wasm32-braam` set — the common sources compiled for `wasm32`
  by the passes, `memory.s`, and `braam/`'s sources compiled by the driver —
  with musl's libm beside them (below). Both go in `build/devel/cc/lib/`.
- **`vcc_add_program(NAME n SOURCES ...)`**, the counterpart of
  `braam_add_program` for C with coroutines: `vcc -S` on each source, the
  SDK's clang to assemble, `wasm-ld` (or `ld` under `make LINKER=ld`) with
  the driver's flags, `crt0.o` and `libc.a`, then the SDK's `stamp.py`, so
  the ABI is the SDK's. `FLAGS`, `STACK`, `PAGES` and `TASK_BYTES` set the
  driver's options, the stack, the initial memory and the root task's arena.
- **The compiler itself**: `vcc`, `cpp`, `parse`, `lower` and `genwasm` as
  Braam programs, from the same sources as the host's
  ([sources.cmake](sources.cmake)), with a 1 MiB stack.
- **A check**: [check_braam_abi.py](scripts/check_braam_abi.py) holds
  `braam.h` and `errno.h` to the SDK's `sysabi.h` and `result.h`, and a
  disagreement fails the build.

[test/hello.c](test/hello.c) is built that way, and
[test/hello.mjs](test/hello.mjs) runs it under the SDK's harness.

## On Braam

The compiler is C with coroutines, and the host compiles the same sources:
[libutil/host.h](libutil/host.h) makes `coro(Y)` and `await` nothing there,
and a call that waits is then C's own. Coroutines stop at the door: a pass
reads its input whole (`read_file`, or `wopen`/`wdopen` for the word streams),
works in plain C over memory, and writes its output whole when it closes it.
Only `main`, the opens and the closes wait, and `lower` and `genwasm` park
once per declaration so `^C` reaches a long run.

- **A fatal error is `exit()`**, from wherever it is found. Braam's `exit()`
  posts every stream's buffer as a write nobody waits for and ends the
  process in that step, and the kernel performs those writes before it reaps
  it. That needs Braam 0.10.1 or later (`System_Calls.md` §5); on an older
  kernel such an exit is reported as a crash. The host recovers after an
  error and reports more; on Braam, which has no `setjmp`, the first ends the
  pass.
- **`cpp` reads from memory.** It reads from deep inside its scanner, so
  before it starts, [cpp/files.c](cpp/files.c) loads the input and every file
  an `#include` line of it, or of a file found that way, could name under any
  directory of the search path, conditionals ignored. A computed `#include`
  cannot be foreseen and is not found on Braam.
- **The caret** under a diagnostic is drawn from text in memory:
  `parse` registers the `.i` and every file its line markers name. `lower`
  and `genwasm` have no text, and print no caret on Braam.
- **The driver** spawns its passes with `posix_spawnp` and waits with
  `waitpid`, finds them in `libexec/` beside its `bin/`, assembles with
  devel/wasm's `as` and links with its `ld`. It removes its temporary files
  when `main` returns; an `exit()` leaves them in `/tmp`.
- **A 32-bit compiler.** `size_t` and `long` are 32 bits on Braam. The word
  streams carry a 64-bit value in two words where `size_t` has 32
  (`wputll`), the syntax tree's `long` constants are `int64_t`, and the
  semantic layer's constants are `long long` rather than the host's `long`.
  One upstream bug only this showed: `wgetd` took a word of all ones for the
  end of the stream, which is the low half of `DBL_MAX`.
- **Names.** vcc does not let a local name shadow a function, and the
  compiler's own sources name things `yield` and `defer`; they are built
  with `_CORO_NO_DEFER_YIELD`, which keeps `<coro.h>` from defining those two.

## The package

`make package` packs `cc`, which needs `wasm` for `as` and `ld`:

| Entry | What it is |
| --- | --- |
| `bin/cc` | the driver, the one command |
| `libexec/vcpp`, `vparse`, `vlower`, `vgenwasm` | the passes, not on `PATH` |
| `share/vcc/wasm32-braam/include/`, `lib/` | the headers, `crt0.o` and `libc.a`, where the driver looks for them |
| `share/Manual.md` | the user's guide |
| `share/Coroutines_in_C.md`, `share/README.md` | the language extension, and this file |
| `share/hello.c`, `share/notes.c` | two examples |

The runtime stays where upstream's installation keeps it rather than in a
top-level `include/` and `lib/`, so the driver finds it as it always has:
through `/pkg/bin/cc`, its links resolved, and `../share/vcc/wasm32-braam`.
It is 428 KB packed and 1.3 MB unpacked.

Every test of the compiler on Braam starts from the install as a user
makes it ([test/ccfix.mjs](test/ccfix.mjs)): a repository signed for the
run, with an anchor of its own, serves `cc` and `wasm`, and `pkg install
cc` pulls in both. `make test-packages` packs the two zips first.

## The C library

Upstream's Braam runtime covered what its example programs call. What the
compiler's own sources call was added, as C in upstream's layout, so it can
be offered back:

- **`libc/common/`**, target-neutral: `<ctype.h>`, `qsort` (a heapsort),
  `bsearch`, the `strtol` family, `strdup`, `strndup`, `strnlen`, `strspn`,
  `strcspn`, `strpbrk`, `strtok_r`, `memccpy`, `vsnprintf`, `vsprintf`,
  `gmtime`, `strftime`, and `getopt`/`getopt_long` in `<getopt.h>`, which is
  FreeBSD's `getopt_long.c` with its licence, since the host's (macOS's)
  behaviour is that one's.
- **`libc/wasm32/braam/`**, on the kernel: `getenv` and `environ` from the
  start blob, `atexit` (run when `main` returns, since `exit()` runs
  nothing), `isatty`, `access`, `time` and `localtime` over `Sys::Clock`,
  `readlink` and `realpath`, `mkstemp` and `mkstemps` over `O_EXCL`,
  `posix_spawn`, `posix_spawnp`, `waitpid` and `wait` over `Sys::Spawn` and
  `Sys::Wait`, `open_memstream`, and `setvbuf`, which does nothing because
  an output buffer grows. Each call that waits is a coroutine, as the rest
  of Braam's calls are.
- **Floating point is musl's**: the SDK's `libbraam_math.a`, whose members
  `libc.a` carries. `strtod` and `strtof` are its `__floatscan`, rounding
  correctly. `printf`'s `%e`, `%f`, `%g` and the new `%a` are its `fmt_fp`
  (`doprnt.c` under `__braam__`): upstream's own conversion was not exact
  (`%.17g` of `1e300`) and had no `%a`, which `genwasm` writes every float
  constant with. `<math.h>` is all of C99's double and float functions, and
  upstream's `fabs`, `frexp`, `ldexp`, `modf`, `fma`, `fmin`, `fmax` and
  `sqrt.s` are left out of the build. `nan` alone is vcc's (`nan.c`),
  because vcc names a function `nan` as `nan.vcc`.
- `PATH_MAX` is 512 under `__braam__`, as in Braam's compat layer, and
  `open` takes POSIX's optional mode and drops it, since a coroutine cannot
  be variadic.
- **A fix to upstream's `fflush`**: it emptied the buffer after its write,
  so what another task printed while the write waited was lost. It now
  drops only what it wrote. Upstream's fake kernel answers at once, which
  is why its test of two tasks never saw it; `test/braam.mjs` does.

[test/libc.c](test/libc.c) prints the pure half (ctype, integer and float
conversions both ways, `printf`'s float formats and flags, sorting,
strings, `getopt_long` with permutation, the calendar, a memory stream).
The host build compiles the same file against its own C library, and
[test/libc.mjs](test/libc.mjs) holds Braam's output to the host's byte for
byte: 207 lines. One difference is accepted rather than tested: musl does
not set `ERANGE` for a subnormal `strtod` result, and macOS does. The same
test runs [test/sys.c](test/sys.c), the half that asks the kernel, against
stated lines.

## Test programs

[test/programs/](test/programs/) holds the C programs of vcc's test suites,
which [test/extract.py](test/extract.py) lifted out of the GoogleTest
sources, one file each, with the output the test expects beside it as
`.expect` when it is a plain string: 517 programs, 481 with their output.
Chapter 20 of the book is left out, being about register allocation, and so
is every program upstream's wasm32 fixture skips, most of them for assuming
a 64-bit `long`.

## Tests on Braam

Each is one boot with the package installed, and in `TESTS` but for the
corpus:

| Test | What it holds |
| --- | --- |
| [toolchain.mjs](test/toolchain.mjs) | `cc hello.c && ./a.out` with the package's example; a program with arguments and a status; six programs compiled to the host's assembly; a compile error's message and status |
| [book.mjs](test/book.mjs) | the book's 354 programs, compiled and run, each output and result as upstream's test expects |
| [coro.mjs](test/coro.mjs) | 21 coroutine, `defer` and `alloca` programs, the same way |
| [wasm.mjs](test/wasm.mjs) | 97 run tests of the wasm32 backend, the same way |
| [braam.mjs](test/braam.mjs) | `braam_tests.cpp`'s programs: streams, files, standard input, tasks, poll, `^C` caught and not, and `notes.c`'s session |
| [interrupt.mjs](test/interrupt.mjs) | `kill -INT` ends a long `vlower` run, with nothing written |
| [corpus.mjs](test/corpus.mjs) | in `LONGTESTS`, 3½ minutes: `vcpp` makes the host's `.i` of the tree's 669 sources, and the other passes the host's `.s` of the 660 that compile |

The run tests are plain wasm32 programs upstream, and on Braam `main` is a
coroutine, so [suites.mjs](test/suites.mjs) compiles each with
`-Dmain=__vcc_main` and links a `main` that calls it and, for the book,
prints the result as upstream's `crt0-status.o` does. That costs three of
the book's programs, which rely on `main`'s implicit `return 0`. Six more
are skipped with their reason (no `setjmp`, a second unit or a helper that
lived in the test, Braam's `errno` numbers), and 17 have no expected
output to hold them to.

`braam.mjs` keeps upstream's expectations where Braam agrees with the fake
kernel, and says where it does not: `exit()` keeps its output; the
harness's clock stands still; `POLLIN` on standard output is refused; and a
sleep of `(unsigned)-10` ms is long. The fixture moves the clock to the
next timer whenever the kernel waits for one, so sleeps end.

## Holding the trimmed compiler to upstream

The trimming was checked against the compiler as imported. Three scripts in
[test/](test/) do it:

- [reference.sh](test/reference.sh) takes `devel/cc` at the import,
  `0b37290`, adds to it every C snippet of vcc's tests
  ([snippets.py](test/snippets.py)), builds it on the host and records what
  it makes of them, in `build/cc-corpus/`.
- [corpus.sh](test/corpus.sh) compiles every C source of a tree through the
  four passes, and writes the assembly, or the diagnostics and status of a
  source that is refused.
- [driver.sh](test/driver.sh) runs the driver with `-v` over a set of
  invocations, with the assembler and linker stubbed, and records what it
  runs.

[check.sh](test/check.sh) rebuilds `build/host-cc` and compares:

    sh devel/cc/test/reference.sh ~/Project/Claude/vcc   # once
    sh devel/cc/test/check.sh

The corpus is 4,199 sources, of which 3,108 compile and 1,091 are refused,
and the trimmed compiler writes the same bytes for every one of them. The
driver's transcript differs only where it should: the targets it no longer
knows, the default target, and its usage text.
