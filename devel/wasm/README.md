# wasm — WebAssembly tools for Braam

The `wasm` package installs seven tools for WebAssembly. All run on Braam.

- `ld`, a linker, turns the object files clang compiles into a program
  Braam can run. It does what `wasm-ld`, LLVM's linker, does, and gives
  the same output.
- `strip` removes names and debug information from a program or an
  object. It does what `llvm-strip` does, and gives the same output.
- `ar` makes and changes libraries of object files: archives, which `ld`
  links as `lib<name>.a`. It is FreeBSD's `ar`, and writes the same bytes
  as `llvm-ar --format=gnu`.
- `size` prints how big a program, an object or each member of an archive
  is: its code and data, or every section. It prints what `llvm-size`
  prints.
- `nm` lists the symbols of a program, an object or each member of an
  archive. It prints what `llvm-nm` prints.
- `disasm` shows the code of a program, an object or each member of an
  archive as instructions, and its data as bytes. The code is laid out as
  `llvm-objdump -d` lays it out, with better comments.
- `as`, an assembler, turns WebAssembly's text format into an object for
  `ld`, or a module. It does what wabt's `wat2wasm` does, and gives the
  same output.

The tests check the first five and `as` byte for byte, and `disasm` line
for line.

## Using ld

    ld [options] file.o ... lib.a ... -lname ... @file

With no options, `ld` makes a Braam program, as `braam_add_program` does:

    ld main.o -L/lib -lbraam_proc -o hello

The output is `a.out` if no `-o` is given. A link that fails writes nothing.
The exit status is 0 on success, 1 on an error and 130 on `^C`.

`@file` reads more arguments from a file, which helps when a command line
would be long.

Options are spelled as in `wasm-ld`. The common ones:

| Option | Meaning |
| --- | --- |
| `-o <file>` | output file |
| `-L <dir>`, `-l <name>` | look for `lib<name>.a` in `<dir>` |
| `--entry=<sym>`, `--no-entry` | the program's entry point, or none (the default) |
| `--export=<sym>` | export a symbol |
| `--allow-undefined` | let undefined functions become imports |
| `--gc-sections`, `--no-gc-sections` | drop unused code and data (on by default) |
| `-z stack-size=<n>` | stack size (128 KB by default) |
| `--initial-memory=<n>`, `--max-memory=<n>` | memory size in bytes |
| `-O0`, `-O1` | whether to merge equal strings (1 by default) |
| `--strip-debug`, `--strip-all` | drop debug info, or all names too |
| `--compress-relocations` | smaller code; needs `--strip-debug` |
| `-Map=<file>` | write a map of where everything went |
| `--print-gc-sections` | list what was dropped |
| `--why-extract=<file>` | say why each archive member was loaded |
| `--verbose` | print the memory layout |
| `--no-demangle` | show C++ names as they are mangled |

Options of `ld`'s own:

| Option | Meaning |
| --- | --- |
| `--braam-pages=<init>,<max>` | pages of memory the program asks for |
| `--braam-abi=<n>` | process ABI to stamp; the SDK's by default |
| `--no-import-memory` | define the memory in the module |
| `--dump <file>` | print an object's symbols and relocations |

## What it does not link

`ld` refuses these with a message, rather than making a broken program:

- LLVM bitcode, from `-flto`;
- position-independent code, from `-fPIC`;
- threads: thread-local storage and shared memory;
- 64-bit wasm;
- exceptions and GC types.

## How it differs from wasm-ld

Only in two ways, both because Braam gives every process fresh, zeroed
memory:

- The memory is imported by default, as Braam programs need.
- Data is placed directly, so there is no start function to copy it in.

With `--no-import-memory`, the output is exactly `wasm-ld`'s, apart from
the `braam` section that marks it as a Braam program.

## Linking the whole tree with ld

    make LINKER=ld

builds every program in the tree with `ld` instead of `wasm-ld`. Then
`make test longtest LINKER=ld` tests them. A plain `make` switches back.

`ld` only runs on Braam, so on the host it runs under the SDK's test
harness: [ld/host.mjs](ld/host.mjs) boots a Braam kernel, links there and
copies the result out. The whole tree relinks in a few seconds.

`node devel/wasm/ld/sizes.mjs` compares, program by program, what the two
linkers wrote.

## Using strip

    strip [options] file...

With no options, `strip` removes every custom section but `braam`: debug
information, function names, and the rest. It strips each file in place,
and leaves a file alone if nothing was removed.

    strip hello
    strip -g main.o -o main-nodebug.o
    strip -g libfoo.a

An archive has each member stripped, and is written anew as `llvm-strip`
writes it: every date, owner and group 0, every mode 644, and a symbol
table if it had one. A member that is not wasm is an error, and so is a
BSD-format archive; `ar` makes the GNU format.

The exit status is 0 on success, 1 on an error and 130 on `^C`. An error in
one file is reported, and the other files are still stripped.

| Option | Meaning |
| --- | --- |
| `-s`, `--strip-all` | remove every custom section but `braam` (the default) |
| `-g`, `-S`, `-d`, `--strip-debug` | remove debug information only |
| `-R <name>`, `--remove-section=<name>` | remove that custom section too |
| `--keep-section=<name>` | keep that custom section |
| `-o <file>` | write here instead of in place; one input only |
| `--help` | usage |

`-R` and `--keep-section` may be given more than once, and
`--keep-section` wins over everything else.

Standard sections, such as the code and the data, are never removed. In an
object file, a removed section leaves an empty one in its place, because
the object's symbols and relocations count sections by number. `ld` links
such an object. A program is simply made smaller.

`strip` differs from `llvm-strip` in two ways, both because the result
would not run:

- **It keeps the `braam` section.** That section is what makes a module
  a Braam program, and without it the system refuses to run it. So
  `strip` does what `llvm-strip --keep-section=braam` does, and `-R braam`
  is an error.
- **It refuses to remove a standard section.** `llvm-strip -R CODE` writes
  a module no browser will load.

A program can also be linked stripped: `ld --strip-all` writes exactly
what `ld` and then `strip` would, without the second pass.

## Using ar

    ar -d [-Tjsvz] archive file ...
    ar -m [-Tjsvz] [-a position-after | -b position-before] archive file ...
    ar -p [-Tv] archive [file ...]
    ar -q [-TcDjsUvz] archive file ...
    ar -r [-TcDjsUuvz] [-a position-after | -b position-before] archive file ...
    ar -s [-jz] archive
    ar -t [-Tv] archive [file ...]
    ar -x [-CTouv] archive [file ...]

The first letter says what to do; the rest change how:

| Mode | Meaning |
| --- | --- |
| `-r` | add files, replacing members of the same name in their place |
| `-q` | append files, without looking for members of the same name |
| `-d` | delete members |
| `-m` | move members, to the end or to `-a`/`-b`'s position |
| `-s` | write the symbol table |
| `-t` | list members; with `-v`, as `ls -l` would |
| `-p` | print members to stdout |
| `-x` | extract members into the current directory |

The dash may be left out, so the usual way to make a library is

    ar rc libfoo.a one.o two.o

`-c` creates the archive without a warning. `-v` says what was done to each
member. `-u` replaces or extracts only what is newer, and `-C` extracts
nothing that is already there.

The archive is written deterministically: every date, owner and group is
0 and every mode 644, so the same files always make the same bytes. `-U`
keeps each file's date instead. A symbol table, `/`, is written whenever a
member defines a symbol; `-S` leaves it out. `ld` does not read it, as
`wasm-ld` does not, but other linkers do.

The exit status is 0 on success and 1 on an error.

What differs from FreeBSD's `ar`:

- **The archive is replaced safely.** It is written to `<archive>.ar`
  first and then renamed over the old one, so a failure leaves the old
  archive whole. If `<archive>.ar` already exists, that is an error.
- **Files have no mode, owner or group on Braam.** A member added with
  `-U` has mode 644, and owner and group 0. An extracted file gets the
  default mode.
- **A file's date cannot be set.** `-x -o` warns once that it cannot
  restore the dates, and extracts anyway.
- **There is no `-M`**, the MRI script mode, and no `ranlib`. `ar -s` does
  what `ranlib` did.

## Using size

    size [options] [file...]

With no file, `size` reads `a.out`, and `-` reads standard input. An
archive has each member sized in turn, and a member that is not wasm is
skipped.

    $ size hello
       text	   data	    bss	    dec	    hex	filename
       5514	    289	      0	   5803	   16ab	hello

Text is the code section, data the data section. Bss is always 0: memory
the data does not fill is zero already, and takes no room in the file.

`-A` lists every section instead, with its size and its offset in the
file. An object's sections are all at 0, as `llvm-size` has them.

| Option | Meaning |
| --- | --- |
| `-B`, `--format=berkeley` | text, data and bss, a line per module (the default) |
| `-A`, `--format=sysv` | every section, its size and address |
| `-m`, `--format=darwin` | as `-B`, since a wasm module is not Mach-O |
| `-d`, `-o`, `-x`, `--radix=10\|8\|16` | numbers in decimal, octal or hex |
| `-t`, `--totals` | a last line adding up every module; `-B` only |
| `--common` | accepted; wasm has no common symbols |
| `-h`, `--help` | usage |

The exit status is 0 on success and 1 on an error. An error in one file
is reported, and the other files are still sized. A `--format` or
`--radix` that is not known is an error too, but the default stands and
the files are sized anyway, as `llvm-size` does.

`size` differs from `llvm-size` only in its messages, which are worded as
`strip`'s are. It has none of the Mach-O options.

## Using nm

    nm [options] [file...]

With no file, `nm` reads `a.out`, and `-` reads standard input. Each
symbol is a line: its address, a letter for its kind, and its name,
sorted by name.

    $ nm hello.o
    00000000 d .L.str
    00000001 T fx_main
             U fx_puts

| Letter | Meaning |
| --- | --- |
| `T`, `t` | a function |
| `D`, `d` | data, a global, a table, or a section |
| `W` | a weak definition |
| `U` | undefined |
| `w` | undefined and weak |

A capital letter is a symbol other files can see; a small one is local.

An object's symbols are those of its symbol table, and an address counts
from the start of the symbol's section: a function's from the start of
the code, a global's from the start of the globals. Data is at its
address in memory.

A program has no symbol table. Its symbols come from its `name` section:
every function, global and data segment named there. A function is
global if it is exported. Where the `name` section has been stripped,
the exports are the symbols instead. An address in a program is an
offset in the file, but data is still at its address in memory.

An archive's members are listed one by one, each under its own name. A
member that is not wasm is skipped.

| Option | Meaning |
| --- | --- |
| `-A`, `-o`, `--print-file-name` | name the file on every line |
| `-B`, `--format=bsd` | address, letter and name (the default) |
| `-P`, `--portability`, `--format=posix` | name, letter, address and size |
| `-f sysv`, `--format=sysv` | a table with every column |
| `-j`, `--format=just-symbols` | names only |
| `-m`, `--format=darwin` | as `-B`, since a wasm module is not Mach-O |
| `-C`, `--demangle` | show C++ names demangled; `--no-demangle` undoes it |
| `-g`, `--extern-only` | only symbols that are not local |
| `-u`, `--undefined-only` | only undefined symbols |
| `-U`, `--defined-only` | only defined symbols |
| `-W`, `--no-weak` | no weak symbols |
| `-n`, `-v`, `--numeric-sort` | sort by address, undefined symbols first |
| `--size-sort` | sort by size, and print sizes rather than addresses |
| `-p`, `--no-sort` | keep the file's order |
| `-r`, `--reverse-sort` | sort backwards |
| `-S`, `--print-size` | print each symbol's size too |
| `-t <radix>`, `--radix=<radix>` | `o`, `d` or `x` (the default) |
| `-M`, `--print-armap` | print an archive's symbol table first |
| `--export-symbols` | every defined global name of every file, sorted, once each |
| `--quiet` | no `no symbols` note |
| `-h`, `--help` | usage |
| `-V`, `--version` | version |

Letters can be run together, as in `nm -gS`. `-f`, `-t` and `-X` take
their value in the same word or the next.

A module with no symbols at all is noted on stderr as `<file>: no
symbols`, and that is no error. The exit status is 0 on success and 1 on
an error. An error in one file is reported, and the other files are
still listed. A `--format`, `--radix` or `-X` value that is not known is
an error too, but the files are listed anyway, as `llvm-nm` does.

`nm` differs from `llvm-nm` in these ways:

- **Its messages** are worded as `strip`'s are.
- **It does not read LLVM bitcode**, from `-flto`, and says so. `llvm-nm`
  lists a bitcode file's symbols.
- **`-D` is always an error**, as it is in `llvm-nm` for wasm: a wasm
  module has no dynamic symbol table.
- **It has no `-l`**, which needs debug information read, and none of the
  Mach-O or XCOFF options. `-a`, `--special-syms`, `--no-llvm-bc`,
  `--without-aliases` and `-X` are accepted, and change nothing for wasm.

`nm` reads any well-formed module, also what `ld` does not link:
thread-local storage, shared memory, exceptions and their tags, `-fPIC`,
wasm64 and GC types.

## Using disasm

    disasm [options] [file...]

With no file, `disasm` reads `a.out`, and `-` reads standard input. It
shows the code, one instruction to a line, as `llvm-objdump -d` does.
With `-D` it also shows the data, sixteen bytes to a line: the two
sections a module loads into memory.

    $ disasm -D hello.o

    hello.o:	file format wasm

    Disassembly of section CODE:

    00000000 <CODE>:
            # 1 functions in section.

    00000001 <fx_main>:

           3: 41 80 80 80 80 00    	i32.const	0
           9: 10 80 80 80 80 00    	call	0               # fx_puts
           f: 0b           	end

    Disassembly of section DATA:

    00000000 <.L.str>:
           0: 68 65 6c 6c 6f 2c 20 77 6f 72 6c 64 0a 00        hello, world..

Each function starts with its name and its locals. An address in the code
is an offset: in the file for a program, in the section for an object.
Data is at its address in memory, which is what an `i32.const` in the code
names; a passive segment starts at 0. A run of zero bytes is shown as
`...`.

| Option | Meaning |
| --- | --- |
| `-d`, `--disassemble` | the code (the default) |
| `-D`, `--disassemble-all` | the code and the data |
| `-r`, `--reloc` | an object's relocations, under what they patch |
| `-C`, `--demangle` | show C++ names demangled; `--no-demangle` undoes it |
| `--no-show-raw-insn` | no instruction bytes |
| `-h`, `--help` | usage |

The comments say what llvm's do not. A branch is annotated with the
block it goes to, and the place it lands is marked; labels are numbered
in each function. A call, a global or a tag is annotated with its name, an
indirect call with its signature, and a float with its exact value in hex.
[Wasm_Bytecode.md](Wasm_Bytecode.md) explains every instruction it prints.

The exit status is 0 on success, 1 on an error and 130 on `^C`. An error
in one file is reported, and the other files are still shown.

`disasm` differs from `llvm-objdump -D` in these ways:

- **Data is shown as bytes.** `llvm-objdump -D` decodes every section as
  code, the data and the type section too, and crashes on some.
- **Branch comments are right.** llvm closes no block at an `end` and
  opens none at an `if`, so after the first inner `end` its depths are
  counted among blocks long closed.
- **Operands are spelled as WebAssembly's text format spells them.**
  `select`, not `f32.select`; `ref.null func`; `i32.load offset=8
  align=1`, not `i32.load 8:p2align=0`; `call_indirect type=2 table=0`,
  where llvm leaves the table out; a block's signature, where llvm says
  `unknown_type`; a float in decimal, where llvm prints hex; and no
  padding after a name.
- **Only code and data are shown.** The other sections are not loaded,
  and `nm` and `size` say what is in them.
- **Its messages** are worded as `strip`'s are, and it does not read LLVM
  bitcode.

Like `nm`, it reads any well-formed module, also what `ld` does not link.
A wasm64 module's addresses have sixteen digits, as llvm prints them.

## Using as

    as [options] file.wat ...

`as` reads modules in WebAssembly's text format, the language
[Wasm_Assembly_Language.md](Wasm_Assembly_Language.md) describes, and
writes each one to a file of its own:

    $ as --module hello.wat
    $ ls
    hello.wasm  hello.wat

`hello.wat` is written to `hello.o`, a relocatable object for `ld` to link
with what clang compiles, or with `--module` to `hello.wasm`, a module that
runs as it is; both in the current directory, and `-o` names the output of
a single input.

An object's symbols come from the module, as `wat2wasm --relocatable`
makes them. Every function, table, global and tag is a symbol. One defined
with an id is global, named by the id without its `$`; one without an id
is local and hidden. An import is undefined, and named by its import. An
export is also hidden and kept. Every index to one of them in code, in an
element segment or in an initializer is a padded LEB with a relocation, as
is the type index of a `call_indirect`, a `call_ref` or a block. So WAT can define functions C calls, and call
C:

    $ cat add.wat
    (module
      (func $add (param i32 i32) (result i32)
        (i32.add (local.get 0) (local.get 1))))
    $ as add.wat
    $ nm add.o
    00000001 T add

Data needs two annotations of `as`'s own, since the text format has no
data symbols. `(@sym)` after a data segment's id makes it a data symbol,
named by the id, in a section `.data.<id>`; `(@sym rodata)` and
`(@sym bss)` name `.rodata.` and `.bss.` instead, and `align=n` gives its
alignment, 1 by default. A segment written without an offset is placed
after the one before it, as its alignment allows. `(@reloc $x k)` is the
address of `$x` plus `k`, a relocation where it stands: the value of an
`i32.const` or `i64.const`, a load's or store's offset in place of
`offset=`, or four bytes among a segment's strings. An object with data
symbols imports memory 0 as `env.__linear_memory`, as clang's do; a module
has the addresses written in instead.

    $ cat counter.wat
    (module
      (memory 1)
      (data $count (@sym align=4) "\00\00\00\00")
      (data $count_ptr (@sym align=4) (@reloc $count))
      (func $bump (result i32)
        (i32.store (i32.const (@reloc $count))
          (i32.add (i32.load (@reloc $count) (i32.const 0)) (i32.const 1)))
        (i32.load (i32.const (@reloc $count)))))
    $ as counter.wat
    $ nm counter.o
    00000001 T bump
    00000000 D count
    00000004 D count_ptr

C declares them `extern int count; extern int *count_ptr;` and reads and
writes them as its own.

The other annotations are the language's. `(@name "…")` after an id, or
in its place, names what it stands on in the `name` section; with
`--debug-names` an id alone does too, as `wat2wasm --debug-names` has it.
`(@custom "name" (before code) "bytes" …)` between module fields is a custom
section, placed as it says, or last. And `(@metadata.code.branch_hint "\01")`
before an `if` or a `br_if` is a hint, likely with 1 and unlikely with 0, in
the `metadata.code.branch_hint` section.

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
reference interpreter words it, which is what the WebAssembly test suite
expects: `unknown operator`, `unexpected token`, `unknown function $f`.
The first error in a file ends that file; the other files are still
assembled. The exit status is 0 on success, 1 on an error and 130 on
`^C`.

`as` differs from `wat2wasm` in these ways:

- **It reads GC's text syntax**: `rec`, `sub`, `struct`, `array`, and the
  abstract heap types, which `wat2wasm` does not.
- **It does not type-check code.** It refuses what the text format rules
  out, and what resolution needs: unknown or duplicate ids, imports after
  definitions, a second start, a type use that does not match its type.
  A module that is well-formed but invalid is written, and the engine
  refuses it.
- **It is exact where wabt is not.** Some hex floats a little above a
  halfway point are rounded up, as they should be, and `align=2**63` is
  written as 63, where `wat2wasm` truncates it to 32 bits.
- **It writes a plain import section**, where `wat2wasm --enable-all`
  groups imports by module into the compact encoding, which is not in the
  language.
- **It places a custom section where `@custom` says**, as the reference
  interpreter does; `wat2wasm` writes each one last.
- **A branch hint is on its instruction.** On a folded `if` or `br_if`,
  `wat2wasm` puts it on the first of its operands instead. A hint on any
  other instruction is refused, as the reference refuses it.
- **It has `@name`**, and the data annotations of its own.
- **Its objects are clang's where wabt's are not.** A relocation section is
  named `reloc.CODE`, not `reloc.Code`, and the branch hints' is
  `reloc.metadata.code.branch_hint`, not `reloc.Custom`. Tags have symbols
  and relocations,
  and so do the function indices of an element segment, where wabt leaves
  them unrelocated; a concrete `ref.null` is not relocated as a function.
  Two exports without an id are not a duplicate symbol.

## Inside

[lib/](lib/) reads and writes wasm modules, objects and archives, for every
tool in the package. Three rules keep it shared:

- **Pure.** It never awaits and never opens a file. It takes bytes already
  in memory and fills byte vectors and text.
- **No policy.** It reads anything well-formed and says what it found.
  What a tool refuses, the tool decides and words. Malformed framing is
  the one error in every tool.
- **Views.** Every name and byte range it returns points into the caller's
  file, which the caller keeps alive.

| File | What it does |
| --- | --- |
| `wasm.h` | the format's numbers: section ids, opcodes, symbols, relocations |
| `cursor.h` | bounds-checked reads, with a sticky failure |
| `emit.h` | encoders into a byte vector, with a sticky out of memory |
| `module.h` | the header and sections of any module |
| `object.h` | a relocatable object, parsed |
| `symbols.h` | a module's symbols, as llvm lists them |
| `archive.h` | archives, read and written |
| `stamp.h` | the `braam` section |
| `diag.h` | errors, as lld words them, after the tool's name |
| `out.h` | text built up in memory |
| `files.h` | reading and writing whole files; the one part that awaits |
| `demangle/` | LLVM's C++ name demangler, trimmed to what the tools use |

`object.h` refuses what `ld` cannot link, in `ld`'s words: thread-local
storage, shared memory, exception tags, `-fPIC`, wasm64 and GC types.
That is a policy, and `Object::link` turns it off for the tools that only
show an object. Bitcode and a module with no `linking` section are not
objects at all, and are refused either way.

Each tool's core is plain C++ that works on files already read into
memory. Only its `braam.cpp` reads and writes files.

### ld

| File | What it does |
| --- | --- |
| `symtab.cpp` | matches each name to its definition |
| `gc.cpp` | finds what is used |
| `layout.cpp` | numbers functions and places data in memory |
| `writer.cpp` | writes the output, fixing up addresses |
| `driver.cpp` | parses the command line |

### strip

| File | What it does |
| --- | --- |
| `strip.cpp` | copies what is kept, and leaves placeholders in an object |
| `driver.cpp` | parses the command line |

### size

| File | What it does |
| --- | --- |
| `size.cpp` | adds up the sections and prints them, in either format |
| `driver.cpp` | parses the command line |

### nm

| File | What it does |
| --- | --- |
| `nm.cpp` | filters, sorts and prints a module's symbols, in each format |
| `driver.cpp` | parses the command line |

`-C` uses LLVM's demangler in [lib/demangle/](lib/demangle/).

### disasm

| File | What it does |
| --- | --- |
| `opcodes.cpp` | every opcode llvm decodes, its name and its operands |
| `code.cpp` | decodes and prints one instruction, and its comments |
| `disasm.cpp` | splits the code and data at symbols, names what the code refers to, and prints each part |
| `driver.cpp` | parses the command line |

The opcode table was made from `llvm-objdump`'s own output for every
opcode, so it keeps llvm's names, but for `select`. A large program is
written out a part at a time, so its text is never all in memory.

### as

| File | What it does |
| --- | --- |
| `lexer.cpp` | tokens with their locations |
| `number.cpp` | integer and float literals to bits, exactly |
| `ast.h`, `ast.cpp` | the tree of [as/wat.asdl](as/wat.asdl), its arena, and a printer |
| `parser.cpp` | tokens to the tree, the text format's abbreviations undone |
| `resolve.cpp` | ids to indices, implicit types, inline exports and segments, data addresses |
| `encode.cpp` | the tree to a module's or an object's bytes, custom sections and names included |
| [lib/optable.cpp](lib/optable.cpp) | every instruction by its text name: encoding and immediates |
| `driver.cpp` | parses the command line |

Blocks and folded instructions nest without bound, and the native stack is
small, so the parser, the resolver and the encoder each keep a stack of
their own. Where the binary format leaves a choice, such as an element
segment's form or whether a data count section is written, `as` makes the
one `wat2wasm` makes.

### ar

[ar/](ar/) is a port: FreeBSD's sources, with its names, comments and
messages. [ar/README.md](ar/README.md) says what had to change.

| File | What it does |
| --- | --- |
| `ar.cpp` | the command line and the modes |
| `read.cpp` | `-t`, `-p` and `-x` |
| `write.cpp` | every mode that writes, and the symbol table |
| `util.cpp` | warnings and errors, and the output |
| `getopt.cpp` | the options, as FreeBSD's `getopt_long` reads them |

[Wasm_Object_Format.md](Wasm_Object_Format.md) describes the file format
`ld` reads and writes, byte by byte,
[Wasm_Bytecode.md](Wasm_Bytecode.md) the instructions `disasm` prints, and
[Wasm_Assembly_Language.md](Wasm_Assembly_Language.md) the text format
`as` reads.

## Tests

`make test` runs [ld/test/](ld/test/). Each test links small programs with
both `ld` and `wasm-ld` and compares the results: symbols, what was
dropped, the memory map, and the output bytes. Every program must also
run on Braam. `relink.mjs` links three real programs and `ld` itself with
`ld`, and runs their own tests.

It also runs [strip/test/strip.mjs](strip/test/strip.mjs). It strips every
test program, object and archive, the SDK's libraries, and `ld` itself,
with both `strip` and `llvm-strip`, and the results must be equal byte for
byte. Then it checks
that a stripped program still runs, that `ld` links stripped objects as
`wasm-ld` does, and that each error is reported as it should be.

And it runs [ar/test/ar.mjs](ar/test/ar.mjs). Each mode is run by both
`ar` and `llvm-ar --format=gnu`, and the archives must be equal byte for
byte. Every SDK library is taken apart with `ar x` and put back with
`ar rc`, and must come out as the SDK's own file. Then `ld` links archives
`ar` made as `wasm-ld` does, `ar`'s listings are checked against golden
files, and each error is reported as upstream reports it.

And it runs [size/test/size.mjs](size/test/size.mjs). Every test program,
object and archive, the SDK's libraries and `ld` itself are sized by both
`size` and `llvm-size`, in every format and radix, and the output must be
equal byte for byte. Then each error is checked: its message, and that
what is printed beside it is still `llvm-size`'s.

And it runs [nm/test/nm.mjs](nm/test/nm.mjs). The same files, each test
program stripped too, two modules made by the test, and those
[nm/test/foreign.mjs](nm/test/foreign.mjs) builds of what `ld` refuses
are listed by both `nm` and `llvm-nm`, in every format, order, radix and
filter. The output must be equal byte for byte. Then each error is
checked, as for `size`.

And it runs [disasm/test/disasm.mjs](disasm/test/disasm.mjs). The same
files, the same modules of what `ld` refuses, and modules made by the
test holding every opcode with many operand values, are shown by both
`disasm -d` and `llvm-objdump -d`, also with `-r`, `-C` and
`--no-show-raw-insn`. The output must be equal line for line: the
layout, addresses, bytes and relocations exactly, and each instruction
once llvm's spelling is brought to `disasm`'s, a float by its value. The
branch comments are checked against a model of the blocks, and the names
against the relocations and the functions called. The data rows must give
back every segment's bytes at its address, and one object is checked
whole against a golden file.

And it runs [as/test/asdl.mjs](as/test/asdl.mjs), which checks
[as/wat.asdl](as/wat.asdl), the syntax tree the assembler will build, with
[as/validate_asdl.py](as/validate_asdl.py): it must parse, every field's
type must be defined, no constructor may be defined twice, and every type
must be reachable from `Module`. Broken copies of it must each be
refused. It needs `python3` with the `pyasdl` package. The same test holds
[as/ast.h](as/ast.h), the tree in C++, to it: every type and constructor,
each field with its C++ type by the mapping `ast.h` states, and nothing
else; and every constructor must be named by the printer in
[as/ast.cpp](as/ast.cpp). Broken copies of `ast.h` must be refused too.
[as/test/empty.mjs](as/test/empty.mjs) runs the assembler through
[as/host.mjs](as/host.mjs), which boots the harness once for many runs:
`(module)` must be the empty module, which V8 must load, and the command
line's outputs and errors are checked.
[as/test/lexer.mjs](as/test/lexer.mjs) holds `as --tokens` of a crafted
file to a golden one, and checks crafted errors with their places.
[as/test/parser.mjs](as/test/parser.mjs) holds `as --tree` of crafted
modules to a golden file, one for each abbreviation the parser expands and
each ambiguity it settles, checks crafted errors with their places, and
parses blocks and folded instructions nested 10000 deep.
[as/test/resolve.mjs](as/test/resolve.mjs) does the same for `as --resolved`:
crafted modules for each rule of numbering and implicit types, crafted
errors, and labels 10000 deep. Then it assembles every module of the
WebAssembly test suite, vendored in [as/test/suite/](as/test/suite/): what
is not malformed must be accepted, and a malformed module must be refused
with the expected message.
[as/test/number.mjs](as/test/number.mjs) holds `as --numbers` to
`wat2wasm` on nine thousand literals: every one in the suite, and generated
ones at each width's limits and around halfway points between floats.
Hex floats are held to an exact computation instead, since `wat2wasm`
rounds some of them wrongly, and f64 decimals to JS's `Number()` as well.
[as/test/optable.mjs](as/test/optable.mjs) checks the instruction table,
[lib/optable.cpp](lib/optable.cpp): its names are exactly those the
language lists, its alignments those the language gives, and its encodings
those of disasm's table wherever both have the instruction.
[as/test/module.mjs](as/test/module.mjs) assembles every module of the
suite with `as --module`. For the 233 scripts `wast2json` reads, each
module's bytes must be those `wast2json` wrote for it, with every feature
but compact imports; one differs where wabt is wrong, and is listed. The
other 25 use GC's text syntax: each valid module must load in V8, `disasm`
must read them back, and their encodings are held to bytes checked by hand.
It needs wabt 1.0.42 on the host.
[as/test/spec.mjs](as/test/spec.mjs) runs the suite: every script's
commands in order, each text module assembled by `as` and instantiated in
V8, with the `spectest` module for imports. Every `assert_return`,
`assert_trap`, `assert_exhaustion` and `assert_exception` must hold, a
malformed module must be refused by `as`, an invalid one by `as` or V8,
and an unlinkable or uninstantiable one by V8 when instantiated. JS cannot
carry a v128 or a signalling NaN, so a function whose type is all numbers
is called from a module `as` makes for it, which compares the results'
bits in wasm. It prints how many of each command passed.
[as/test/link.mjs](as/test/link.mjs) assembles three WAT objects on
Braam, one defining functions C calls, one calling C, and one with data C
reads and writes, and links them with a C object by both `ld` and
`wasm-ld`: the outputs must be equal, and the program must run and print
what it should.
[as/test/annot.mjs](as/test/annot.mjs) assembles the suite again with
`--debug-names`, and each module's names must be those `wast2json
--debug-names` writes. It runs the spec's own tests of annotations,
vendored beside the suite: each module is held to the sections it must
have, and each malformed one refused with the reference's message. Branch
hints, custom sections and names are held to `wat2wasm`'s bytes, module
and object, where it writes the same; and an object with data to the
symbols and relocations `llvm-objdump` reads in clang's own.
`make longtest` runs [as/test/object.mjs](as/test/object.mjs), which
assembles every module of the suite as an object. Each must equal what
`wast2json -r` writes, but for the relocation sections' names, where wabt
writes one; where the rule goes beyond wabt's, tags and element segments,
they are counted. Then every valid module's object is read by `llvm-nm`,
`llvm-objdump -r` and `wasm-objdump -x`, and by our `nm` and `disasm -r`:
`nm` must print what `llvm-nm` prints, and `disasm` the relocations
`llvm-objdump` lists for the code. Where llvm cannot read an object, its
refusal must be one the test lists, with the reason: GC types, a table's
init expression and a few more that llvm does not know.
