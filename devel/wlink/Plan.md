# wlink — a wasm32 linker for Braam

A plan, not a design document: what to build, in what order, and how each
step is proved before the next one starts.

## 1. What it is

`wlink` is a static linker for wasm32 relocatable object files and `ar`
archives of them. It produces the one kind of binary Braam runs: a process
image that imports `env.memory`, `kernel.sys` and `kernel.sys_async`, exports
`_alloc`, `_free`, `_sig`, `_start` and `_resume`, and carries a `braam`
custom section. It is a Braam program in its own right (`devel/wlink`,
`bin/wlink`), so a system with a compiler on it could link on it.

Like `lang/python`, it is not a port. There is no upstream source to keep.
Its reference is:

- the object format in WebAssembly tool-conventions `Linking.md`, which is
  what clang emits (`linking` section version 2);
- what `wasm-ld` observably does with the same inputs and flags. Every claim
  is a test against it.

**About "GNU compiler for wasm".** Mainline GCC has no wasm32 back end, so no
GCC-built wasm objects exist to test against. `wlink` accepts any producer
that emits the tool-conventions format and names that format as its contract.
It rejects anything else with a message that says why: an object without a
`linking` section, a `linking` version other than 2, LLVM bitcode (`BC\xC0\xDE`,
which is what `-flto` produces), or wasm64 relocations. If a GCC wasm port
appears later, it either emits this format and links, or it gets its own
front end behind the same resolver.

## 2. What the tree links today

Surveyed from `build/` against SDK 0.10.280. This is the target the first
milestones aim at, and it is small.

**The link line** that `braam_add_program` and `braam::flags` produce:

```
wasm-ld --no-entry --gc-sections --stack-first -z stack-size=131072
        --import-memory --initial-memory=<16·64K or 4·64K>
        <objs> <libbraam_*.a ...>
```

After the link, `stamp.py` appends the `braam` section: magic, `PROC_ABI`
(21), flags, initial pages and max pages (1600).

**The inputs:**

- `.obj` files, one per source;
- `libbraam_*.a` archives in GNU `ar` format, with a `/` symbol index and
  `//` long names;
- 1.2 MB of archives in the SDK, and a few MB of objects for `lang/python`.

**Relocations actually seen**, counted over every object and archive in the
tree:

| Type                          | Count |
|-------------------------------|------:|
| `R_WASM_FUNCTION_INDEX_LEB`   | 76795 |
| `R_WASM_MEMORY_ADDR_SLEB`     | 36177 |
| `R_WASM_MEMORY_ADDR_LEB`      | 29264 |
| `R_WASM_GLOBAL_INDEX_LEB`     | 13738 |
| `R_WASM_TABLE_NUMBER_LEB`     | 11486 |
| `R_WASM_TYPE_INDEX_LEB`       | 11486 |
| `R_WASM_MEMORY_ADDR_I32`      |  8653 |
| `R_WASM_TABLE_INDEX_SLEB`     |  4810 |
| `R_WASM_TABLE_INDEX_I32`      |  4066 |

Nine types cover everything. There is no PIC, no TLS and no debug info.

**Object sections:**

- the standard ones, including `DATACOUNT`, which comes from bulk-memory;
- `linking`, `reloc.CODE`, `reloc.DATA`, `producers` and `target_features`;
- function sections, because clang's wasm default is `-ffunction-sections`
  and `-fdata-sections`. Each function and each variable is a separately
  collectable chunk.

**Special symbols the objects reference:**

- `__stack_pointer` (a global), `__indirect_function_table`,
  `__wasm_call_ctors` and `__heap_base`. `_start` calls `__wasm_call_ctors`
  itself, because the link is `--no-entry`.
- Exports come from symbols flagged `WASM_SYM_EXPORTED`
  (`__attribute__((export_name))`), not from command-line `--export`s. They
  are `.hidden` and exported anyway.

**What wasm-ld writes for `c4.wasm`:**

- **Sections:** TYPE, IMPORT, FUNCTION, TABLE (one funcref, min = max), GLOBAL
  (only `__stack_pointer`, initialised to 131072), EXPORT, START, ELEM, CODE,
  DATA, `name`, `producers` and `target_features`. The stamp adds `braam`.
- **START and `__wasm_init_memory`:** START names `__wasm_init_memory`.
  Because the memory is imported, wasm-ld cannot assume it is zeroed, so it
  makes the segments passive and fills `.bss` at start. On Braam every
  process gets a fresh `WebAssembly.Memory` (`web/proc.js`), so this isn't
  needed. `wlink` emits active segments and no `.bss` bytes. That's smaller
  and simpler, and it rests on one assumption, which step 5 tests.

## 3. Shape of the program

The same split as `simbesm` and `c4`: the linker is plain C++ over bytes
already in memory, and it never blocks. Only the front end awaits.

```
devel/wlink/
  Plan.md          this file
  README.md        what it does, what it refuses, how it differs from wasm-ld
  CMakeLists.txt   braam_add_program(NAME wlink ...), braam_add_package
  wasm.h/.cpp      opcodes, section ids, reloc and symbol enums
  out.h            text built in a String: messages and --dump
  reader.cpp/.h    bounds-checked cursor; object parser; archive parser
  input.h          Object, Function, Segment, Symbol, Reloc
  dump.cpp/.h      --dump, in llvm-objdump -t -r's layout
  diag.h           errors, worded and limited as lld's are
  symtab.cpp/.h    resolution: strong/weak/lazy/undefined, comdats, imports
  gc.cpp/.h        liveness from roots through relocations (worklist)
  layout.cpp/.h    index spaces, types, table, memory map, synthetic symbols
  writer.cpp       output sections, relocation patching, name, stamp
  driver.cpp/.h    options → Config; link(Config, inputs) → bytes | error
  braam.cpp        proc_main: args, @file, -L/-l search, read, write
  host.cpp         native main() over the same driver, for the tree relink
  test/            *.mjs under the SDK harness, fixtures, goldens
```

Constraints the Braam side imposes, and how the plan meets them:

- **No blocking in the core.** `braam.cpp` resolves every path, reads each
  input whole (`stat_fd`, then one read) and hands `driver.cpp` spans.
  Archives are read whole too, because a member's bytes are sliced out and
  not copied.
- **The native stack is small.** GC marking, archive pulling and comdat
  handling run on explicit worklists and never recurse.
- **The memory cap is 100 MB.** Inputs are held once. Chunks are views into
  them. Output size is computed before the output buffer is allocated, so
  there's one allocation with no doubling. Relocations are applied while
  copying into the output, never to a second copy of the input.
- **No exceptions.** A parser function returns false and leaves one message.
  Errors carry the file, archive member, section and offset:
  `wlink: libbraam_proc.a(io.cpp.obj): reloc.CODE +0x1a4: symbol 812 out of
  range`.
- **Coroutine frames stay under 512 bytes.** State lives in one heap
  `Linker` object that the frames point at.
- **Determinism.** No hash-order iteration reaches the output. Index
  assignment follows input order, so the same inputs give identical bytes.
- **Errors leave no output.** The output file is opened only after
  `link()` has succeeded.

The native `host.cpp` build is there so that the whole tree can be relinked
with `wlink` at host speed (step 8). It uses only `kernel/` headers, which
`../braam-core/test/unit` already compiles natively. If that turns out not
to hold, step 8 falls back to driving `wlink.wasm` under the harness.

## 4. Command line

`wlink` takes wasm-ld's spelling for the subset it implements, so
`clang --ld-path=wlink` and the tree's CMake can drive it unchanged:

- `-o <file>`, `-L <dir>`, `-l <name>`, and `@<rspfile>`. The response file
  matters because a harness-typed command line has to fit in 60 characters.
- `--no-entry`, `--entry=<sym>`, `--export=<sym>`, `--allow-undefined`.
- `--gc-sections` and `--no-gc-sections`, plus `--print-gc-sections`.
- `--stack-first` (the default) and `--no-stack-first`, `--global-base=<n>`,
  `-z stack-size=<n>`, `--initial-memory=<n>`, `--max-memory=<n>`,
  `--import-memory`.
- `--verbose`, which prints wasm-ld's `mem:` lines, and `-O<n>`, which is
  accepted and changes nothing until strings are merged.
- `--strip-debug` and `--strip-all`.
- `--trace`, `--why-extract=<file>` and `--error-limit=<n>`, printing what
  wasm-ld prints.
- `-m wasm32`, `--no-default-config` noise and similar, accepted and ignored
  where clang passes them.
- **`wlink` additions:**
  - `--braam` is the whole `braam_add_program` set in one flag, with the
    stamp included.
  - `--braam-pages=<init>,<max>` and `--braam-abi=<n>` set the stamp. The
    ABI defaults to the `PROC_ABI` of the SDK that `wlink` was built
    against.
  - `--dump <obj|archive>` prints symbols and relocations in
    `llvm-objdump -t -r`'s layout, `--dump-symtab` the resolved symbol
    table, and `--dump-layout` the index spaces and the memory map. They're
    debugging aids, and test oracles.
  - `-Map=<file>` writes a map of addresses and indices. It's optional and
    comes late.

**Braam is the default.** A plain `wlink a.o b.o -o p` produces a runnable
stamped binary. The generic wasm-ld flags exist for the tree's CMake and for
testing against wasm-ld.

## 5. Steps

Each step ends with a test that runs under `make test`, and none starts
until the previous one's test passes.

### Step 5 — Writing and relocation

- **Section order:** TYPE, IMPORT, FUNCTION, TABLE, GLOBAL, EXPORT, ELEM,
  DATACOUNT, CODE, DATA, `name`, `producers`, `target_features`, `braam`.
  There is no MEMORY section, because the memory is imported (`env.memory`,
  min = initial pages, no max). There is no START.
- **Relocation:** each function body is copied verbatim with its
  relocations patched in place:
  - LEB relocations are rewritten as padded 5-byte LEBs, so no body changes
    length and no offset moves. No instruction decoding is needed anywhere
    in the linker.
  - `I32` relocations are 4-byte little-endian values.
  - The SLEB forms are for `i32.const` addresses.
- **Data:** one active segment per non-empty merged output segment, with
  relocations patched in place. `.bss` is omitted, because a Braam process
  starts on fresh zeroed memory.
- **`__wasm_call_ctors`:** a synthesised body that calls each live init
  function, ordered by priority and then by input order.
- **Other sections:**
  - `name` holds function, global and data segment names.
  - `producers` is the union of the inputs' entries.
  - `target_features` is the union of `+` features. It's an error if any
    input disallows a feature another uses, or requires one that another
    lacks.
- **`braam` section:** five `u32`s — magic `0x6D617262`, ABI, flags,
  initial pages, max pages.
- **Test:**
  - Every fixture links under Braam and runs, and its output equals the
    wasm-ld build's output.
  - Node's `WebAssembly.validate` accepts each output.
  - The module surface matches exactly: the same imports, the same five
    exports, and the `braam` section.
  - The `.bss` fixture proves that omitting zero bytes is safe on Braam.
    It fills and checks a large array after a sibling process has run in
    the same worker.

### Step 6 — The Braam front end and the package

- `braam.cpp`:
  - `OptParse` for the flags, plus `@file` expansion;
  - `-l` searches each `-L` for `lib<name>.a`;
  - reads inputs, calls `link()` and writes the output in one pass;
  - exits 0 on success, 1 on an error, and 130 on `^C`.
- `braam_add_package(NAME wlink ...)`, and a line in
  `devel/CMakeLists.txt`.
- Add `README.md`, covering:
  - what it links;
  - what it refuses and why;
  - where it differs from wasm-ld: active segments, no `.bss`, no START,
    and an error rather than a stub on signature mismatch.
- **Test:** `test/link.mjs` in `TESTS`:
  1. the step-5 cases;
  2. a link driven by a response file;
  3. a link with a missing library, which checks the message;
  4. a check that a failed link leaves no output file.

### Step 7 — Real programs on Braam

- Plant the SDK's `libbraam_*.a` and the objects of `devel/c4`,
  `games/asciifluid` and `benchmarks/dhrystone` into the harness store.
  Link them with `wlink` on Braam and run each program's existing test
  against the relinked binary.
- **Self-hosting:** link `wlink`'s own objects with `wlink` on Braam, then
  use that output to link c4 again. Both c4 outputs must be byte-identical.
- **Test:** `test/relink.mjs` in `LONGTESTS`, if it takes more than a few
  seconds.

### Step 8 — The whole tree

- Build `host.cpp` natively. Add `make LINKER=wlink`, which configures the
  tree with `wlink` as the link step through `CMAKE_CXX_LINK_EXECUTABLE`,
  and drops `stamp.py` because `--braam` stamps.
- `make test longtest LINKER=wlink` must pass unchanged. `lang/python` is
  the real exam here: 3.8 MB of output, 600 KB of initialised data and
  thousands of address-taken functions.
- Compare with wasm-ld: section sizes per program. Expect a smaller DATA
  section, because `.bss` is omitted, and a slightly larger CODE section,
  because wasm-ld compacts its padded LEBs and `wlink` doesn't yet.

### Step 9 — Breadth, after the tree links

Items are ordered by how likely a real input is to need them:

1. **LEB compaction:** re-encode padded LEBs at their minimal length while
   copying. This needs `CODE` offsets adjusted per body, but still no
   instruction decoding. wasm-ld's `--compress-relocations` is the
   reference.
2. **String merging** for `WASM_SEG_FLAG_STRINGS` segments, which is on by
   default in wasm-ld.
3. **Debug info:** `R_WASM_FUNCTION_OFFSET_I32`, `R_WASM_SECTION_OFFSET_I32`
   and `.debug_*` custom sections. It's dropped by default until then,
   which is what `--strip-debug` does anyway.
4. **Signature-mismatch stubs**, and `-Map`.
5. **Explicitly out of scope:**
   - `-shared`, `-pie` and `dylink.0`;
   - TLS and shared memory;
   - wasm64 and multi-memory;
   - exception tags, GC types and LTO.

   Each is refused with a message rather than mislinked.

## 6. Risks worth watching

- **Padded LEBs and body sizes.** A function body's size prefix covers the
  padded relocation fields. Copying bodies verbatim keeps that true.
  Compaction in step 9 has to rewrite the size prefix too.
- **Relocation addends.** `MEMORY_ADDR_*` relocations carry an addend
  (`&arr[3]`). `TABLE_INDEX` relocations don't. Getting this wrong makes
  pointers that point near the right place, so the fixtures include an
  interior pointer into `.rodata` and one into `.data`.
- **Section symbols.** A relocation can name a section symbol rather than a
  data symbol. Those resolve to the chunk's output offset.
- **Init function liveness.** A constructor that isn't a root disappears
  silently. The constructor fixture has one in an archive member pulled in
  only by another symbol, which is the case lld handles specially.
- **Initial memory versus data size.** Python's 16 pages exist for exactly
  this reason. The error has to be clear, because the default of 4 pages
  will be too small for some ports.
- **Harness limits.** Command lines under 60 characters mean response
  files. The frozen clock is irrelevant here. Every run is deterministic,
  so goldens are exact.
