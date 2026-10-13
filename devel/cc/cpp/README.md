# cpp — the C preprocessor

The C preprocessor of VCC, built as `build/cpp/cpp` and installed as `bin/vcpp`. It
expands `#include`, `#define` and conditional compilation before the source reaches
`parse`, which has no preprocessor of its own.

The engine descends from John F. Reiser's Unix v7 `cpp` (1978). It was modernized to
**C11 (N1570)** in the [v7besm](https://github.com/besm6/v7besm) project, where it is
`b6cpp` (`cmd/cpp/`); these are the same sources, plus target selection (`-t`) and the
`-nostdinc` option. The v7besm copy also builds natively for the BESM-6 under a
reduced size profile; that build and its documentation stay in v7besm.

## Usage

```text
cpp [options] [infile [outfile]]
```

With no file arguments it reads standard input and writes standard output; the first
positional argument is the input file, the second the output file (`-` stands for either
standard stream). **The exit status is the number of errors reported** (0 on success).

| Option | Meaning |
| --- | --- |
| `-t NAME`, `-tNAME`, `--target NAME` | Target: `wasm32-braam` (default, like `lower`; wasm32's macros and `__braam__`) or `wasm32`. Selects the predefined macros and the standard include directory. |
| `-Ipath` | Add a directory to the header search list (up to 64). |
| `-isystem dir`, `-isystemdir` | Add a system header directory: searched after every `-I` and before the standard one, which is a system directory too. |
| `-nostdinc` | Do not search the target's standard include directory. |
| `-M`, `-MM` | Write a make rule naming the input and every file it includes, in place of the output; `-MM` leaves out a system header and anything it includes. |
| `-MD`, `-MMD` | Preprocess as usual, and write the same rule to `-MF` or `<input base name>.d`. |
| `-MF file` | Where the rule goes. |
| `-MT target`, `-MQ target` | The rule's target, as given or quoted for make (`$` doubled, blank and `#` escaped); repeatable. The default is the input's base name with `.o`. |
| `-MP` | An empty rule for each header. |
| `-MG` | With `-M` or `-MM`, a header not found is listed rather than an error. |
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

The rule is GCC's: each file once, in the order it was first included, lines
wrapped at 72 columns, and no rule at all when the input has an error.

A full compilation:

```sh
vcpp hello.c hello.i       # searches <prefix>/share/vcc/wasm32-braam/include
vparse hello.i hello.ast
vlower hello.ast hello.tac
vgenwasm hello.tac hello.s
```

In the build tree, point it at the source headers instead:

```sh
build/host-cc/cpp -nostdinc -Ilibc/wasm32/braam/include -Ilibc/wasm32/include \
    -Ilibc/ilp32/include -Ilibc/common/include prog.c prog.i
```

## Targets

| Target | Predefined macros | Standard include directory |
| --- | --- | --- |
| `wasm32` | `__wasm__`, `__wasm`, `__wasm32__`, `__wasm32`, `__ILP32__`, `_ILP32`, and the feature macros `__wasm_bulk_memory__`, `__wasm_bulk_memory_opt__`, `__wasm_multivalue__`, `__wasm_mutable_globals__`, `__wasm_nontrapping_fptoint__`, `__wasm_reference_types__`, `__wasm_sign_ext__` (clang's with Braam's features; no `__ELF__`), and `__vcc_coroutines__` (vcc's coroutines, [docs/Coroutines_in_C.md](../docs/Coroutines_in_C.md)) | `<prefix>/share/vcc/wasm32/include` |
| `wasm32-braam` | wasm32's, and `__braam__` (a process of Braam, [docs/Braam.md](../docs/Braam.md)) | `<prefix>/share/vcc/wasm32-braam/include` |

The wasm32 set is clang's, so a header written for clang takes the same branches.
`<prefix>` is the install prefix the build was configured with, compiled in as
`VCC_SHARE_DIR`. The standard directory is searched after every `-I` directory.

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
| `-D` / `-U` options | 200 each |
| Macro pushback buffers in flight | 64 |

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
| `deps.c` | The make rule of `-M` and its family. |
| `defs.h` | The state struct, the symbol-table entry, the size limits. |
| `intern.h` | Scan-table macros, marker bytes, the macro-name filter. |
