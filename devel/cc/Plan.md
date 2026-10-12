# cc — a plan for porting vcc to Braam

`devel/cc` is to be vcc, the C11 compiler with coroutines at
`~/Project/Claude/vcc`, cut down to its `wasm32-braam` target and rewritten
in vcc's own C-with-coroutines, so that it runs on Braam and can compile
itself there. This file is the plan: what is taken, what changes, and the
order of the steps, each with what proves it done. The import, the trim,
the build in this tree, the C library, the rewrite for Braam, the package,
its tests and self-compilation are done, and [README.md](README.md) says
how they were checked.

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
  and parameters and is `await`ed, `printf` and the other output calls too
  (the stream buffer is fixed, and written out when full), and the runtime is
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
   byte for byte against the host (`test/selfhost.mjs`) without a third
   stage. The only `#ifdef __braam__` expected is the driver's process
   spawning.
2. **Coroutines stop at the door.** Each pass reads its whole input into
   memory and works in plain C — iconv's shape, and `devel/wasm/lib`'s
   rule ("pure: never awaits, never opens a file"). Only `main`, the file
   loaders and the writers are coroutines; since `printf` is one, a writer
   is every function that reaches output, the YAML and print dumps,
   `genwasm`'s emitters and cpp's scanner among them. The one wait in the
   middle of work is cpp's `#include`, met by loading every file it could
   name before cpp starts. Diagnostics are the exception: they come from
   plain code everywhere, so they are held in memory and written out at
   the passes' await points, or posted by `exit` (`diag_printf`), and the
   debug traces behind the passes' `-D` are compiled out (`trace_printf`).
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

Not vendored: the GoogleTest suites (their programs are, in `test/programs/`),
`scripts/` but for `check_braam_abi.py`.

## 4. Steps

Each step ends green: `make test` passes and the package, once it exists,
still builds.

### Step 8 — after the first release

- Error recovery on Braam, if a sticky flag is affordable in the parser
  and translator.
- The caret for `lower` and `genwasm` diagnostics, by reading the source
  before the pass starts.
- `scanf`, `tmpfile`, reading directories — the gaps `docs/Braam.md` §7.6
  lists — as programs want them.
- Whether `cpp` and `parse` should be one process, if spawning shows in
  the times.
- Measured in a browser, since the harness clock is frozen: the time to
  compile the largest sources (`translator/expr.c`, the optimizer) and the
  peak memory against the 100 MB cap, and `__braam_task_bytes` and
  `--braam-pages` set from what is seen.

## 5. Risks

- **Memory**: vcc's `xalloc` and the optimizer were written for a host;
  the 100 MB cap may bind on the largest files.
- **Drift from upstream.** vcc moves fast; the recorded commit and the
  README's list of changes are what keep a re-sync possible.
