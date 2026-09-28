# Plan: a wasm object library, and strip

Two goals, in this order:

1. Factor out of `ld` a library that reads and writes wasm modules, objects
   and archives: `devel/wasm/lib/`. `ld` is its first user and must not
   change its output by a byte.
2. Write `strip` on that library: `devel/wasm/strip/`. It is the smallest
   useful second user, so it shows what the library needs to be before
   `nm`, `size` and `as` arrive.

Each numbered step below ends with `make test` passing and is one commit.
Plain `make test` is the gate; nothing here touches Python.

## What exists now

`ld`'s core is plain C++ over bytes already in memory; only `braam.cpp`
awaits. The pieces a second tool would want are already separate files,
but they are mixed with linker policy. The reader's refusals are worded for
`ld`: bitcode, TAG, wasm64, no `linking`. The two names in `input.h` are
filled in by the symbol table, not the reader.

| File | What it holds | Linker-specific parts |
| --- | --- | --- |
| `wasm.h/.cpp` | section ids, opcodes, symbol and reloc numbers | none |
| `out.h` | `Out`, text built up in a `String` | none |
| `reader.h/.cpp` | `Cursor`, `read_object`, `read_archive` | its refusals |
| `input.h` | `Object` and its parts | `Function::name`, `Global::name` |
| `writer.cpp` | `Writer`'s encoders: `uleb`, `name`, ... | all the rest |
| `layout.cpp` | a second `uleb` encoder | all the rest |
| `diag.h` | lld-style errors | the `ld:` prefix is hard-coded |
| `dump.cpp` | `--dump`: `llvm-objdump -t -r` output | none; `nm` will want it |
| `demangle/` | LLVM's Itanium demangler | none; `nm` will want it |

Two facts decide the library's shape:

- **The reader only reads relocatable objects.** `Reader::sections()` frames
  any module, but `read()` then refuses a module with no `linking` section,
  with a TAG section or with 64-bit limits. `strip` has to accept linked
  programs too, with any sections, and so will `size`. So framing must be a
  separate layer under `read_object`.
- **Nothing writes a module except `Writer`**, whose encoders are private
  members, and `layout.cpp` has its own copy of `uleb`. `strip` and `as`
  need the encoders without a `Linker`.

## Part 1: the library

### Shape

    devel/wasm/lib/
        wasm.h wasm.cpp      numbers of the format (moved from ld as is)
        out.h                Out (moved as is)
        diag.h               Diag, with the tool's name as a field
        cursor.h cursor.cpp  Cursor: bounds-checked reads, sticky failure
        emit.h emit.cpp      Emit: encoders into a Vec<u8>, sticky oom
        module.h module.cpp  framing: header, sections, in any module
        object.h object.cpp  Object and read_object (from input.h, reader.cpp)
        archive.h archive.cpp  is_archive, read_archive; later write_archive
        stamp.h stamp.cpp    the braam section: find, parse, write
        CMakeLists.txt       add_library(wasmobj STATIC ...)

Rules for the library:

- **Pure.** No `co_await`, no `proc/io.h`, no file names opened. It takes
  `Bytes` and fills `Vec<u8>` and `Out`, as `ld`'s core does now.
- **No policy.** It reads anything well-formed, and says what it found.
  What a tool refuses — bitcode, TAG, wasm64, a module that is not an
  object — the tool decides and words. The one exception is framing that
  is malformed: that is an error in every tool.
- **Messages name the file, the section and the offset**, as the reader's
  do now; the tool adds its own `ld:` or `strip:` in front.
- **Views, not copies.** Every `Str` and `Bytes` it returns points into the
  caller's file, as now.
- It keeps `ld`'s naming, `.clang-format` and comment style. It is not a
  port, so there is no upstream to keep.

The CMake target follows the fixtures' `wasm_archive()` precedent:
`add_library(wasmobj STATIC ...)` with `target_link_libraries(... PRIVATE
braam::flags)`, then `braam_add_program(... LIBS wasmobj)` in each tool.
`devel/wasm/CMakeLists.txt` adds `lib` before `ld`.

### Step 8. Document the library

- A short section in [README.md](README.md)'s *Inside*: the files of
  `lib/` and the rules above: pure, no policy, views.
- The table of `ld`'s files loses `reader.cpp`.
- [Wasm_Object_Format.md](Wasm_Object_Format.md) is unchanged; say there
  that `lib/` is what reads it.

`dump.cpp` and `demangle/` stay in `ld` for now. `nm` is where they move
to `lib/`, together, since `nm` is the second tool to need them.

## Part 2: strip

### What it does

    strip [options] file...

| Option | Meaning |
| --- | --- |
| `-s`, `--strip-all` | drop every custom section but `braam` (the default) |
| `-g`, `-S`, `--strip-debug` | drop `.debug_*` sections only |
| `-R <name>`, `--remove-section=<name>` | drop that section too; repeatable |
| `--keep-section=<name>` | keep that custom section; repeatable |
| `-o <file>` | write here instead of in place; one input only |
| `--help` | usage |

The exit status is 0 on success, 1 on an error and 130 on `^C`, as `ld`'s.

It follows `llvm-strip` 23.1.2, the reference the tests hold it to.
These rules were checked against that version:

- **Standard sections are never removed.** `--strip-all` removes every
  custom section. `--strip-debug` removes custom sections named `.debug*`.
- **An object keeps its section numbers.** `linking`'s section symbols and
  every `reloc.*`'s target count sections by index. So in a module with a
  `linking` section, a removed section is replaced by an empty custom
  section named `.objcopy.removed`, not dropped. `--strip-debug` also
  removes the `reloc..debug_*` sections, so no reloc points at a
  placeholder. wasm-ld links such an object.
- **A linked program is compacted:** removed sections simply go.
- `-R` with no `-g` still strips everything, as `llvm-strip` does: its
  default mode stays on.

Where `strip` differs from `llvm-strip`, both times because the result
would not run:

- **It keeps `braam`.** `llvm-strip`'s `--strip-all` removes it, and `exec`
  then refuses the program. So `strip` behaves as `llvm-strip
  --keep-section=braam`, and the tests compare it with exactly that.
  `-R braam` is refused: "removing the braam section makes the program
  unrunnable".
- **`-R` of a standard section is refused.** `llvm-strip -R CODE` exits 0
  and writes a module the browser will not compile.

Not in the first version: wildcards in `-R`, `--only-keep-debug`,
`--strip-unneeded`, `-p`, and archives (step 14).

### Step 9. The core: `strip.h/.cpp`

A pure function, like `ld`'s core:

    struct StripConfig {
        bool debug_only = false;       // -g
        Vec<Str> remove;               // -R
        Vec<Str> keep;                 // --keep-section, plus "braam"
    };
    // `file` stripped into `out`. False on a malformed module, with the
    // message in `err`.
    bool strip_module(Str name, Bytes file, const StripConfig &c,
                      Vec<u8> &out, Out &err);

1. `read_module` frames the file. Bitcode and non-wasm input are refused
   here, with the file's name.
2. `is_object` decides whether removed sections become placeholders.
3. Walk the sections in order. Standard sections and those in `keep` are
   always kept. Any other custom section is removed if one of these holds:
   the mode is `--strip-all`; the mode is `-g` and the name starts with
   `.debug`; it is a `reloc.` section whose target is removed; its name is
   in `remove`. A kept section is copied byte for byte, id and size field
   included. The size field is not re-encoded, because an object's size
   fields may be padded LEBs and they must stay as they are.
4. A removed section in an object is written as `emit_custom(e,
   ".objcopy.removed", {})`.
5. If nothing was removed, `out` equals `file`. The front end then skips
   the write.

Finding a reloc section's target needs only the first uleb of its body.
`strip` never parses `linking`, symbols or code, so it is linear in the
file and does not depend on `read_object`'s policy.

### Step 10. The front end: `braam.cpp` and `driver`

Model it on `ld/braam.cpp`: one `Front` struct off the coroutine frame,
`slurp`, `say` and `finish` as there.

- **Options:** a small hand parser like `ld/driver.cpp`, since the
  spellings are llvm-strip's (`-R name`, `-Rname`, `--remove-section=name`).
- **In place, safely.** Write `<file>.strip` with `SYS_O_CREATE |
  SYS_O_EXCL`, then `rename_path` it over the original. Remove it on any
  failure. A `^C` between the two leaves the original whole. If the
  temporary name is taken, fail with that name rather than overwrite it.
- **Several files:** each is independent. An error on one is reported and
  the rest are still stripped; the status is 1 if any failed, as with
  `llvm-strip`.
- `-o` with more than one input is an error before anything is read.

If `ld`'s `slurp`, `say` and `spill` would then exist twice, move them to
`lib/files.h/.cpp` as the library's only awaiting part, kept apart so the
pure core never includes it. Decide at this step, when there are two
copies to compare, not before.

### Step 11. Build and package

- `devel/wasm/strip/CMakeLists.txt`: `braam_add_program(NAME strip
  SOURCES strip.cpp braam.cpp LIBS wasmobj)`, guarded to configure
  standalone as `ld`'s is.
- `devel/wasm/CMakeLists.txt`: `add_subdirectory(strip)`, and the package
  gains `$<TARGET_FILE:bin_strip>=bin/strip`. Version `0.2-r0`;
  `T=WebAssembly tools: ld, the linker, and strip`.
- Verify the binary's surface (the imports, the five exports, the
  `braam` section) with the one-liner in the top `CLAUDE.md`.

### Step 12. Tests: `devel/wasm/strip/test/strip.mjs`

Built on `ld/test/wasmlib.mjs`: `boot`, `plant`, `run`, `get`,
`manifest`. The manifest already names every fixture's objects, the SDK
archives, the reference programs and the LLVM tools' directory, where
`llvm-strip` is. No new CMake is needed for inputs.

Inputs:

- every fixture's reference program (wasm-ld's link, stamped);
- every fixture's objects, including the `debug` fixture's, which carry
  `.debug_*` and `reloc..debug_*`;
- `ld.wasm` itself, a large program with a `name` section.

For each input, and for each of `(none)`, `-g`, `-R producers`, `-g -R
producers` and `--keep-section=name`:

1. `llvm-strip --keep-section=braam <opts> in -o want` on the host.
2. `strip <opts> in -o got` on Braam.
3. `got` must equal `want` byte for byte.

Then check what the bytes mean:

- A stripped program still runs: `check_run` gives the same result as the
  unstripped one, for every fixture.
- **`ld` links `-g`-stripped objects.** Link the `debug` fixture's objects,
  stripped with `-g`, with both linkers through `linkers()`. The outputs
  must be equal, as in `write.mjs`. This is the first time `ld` sees
  `.objcopy.removed` placeholders and section symbols naming them. If it
  refuses them, fix `ld` in this step.
- In-place stripping: `strip f` leaves `f` equal to `strip f -o g`'s `g`,
  and leaves no `f.strip` behind.
- A second `strip` of a stripped file changes nothing.

Errors, each with its message and status 1:

- a file that is not wasm; truncated wasm; bitcode;
- a missing file; `-o` with two inputs;
- `-R braam`; `-R CODE`;
- one bad file among good ones: the good ones are still stripped.

Add the test to `TESTS` in the top [Makefile](../../Makefile). It should
run in seconds; if not, it belongs in `LONGTESTS`.

### Step 13. Document strip

In [README.md](README.md): the package now installs `ld` and `strip`, and a
*Using strip* section with the option table, the two differences from
`llvm-strip` and why. Mention that `ld --strip-all` at link time gives the
same result without a second pass. The existing *Inside* table becomes
one per tool, under the library's.

### Step 14. Archives (later)

`llvm-strip lib.a` strips every member and rewrites the archive with its
symbol table. That needs `write_archive` in `lib/archive.cpp`: the GNU
format `read_archive` reads, a `/` symbol table rebuilt from each member's
defined, non-local symbols (so `read_object` on each stripped member), and
`//` long names. The tests compare with `llvm-strip` on the SDK's
archives. `ar` and `ranlib` will need exactly this, so it may be better
done as part of `ar`.

## Order and size

| Steps | Change | Risk |
| --- | --- | --- |
| 1–2 | files move, no code changes | build wiring only |
| 3 | encoders shared | `ld`'s speed and size; measured |
| 4–5 | reader split into framing and object | `ld`'s messages; tested |
| 6–8 | stamp, diag, docs | small |
| 9–13 | `strip` | new code, held to `llvm-strip` byte for byte |
| 14 | archives | deferred |

`ld`'s own tests are the safety net for steps 1 to 8. Each step keeps
them passing with no change to a golden file. A golden file that has to
change means the step changed behaviour, and the step is wrong.

## Open questions

- **The library's name.** `wasmobj` for the CMake target and `lib/` for
  the directory are placeholders.
- **Should packages ship stripped?** `braam_add_package` could run `strip`,
  or `braam_add_program` could link with `--strip-all`, for release
  builds. That would lose function names in browser stack traces. It is
  the SDK's decision, not this package's.
- **Placeholder names.** `.objcopy.removed` is LLVM's spelling, kept for
  byte-for-byte comparison. Nothing in Braam reads it.
