# ar (FreeBSD)

FreeBSD's `usr.bin/ar`, commit `e9ac41698b2f` (2024-07-15), ported to
Braam. `ar.cpp`, `read.cpp`, `write.cpp`, `util.cpp` and `ar.h` are
upstream's `ar.c`, `read.c`, `write.c`, `util.c` and `ar.h`, with their
names, comments and messages. How to use it is in the package's
[README](../README.md#using-ar).

## What had to change

Upstream stands on libarchive, libelf and a C library that can block.
Braam has none of the three, so those are the parts that changed.

| Upstream | Braam |
| --- | --- |
| `open`, `fstat`, `mmap` of each file | the front end reads it first; `ar_lookup` |
| libarchive, reading an archive | `read_archive` in [lib/](../lib/) |
| libarchive, writing headers | `emit_ar_header` in `lib/`; `write_header` |
| libelf, in `create_symtab_entry` | `defined_symbols` in `lib/` |
| `main` | `ar_options`, then the front end, then `ar_run` |
| `bsdar_errc`'s `exit` | a sticky stop; each caller returns |
| `fprintf(stdout, ...)`, `stderr` | `bsdar_printf` into a list of output |
| `getopt_long` | a small one of the port's own, `getopt.cpp` |
| `basename` | `bsdar_basename`: what follows the last `/` |
| `localtime` | `gmtime_r` and the clock's offset |
| `strmode` | a copy in `read.cpp` |
| `-M`, `acpyacc.y`, `acplex.l` | gone |

**The shape is `ld`'s.** A file here is read with `co_await`, and a C
function cannot await. So [braam.cpp](braam.cpp) reads everything first:
the archive, the files to add for `-q` and `-r`, and for `-x` whether
each member's name is already taken. Then `ar_run` works on those bytes,
with no I/O, and leaves a list of files to write and text to print. The
front end writes and prints them last. Every file `ar` touches is named
on the command line or in the archive, so nothing is missed.

**An error cannot `exit` from any depth.** `bsdar_errc` records the
message and marks `ar` stopped. From then on nothing more is printed and
nothing is written, and each caller returns when it sees the mark. Most
of the 37 call sites were libarchive and libelf failures, and went with
them.

**The symbol table comes from wasm.** Upstream took each ELF member's
global and weak defined symbols. Here, `defined_symbols` takes what
`llvm-ar` takes: an object's defined non-local symbols, in `linking`
order, and a linked program's names or exports. A member that is not wasm
adds nothing, as before.

## Where it differs from upstream

- **The archive is replaced safely.** It is written to `<archive>.ar`
  with `SYS_O_EXCL`, then renamed over the old one. Upstream truncated
  the old archive first, so a failure lost it.
- **Only permission bits are written as a mode:** `644`, as `llvm-ar`
  writes it. libarchive wrote `100644`.
- **The symbol table's date is always 0.** Upstream wrote the current
  time unless the mode was deterministic, which `-d` and `-m` were not.
  Now every archive is the same bytes for the same members, as with
  `llvm-ar`.
- **`-r` keeps a replaced member in its place.** Upstream moved it to the
  end, though its own manual says "Replacing existing members will not
  change the order of members within the archive". `llvm-ar` keeps the
  place too.
- **A file has no mode, owner or group,** so every member gets what `-D`
  would give it, and `-U` keeps only the file's date.
- **A file's date cannot be set.** `-x -o` warns `Can't restore time`
  once, and extracts anyway with status 0.
- **A `..` in a member's name** is refused with libarchive's message,
  `Path contains '..'`, as `ARCHIVE_EXTRACT_SECURE_NODOTDOT` refused it.
- **A file that is not an archive** gives libarchive's `Unrecognized
  archive format`, without the errno text after it.
- **`-V`** prints `BSD ar 1.1.0`, with no libarchive version after it.
- **An error names the error as this system does:** `not found` rather
  than `No such file or directory`.

`ranlib` is still in the code, as upstream has it: `ar` run under a name
ending in `ranlib` acts as `ranlib`. The package does not install it
under that name, because `ld` does not read the symbol table.

## Tests

[test/ar.mjs](test/ar.mjs), in `make test`. It holds `ar` to
`llvm-ar --format=gnu` byte for byte, and rebuilds every SDK library from
its members.
