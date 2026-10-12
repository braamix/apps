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
`grammar/`, the benchmarks and the GoogleTest suites. The C programs of
those suites are, as files: [test/extract.py](test/extract.py) lifted them
into [test/programs/](test/programs/).

## Building on the host

[CMakeLists.txt](CMakeLists.txt) builds the compiler with the host's C
compiler, without the SDK's toolchain file:

    cmake -S devel/cc -B build/host-cc && cmake --build build/host-cc

## The reference corpus

[test/corpus.sh](test/corpus.sh) compiles every C source of a tree — the
test programs, the runtime and the compiler's own sources — through the
four passes, and writes the assembly, or the diagnostics of a source that is
refused. Two builds of the compiler are then compared with `diff -r`:

    sh devel/cc/test/corpus.sh build/host-cc <tree> build/cc-corpus/ref

At the import, 717 sources compile and 89 are refused, the same files as
upstream's own build of `937a00c4` writes, byte for byte.
