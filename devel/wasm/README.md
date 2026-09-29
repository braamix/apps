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

It also ships [example programs](#examples) written in the text format,
which these tools build on Braam, and `crt.o` and `libw.a` in `lib/`,
which they link with. [Tutorial.md](Tutorial.md) explains two of them
line by line, for a beginner.

Each tool run with no arguments, or with `-h` or `--help` alone, prints
its usage: what it does, and its options.

The tests check the first five and `as` byte for byte, and `disasm` line
for line.

## Using ld

    ld [options] file.o ... lib.a ... -lname ... @file

With no options, `ld` makes a Braam program, as `braam_add_program` does:

    ld main.o -L/lib -lbraam_proc -o hello

A library `-lname` is looked for in each `-L` directory in turn, then in
the package's `lib/`, which holds the examples' `crt.o` and `libw.a`
([Examples](#examples)). So is an object named without a directory that
is not in the current directory: `ld crt.o fib.o -lw` finds both. The
package's `lib/` is the one `/pkg/bin/ld` leads to.

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

    size [options] file...

A file must be named: unlike `llvm-size`, `size` does not read `a.out`
without one. `-` is standard input. An archive has each member sized in
turn, and a member that is not wasm is skipped.

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

    nm [options] file...

A file must be named: unlike `llvm-nm`, `nm` does not read `a.out` without
one. `-` is standard input. Each symbol is a line: its address, a letter
for its kind, and its name, sorted by name.

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

    disasm [options] file...

A file must be named: unlike `llvm-objdump`, `disasm` does not read
`a.out` without one. `-` is standard input. It shows the code, one
instruction to a line, as `llvm-objdump -d` does. With `-D` it also shows
the data, sixteen bytes to a line: the two sections a module loads into
memory.

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

    as [options] file.s ...

`as` reads modules in WebAssembly's text format and writes each one as an
object for `ld`, `file.o`, or with `--module` as a module, `file.wasm`. It
writes what `wat2wasm` writes, and its messages are the reference
interpreter's. [as/README.md](as/README.md) says how to use it: its
options, the symbols of its objects, the annotations for data that C
reads and writes, where it differs from `wat2wasm`, how it works and how
it is tested.

## Examples

The package ships programs written in WebAssembly's text format, in its
`share/` directory. There is no C library on Braam to link them with, so
each one talks to the kernel itself. Copy them out and build them:

    $ cp /pkg/store/wasm-*/share/* .
    $ sh build.sh
    $ ./hello
    hello, world

| File | What it is |
| --- | --- |
| `hello.s` | the smallest program: one write, and exit |
| `echo.s` | writes its arguments, which it finds in the block `_start` is given |
| `cat.s` | copies stdin to stdout: a loop of reads and writes, turned inside out |
| `hello2.s` | `hello.s` again, on `crt.o`: only a `main` |
| `fib.s` | the Fibonacci numbers, in `i64` and by recursion, on `crt.o` |
| `primes.s` | the sieve of Eratosthenes, on `crt.o` |
| `crt.s` | the start of a program that computes, prints and exits: `crt.o` |
| `proc.s` | `_alloc`, `_free` and `_sig`, which every program needs; in `libw.a` |
| `fmt.s` | output into a buffer, and numbers in decimal; in `libw.a` |
| `args.s` | the arguments, one by one; in `libw.a` |
| `build.sh` | the commands that build the six programs |

`crt.o` and `libw.a` are also in the package's `lib/`, built, where `ld`
finds them. The first three programs link with `libw.a` for `proc.s`:

    as hello.s
    ld hello.o -lw -o hello

The last three define only `main`. `crt.o` calls it, and writes what it
printed once it returns. It is linked by name, since a library's member is
linked only for a symbol that something needs, and nothing needs
`_start`:

    as fib.s
    ld crt.o fib.o -lw -o fib

To change the library, build it in the current directory, and name that
directory to `ld` first:

    as crt.s proc.s fmt.s args.s
    ar rc libw.a proc.o fmt.o args.o
    ld crt.o fib.o -L. -lw -o fib

The kernel's two functions are imports from `kernel`, and `as` marks an
import from any module but `env` as the host's, so `ld` leaves it an
import. An import from `env` is a symbol that another object defines, and
`ld` refuses it if none does, as it refuses a missing C function.

`ld` stamps the program for the SDK's process ABI. So do not write a
`braam` section by hand with `as --module`: it breaks when the ABI moves.

A program is a module that imports `env.memory`, `kernel.sys` and
`kernel.sys_async`, and exports these five functions and nothing else:

| Export | What it is for |
| --- | --- |
| `_start(ptr, len) → i32` | the first step |
| `_resume(token, ptr, len) → i32` | each later step, with one reply |
| `_alloc(n) → ptr` | a block for the arguments or a reply; 0 fails the step |
| `_free(ptr, n)` | never called by the kernel |
| `_sig(sig)` | a signal the program asked for |

A step returns 1 while a call is outstanding, and 0 when the program is
done. The program cannot wait inside a step. It asks for something, returns,
and is resumed with the answer.

- **`sys(op, a0, a1, a2) → i32`** is answered at once. `sys(1, status, 0, 0)`
  is Exit: it records the status, and the step then returns 0. Without it,
  the status is 1.
- **`sys_async(op, token, ptr, len)`** is answered later, by
  `_resume(token, reply, len)`. The kernel copies the bytes at once, so the
  program can reuse the block as soon as the call returns. The op's low
  byte is the call and the rest is its argument:
  - Write, `16 | fd << 8`: 272 writes to stdout and 528 to stderr.
  - Read, `17 | fd << 8`: 17 reads up to 512 bytes of stdin.
- **A reply** is an `i32` status, then any data. For a write, the status
  is the count written, and a write may be short. For a read, it is the
  count read, followed by the bytes, and 0 is the end of the input. A
  negative status is an error.
- **The arguments** are one block. It holds a `u32` count, then each
  argument as a `u32` length and its bytes. `argv[0]` is the command's
  name, and the environment follows in the same form.

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

[as/README.md](as/README.md) describes its passes and files.

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

And it runs the tests of `as`, and `make longtest` the one that is slow;
[as/README.md](as/README.md#tests) describes them.

And it runs [examples/test/examples.mjs](examples/test/examples.mjs). The
examples are built on Braam by their own `build.sh`, with `as` and `ld`,
which finds `crt.o` and `libw.a` in the package's `lib/`. The library is
then built there from its sources with `as` and `ar`, and must be the
package's byte for byte. `ld` must refuse a program that needs `libw.a`
and is linked without it. Each program must import and export exactly what a Braam program
does. `wasm-ld` must link the same objects to the same bytes, apart from
the `braam` section. Each program is then run, and what it prints and its
exit status are held to [a golden transcript](examples/test/examples.golden).
`cat` must copy 3000 bytes that are not text exactly, over several reads,
and `primes` must print 5 MB.
