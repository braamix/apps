# cc — vcc, the C compiler with coroutines, for Braam

This is vcc, Serge Vakulenko's C11 compiler with `defer` and coroutines, cut
down to its `wasm32-braam` target, on its way to running on Braam and
compiling itself there. [Plan.md](Plan.md) is the plan; this file says
where it stands.

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

## Building on the host

[CMakeLists.txt](CMakeLists.txt) builds the compiler with the host's C
compiler, without the SDK's toolchain file:

    cmake -S devel/cc -B build/host-cc && cmake --build build/host-cc

## Test programs

[test/programs/](test/programs/) holds the C programs of vcc's test suites,
which [test/extract.py](test/extract.py) lifted out of the GoogleTest
sources, one file each, with the output the test expects beside it as
`.expect` when it is a plain string: 517 programs, 481 with their output.
Chapter 20 of the book is left out, being about register allocation, and so
is every program upstream's wasm32 fixture skips, most of them for assuming
a 64-bit `long`.

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
