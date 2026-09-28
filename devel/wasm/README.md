# wasm — WebAssembly tools for Braam

The `wasm` package installs `ld`, a linker for WebAssembly. It runs on
Braam and turns the object files clang compiles into a program Braam can
run. More tools, such as `as` and `nm`, will join it here, one directory
each.

`ld` does what `wasm-ld`, LLVM's linker, does, and gives the same output.
The tests check this byte for byte.

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

## Inside

The linker core is plain C++ that works on files already read into memory.
Only [ld/braam.cpp](ld/braam.cpp) reads and writes files.

| File | What it does |
| --- | --- |
| `reader.cpp` | parses object files and archives |
| `symtab.cpp` | matches each name to its definition |
| `gc.cpp` | finds what is used |
| `layout.cpp` | numbers functions and places data in memory |
| `writer.cpp` | writes the output, fixing up addresses |
| `driver.cpp` | parses the command line |
| `demangle/` | LLVM's C++ name demangler, unchanged but for its library |

[Wasm_Object_Format.md](Wasm_Object_Format.md) describes the file format
`ld` reads and writes, byte by byte.

## Tests

`make test` runs [ld/test/](ld/test/). Each test links small programs with
both `ld` and `wasm-ld` and compares the results: symbols, what was
dropped, the memory map, and the output bytes. Every program must also
run on Braam. `relink.mjs` links three real programs and `ld` itself with
`ld`, and runs their own tests.
