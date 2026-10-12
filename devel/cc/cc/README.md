# cc — the compiler driver

The C compiler driver of VCC, built as `build/cc/cc` and installed as `bin/vcc`. It is a
**driver**, not a compiler itself: it runs the toolchain one program per stage, as Unix
`cc(1)` always has, so that

```sh
vcc hello.c
```

does what otherwise takes six commands, and builds a process of
[Braam](../docs/Braam.md). It is ported from the
[v7besm](https://github.com/besm6/v7besm) project's `b6cc` (`cmd/cc/`), a C11 rewrite of the
v7 driver, and made target-aware.

## Pipeline

```text
vcpp         preprocess    .c   -> .i
vparse       parse         .i   -> .ast
vlower       lower + opt   .ast -> .tac
vgenwasm     code gen      .tac -> .s
assembler    assemble      .s   -> .o
linker       link          .o   -> a.out
```

| Target | `-t` | Code generator | Assembler | Linker |
| --- | --- | --- | --- | --- |
| A process of Braam (wasm32, [docs/Braam.md](../docs/Braam.md)) | `wasm32-braam` | `vgenwasm` | `clang --target=wasm32 --no-default-config -mreference-types -mbulk-memory -msign-ext -mmutable-globals -mnontrapping-fptoint -c` | `wasm-ld --no-entry --import-memory --stack-first -z stack-size=131072 --gc-sections --initial-memory=<pages * 64 KiB>`, then the `braam` section |

WebAssembly has no binutils: clang assembles it and `wasm-ld` links it, always.

The intermediate files are temporaries in `$TMPDIR` (or `/tmp`), named `vccXXXXXX.<suffix>`
and removed on exit.

## Input files

| Suffix | Handling |
| --- | --- |
| `.c` | full pipeline |
| `.S` | assembly preprocessed first (`vcpp` with `__ASSEMBLER__` defined, then the assembler) |
| `.s` | assembly (assembler only) |
| `.o`, `.a` | passed straight to the linker |

## Options

| Option | Meaning |
| --- | --- |
| `-t NAME`, `-tNAME`, `--target NAME`, `--target=NAME` | Target: `wasm32-braam`, the default and the only one |
| `-c` | Compile and assemble, but do not link |
| `-S` | Compile only; emit assembly (`.s`) |
| `-E` | Preprocess only; write to `-o` or standard output |
| `-P` | With `-E`, no line markers |
| `-x LANG` | The language of every input, whatever its suffix: `c`, `assembler-with-cpp` (as `.S`), `assembler` (as `.s`), or `none` (by suffix) |
| `-o file` | Set the output file name (default `a.out` for a link) |
| `-v` | Echo each sub-command before running it |
| `-Dname[=v]`, `-Uname`, `-Ipath` | Passed to the preprocessor (`-D name` is folded into `-Dname`) |
| `-Lpath`, `-lname` | Passed to the linker, after the objects |
| `-T file` | A linker script, which `wasm-ld` has none of by default |
| `-nostdinc` | Do not add the target's standard include directory |
| `--initial-pages=N` | The process's initial memory, 1 to 1600 pages of 64 KiB (default 4, braam-core's `BRAAM_BIN_INITIAL_PAGES`); the link's `--initial-memory` and the `braam` section both say it |
| `-nostdlib` | No `crt0.o`, no standard library directory, no implicit libraries |
| `-fmax-errors=N` | Stop after N errors (default 20, 0 for no limit); `vparse` and `vlower` take it as `--max-errors=N` |
| `-fdiagnostics-color[=WHEN]`, `-fno-diagnostics-color` | Color diagnostics: `always` (what the bare flag means), `never` (the `-fno-` form), or `auto` (the default: color only when stderr is a terminal, `NO_COLOR` is unset or empty, and `TERM` is not `dumb`). Every pass takes it as `--color=WHEN` |
| `-fno-diagnostics-show-caret` | Don't show the source line and caret under a diagnostic; `vparse` and `vlower` take it as `--no-show-source` |
| `-O`, `-g` | Accepted and ignored: `vlower` always optimizes, and there is no debug info yet |
| `-W…`, other `-f…`, `-w`, `-std=…`, `-pedantic`, `-pipe`, `-arch A`, `-isysroot D` | Accepted and ignored, so that a build system written for GCC or clang (CMake among them) can drive `vcc` |

The last stage is chosen by `-E`, `-S` or `-c`. With none of them, the objects are linked.
Output names derive from the input's base name in the current directory (`src/foo.c` →
`foo.o`). When linking, the objects of the sources are temporaries, removed afterwards,
as with gcc: `vcc -o t t.c` leaves no `t.o`. `-o` with `-c`, `-S` or `-E` takes a single
input.

The exit status is 0 on success and 1 on any failure. One failing file does not stop
the others from compiling, but it skips the link.

## Where things are found

**The installation is relocatable.** `vcc` finds its own directory (`_NSGetExecutablePath`
on macOS, `/proc/self/exe` on Linux, otherwise `argv[0]`), with symlinks resolved, and
takes everything relative to it:

| What | Where |
| --- | --- |
| `vcpp`, `vparse`, `vlower`, `vgenwasm` | the directory `vcc` is in |
| standard headers | `../share/vcc/<target>/include` |
| `crt0.o`, `libc.a`, and the host `run.mjs` | `../share/vcc/<target>/lib` |

On Braam itself the passes are in `../libexec/` rather than beside `vcc`, as a
package's `bin/` holds commands only; the assembler is `as` and the linker `ld`,
devel/wasm's, found on `PATH`; and the passes are spawned with `posix_spawnp`.

`vcc` passes `-nostdinc -I<share>/include` to `vcpp`, so `vcpp`'s own compiled-in include
directory plays no part. The assembler and linker belong to other projects. They are the
ones found when the build was configured, or else `clang` and `wasm-ld` on `PATH`. Which
kind a tool is, by its name, decides its flags.

Each tool can be overridden with an environment variable:

| Variable | Tool |
| --- | --- |
| `VCC_CPP`, `VCC_PARSE`, `VCC_LOWER` | the preprocessor, parser and lowerer |
| `VCC_GEN` | the code generator |
| `VCC_AS`, `VCC_LD` | the assembler and linker; split into words at blanks, so they may carry arguments |

## Linking

```text
wasm-ld --no-entry --import-memory --stack-first -z stack-size=131072 --gc-sections
        --initial-memory=<pages * 64 KiB> -o a.out -L<lib> <lib>/crt0.o objects... -L/-l flags... -lc
```

There is no linker script: `wasm-ld` lays out the linear memory, the 128 KiB shadow
stack first (`--stack-first`, so that an overflow traps below address 0 rather than
running into the data), then the data and the heap. `vgenwasm` puts every function and
variable in a section of its own, and `wasm-ld` drops those nothing reaches.
`-nostdlib` drops `-L<lib>`, `crt0.o` and the implicit `-lc`. See
[docs/Wasm_Backend.md](../docs/Wasm_Backend.md).

## A process of Braam

`-t wasm32-braam` builds a process for [Braam](../docs/Braam.md):
`lower` with Braam's process model (`main` is
`coro(braam_call *) int main(int, char **)`), the headers and runtime of
`share/vcc/wasm32-braam`, and a link with the memory imported and no entry, the five
exports coming from `crt0.o`. After the link the driver appends the custom section
`braam` that `exec` reads (magic, `PROC_ABI`, flags, the initial pages, 1600), as
braam-core's `tools/stamp.py` does for its own programs, replacing an earlier one.
`node share/vcc/wasm32-braam/lib/run.mjs prog [args...]` runs the result on a fake
kernel, and `fimport` in a Braam tab brings it to Braam itself;
[docs/Braam.md](../docs/Braam.md) is the target's reference, and
[docs/Braam_Example.md](../docs/Braam_Example.md) works a program through.
