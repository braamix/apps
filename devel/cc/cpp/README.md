# cpp — the C preprocessor

The C preprocessor of VCC, built as `build/cpp/cpp` and installed as `bin/vcpp`. It
expands `#include`, `#define` and conditional compilation before the source reaches
`parse`, which has no preprocessor of its own.

The engine descends from John F. Reiser's Unix v7 `cpp` (1978). It was modernized to
**C11 (N1570)** in the [v7besm](https://github.com/besm6/v7besm) project, where it is
`b6cpp` (`cmd/cpp/`); these are the same sources, plus target selection (`-t`) and the
`-nostdinc` option. The v7besm copy also builds natively for the BESM-6 under a
reduced size profile (`#ifdef besm6` in [`defs.h`](defs.h)); that build and its
documentation stay in v7besm. Here it is a host tool only.

## Usage

```text
cpp [options] [infile [outfile]]
```

With no file arguments it reads standard input and writes standard output; the first
positional argument is the input file, the second the output file (`-` stands for either
standard stream). **The exit status is the number of errors reported** (0 on success).

| Option | Meaning |
| --- | --- |
| `-t NAME`, `-tNAME`, `--target NAME` | Target: `riscv64` (default, like `lower`), `riscv32`, `aarch64`, `arm32`, `x86_64`, `avr`, `msp430`, `mmix`, `wasm32`, `wasm32-braam` (wasm32's macros and `__braam__`), `besm6`, or the hosted `x86_64-linux`, `aarch64-linux` and `riscv64-linux` (the architecture's macros, for RISC-V also `__riscv_flen=64`, `__CHAR_UNSIGNED__` and `__ELF__`, plus `__linux__`, `__linux`, `__gnu_linux__`, `__unix__` and `__unix`) and `aarch64-darwin` (AArch64's, `__arm64__`, `__APPLE__` and `__MACH__`, without `__CHAR_UNSIGNED__` and `__ELF__`). Selects the predefined macros and the standard include directory. |
| `-Ipath` | Add a directory to the header search list (up to 64; 8 in the BESM-6 build). |
| `-nostdinc` | Do not search the target's standard include directory. |
| `-Dname[=value]` | Predefine a macro; bare `-Dname` defines it as `1`. Up to 20. |
| `-Uname` | Undefine a macro at startup. Up to 20. |
| `-R` | Allow macro recursion (disables the "blue paint" recursion stop). |
| `-P` | Suppress the `# line "file"` line markers. |
| `-C` | Keep comments in the output. |
| `-w` | Suppress warnings. |
| `-trigraphs` | Enable translation-phase-1 trigraph replacement. |
| `-E` | Accepted and ignored, for compatibility. |
| `--color=WHEN` | Color diagnostics: `always`, `never`, or `auto` (the default: when stderr is a terminal, `NO_COLOR` is unset or empty, and `TERM` is not `dumb`). |

`-I`, `-D` and `-U` take their argument in the same word (`-Ifoo`, not `-I foo`).

A full compilation:

```sh
vcpp -t riscv64 hello.c hello.i       # searches ~/.local/share/vcc/riscv64/include
vparse hello.i hello.ast
vlower -t riscv64 hello.ast hello.tac
vgenriscv64 hello.tac hello.s
```

In the build tree, point it at the source headers instead:

```sh
build/cpp/cpp -t besm6 -nostdinc -Ilibc/besm6/include -Ilibc/common/include prog.c prog.i
```

## Targets

| Target | Predefined macros | Standard include directory |
| --- | --- | --- |
| `riscv64` | `__riscv`, `__riscv_xlen` = 64, `__LP64__`, `_LP64`, `__riscv_float_abi_double`, `__riscv_mul`, `__riscv_div` | `<prefix>/share/vcc/riscv64/include` |
| `riscv32` | `__riscv`, `__riscv_xlen` = 32, `__ILP32__`, `_ILP32`, `__riscv_float_abi_double`, `__riscv_mul`, `__riscv_div` | `<prefix>/share/vcc/riscv32/include` |
| `aarch64` | `__aarch64__`, `__ARM_ARCH` = 8, `__ARM_64BIT_STATE`, `__LP64__`, `_LP64`, `__CHAR_UNSIGNED__`, `__ELF__` | `<prefix>/share/vcc/aarch64/include` |
| `arm32` | `__arm__`, `__ARM_ARCH` = 7, `__ARM_ARCH_7A__`, `__ARM_ARCH_PROFILE` = `'A'`, `__ARM_32BIT_STATE`, `__ARM_EABI__`, `__ARMEL__`, `__ARM_PCS_VFP`, `__VFP_FP__`, `__ARM_FP` = `0xe`, `__ARM_FEATURE_IDIV`, `__ILP32__`, `_ILP32`, `__CHAR_UNSIGNED__`, `__WCHAR_UNSIGNED__`, `__ELF__` | `<prefix>/share/vcc/arm32/include` |
| `x86_64` | `__x86_64__`, `__x86_64`, `__amd64__`, `__amd64`, `__LP64__`, `_LP64`, `__SSE__`, `__SSE2__`, `__SSE_MATH__`, `__SSE2_MATH__`, `__code_model_small__`, `__ELF__` | `<prefix>/share/vcc/x86_64/include` |
| `avr` | `__AVR`, `__AVR__`, `__AVR_ARCH__` = 51, `__AVR_ATmega1280__`, `__AVR_HAVE_MUL__`, `__AVR_HAVE_MOVW__`, `__AVR_HAVE_LPMX__`, `__AVR_HAVE_ELPM__`, `__AVR_HAVE_ELPMX__`, `__AVR_HAVE_JMP_CALL__`, `__AVR_2_BYTE_PC__`, `__ELF__` | `<prefix>/share/vcc/avr/include` |
| `msp430` | `__MSP430__`, `__CHAR_UNSIGNED__`, `__ELF__` | `<prefix>/share/vcc/msp430/include` |
| `mmix` | `__mmix__`, `__MMIX__`, `__MMIX_ABI_MMIXWARE__`, `__LP64__`, `_LP64` (GCC's; no `__ELF__`) | `<prefix>/share/vcc/mmix/include` |
| `wasm32` | `__wasm__`, `__wasm`, `__wasm32__`, `__wasm32`, `__ILP32__`, `_ILP32`, and the feature macros `__wasm_bulk_memory__`, `__wasm_bulk_memory_opt__`, `__wasm_multivalue__`, `__wasm_mutable_globals__`, `__wasm_nontrapping_fptoint__`, `__wasm_reference_types__`, `__wasm_sign_ext__` (clang's with Braam's features; no `__ELF__`), and `__vcc_coroutines__` (vcc's coroutines, [docs/Coroutines_in_C.md](../docs/Coroutines_in_C.md)) | `<prefix>/share/vcc/wasm32/include` |
| `wasm32-braam` | wasm32's, and `__braam__` (a process of Braam, [docs/Braam.md](../docs/Braam.md)) | `<prefix>/share/vcc/wasm32-braam/include` |
| `besm6` | `besm6`, `__besm6__` | `<prefix>/share/vcc/besm6/include` |

The RISC-V set is a subset of clang's, so a header written for clang takes the same
branches. `<prefix>` is the install prefix the build was configured with (`~/.local` by
default), compiled in as `VCC_SHARE_DIR`. The standard directory is searched after every
`-I` directory. For BESM-6 only the freestanding headers and `besm6.h` are installed there;
the hosted headers come with v7besm's libc, so pass `-I` for them.

The target macros are ordinary macros and can be `#undef`'d.

## Directives

`#include` (`<header>` and `"header"`), `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`,
`#elif`, `#else`, `#endif`, `#line`, `#error` and `#pragma`. `#pragma once` keeps a file
from being included again (it is known by device and inode); other pragmas are ignored.
Leading whitespace before the `#` is allowed (C11 §6.10), unlike a traditional `cpp`.

`#if`/`#elif` take a full integer constant expression: arithmetic, bitwise, shift,
relational, equality and logical operators, `?:`, the comma operator, and both
`defined name` and `defined(name)`. Operands are decimal, octal or hexadecimal integers
(with `u`, `l` and `ll` suffixes) or character constants; an undefined identifier is `0`.
The arithmetic is in `intmax_t`, or `uintmax_t` when an operand is unsigned (C11
§6.10.1p4). Division or modulo by zero is diagnosed.

## Macros

- Object-like and function-like macros.
- Variadic macros with `__VA_ARGS__`, GNU named varargs (`#define M(args...)`), and GNU
  comma elision (`, ## __VA_ARGS__`).
- The `#` and `##` operators, with their C11 constraints.
- Rescanning with recursion prevention (the "blue paint" rule of §6.10.3.4); `-R`
  overrides it.
- A wrong argument count is an error. An identical redefinition is accepted silently, an
  incompatible one warned.

Predefined, besides the target macros: `__LINE__`, `__FILE__`, the `_Pragma` operator,
`__STDC__` (1), `__STDC_VERSION__` (`201112L`), `__STDC_HOSTED__` (1), `__DATE__`,
`__TIME__`, and the conditional-feature macros `__STDC_NO_COMPLEX__`,
`__STDC_NO_ATOMICS__`, `__STDC_NO_THREADS__`, `__STDC_NO_VLA__` (the compiler has none of
those features). The standard ones are protected: `#define`/`#undef` of them, or `-D`/`-U`,
is rejected (§6.10.8.4).

Identifiers are significant to their full length. Bytes 0x80–0xFF are identifier
characters, so UTF-8 names such as `#define длина 100` work, as in GCC and clang.

## Limits

From [`defs.h`](defs.h); all meet the C11 §5.2.4.1 minimums.

| Limit | Value |
| --- | --- |
| Logical source line | at least 4095 characters |
| Simultaneously defined macros | 4095 or more (hash table of 6151) |
| Macro parameters | 127 |
| `#include` nesting | 10 |
| `#if` nesting | 64 |
| `-D` / `-U` options | 200 each (20 in the BESM-6 build) |
| Macro pushback buffers in flight | 64 (14 in the BESM-6 build) |

## Source layout

| File | Responsibility |
| --- | --- |
| `cpp.c` | Entry point: state init, scan tables, option parsing, targets, built-in directives and predefined macros. |
| `buffer.c` | I/O and the sliding scan buffer, output, macro pushback, trigraphs. |
| `scan.c` | The lexical scanner: tokens, comments, strings, line continuation. |
| `macro.c` | Macro definition, the symbol table, and expansion (arguments, `#`/`##`, blue paint). |
| `direct.c` | Directive dispatch, `#include` search and the include stack. |
| `parser.c`, `yylex.c` | The `#if` expression evaluator and its tokenizer. |
| `diag.c` | Diagnostics and small string helpers. |
| `defs.h` | The state struct, the symbol-table entry, the size limits. |
| `intern.h` | Scan-table macros, marker bytes, the macro-name filter. |

## Testing

`cpp-tests` ([`test/`](test)) is a C11 conformance suite, one file per clause of the
standard, plus the target options. Every suite derives from the `PreprocessorTest`
fixture ([`test/test_support.h`](test/test_support.h)), which runs the built `cpp` in a
temporary directory and compares normalized output (`EXPECT_TOKENS`, `EXPECT_PP_OK`,
`EXPECT_PP_DIAGNOSES`).

The `besm-headers-cpp` and `riscv-headers-cpp` ctests preprocess every shipped header with
this `cpp` and parse the result, alongside the `besm-headers`/`riscv-headers` checks that
use the system `cc -E`.

```sh
./build/cpp/test/cpp-tests
ctest --test-dir build -R headers
```
