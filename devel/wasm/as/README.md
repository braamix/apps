# as — the WebAssembly assembler

`as` reads modules in WebAssembly's text format, the language
[Wasm_Assembly_Language.md](../Wasm_Assembly_Language.md) describes. It
writes each one as a relocatable object for `ld` to link with what clang
compiles, or with `--module` as a module that runs as it is.

There is no llvm text assembler, so the reference is wabt's `wat2wasm`
1.0.42: `as --module` writes what `wat2wasm` writes, and `as` what
`wat2wasm --relocatable` writes, byte for byte, but where this file says
otherwise. Its messages are the reference interpreter's, which is what the
WebAssembly test suite expects.

## Using as

    as [options] file.s ...

A source is named `.s` on Braam, as assembly is, where elsewhere the text
format is `.wat`. Each is written to a file of its own in the current
directory, its extension replaced: `hello.s` to `hello.o`, or to
`hello.wasm` with `--module`.

    $ as --module hello.s
    $ ls
    hello.wasm  hello.s

| Option | Meaning |
| --- | --- |
| `--module` | write a module, `file.wasm` |
| `--debug-names` | name what has an id, in the `name` section |
| `-o <file>` | write there instead; one input only |
| `--tokens` | print the tokens, write nothing |
| `--numbers` | print typed literals' bits, write nothing |
| `--tree` | print the syntax tree, write nothing |
| `--resolved` | print it after resolution, write nothing |
| `-h`, `--help` | usage |

An error is reported as `file:line:col: error: message`, worded as the
reference interpreter words it: `unknown operator`, `unexpected token`,
`unknown function $f`, `type mismatch: instruction requires [i32] but
stack has [i64]`. The first error in a file ends that file; the other
files are still assembled. The exit status is 0 on success, 1 on an error
and 130 on `^C`.

## Objects

The text format has no symbols, so an object's come from the module, by
the rule `wat2wasm --relocatable` follows:

- Every function, table, global and tag is a symbol, in that order.
- One defined with an id is global, named by the id without its `$`; one
  without an id is local and hidden.
- An import is undefined, and named by its import. One from a module other
  than `env` also has an explicit name, as clang gives what
  `import_module` declares: it is the host's, and the linkers leave it an
  import, where an import from `env` must be defined by another object.
- An exported definition is also hidden, `EXPORTED` and `NO_STRIP`.

Every index to one of them is a padded LEB with a relocation: in code, in
an element segment, in an initializer. So is the type index of a
`call_indirect`, a `call_ref` or a block. After the sections comes
`linking`, version 2 with a symbol table, and a `reloc.<SECTION>` section
for each section with relocations.

So WAT can define functions C calls, and call C:

    $ cat add.s
    (module
      (func $add (param i32 i32) (result i32)
        (i32.add (local.get 0) (local.get 1))))
    $ as add.s
    $ nm add.o
    00000001 T add

## Data

A C object's variables are data symbols, and `&x` is a relocation. The text
format can say neither, and `wat2wasm` makes no data symbols: its segment
has none, so `wasm-ld --gc-sections` drops it, and code that reads its
address reads the wrong place. `as` has two annotations of its own for
this, §9.1 of the language.

- `(@sym)` after a data segment's id makes the segment a data symbol,
  named by the id, in a section `.data.<id>`. `(@sym rodata)` and
  `(@sym bss)` name `.rodata.` and `.bss.` instead; a `bss` segment must
  be all zeros. `align=n` is its alignment, 1 by default. The segment must
  have an id and be in memory 0.
- A `@sym` segment written without an offset is placed after the `@sym`
  segment before it, as its alignment allows, from address 0.
- `(@reloc $x k)` is the address of `$x`, a `@sym` segment, plus `k`, 0 by
  default. It stands, in a function, after `i32.const` or `i64.const` in
  place of the number, or after a load's or store's memory index in place
  of `offset=`; and among a data segment's strings as four bytes. In an
  object each is a relocation: `MEMORY_ADDR_SLEB` (or `_SLEB64`),
  `MEMORY_ADDR_LEB` (or `_LEB64`) and `MEMORY_ADDR_I32`.
- An object with data symbols imports memory 0 as `env.__linear_memory`,
  as clang's do, and has segment info. A module has the addresses written
  in instead.

For the same variables, the object has the segments, offsets, symbols and
relocations clang would give it:

    $ cat counter.s
    (module
      (memory 1)
      (data $count (@sym align=4) "\00\00\00\00")
      (data $count_ptr (@sym align=4) (@reloc $count))
      (func $bump (result i32)
        (i32.store (i32.const (@reloc $count))
          (i32.add (i32.load (@reloc $count) (i32.const 0)) (i32.const 1)))
        (i32.load (i32.const (@reloc $count)))))
    $ as counter.s
    $ nm counter.o
    00000001 T bump
    00000000 D count
    00000004 D count_ptr

C declares them `extern int count; extern int *count_ptr;`, and reads and
writes them as its own.

`@reloc` is refused outside a function body, since the linkers relocate
only code and data; and in the bytes of a segment of a 64-bit memory.

## Other annotations

These are the language's, §9.

- **`(@name "…")`**, next to an id or in its place, names the thing in the
  `name` section. With `--debug-names` an id does too, where there is no
  `@name`; the section is then laid out as `wat2wasm --debug-names` lays
  it out, and otherwise as the reference interpreter does.
- **`(@custom "name" (before code) "bytes" …)`**, between module fields, is
  a custom section, placed as it says: before or after a section, whether
  the module has one or not, and last when it says nothing.
- **`(@metadata.code.branch_hint "\01")`** before an `if` or a `br_if` is a
  hint, likely with 1 and unlikely with 0. The hints go into the
  `metadata.code.branch_hint` section, before the code. In an object, each
  function's index there is relocated, as `wat2wasm` does it.

An annotation of these kinds anywhere else is an error, as are malformed
ones, in the reference's words. Any other annotation is white space.

## How it differs from wat2wasm

Where `wat2wasm`'s output is a choice rather than the spec's, `as` makes
the same one: sections with nothing in them are left out, the data count
section is dropped when no code names a data segment, an element segment
takes the smallest form that says it, locals are grouped by runs of one
type. Where it differs:

- **It reads GC's text syntax**: `rec`, `sub`, `struct`, `array`, and the
  abstract heap types, which `wat2wasm` does not.
- **Its messages are the reference interpreter's**, from parsing to
  validation.
- **It is exact where wabt is not.** Some hex floats a little above a
  halfway point are rounded up, as they should be, and `align=2**63` is
  written as 63, where `wat2wasm` truncates it to 32 bits.
- **It writes a plain import section**, where `wat2wasm --enable-all`
  groups imports by module into the compact encoding, which is not in the
  language.
- **It places a custom section where `@custom` says**, as the reference
  does; `wat2wasm` writes each one last.
- **A branch hint is on its instruction.** On a folded `if` or `br_if`,
  `wat2wasm` puts it on the first operand. A hint on any other
  instruction is refused, as the reference refuses it.
- **It has `@name`**, and the data annotations.
- **Its objects are clang's where wabt's are not.** A relocation section is
  named `reloc.CODE`, not `reloc.Code`, and the branch hints' is
  `reloc.metadata.code.branch_hint`, not `reloc.Custom`. Tags have symbols
  and relocations, and so do the function indices of an element segment;
  a concrete `ref.null` is not relocated as a function. Two exports
  without an id are not a duplicate symbol. An import from a module other
  than `env` has an explicit name, so that `ld` and `wasm-ld` link it
  without `--allow-undefined`.

## Inside

A source goes through five passes, each over the whole module:

| File | What it does |
| --- | --- |
| `lexer.cpp` | tokens with their locations (§3) |
| `number.cpp` | integer and float literals to bits, exactly (§4) |
| `parser.cpp` | tokens to the tree, the text format's abbreviations undone |
| `resolve.cpp` | ids to indices, implicit types, inline exports and segments, data addresses |
| `valid.cpp` | validation, the reference interpreter's algorithm and messages |
| `encode.cpp` | the tree to a module's or an object's bytes: sections, relocations, `linking`, names, custom sections, branch hints |
| `ast.h`, `ast.cpp` | the tree, its arena, and a printer |
| [../lib/optable.cpp](../lib/optable.cpp) | every instruction by its text name: encoding and immediates |
| `as.cpp` | the core's entry: a source in, an object or module out |
| `driver.cpp` | the command line |
| `braam.cpp` | reads the sources, writes the outputs |

As in every tool here, the core is plain C++ over bytes already in memory,
and only `braam.cpp` awaits.

**The tree** is [wat.asdl](wat.asdl), whose header says what each pass
leaves in it. The parser unfolds folded instructions and splits grouped
declarations. Resolution numbers every id and label, makes the implicit
types of §5.4 in the order `wat2wasm` makes them, turns inline exports
into `Export`s and inline segments into `Elem`s and `Data`s, and lays out
`@sym` segments. `ast.h` is the same tree in C++, held to the ASDL by a
test.

**Validation** follows the reference's `valid.ml`: its order of checks,
and a stack of operand types where the unknown type of an unreachable
stack is bottom. Rec groups are numbered so that equal groups, and so
equal types, get equal numbers; a subtype is found by its declared
supertypes. It covers §10's instructions too: an atomic access must be
aligned naturally, a shared memory must have a maximum, `rethrow` must
name a `catch`.

**Limits.** Blocks and folded instructions nest without bound, and the
native stack is small, so the parser, the resolver, the validator and the
encoder each keep a stack of their own; 10000
nested blocks are tested. The source is read whole and the tree is in an
arena. There are no exceptions: errors are values, and the first one
ends the file.

## Tests

The tests need wabt 1.0.42 on the host (`wat2wasm`, `wast2json`,
`wasm-objdump`, `wasm-validate`), and llvm. The WebAssembly test suite is
vendored in [test/suite/](test/suite/): the spec's `core/` tests and its
`custom/` tests of annotations. `wast2json` reads 233 of the 258 core
scripts, with every feature but compact imports; the other 25 use GC's
text syntax.

- [test/asdl.mjs](test/asdl.mjs) checks [wat.asdl](wat.asdl) with
  [validate_asdl.py](validate_asdl.py): it must parse, every type be
  defined and reachable, no constructor twice; broken copies must be
  refused. It holds [ast.h](ast.h) to it, and the printer in
  [ast.cpp](ast.cpp) to its constructors. It needs `python3` with
  `pyasdl`.
- [test/empty.mjs](test/empty.mjs) runs `as` through
  [host.mjs](host.mjs), which boots the harness once for many runs:
  `(module)` must be the empty module, and the command line's outputs and
  errors are checked.
- [test/lexer.mjs](test/lexer.mjs), [test/parser.mjs](test/parser.mjs)
  and [test/resolve.mjs](test/resolve.mjs) hold `--tokens`, `--tree` and
  `--resolved` of crafted sources to golden files, and crafted errors to
  their messages and places. The parser nests 10000 deep, and so do the
  labels. `resolve.mjs` then assembles the whole suite: a malformed
  module must be refused with its message, and an invalid one may be
  refused only with its own.
- [test/number.mjs](test/number.mjs) holds `--numbers` to `wat2wasm` on
  nine thousand literals: every one in the suite, and generated ones at
  each width's limits and around halfway points. Hex floats are held to
  an exact computation instead, and f64 decimals to JS's `Number()` too.
- [test/optable.mjs](test/optable.mjs) checks the instruction table: its
  names are those the language lists, its alignments the language's, its
  encodings those of disasm's table.
- [test/module.mjs](test/module.mjs) assembles every valid module of the
  suite with `--module`. Each must equal what `wast2json` wrote, but one
  where wabt is wrong, which is listed. The 25 GC scripts' modules must
  load in V8 and be read back by `disasm`, and their encodings are held
  to bytes checked by hand.
- [test/spec.mjs](test/spec.mjs) runs the suite: every script's commands
  in order, each module assembled by `as` and instantiated in V8. Every
  assertion must hold, and a malformed or invalid text module must be
  refused by `as` with the reference's message. A function whose type is
  all numbers is called from a module `as` makes for it, which compares
  the results' bits in wasm, since JS cannot carry a v128 or a signalling
  NaN.
- [test/valid.mjs](test/valid.mjs) validates what the suite does not
  reach, §10's instructions: valid modules, which `wasm-validate` must
  pass too, and invalid ones with their messages.
- [test/annot.mjs](test/annot.mjs) assembles the suite again with
  `--debug-names`, each module's names as `wast2json --debug-names` writes
  them. It runs the `custom/` scripts: each module is held to the sections
  it must have, each malformed one refused with the reference's message.
  Branch hints, custom sections and names are held to `wat2wasm`'s bytes,
  module and object, where it writes the same; and an object with data to
  what `llvm-objdump` reads in clang's.
- [test/link.mjs](test/link.mjs) assembles three objects on Braam, one
  defining functions C calls, one calling C, and one with data C reads
  and writes, and links them with C by both `ld` and `wasm-ld`. The
  outputs must be equal, and the program must run and print what it
  should.
- `make longtest` runs [test/object.mjs](test/object.mjs), which assembles
  every valid module of the suite as an object. Each must equal what
  `wast2json -r` writes, but for the relocation sections' names and the
  explicit names of imports from modules other than `env`; where
  the rule goes beyond wabt's, the objects are counted. Each is then read
  by `llvm-nm`, `llvm-objdump -r`, `wasm-objdump -x`, `nm` and
  `disasm -r`, which must agree. Where llvm cannot read an object, its
  refusal must be one the test lists, with the reason.

A test still to write: every program in this tree, through `wasm2wat` and
back through `as --module`, must equal `wat2wasm`'s bytes.
