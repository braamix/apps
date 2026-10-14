# csources — vcc, built on Braam by itself

This package holds the sources of `cc`, Braam's C compiler: about 55,000 lines
of C, a Makefile and nothing else. Running `make` builds the compiler again with
the installed one. The result is the `cc` package, byte for byte.

```
pkg install csources
cd /pkg/store/csources-*/
make -j4
make check
```

`pkg` installs `cc` and `make` along with it. `make` builds into `obj/` beside
the sources, and `make check` compares each program, `crt0.o`, `libc.a` and
every header with the installed `cc` package. It stops at the first that
differs, and otherwise ends with `obj/ is the cc package, byte for byte.`
A full build compiles 138 files and takes several minutes. `-j` runs that
many compiles at once, each in a process of its own.

`obj/` is laid out as the `cc` package is, so the compiler you built runs from
where it is:

```
obj/bin/cc /pkg/store/cc-*/share/hello.c -o /tmp/hello && /tmp/hello
```

The store is meant to be immutable: until `make clean` removes `obj/`,
`pkg verify csources` reports every file in it as `extra`.

## How the sources are organized

vcc is five programs. `cc` is the driver. The other four are passes, and each
reads a file and writes the next:

```
 hello.c ─ vcpp ─▶ .i ─ vparse ─▶ .ast ─ vlower ─▶ .tac ─ vgenwasm ─▶ .s ─ as ─▶ .o
                                                                    ld ─▶ program
```

| Directory | Lines | Part of | What it does |
| --- | ---: | --- | --- |
| `cc/` | 1.3k | `cc` | the driver: runs the passes, then `as` and `ld` |
| `cpp/` | 4.3k | `vcpp` | the preprocessor; writes `.i` |
| `scanner/` | 1.2k | `vparse` | tokens |
| `parser/` | 4.3k | `vparse` | C11 and the coroutine syntax to a tree |
| `ast/` | 6.8k | `vparse`, `vlower` | the syntax tree, and its binary file `.ast` |
| `semantic/` | 9.2k | `vlower` | types, symbols, initializers, `defer`, coroutines |
| `translator/` | 7.4k | `vlower` | the tree to three-address code |
| `optimize/` | 6.2k | `vlower` | constant folding, copy propagation, CSE, dead stores |
| `tac/` | 5.4k | `vlower`, `vgenwasm` | three-address code, and its file `.tac` |
| `backend/common/` | 0.6k | `vgenwasm` | control flow over the code |
| `backend/wasm/` | 5.5k | `vgenwasm` | wasm assembly, `.s` |
| `libutil/` | 3.2k | all five | memory, strings, word streams, diagnostics |
| `libc/` | 4k | the runtime | `crt0.o` and `libc.a`, which every program links |

Each pass has a `main.c` in its first directory: `parser/main.c`,
`translator/main.c` and `backend/wasm/main.c`. `vcpp`'s is `cpp/cpp.c`.

**One set of sources is two compilers.** The same files build the cross
compiler on a Linux or macOS host and this one on Braam.
[libutil/host.h](libutil/host.h) is the switch. On Braam, a function that
reaches a file or the console is a `coro(braam_call *)` and every call to it is
`await`ed. On the host, `coro` and `await` are defined as nothing and the same
code is ordinary C. The Makefile's `-D_CORO_NO_DEFER_YIELD` keeps `<coro.h>`
from taking the names `defer` and `yield`, which the compiler uses for its own
identifiers.

The runtime has three parts:

- [libc/common/](libc/common/) is portable C for any wasm32 machine (`strlen`,
  `qsort`, `snprintf` and the rest), with its own headers in
  `libc/{common,ilp32,wasm32}/include/`.
- [libc/wasm32/braam/](libc/wasm32/braam/) is the half that knows Braam:
  system calls, `malloc`, and stdio built on coroutines. Its headers,
  `braam.h` and `errno.h`, are in `include/` beside it.
- Three files of assembly. `exports.s` holds the entry points the kernel
  calls (`_start`, `_resume`, `_alloc`, `_free`, `_sig`) and becomes
  `crt0.o`. `memory.s` holds `memcpy`, `memmove`, `memset` and the calls
  that size and grow memory. `math.s` holds `sqrt`, `floor` and the other
  math functions that are one wasm instruction each.

`math/` is not vcc's: it is musl's `libm` and its `strtod` and
`printf` engines, from the Braam SDK (`math/musl/COPYRIGHT`).

## How it comes together

The Makefile's variables are the parts, named as the directories are. Each
program's list starts with its `main` and ends with `$(LIBUTIL)`, and the
linker takes the objects in that order. Every object comes from one rule:

```
obj/%.o: %.c
	cc $(CFLAGS) -MMD -c -o $@ $<
```

The rule runs in this directory and names each source by its relative path,
as the host build did. A source's path becomes `__FILE__` inside its object,
so this is the first condition for the bytes to come out the same. `-MMD`
writes a `.d` beside each object, so a changed header remakes only what
includes it.

The runtime is built in four steps:

1. **`libc/common/*.c`** is plain C for any wasm32, so `cc` compiles it
   with `-nostdinc` and the three `include/` directories in place of
   Braam's headers. Upstream compiles these for plain `wasm32`; `cc`'s
   `wasm32-braam` gives the same bytes, since none of them has a `main` or
   asks for `__braam__`.
2. **`libc/wasm32/braam/*.c`** and the printf engine, `doprnt.c`, which does
   ask for `__braam__`, go through `cc` like the compiler's own sources.
3. **The math library** is musl's C in `math/`, the SDK's
   `share/braam/src/math` that the `cc` package's `libc.a` was compiled
   from. It goes through `cc` with `-Ilibc/wasm32/braam/musl -Dhidden=`, and
   `math.s` holds the functions wasm answers with one instruction.
   `libc.a` is the objects of steps 1 and 2 in a fixed order, followed by
   the math library's, in the order `$(sort $(wildcard ...))` gives, which
   is the order the host build's glob gives too.
4. **The headers** are copied into `obj/share/vcc/wasm32-braam/include/`.
   Braam's come first, then wasm32's, ILP32's and the common ones, and the
   first file with a given name wins. That is the set and order the installed
   `cc` reads.

**Linking** runs devel/wasm's `ld` directly, with the driver's flags and two
changes. The driver gives every program a 128 KB stack and 256 KB of memory to
start with. The passes recurse deeply over large trees, so they get a 1 MB
stack and 2–3 MB. `--braam-pages=N,1600` stamps the process ABI and the
memory limits into the binary: N pages at start, and at most 1600, which is
100 MB. Memory starts zeroed in a Braam process, so `ld` places the data
directly and adds no start function to clear `.bss`.

`ld` also writes the output file's name into the module, in its `name`
section. The package's programs were linked as `vcc.wasm`, `cpp.wasm`,
`parse.wasm`, `lower.wasm` and `genwasm.wasm`, so the Makefile links them
under those names in `obj/` and then copies each to `bin/cc` or
`libexec/v<name>`. Linked straight to `bin/cc`, the compiler would be the
same except for that one name.

## What `make check` proves

The `cc` package was built on the host by the host's vcc, then assembled and
linked by the same `as` and `ld` code, run on the host. `obj/` was built on
Braam by that package. When `cmp` finds no difference, the compiler running in
the browser produces exactly what the native compiler produces, for its own
sources, every pass included. Then the copy in `obj/` is not just like the
compiler that built it. It is the same compiler.

vcc is Serge Vakulenko's, under the MIT license in [LICENSE](LICENSE). The
package is built from `devel/cc` in braam-apps, where its README says what
changed from upstream.
