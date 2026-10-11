# as — the WebAssembly assembler

`as` reads LLVM's WebAssembly assembly, the language `clang -S` writes
and `llvm-mc` reads, and writes each source as a relocatable object for
`ld`. Its first user is vcc, the C compiler with coroutines, whose
`wasm32-braam` target writes this assembly: with `as` its output is
assembled on Braam rather than by clang on a host.

The reference is `llvm-mc` of LLVM 23.1.3, as `clang --target=wasm32 -c`
runs it: `as` writes the same object byte for byte, warns as it warns and
refuses what it refuses, in its words. It reads the part of the language
vcc writes, listed below; more of it is for later.

For WebAssembly's text format, `(module (func ...))`, the assembler is
[`wat`](../wat/README.md).

## Using as

    as [options] file.s ...

Each source is written to a file of its own in the current directory, its
extension replaced: `hello.s` to `hello.o`.

    $ as hello.s
    $ ld crt0.o hello.o -lc -o hello

| Option | Meaning |
| --- | --- |
| `-o <file>` | write there instead; one input only |
| `-h`, `--help` | usage |

An error is reported as llvm reports it: `file:line:col: error: message`,
then the line and a caret under the column. The next source is still
assembled after one that fails, and none of a failed one is written.

## The language

A statement is a label, a directive or an instruction, one to a line or
separated by `;`. `#` and `//` start a comment, and `/* */` encloses one.

    	.section	.text.add,"",@
    	.globl	add
    	.type	add,@function
    add:
    	.functype	add (i32, i32) -> (i32)
    	local.get	0
    	local.get	1
    	i32.add
    	end_function

**Directives.**

| Directive | What it does |
| --- | --- |
| `.section name,"flags",@` | switches to a section: `.text.*` for code, `.data.*`, `.rodata.*` and `.bss.*` for data, `.custom_section.*` for a custom section. Flags: `p` passive, `S` strings, `R` retain, `T` TLS |
| `.text`, `.data` | `.text` does nothing; `.data` switches to `.data` |
| `.globl`, `.global`, `.weak`, `.hidden` | a symbol's binding and visibility |
| `.type sym,@function` | also `@object` and `@global` |
| `.size sym, n` | a data symbol's size, which it must have |
| `.functype sym (params) -> (results)` | a function's signature, before it is called; after its label, the start of its body |
| `.local types` | the locals, right after `.functype` |
| `.globaltype sym, type[, immutable]` | a global, such as `__stack_pointer` |
| `.tabletype sym, type[, min[, max]]` | a table |
| `.import_module sym, name`, `.import_name sym, name` | where an import comes from |
| `.export_name sym, name` | a function's export |
| `.int8`, `.int16`, `.int32`, `.int64` | a value, or a symbol plus a constant |
| `.ascii`, `.asciz`, `.string` | strings, with C's escapes |
| `.skip n[, fill]`, `.space` | bytes |
| `.p2align k[, fill[, max]]`, `.align`, `.balign` | alignment, which the section takes as its own |
| `.ident`, `.file` | ignored |

**Instructions** are the MVP's control, variable and memory instructions,
its numbers, sign extension, `trunc_sat`, and `memory.copy` and
`memory.fill`: everything vcc writes. A block ends with `end_block`,
`end_loop` or `end_if`, a function with `end_function`. The operands are
written as llvm writes them:

| Form | Example |
| --- | --- |
| a block type | `block i32`, `if`, `block (i32) -> (i32)` |
| a depth, a local | `br_if 1`, `local.get 0` |
| `br_table` | `br_table {2, 1, 0, 2}`, the default last |
| a symbol | `call f`, `global.get __stack_pointer`, `i32.const s+4` |
| `call_indirect` | `call_indirect __indirect_function_table, (i32) -> (i32)`; the table may be left out |
| a load or a store | `i32.load 8`, `i64.store s+16:p2align=2` |
| a float | `f64.const 0x1.8p+1`, `1.5e3`, `-infinity`, `nan` |

A function's address, `i32.const f` or `.int32 f`, is a table index:
the object's table holds each such function, from 1.

## Objects

An object is laid out as llvm's `WasmObjectWriter` lays it out, and the
order of everything in it is llvm's:

- **Symbols are in the order llvm registers them**: at a label, at
  `.globl` and its kin, and where an instruction or `.int32` names them,
  an instruction's operands last to first. Imports, functions and the
  symbol table all follow that order, not the order of definitions.
- **The table comes last** when only addresses use it: llvm registers
  `__indirect_function_table` when it records the first relocation that
  takes an address, after the whole source is read.
- **Types** are numbered as each function's signature is first met in
  that order, `call_indirect`'s included.
- **Every size is a five-byte LEB**, and every relocated field a padded
  one holding llvm's provisional value: the function's index, the
  table slot, or the address in the object's own data.
- **The memory is imported** as `env.__linear_memory`, with pages for the
  data; an object with nothing in it still imports it.

## How it differs from llvm-mc

- **A plain `select` is `0x1b`.** llvm 23 writes it as the typed
  `select` with no types, `0x1c 0x00`, which no engine loads, and its type
  checker refuses the code it would come from. `select i32` is the typed
  one, as llvm writes it.
- **There is no type checker.** llvm checks the operand stack of every
  function and refuses a mismatch. `as` keeps only the two of its checks
  vcc relies on: a call before the callee's `.functype` (`symbol f:
  missing .functype`), and a global without `.globaltype`.
- **llvm's fatal errors** are reported as `as: error: message`, without
  the stack dump.
- **A real must be exact**, as for llvm: `f64.const 0.1` is refused,
  `0.5` and `0x1.999999999999ap-4` taken. A NaN takes no payload, also as
  for llvm, though vcc writes one as `nan:0x...`.

## Inside

| File | What it does |
| --- | --- |
| `lexer.cpp` | tokens, as llvm's AsmLexer makes them |
| `parser.cpp` | statements into sections of bytes, fixups and symbols, with llvm's state machine for functions and its messages |
| `asm.h`, `asm.cpp` | what a source comes to: sections, fixups, symbols in registration order, signatures |
| `writer.cpp` | the object: relocations chosen and recorded as llvm records them, imports, indices, the symbol table, the table's elements, the sections |
| [../lib/optable.cpp](../lib/optable.cpp) | each instruction's encoding, by its name |
| [../lib/number.cpp](../lib/number.cpp) | floats, exactly |
| `as.cpp` | the core's entry: a source in, an object out |
| `driver.cpp` | the command line |
| `braam.cpp` | reads the sources, writes the objects |

## Tests

They need LLVM's clang and llvm-objdump, which `ld`'s tests find.

- [test/empty.mjs](test/empty.mjs) runs `as` through
  [../wat/host.mjs](../wat/host.mjs): an empty source is llvm's empty
  object, outputs go where they should, and each error of the command
  line is reported.
- [test/objects.mjs](test/objects.mjs) assembles every source in
  [test/probe/](test/probe/) and [test/corpus/](test/corpus/) with `as` on
  Braam and with clang on the host. The objects must be equal byte for
  byte, the warnings too, and V8 must load each. The probes are written
  by hand, one for each part of the language. The corpus is vcc's:
  `crt0.s`, `doprnt.s` and the program `notes.s`, which
  [test/mkcorpus.sh](test/mkcorpus.sh) takes from a vcc checkout. Then a
  plain `select` must be `0x1b`.
- [test/errors.mjs](test/errors.mjs) gives `as` and clang 58 sources
  each must refuse: bad tokens, directives, operands, nesting and
  sizes. The messages must be the same, line, column and caret.
