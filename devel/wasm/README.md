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
- `--trace`, `--why-extract`, `--error-limit`, `--verbose` and `-Map=<file>`;
- `-O<n>`: at 1, the default, strings are merged as `wasm-ld` merges them,
  `.debug_str` among them, and at 0 they are not;
- `--compress-relocations`, which writes every relocated LEB at its
  shortest and, as in `wasm-ld`, wants `--strip-debug` or `--strip-all`;
- `--strip-debug` (`-S`), `--strip-all` (`-s`), `--demangle` and
  `--no-demangle`.

Braam's `OptParse` has no long options, so `ld` parses these itself. What
clang passes and `ld` has no use for (`-m wasm32`) is accepted and changes
nothing.

More options are `ld`'s own:

- `--braam-abi=<n>` and `--braam-pages=<initial>,<max>` set the stamp;
- `--no-import-memory` defines the memory in the module, as `wasm-ld` does
  by default;
- `--dump` prints an object's or archive's symbols and relocations in
  `llvm-objdump -t -r`'s layout;
- `--dump-symtab` and `--dump-layout` print the resolved symbols, the index
  spaces and the memory map;
- `--dump-demangle` prints each line of its inputs as a name is shown.

The exit status is 0 on success, 1 on an error and 130 on `^C`. A link that
fails writes no output.

### What it links

`ld` links what clang emits for Braam: `linking` version 2, the nine
relocation types the tree uses, function and data sections, comdats,
constructors, weak symbols and GNU `ar` archives. Symbols resolve, archive
members load and unused chunks are dropped in `wasm-ld`'s order. So the
index spaces, the memory map and the diagnostics are `wasm-ld`'s, and so
is every section except the ones listed below.

`ld` also copies custom sections, debug info among them. It applies their
relocations, and gives code or data that was dropped `wasm-ld`'s
tombstones. A call whose signature differs from the definition gets
`wasm-ld`'s warning and a stub that traps. With several such names, the
warnings and stubs come in the order of `wasm-ld`'s hash table, whose hash
[ld/xxh3.cpp](ld/xxh3.cpp) reproduces.

C++ names are demangled wherever `wasm-ld` demangles them: in the `name`
section, the map, `--why-extract` and the messages. The demangler in
[ld/demangle/](ld/demangle/) is LLVM 23.1.2's own `ItaniumDemangle.h`,
under LLVM's Apache-2.0 licence with its exception. Only its C++ library
changed: [ld/demangle/std.h](ld/demangle/std.h) stands in for it, over the
kernel's heap. Rust and D names are not demangled; a float in a template
argument prints only as a `double`.

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

- **Active data segments.** With `--import-memory`, `wasm-ld` cannot assume
  the memory is zeroed. So it makes the segments passive and clears `.bss`
  in a `__wasm_init_memory` it names in START. Every Braam process gets a
  fresh memory, so `ld` writes neither START nor `__wasm_init_memory`.
  Neither linker writes `.bss` bytes.
- **The memory is imported by default**, as `braam_add_program` asks. With
  `--no-import-memory` too, the whole module is `wasm-ld`'s byte for byte,
  but for the stamp.

## Linking the tree with ld

    make LINKER=ld

builds every program with `ld` instead of `wasm-ld`, and `make test
longtest LINKER=ld` then tests those binaries. A plain `make` goes back to
`wasm-ld`. The kernel's headers are wasm32's alone, so `ld` has no native
build:

- `ld.wasm` is linked by `wasm-ld` in `build/bootstrap`;
- clang runs `build/bootstrap/ld` through `-fuse-ld`, a script that runs
  [ld/host.mjs](ld/host.mjs);
- `host.mjs` boots the SDK's harness, plants `ld.wasm` and the inputs in
  its `/tmp`, links there and copies the output back.

A link costs a kernel boot, so the whole tree relinks in about five
seconds. `ld` stamps as it links, so `stamp.py`'s step becomes
[ld/stamped.py](ld/stamped.py), which checks the stamp is the one
`stamp.py` would have written. A new `ld` relinks everything.

`node devel/wasm/ld/sizes.mjs` links every program in `build/` with both
linkers and prints the sizes. `wasm-ld`'s output is larger by 12 to 14
bytes a program. It has a 16-byte `__wasm_init_memory`, a START and a
DATACOUNT section, and it lacks the 26-byte stamp. The names in the `name`
section are the same in both.

### Tests

`make test` runs `ld/test/*.mjs`. Each test compiles its fixtures once
with clang and links them twice: by `ld` under the SDK's harness, and by
`wasm-ld` on the host. Then it compares, in this order:

- `--dump` against `llvm-objdump`;
- symbol resolution against `--trace` and `--why-extract`;
- dropped chunks against `--print-gc-sections`;
- the layout against `-Map`;
- the written sections, byte for byte, against `wasm-ld` at `-O0`, at `-O1`
  and with `--compress-relocations`;
- with the memory defined, the whole module and `-Map`, plain, packed and
  with `--strip-all`;
- every symbol name, demangled, against `llvm-cxxfilt`.

Every linked fixture must also run on Braam. `link.mjs` covers the front
end: the defaults, `-l`, and failed links.

`relink.mjs` links real programs on Braam: `c4`, `asciifluid` and
`dhrystone`, from the objects and SDK archives their build linked and with
the flags their `link.txt` passes. The stamp must be `stamp.py`'s, and each
program's own tests must pass against the relinked binary. Then `ld` links
itself. The `ld` that results links `c4` to the same bytes, and links itself
to the same bytes again.
