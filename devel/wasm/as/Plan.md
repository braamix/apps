# Plan: `as`, the WebAssembly assembler

`as` reads a module in the text format,
[Wasm_Assembly_Language.md](../Wasm_Assembly_Language.md), and writes a
relocatable object, [Wasm_Object_Format.md](../Wasm_Object_Format.md), that
`ld` links with what clang compiles. With `--module` it writes a plain
module instead. The syntax tree between the two halves is
[wat.asdl](wat.asdl).

The work is in steps; those done are removed. Each ends with a test in `make test`, and none
starts before the one it builds on passes.

## Decisions to take first

These shape several steps; each has a recommendation.

1. **What an object's symbols are.** The text format has no symbols, so
   they come from the module. Recommended: wabt's rule, which
   `wat2wasm --relocatable` implements. Every function, table, global and
   tag is a symbol. A definition with an id is a global symbol named by the
   id without its `$`; one without an id is local and hidden. An import is
   undefined and named by its import. An exported definition is also
   `EXPORTED`, `NO_STRIP` and hidden. Every index to one of them in code is
   a padded LEB with a relocation. Tags go beyond wabt, which leaves them
   out.

   Two things wabt writes differently from clang: it names the section
   `reloc.Code` where clang writes `reloc.CODE`, and it keeps the module's
   own memory where clang imports `env.__linear_memory`. `wasm-ld` accepts
   both. Recommended: clang's section name, since the objects are linked
   with clang's, and the memory as the source says; the tests compare with
   wabt's bytes with that one name changed.

2. **Data addresses.** A C object's data is relocatable: its variables are
   data symbols and `i32.const &x` is a relocation. The text format cannot
   say either, and wabt makes no data symbols at all. Recommended: two
   annotations of our own, in step A10.
   - `(data $x (@sym) …)` makes the segment a data symbol, with segment
     info named `.data.x` (`.rodata.`, `.bss.` by an option of the
     annotation), and the memory imported as `env.__linear_memory`.
   - `(i32.const (@reloc $x 4))` is the address of `$x` plus 4: a
     `MEMORY_ADDR_SLEB` relocation. The same annotation in a load's or
     store's `offset=` position, and in a data segment's bytes, gives
     `MEMORY_ADDR_LEB` and `MEMORY_ADDR_I32`.

   Until step A10 an object has no data symbols. A module whose code uses
   data addresses gets the addresses written, and links only on its own.

   This is not hypothetical. `wat2wasm -r` on a module with
   `(data (i32.const 16) "hi\00")` and code that reads address 16 links
   under `wasm-ld`, but the segment has no symbol, so `--gc-sections`
   drops it; with `--no-gc-sections` it lands at 65536, and the code still
   reads 16.

3. **Whose bytes to match.** The other tools in this package match llvm's.
   There is no llvm text assembler, so the reference is wabt's `wat2wasm`:
   `as --module` writes what `wat2wasm` writes, and `as` what
   `wat2wasm --relocatable` writes, wherever wabt's rule covers the
   module. Where wabt is not canonical (its output is a choice, not the
   spec's), follow it anyway, and say so in the README. Where wabt is
   wrong, do not: `wat2wasm` 1.0.42 rounds some hex floats a little above
   a halfway point down instead of up, and `as` is exact. No module of the
   suite has such a literal; two assertions in `simd_lane` do. The tests need
   wabt on the host, as they need `llvm-objdump` now: `wat2wasm`,
   `wast2json`, `wasm-objdump`, version 1.0.42 from Homebrew, checked by
   the tests.

   `wast2json --enable-all` is the oracle for the suite: it writes every
   module of a script as a `.wasm`, numbered in order, which is what `as`
   is compared with. But not compact imports, which `--enable-all` also
   turns on: they group the import section into an encoding outside the
   language, which V8 does not load, so the tests name every other
   feature instead. It reads 233 of the suite's 258 scripts. The other
   25 use GC's text syntax, which wabt does not parse (`rec`, `struct`,
   `array`, `anyref` and the abstract heap types, `(ref exn)`): all of
   `gc/`, `type-rec`, `type-canon`, `type-equivalence`,
   `type-subtyping`, `ref_null`, `annotations`, `exceptions/tag`,
   `exceptions/try_table`, and the `table_init` tests. Those are checked by
   V8, by `disasm`, and by golden bytes reviewed by hand.

4. **Validation.** `wat2wasm` validates by default. Recommended: `as` does
   not type-check code; it does every check the text format defines, and
   what resolution needs (unknown or duplicate ids, the order of imports,
   one start, types that match a type use). Type checking is step A11, and
   optional. Until then the tests leave `assert_invalid` to V8.

5. **The test suite.** Tests cannot read `~/Daily`. Recommended: vendor
   the WebAssembly 3.0 core tests (`spec/test/core`, 12 MB, 258 scripts) and
   the proposals of §10 of the language (threads, legacy exceptions, wide
   arithmetic, custom page sizes) and `annotations` into
   `as/test/suite/`, with the commit each came from.

## Shape of the code

As in every tool here: a core of plain C++ that works on bytes already in
memory and never awaits, and one `braam.cpp` that reads and writes files.

| File | What it does |
| --- | --- |
| `as.cpp` | the core's entry: a source in, an object or module out |
| `lexer.cpp` | tokens with their locations (§3 of the language) |
| `number.cpp` | integer and float literals to bits, exactly (§4) |
| `ast.h`, `ast.cpp` | the tree of [wat.asdl](wat.asdl), its arena, and a printer |
| `parser.cpp` | tokens to the tree, the parse-time abbreviations done |
| `lib/optable.cpp` | every instruction by its text name: encoding and immediate shape |
| `resolve.cpp` | ids to indices, implicit types, exports, segment order |
| `encode.cpp` | the tree to sections, and the code's bytes |
| `linking.cpp` | symbols, relocations and the `linking` section |
| `custom.cpp` | `name`, `@custom` and branch-hint sections |
| `driver.cpp` | the command line |
| `braam.cpp` | reads the source, writes the output |

The command line: `as [options] file.wat …`, each file into its own
output, `file.o` by default (`file.wasm` with `--module`), or `-o` for one
input. Exit status 0, 1 on an error, 130 on `^C`. Messages are
`file:line:col: error: …`, worded as the reference interpreter words them
(`unknown operator`, `unexpected token`, `alignment must be a power of
two`), which is what the test suite's `assert_malformed` expects.

Limits to keep in mind throughout:

- **Nesting.** Blocks and folded instructions nest without bound, and the
  native stack is small. The parser keeps its own stack of open blocks
  rather than recursing per block; folded operands likewise. A test with
  10000 nested blocks proves it.
- **Memory.** The source is read whole and the tree is in an arena. A
  100 MB cap means a source of some tens of MB at most; the code section
  is written function by function so the output does not double the peak.
- **No exceptions, no `new`.** Errors are values, first one wins, as in
  `lib/diag.h`.

## Steps

### A9. The object

`linking.cpp`, by decision 1: the symbol table, padded LEBs with
relocations for every function, global, table, tag and type index in code
and in element segments, `reloc.CODE` and `reloc.ELEM`, and the `linking`
section, version 2. This is `as`'s default output.

*Done when*:

- `test/object.mjs` compares `as` with `wat2wasm --relocatable` on the
  suite's modules that its rule covers, byte for byte but for the
  relocation section's name;
- `llvm-nm`, `llvm-objdump -r`, `wasm-objdump -x` and our `nm`,
  `disasm -r` read every object, and agree on its symbols and
  relocations;
- a WAT object that defines functions and one that calls a C function are
  each linked with clang's objects by both `ld` and `wasm-ld`, the outputs
  are equal, and the program runs on Braam.

### A10. Annotations

`@name` into the `name` section (with `--debug-names`, as `wat2wasm` has
it; ids alone give names only with that option too); `@custom` sections in
their place; branch hints into `metadata.code.branch_hint`; and the data
annotations of decision 2, with their relocations.

*Done when* the suite's `annotations` tests and the spec repository's
`test/custom/` cases pass: bytes against `wat2wasm` where it implements
the annotation, and against the section's own layout where it does not.
And a WAT object with data, linked with a C program that reads it and
writes it, runs on Braam under both linkers.

### A11. Type checking (optional)

The validation algorithm of the core spec over the resolved tree, so `as`
refuses what V8 would, with a message saying where. Worth it once `as` is
used by hand; the tests so far do not need it.

*Done when* every `assert_invalid` of the suite is refused by `as`
itself.

## Beyond the steps

- A big test once the rest pass: every program in this tree, through
  `wasm2wat` and back through `as --module`, must equal `wat2wasm`'s
  bytes. It belongs in `make longtest`.
- `Wasm_Assembly_Language.md` gains a section for the data annotations
  when decision 2 is settled.
