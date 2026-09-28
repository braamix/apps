# wasm — WebAssembly tools for Braam

The `wasm` package installs three tools for WebAssembly. All run on Braam.

- `ld`, a linker, turns the object files clang compiles into a program
  Braam can run. It does what `wasm-ld`, LLVM's linker, does, and gives
  the same output.
- `strip` removes names and debug information from a program or an
  object. It does what `llvm-strip` does, and gives the same output.
- `ar` makes and changes libraries of object files: archives, which `ld`
  links as `lib<name>.a`. It is FreeBSD's `ar`, and writes the same bytes
  as `llvm-ar --format=gnu`.

The tests check all three byte for byte. More tools, such as `as` and `nm`,
will join them here, one directory each.

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
| `archive.h` | the members of an archive |
| `stamp.h` | the `braam` section |
| `diag.h` | errors, as lld words them, after the tool's name |
| `out.h` | text built up in memory |
| `files.h` | reading and writing whole files; the one part that awaits |

`object.h` still refuses what `ld` cannot link, in `ld`'s words: bitcode,
exception tags, 64-bit limits and a module with no `linking` section.

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
| `demangle/` | LLVM's C++ name demangler, trimmed to what `ld` uses |

### strip

| File | What it does |
| --- | --- |
| `strip.cpp` | copies what is kept, and leaves placeholders in an object |
| `driver.cpp` | parses the command line |

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
`ld` reads and writes, byte by byte.

## Tests

`make test` runs [ld/test/](ld/test/). Each test links small programs with
both `ld` and `wasm-ld` and compares the results: symbols, what was
dropped, the memory map, and the output bytes. Every program must also
run on Braam. `relink.mjs` links three real programs and `ld` itself with
`ld`, and runs their own tests.

It also runs [strip/test/strip.mjs](strip/test/strip.mjs). It strips every
test program and object, and `ld` itself, with both `strip` and
`llvm-strip`, and the results must be equal byte for byte. Then it checks
that a stripped program still runs, that `ld` links stripped objects as
`wasm-ld` does, and that each error is reported as it should be.

And it runs [ar/test/ar.mjs](ar/test/ar.mjs). Each mode is run by both
`ar` and `llvm-ar --format=gnu`, and the archives must be equal byte for
byte. Every SDK library is taken apart with `ar x` and put back with
`ar rc`, and must come out as the SDK's own file. Then `ld` links archives
`ar` made as `wasm-ld` does, `ar`'s listings are checked against golden
files, and each error is reported as upstream reports it.
