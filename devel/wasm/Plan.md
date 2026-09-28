# Plan: ar

Port FreeBSD's `ar` onto the library in `lib/`: `devel/wasm/ar/`.

`ld` ignores the archive symbol table, as `wasm-ld` does: trusting it
bought no measurable speed, and a stale one would be a failure `wasm-ld`
does not have. So no `ranlib` is shipped; `ar -s` still writes the table,
for other linkers.

`lib/` and `strip` are done; [README.md](README.md)'s *Inside* gives the
library's rules, which hold here too: pure, no policy, views.

Each numbered step below ends with `make test` passing and is one commit.
Plain `make test` is the gate; nothing here touches Python.

## What exists

FreeBSD's `usr.bin/ar` (commit `e9ac41698b2f`, 2024-07-15, in
`~/Project/BSD/FreeBSD-github`) is 2,466 lines:

| File | Lines | What it does |
| --- | --- | --- |
| `ar.c` | 407 | options and modes; `ranlib` when its name ends in `ranlib` |
| `read.c` | 204 | `-t`, `-x`, `-p` |
| `write.c` | 914 | `-d`, `-m`, `-q`, `-r`, `-s`, and the symbol table |
| `util.c` | 86 | `bsdar_warnc`, `bsdar_errc` |
| `ar.h` | 119 | `struct bsdar`, `struct ar_obj` |
| `acpyacc.y`, `acplex.l` | 736 | `-M`, MRI scripts: **not ported** |

It stands on two libraries that Braam has not got:

- **libarchive** reads the archive in `read.c` and `write.c`'s
  `read_objs`, and writes it in `write_objs`. For writing it does little:
  `write_objs` builds the `/` symbol table and the `//` name table itself,
  and libarchive only formats each 60-byte header and pads each member.
  Extracting uses `archive_read_extract` with `ARCHIVE_EXTRACT_TIME`.
- **libelf** is used in `create_symtab_entry` alone: about 110 lines
  that collect an ELF member's defined `STB_GLOBAL` and `STB_WEAK`
  symbols.

Facts checked on the host, which the steps rely on:

- **`llvm-ar`'s symbol table for wasm** is, member by member in archive
  order, each member's defined non-local symbols in the order of its
  `linking` symbol table. That holds for all 12 SDK archives, 1,492
  symbols, against `llvm-nm -p`.
- **`wasm-ld` 23 ignores the symbol table.** An archive whose `/` names
  only `one.o`'s symbols, with `two.o` appended after it, still links
  `two` out of `two.o`. `wasm-ld` parses every member as a lazy object,
  and so does `ld` today. An archive with no symbol table links too.
- **FreeBSD `ar` is deterministic by default** for `-q`, `-r` and `-s`,
  as `llvm-ar` is: mtime, uid and gid 0, mode 644.
- **`llvm-ar` writes the Darwin format on a Mac** unless told
  `--format=gnu`. The tests always pass it.

## What it does

    ar -d [-Tjsvz] archive file ...
    ar -m [-Tjsvz] [-a position-after | -b position-before] archive file ...
    ar -p [-Tv] archive [file ...]
    ar -q [-TcDjsUvz] archive file ...
    ar -r [-TcDjsUuvz] [-a position-after | -b position-before] archive file ...
    ar -s [-jz] archive
    ar -t [-Tv] archive [file ...]
    ar -x [-CTouv] archive [file ...]

As upstream's `ar(1)`, less `-M`. The dash before the first option may be
left out, as upstream allows. `-j`, `-z`, `-l` and `-T` are accepted and
ignored, as upstream does.

What Braam cannot do, and what the port does instead:

- **No file mode, owner or group.** `stat_fd` gives a kind, a size and an
  mtime. A member added with `-U` has the file's mtime, uid and gid 0 and
  mode 644, as `-D` would give it.
- **No setting a file's time.** `touch_path` only moves an mtime to now,
  so `-x -o` cannot restore a member's date. It warns once, as upstream
  warns when setting a time fails, and extracts anyway.
- **No modes on extraction.** A file is created with the default mode.

## Step 3. Writing archives: headers and symbols

What `write_objs` asks of libarchive, and what `create_symtab_entry` asks
of libelf, goes into `lib/archive`:

    // A member's 60-byte header, GNU style: `name` is already the field,
    // "name/" or "/<offset into //>".
    void emit_ar_header(Emit &e, Str name, u64 mtime, u32 uid, u32 gid,
                        u32 mode, u64 size);

    // A member's defined non-local symbols, in the order of its linking
    // symbol table. A member that is not a wasm object has none: false,
    // and no error, as create_symtab_entry skips what is not ELF.
    bool defined_symbols(Bytes file, Vec<Str> &names);

`defined_symbols` walks the `linking` section's symbol table after
`read_module`, and nothing else. It does not go through `read_object`, so
it takes none of `ld`'s refusals.

- Check: `make test`. The bytes are checked in step 6.

## Step 4. The port: `devel/wasm/ar/`

`ar.cpp`, `read.cpp`, `write.cpp`, `util.cpp` and `ar.h`, upstream's
identifiers, structure, comments and messages kept, clang-formatted.
`braam_add_program(... PORT LIBS wasmobj)`.

- **The shape is `ld`'s.** The core is plain C++ over bytes in memory, and
  only `braam.cpp` awaits. Every file `ar` touches is named on the
  command line, so the front end reads the archive and the named files
  before `ar_write_archive` or `ar_read_archive` runs. Upstream's
  `create_obj_from_file` looks its file up rather than opening and
  mapping it. `-x` and `-p` leave a list of writes the front end then
  performs. `-u` compares the mtimes the front end collected.
- **libarchive goes.** `read_objs`, `read.c`'s loop and the pathname tests
  work over `read_archive`'s members. `write_objs` keeps its own symbol
  and name tables, and writes each header with `emit_ar_header` into one
  `Vec<u8>`.
- **libelf goes.** `create_symtab_entry` calls `defined_symbols`, and
  `add_to_ar_sym_table` is unchanged.
- **The archive is replaced safely,** as `strip` replaces a file: written
  to `<archive>.ar` with `SYS_O_EXCL`, then renamed over it. Upstream's
  `archive_write_open_filename` truncates the original first. This is a
  difference, and the README says so.
- **`bsdar_errc` exits from any depth**, at 37 sites. Most are
  libarchive, libelf and `malloc` failures, and they go with them. The
  rest record the message and return a frame at a time, as `vi`'s
  `error()` does. `bsdar_warnc` is unchanged.
- **`getopt_long` is not in the port kit.** Use a small one of the
  port's own, as `archivers/xz/getopt.cpp` does.
- **`-M` goes:** `acpyacc.y`, `acplex.l`, the `M` mode and its usage line.
- **The program's name** comes from `args[0]`. Upstream's `ranlib` mode
  stays in the code, but nothing installs the binary under that name.
- **Messages** keep upstream's text. `strerror` becomes
  `error_name(error_of(errno))`, as `vi` and `le` do.

## Step 5. Build and package

- `devel/wasm/ar/CMakeLists.txt`, guarded to configure standalone as
  `ld`'s and `strip`'s are.
- The package gains `$<TARGET_FILE:bin_ar>=bin/ar`. Version `0.3-r0`;
  `T=WebAssembly tools: ld, strip and ar`.
- Verify the binary's surface with the one-liner in the top `CLAUDE.md`.

## Step 6. Tests: `devel/wasm/ar/test/ar.mjs`

On `ld/test/wasmlib.mjs`, as `strip.mjs` is. `llvm-ar` is the
manifest's `ar`.

- **Byte for byte against `llvm-ar --format=gnu`:** `rc`, `qc`, `r` of a
  member already there, `d`, `m` with `-a` and `-b`, `S` (no symbol
  table), and member names past 15 characters, so `//` is written. Inputs
  are the fixtures' objects.
- **`-s` against `llvm-ar s`,** on an archive written without a symbol
  table.
- **Every SDK archive rebuilt:** `ar x` it, `ar rc` the members back in
  their order, and the result must be the SDK's own archive byte for byte.
  This is the strongest check of the symbol table: 12 archives, 1,492
  symbols.
- **What the bytes mean:** `ld` and `wasm-ld` link archives `ar` made, and
  the outputs are equal, as in `write.mjs`.
- **Text:** `-t`, `-t -v` and `-p` against golden files, in upstream's
  format, which `llvm-ar` does not share.
- **Round trip:** `-x` then `-q` gives the archive back.
- **Errors,** each with upstream's message and status 1: a missing
  archive; a file that is not an archive; `-a` without a position; `-a`
  with `-b`.

Add it to `TESTS` in the top [Makefile](../../Makefile).

## Step 7. Document ar

- In [README.md](README.md): the package installs three commands. A
  *Using ar* section: the synopsis, what Braam cannot do (modes, owners,
  times on extraction), the safe replacement, no `-M` and no `ranlib`.
  In `ld`'s *How it differs from wasm-ld*: nothing; it ignores the symbol
  table as `wasm-ld` does.
- `devel/wasm/ar/README.md`: what had to change and why, as every port
  here has.
- The top [README.md](../../README.md): the `devel/wasm` row names `ar`.

## Order and size

| Steps | Change | Risk |
| --- | --- | --- |
| 3 | writing archives, in `lib/` | none to `ld`: new code |
| 4–7 | `ar` | a port, held to `llvm-ar` and the SDK's archives |

`ld`'s own tests are the safety net for step 3. Each step keeps
them passing with no change to a golden file. A golden file that has to
change means the step changed behaviour, and the step is wrong.

## Open questions

- **The library's name.** `wasmobj` for the CMake target and `lib/` for
  the directory are placeholders.
- **Should packages ship stripped?** `braam_add_package` could run `strip`,
  or `braam_add_program` could link with `--strip-all`, for release
  builds. That would lose function names in browser stack traces. It is
  the SDK's decision, not this package's.
- **`strip` on archives.** `llvm-strip lib.a` strips every member and
  rewrites the symbol table. With step 3 in the library, `strip` can do
  the same. That is not in these steps.
