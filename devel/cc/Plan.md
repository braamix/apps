# cc — a plan for porting vcc to Braam

`devel/cc` is to be vcc, the C11 compiler with coroutines at
`~/Project/Claude/vcc`, cut down to its `wasm32-braam` target and rewritten
in vcc's own C-with-coroutines, so that it runs on Braam and can compile
itself there. This file is the plan: what is taken, what changes, and the
order of the steps, each with what proves it done. No code exists yet.

## 1. Where this starts

vcc today (upstream commit `937a00c4`):

- **The pipeline** is five programs: `cc` (driver, 1.6k lines) runs `cpp`
  (3.8k), `parse` (scanner, parser, AST: 13k), `lower` (semantic,
  translator, optimizer, TAC: 30k) and `genwasm` (backend/common and
  backend/wasm: 6.6k), then an assembler and a linker. Passes talk through
  files: `.i` text, `.ast` and `.tac` word streams written by
  `libutil/wio.c`, `.s` text. About 62k lines in all, plus `libutil` (3k).
- **`wasm32-braam`** is already a complete target: `main` is
  `coro(braam_call *) int main(...)`, every call that waits keeps C's name
  and parameters and is `await`ed, `printf` and the other output calls never
  wait (the stream buffer grows), and the runtime is
  `libc/wasm32/braam/` (`rt.c`, `sys.c`, `stdio.c`, `malloc.c`,
  `exports.s`). `docs/Braam.md` is its reference.
- **On the host** vcc assembles with `clang -c`, links with `wasm-ld`, and
  stamps the `braam` section itself (`braam_stamp` in `cc/cc.c`).

What Braam already has, in this tree:

- **`devel/wasm`** ships `as` (held byte for byte to `llvm-mc` on exactly
  the language vcc writes), `ld` (held to `wasm-ld`, stamps the `braam`
  section, `--braam-pages`), `ar`, `nm`, `size`, `strip` and `disasm`. So
  the assembler and linker on Braam are done; `cc` depends on `wasm`.
- **`devel/c4`** has a self-hosting test (`test/selfhost.mjs`) and the
  `/pkg/bin` trick for finding the package's own files.
- The kernel offers `Sys::Spawn` and `Sys::Wait`, which vcc's Braam libc
  does not yet expose.

## 2. Decisions

1. **One set of sources, two builds.** The vendored sources are written in
   C-with-coroutines, but stay compilable by the host's C compiler: a
   header `host.h`, used only when `__vcc_coroutines__` is not defined,
   makes `coro(Y)` empty and `await` nothing. Since Braam's libc keeps C's
   names and parameters for every call that waits, the result is plain
   POSIX C. So the **host build of `devel/cc` is the cross compiler** that
   builds the package, and the code generator that builds the package is the
   one inside it. That is what lets self-compilation on Braam be checked
   byte for byte against the host (step 7) without a third stage. The only
   `#ifdef __braam__` expected is the driver's process spawning.
2. **Coroutines stop at the door.** Each pass reads its whole input into
   memory, works in plain C, and writes into a growing buffer that is
   written out once at the end — iconv's shape, and `devel/wasm/lib`'s
   rule ("pure: never awaits, never opens a file"). Only `main`, the file
   loaders and the writers are coroutines. The one wait in the middle of
   work is cpp's `#include`, and it gets a step of its own (4.6).
3. **Five programs stay five programs.** The driver spawns the passes, as
   upstream forks them: the files between passes are vcc's tested
   interface, and every pass keeps a pile of globals that one process
   would have to reset. On Braam the driver ends with `as` and `ld` from
   the `wasm` package.
4. **`bin/` holds `cc` alone.** The passes go in the package's `libexec/`
   (not on `PATH`); headers in `include/`, `crt0.o` and `libc.a` in `lib/`.
   The driver finds its store directory through `/pkg/bin/cc`, as `ld` does
   through `/pkg/bin/ld`.
5. **Only `wasm32-braam`.** Other backends, their libc, the hosted
   targets, `grammar/`, `bench/`, `third_party/` and GoogleTest stay
   behind. Shared code keeps its structure; a branch that only another
   target can take goes, and BESM-6's switches with it.
6. **Upstream's identifiers, structure, comments and messages are kept**, as
   in every port here. `devel/cc` is a fork of vcc at a recorded commit; a
   fix found here is offered upstream, and an upstream fix is taken by hand
   and noted in the README.

## 3. What is vendored

| From vcc | To `devel/cc` | Trimmed |
| --- | --- | --- |
| `cc/cc.c` | `cc/` | targets other than `wasm32-braam`; hosted and bare-metal links |
| `cpp/` | `cpp/` | predefined-macro tables of other targets |
| `scanner/`, `parser/`, `ast/` | same | `ast_graphviz.c`, `*.dot`; `--dot` |
| `semantic/`, `translator/`, `optimize/`, `tac/` | same | other descriptors in `target.c`; BESM-6 branches; `tac_graphviz.c` |
| `libutil/` | `libutil/` | `attic/` |
| `backend/common/`, `backend/wasm/` | `backend/` | — |
| `libc/common/`, `libc/ilp32/`, `libc/wasm32/` (less node's runtime), `libc/wasm32/braam/` | `libc/` | `run.mjs` kept only for the host tests |
| `*.asdl` specs | beside their headers | — |
| `docs/Coroutines_in_C.md`, `Braam.md`, `Wasm_Backend.md` | `Manual.md` and `doc/` | other targets' sections |

Whether `--yaml` and `ast_compare`/`tac_compare` stay is decided in step 1
by whether the tests use them. Not vendored: the GoogleTest suites (their
programs are, see step 6), `scripts/` but for `check_braam_abi.py`.

## 4. Steps

Each step ends green: `make test` passes and the package, once it exists,
still builds.

### Step 1 — import and trim

1. Copy the files of §3 verbatim at `937a00c4`, and record the commit in
   `README.md`.
2. Build them on the host with the host compiler (plain CMake, its own
   build directory — never the SDK's toolchain file) and compile a corpus
   to `.s` with `-t wasm32-braam`: vcc's book programs
   (`backend/common/test/book/`), its coroutine programs
   (`backend/common/test/coro/`), its Braam examples, its libc, and its own
   sources. Keep those `.s` as the **reference corpus**.
3. Trim per §3 in small commits. After each, the corpus is unchanged byte
   for byte.
4. `clang-format` the tree with the root `.clang-format` (vcc's is
   different), corpus unchanged.

Done when: the trimmed host build produces the reference corpus exactly.

**Done.** The corpus grew to every C snippet of vcc's tests, 4,199 sources,
and the driver got a transcript of its own; [README.md](README.md) says how
both are checked. Two decisions moved: the passes keep `wasm32` beside
`wasm32-braam`, being one descriptor, and the three-address code keeps every
instruction, BESM-6's included, as the format between the passes.

### Step 2 — the build in this tree

1. **Stage 0, native.** The top `Makefile` configures `devel/cc` natively
   into `build/host-cc/` and builds `cc` and its passes there, before the
   wasm configure; the main configure is told where they are
   (`-DVCC_HOST=`).
2. **`vcc_add_program`**, a CMake function in `devel/cc/`, the
   counterpart of `braam_add_program` for C-with-coroutines: host `cc -t
   wasm32-braam -S`, then the SDK's `clang -c` on the `.s`, then `wasm-ld`
   (or `ld` under `LINKER=ld`) with `crt0.o` and `libc.a`, then the stamp.
   Stamp with the SDK's `stamp.py`, not vcc's `braam_stamp`, so the ABI is
   the SDK's; `check_braam_abi.py` runs against the SDK's `sysabi.h` and
   `result.h` as a build check.
3. **The libc.** `crt0.o` and `libc.a` are built the same way, into the
   package's `lib/`; the headers are staged into `include/`.
4. **A first program.** `hello.c` built by `vcc_add_program`, planted in
   the SDK harness and run (`test/hello.mjs`). This checks the build and
   the runtime against the real kernel, before any of the compiler moves.

Done when: `make` builds the libc and `hello` from vcc's C, and `make test`
runs it on Braam.

### Step 3 — libc gaps

Compile every vendored source with `-t wasm32-braam` and link, and let the
undefined symbols name the work. Expected:

- **Processes**: `posix_spawn`-shaped `await spawn(...)` over Sys::Spawn,
  and `await waitpid(...)` over Sys::Wait, in `libc/braam/`.
- **`open_memstream`** (cpp's `macro.c`), over the growing stream buffer
  Braam's `stdio.c` already has.
- **`isatty`** (colour diagnostics), `getenv`, temporary names (`/tmp/` +
  pid + counter in place of `mkstemp`), `strtod` and the rest
  `libutil/float128.c` and the scanner use.
- **`printf("%a")`** — `genwasm` prints every float constant this way
  (`backend/wasm/emit.c`), so vcc's libc must spell it exactly as the
  host's does, or the corpus differs. A test of its own.
- **`__DATE__`/`__TIME__`**: cpp calls `localtime`/`strftime`. Braam has
  `braam_now()` and no calendar; take it from `SOURCE_DATE_EPOCH` or a
  fixed date, and say so.
- **The fatal exit** — see step 4.1.

Each addition is tested on Braam under the harness, and offered upstream to
vcc's `libc/wasm32/braam/`.

### Step 4 — the coroutine rewrite, pass by pass

Order: the shared library first, then the passes from the simplest. Each
sub-step ends with (a) the host build still producing the reference corpus
and (b) the pass, built for Braam, run under the harness on corpus inputs
with its output equal to the host pass's.

**4.1 Errors.** `libutil/srcloc.c` unwinds with `longjmp` to a recovery
point (parser per statement and declaration, translator per function), and
otherwise calls `exit(1)`. On Braam there is no `setjmp`, and `exit()`
traps and loses what is buffered — the diagnostic itself. So:

- First find out from braam-core (`doc/Concept.md`, the process teardown)
  whether a `sys_async` Write issued in the last step before Exit is
  delivered. If it is, `diag` gets a `diag_exit(status)` that posts the
  stderr and stdout buffers and exits, and a fatal error stops the pass
  there. If not, that is a kernel change to ask for, and until then a
  fatal error unwinds to `main` by a sticky flag (vi's and mbasic's
  answer), checked at each recovery point.
- Error recovery — going on after an error to report more — is dropped on
  Braam at first: the first fatal error ends the pass. The host build keeps
  it. Bringing it to Braam is step 8.
- `internal_error` and "too many errors" go the same way.

**4.2 `libutil`.** `wio`: `wopen` reads the whole file into memory and
`wclose` writes the whole buffer out; both become coroutines, and
`wgetw`/`wputw` and the rest stay plain functions over memory. `srcloc`:
the caret under a diagnostic reopens the source file from inside a plain
function, so the scanner and cpp register their text in memory and the
caret is drawn from there; a pass that has not got the text (`lower`,
`genwasm`) prints no caret line on Braam.

**4.3 `genwasm`** (`backend/common/driver.c`): `main` a coroutine, input
through `wopen`, `.s` into a growing stream, `await fclose`. Between two
top-level functions, `await braam_yield()`, so `^C` reaches a long run.

**4.4 `lower`** (`translator/main.c`): the same, and the recovery point of
4.1. `.ast` in, `.tac` out, a yield per function.

**4.5 `parse`** (`parser/main.c`, `scanner/scanner.c`): the scanner reads
from a memory buffer rather than `fgetc` on a `FILE`.

**4.6 `cpp`.** The one pass that must wait mid-work: `refill_buffer` reads
the current file in chunks from deep inside the lexer, and `do_include`
opens a file from inside `process_directives`. Plan:

- Files are read whole into memory when opened; `refill_buffer` copies
  from memory and never waits.
- `#include` becomes a request: `do_include` records the name and returns,
  and cpp's top loop — a coroutine — searches the directories, loads the
  file, and pushes it. Check first that nothing has to happen between the
  directive and the switch of input; if something does, convert the call
  chain from the top loop to `do_include` into coroutines instead, and
  measure the depth.
- `#pragma once` compares descriptors (`is_once_file(fd)`); compare
  `st_ino` instead, which Braam's `stat` gives as a hash of the path.
- `refill_buffer` "flushes and exits" at the end of input; that becomes a
  return to `main`.

**4.7 `cc`, the driver.** `run()` spawns and waits (`#ifdef __braam__`),
`make_temp` uses step 3's names, `locate_self` reads `/pkg/bin/cc`, the
passes are found in `libexec/`, assembly is `as` and the link is `ld` with
`--braam-pages` (vcc's `--initial-pages`) and `-L<store>/lib`. Check `ld`
for every option vcc's link passes (`--stack-first`, `-z stack-size`,
`--initial-memory`, `--gc-sections`); `braam_stamp` goes on Braam, since
`ld` stamps. On the host it keeps `clang` and `wasm-ld`.

Done when: every pass and the driver run on Braam, and compile the
reference corpus there to the same `.s` as the host.

### Step 5 — the package

`braam_add_package(NAME cc ...)` with `D=wasm`: `bin/cc`,
`libexec/{cpp,parse,lower,genwasm}`, `include/`, `lib/crt0.o`,
`lib/libc.a`, and `share/` with a few example programs. `README.md` says
what changed from vcc and why; `Manual.md` is the user's guide (§3 and §4
of `docs/Braam.md`, and the coroutine tutorial). Under 50 MiB is not in
question; measure the unpacked size anyway.

Done when: in the harness, with the real anchor and a local index, `pkg
install cc` pulls in `wasm`, and `cc hello.c && ./a.out` prints.

### Step 6 — tests

Headless, in `TESTS`, each one boot with the package planted:

- `hello.mjs`: build and run, exit status, a compile error's message and
  status.
- `book.mjs`: vcc's book programs compiled **on Braam** and run, their
  output and status held to what upstream's own tests expect.
- `coro.mjs`: the coroutine programs, the same way; `braam.mjs`: the
  streams, files, signals, tasks and `poll` programs of vcc's
  `braam_tests.cpp`, compiled on Braam.
- `corpus.mjs`: each pass alone on the corpus, outputs equal to the host
  build's.
- `interrupt.mjs`: `^C` in a long `lower` run ends it with 130.

Slow ones go in `LONGTESTS`.

### Step 7 — self-compilation

1. On Braam, `cc -c` every source of `devel/cc` and of its libc, then link
   the five programs, with `ar` for `libc.a`. Every object must equal the
   host-built one byte for byte, and every program the package's —
   decision 1 is what makes that a single comparison.
2. The self-built `cc` compiles `hello.c` and the book programs, and passes
   `book.mjs`.
3. `test/selfhost.mjs` in `LONGTESTS`, after `devel/c4`'s.

Expect differences to hunt here: anything keyed on pointer values (hash
order in `string_map.c`), `size_t` being 4 bytes rather than 8 in `wio`'s
word streams (harmless while a stream never crosses builds), and float
formatting. Each is fixed in the compiler, not papered over in the test.

Also measured here, in a browser since the harness clock is frozen: the
time to compile the largest sources (`translator/expr.c`, the optimizer)
and the peak memory against the 100 MB cap. `__braam_task_bytes` and
`--braam-pages` are set from what is seen.

### Step 8 — after the first release

- Error recovery on Braam (4.1), if the sticky flag is affordable in the
  parser and translator.
- The caret for `lower` and `genwasm` diagnostics, by reading the source
  before the pass starts.
- `scanf`, `tmpfile`, reading directories — the gaps `docs/Braam.md` §7.6
  lists — as programs want them.
- Whether `cpp` and `parse` should be one process, if spawning shows in
  the times.

## 5. Risks

- **The fatal exit** (4.1) may need a kernel change; it is the first thing
  to find out.
- **cpp's include path** (4.6) may be deeper than it looks; the coroutine
  chain is the fallback.
- **Memory**: vcc's `xalloc` and the optimizer were written for a host;
  the 100 MB cap may bind on the largest files.
- **Byte-for-byte self-compilation** depends on vcc being deterministic
  across a 64-bit host and 32-bit wasm. Step 1's corpus on the host and
  step 7's on Braam are what find out.
- **Drift from upstream.** vcc moves fast; the recorded commit and the
  README's list of changes are what keep a re-sync possible.
