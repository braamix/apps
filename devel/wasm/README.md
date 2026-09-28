# wasm — WebAssembly tools for Braam

The package is `wasm`. It installs one command so far, `ld`: a static
linker for wasm32 object files, which runs on Braam and links Braam
programs there. `as`, `nm` and the rest will join it under `devel/wasm/`,
one directory per tool.

Like `lang/python`, it is not a port. Its references are the object format
that clang writes (tool-conventions `Linking.md`, written out in
[Wasm_Object_Format.md](Wasm_Object_Format.md)) and what `wasm-ld` does
with the same inputs. Every claim below is a test against `wasm-ld`.
[Plan.md](Plan.md) holds the steps still to come.

## ld

    ld [options] <file.o | lib.a | -l<name> | @rspfile>...

With no options, `ld` links the way `braam_add_program` does:

- no entry point;
- `--gc-sections`;
- the stack first, 128 KB;
- the memory imported;
- the `braam` stamp appended, with the process ABI of the SDK it was built
  against.

So `ld main.o -L/lib -lbraam_proc` writes an `a.out` that Braam runs.
`--braam` says the same thing explicitly.

Options are spelled as `wasm-ld` spells them:

- `-o`, `-L`, `-l` and `@file`;
- `--entry`, `--no-entry`, `--export`, `-u` and `--allow-undefined`;
- `--gc-sections`, `--no-gc-sections` and `--print-gc-sections`;
- `--stack-first`, `--no-stack-first`, `--global-base`, `-z stack-size=`,
  `--initial-memory`, `--max-memory` and `--import-memory`;
- `--trace`, `--why-extract`, `--error-limit` and `--verbose`.

Braam's `OptParse` has no long options, so `ld` parses these itself. What
clang passes and `ld` has no use for (`-O<n>`, `-m wasm32`, `--strip-debug`,
`--strip-all`, `--no-demangle`) is accepted and changes nothing.

Four more options are `ld`'s own:

- `--braam-abi=<n>` and `--braam-pages=<initial>,<max>` set the stamp;
- `--dump` prints an object's or archive's symbols and relocations in
  `llvm-objdump -t -r`'s layout;
- `--dump-symtab` and `--dump-layout` print the resolved symbols, the index
  spaces and the memory map.

The exit status is 0 on success, 1 on an error and 130 on `^C`. A link that
fails writes no output.

### What it links

`ld` links what clang emits for Braam: `linking` version 2, the nine
relocation types the tree uses, function and data sections, comdats,
constructors, weak symbols and GNU `ar` archives. Symbols resolve, archive
members load and unused chunks are dropped in `wasm-ld`'s order. So the
index spaces, the memory map and the diagnostics are `wasm-ld`'s, and so
is every section except the ones listed below.

### What it refuses

Each of these is refused with a message naming the file and offset, rather
than mislinked:

- LLVM bitcode (`-flto`);
- a wasm module that is not a relocatable object;
- position-independent code;
- thread-local storage and shared memory;
- wasm64;
- exception tags and GC types;
- mutable global imports and table imports.

### Where it differs from wasm-ld

- **Active data segments and no `.bss` bytes.** With `--import-memory`,
  `wasm-ld` cannot assume the memory is zeroed. So it makes the segments
  passive and clears `.bss` in a `__wasm_init_memory` it names in START.
  Every Braam process gets a fresh memory, so `ld` writes neither START nor
  `__wasm_init_memory`. That is why the DATA section is smaller.
- **A signature mismatch is an error.** `wasm-ld` warns and links a stub that
  traps when called. `ld` refuses the link.
- **No string merging.** Every `-O` level is `wasm-ld -O0`'s layout.
- **Debug info is dropped**, as `--strip-debug` would drop it.

### Tests

`make test` runs `ld/test/*.mjs`. Each test compiles its fixtures once
with clang and links them twice: by `ld` under the SDK's harness, and by
`wasm-ld` on the host. Then it compares, in this order:

- `--dump` against `llvm-objdump`;
- symbol resolution against `--trace` and `--why-extract`;
- dropped chunks against `--print-gc-sections`;
- the layout against `-Map`;
- the written sections, byte for byte, against `wasm-ld -O0`.

Every linked fixture must also run on Braam. The last test, `link.mjs`,
covers the front end: the defaults, `-l`, and failed links.
