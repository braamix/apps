# zip — the Info-ZIP archiver

Make a zip archive, or change one that is already there. Four commands: `zip`
itself, and `zipnote`, `zipsplit` and `zipcloak` beside it.

**This is Info-ZIP's zip 3.0.** Mark Adler wrote the first of it in 1990 and
Jean-loup Gailly the deflate under it; Ed Gordon maintained the 3.0 release of
5 July 2008, which is what this directory ports. The Info-ZIP licence is in
[LICENSE](LICENSE), and `zip -L` prints it. The encryption code is not
copyrighted and is in the public domain; `zip -v` says so and where it came
from.

## Using it

```
zip [-options] archive.zip file...   # create, or add to what is there
zip -r archive.zip dir               # a whole tree
zip -u archive.zip file              # replace what changed
zip -d archive.zip name              # delete an entry
zip -h                               # the options, all eighty-odd of them
```

`-0` stores and `-9` deflates hardest; `-i` and `-x` take patterns and come
last, because they take a list. `-e` asks for a password and `-P` takes one on
the command line, where anybody running `ps` can read it. `-@` reads the names
from standard input, one a line.

There is a reason to want it here beyond the obvious: `fexport` hands the
browser one file at a time, so a directory that is to leave the tab in one
piece has to be an archive first.

## What the port changed

No `signal`, no `termios`, and a program is a coroutine rather than a `main`.
The C library is the port kit's — `braam_add_program(... PORT)` — Group B and
all, so `stdio` is `<stdio.h>` and `compat/cio.h`'s `b_*` family, with a
`co_await` in front of every name that blocks. What `braam.cpp` still answers
is only what the kit has not got. Every line zip prints is upstream's, and so
is the archive it writes — a real Info-ZIP `unzip` reads it, encryption and
all.

| Upstream | Here |
| --- | --- |
| `FILE *`, `fopen`, `fread`, `fseek` | the kit's, as `b_fopen`, `b_fread`, `b_fseeko` — C's arguments and C's failures, awaited |
| `malloc`, `realloc`, `free` | the port kit's, over `heap_alloc` |
| the `mem*` and `str*` families, `strtol` | the port kit's |
| `qsort` | the kit's, except at two call sites — see below |
| `printf`, `fprintf`, `sprintf` | `b_printf`, `b_fprintf` and the kit's `sprintf`; the 60-line `zvformat` that stood in for them is gone |
| `opendir`/`readdir` recursion | `list_dir`, one syscall for a whole directory |
| `stat`'s `st_mode`, `st_uid`, `st_gid` | nothing keeps them; the mode an entry carries is synthesized |
| `localtime`, `mktime`, `asctime` | the kit's `<time.h>`: `gmtime_r` and `timegm`, with `clock_now()` for the zone |
| `mkstemp` | `proc_random()` for the name and `SYS_O_EXCL` on the open |
| `rename` with `EXDEV` | `rename_path`, whose `Err(Unsupported)` means copy instead |
| `signal(SIGINT, handler)` | `sig_catch(SIG_INT)`, and `sig_take` at the parks |
| `/dev/tty` and termios `ECHO` off | `keys_claim` and `key_read`, which never echo |
| `srand`/`rand` for the crypt header | `proc_random()`, which is `crypto.getRandomValues` |
| `exit()` from anywhere | a status recorded and returned up the call chain |
| `-DUTIL`, a second build of the shared files | one build, and `--gc-sections` |

**Two sorts are `mergesort`, not `qsort`.** The kit's `qsort` is heapsort and
not stable, where the C library upstream was built against was stable in
practice. It matters twice: `zipsplit`'s bin packing sorts entries by size, and
equal sizes decide which output file each entry lands in; and the duplicate
check in `fileio.cpp` reports a repeated name as a *first* and a *second*, which
is the order they were found in. `mergesort` allocates, so both sites now have a
`ZE_MEM` path they did not have before. Everywhere else the keys are unique and
heapsort is what it should be.

Structure:

- **Everything that would block is a `co_await`**, so most of the port is
  `Task`-returning. Two places are deliberately not. `trees.cpp` is untouched
  and synchronous: its only sink is `flush_outbuf`, which now appends to a
  `String` that the coroutine layer drains at each block boundary, so the sink
  never holds more than one block. And in `deflate.cpp` only `read_buf`,
  `fill_window`, `lm_init`, `deflate` and `deflate_fast` became coroutines —
  `longest_match` stays a plain function marked `noinline`, because inlining it
  into one would move its locals into the heap-allocated frame.
- **`ziperr()` cannot end the process.** `Sys::Exit` records a status and a
  process ends when its root task returns, so nothing can die where it stands:
  the code goes into `zip_fatal` and every caller unwinds with it. Upstream's
  `ziperr()` was a call; here every one of them is a return.
- **A signal is delivered where the process parks**, which for a long run is
  the read of the file being compressed. But the read may still answer, so it
  is `sig_take(SIG_INT)` that says one arrived and not the error. `zipup()`
  ends the entry on it, which is what leaves the archive's name untouched.
- **The mode is invented.** The filesystem keeps a name, a kind, a size and an
  mtime, and nothing else — no permissions, no owner, no atime or ctime. An
  entry carries 0644, 0755 for a directory, `0777|S_IFLNK` for a link, and no
  `UT` or `Ux` extra field at all: a field full of made-up values is worse than
  no field, because an extractor would restore them.
- **The wide-character layer collapses.** The local character set is UTF-8, so
  `mbstowcs` and `wctomb` are `utf8_to_ucs4_string` and `utf8_from_ucs4_char`,
  and upstream's "no multi-byte for this one" branch has nothing to answer.
- **The three tools are one build.** Upstream compiled the shared files a
  second time with `-DUTIL` to strip the create path out of them. Here all four
  link the same objects and `--gc-sections` keeps out of each binary what that
  binary never reaches: `zip` is 420 KB and the tools are under 220 KB each.

## Differences from upstream worth knowing

- **`-T` is refused.** It ran `unzip -t` over what was written, through
  `popen`. Braam's `/bin/unzip` has no `-t`, so there is nothing to spawn.
- **bzip2 is not there.** `-Z bzip2` needs a library the Info-ZIP distribution
  does not ship; `NO_BZIP2_SUPPORT`, as on any system without it.
- **`-o` cannot set the archive's time.** Nothing here can set an mtime —
  `touch_path` moves one to now and there is no other setter — so it says so
  and leaves the archive alone.
- **`-X` is nearly a no-op**, because the extra fields it suppresses are the
  ones that are not written.
- **A directory's mtime is 0** in this filesystem, so a directory entry carries
  the DOS epoch, 1980-01-01. So does every entry under the test harness, whose
  clock is frozen — which is what makes an archive byte-reproducible there.
- **`zip -0` on a pipe** is upstream's own refusal, and stands: a stored entry
  whose size is not known cannot be extracted.
- Three identifiers had to move: upstream's `new` in `trees.c`, `template` in
  `zipsplit.c` and `public` in `zipcloak.c` are C++ keywords. `isize` in
  `zipup.c` collides with the platform's own signed-size type and is `zisize`.

## Building and packaging

From the top of this repository:

```
make            # build/archivers/zip/{zip,zipnote,zipsplit,zipcloak}.wasm
make package    # build/archivers/zip/zip-3.0-r1.zip
```

The package holds `.PKGINFO` and four `bin/` entries and nothing else. `bin/`
is what reaches `PATH` once `/bin/pkg` installs it, and each flat entry becomes
a command of its own.

## Testing

```
make test       # from the top of this repository
```

Each case boots `../braam-core`'s kernel under node and plants the built
binaries in the image. The check that matters most is not a golden: **Braam's
own `/bin/unzip`, written independently against Package_Formats.md §5.2, reads
what this `zip` writes** — and so does a real Info-ZIP `unzip`, which is worth
running by hand over anything the tests produce.

- `test/roundtrip.mjs` puts one file in, stored and deflated, and takes it out
  again byte for byte; two runs of the same input write the same archive.
- `test/tree.mjs` is the create path: `-r` over a tree, `-i` and `-x`, `-j`,
  `-0` against `-9`, and `-h`.
- `test/update.mjs` is the update path: added, updated with `-u`, freshened
  with `-f`, deleted with `-d` and moved with `-m`, each read back with `unzip`.
- `test/interrupt.mjs` sends `^C` part way through a long run, queued as two
  command lines before the first tick so the shell takes its turn where zip
  parks. The archive's name must be untouched and no temporary left behind.
- `test/tools.mjs` is `zipnote`, `zipsplit`, and `zipcloak` as far as its
  password prompt — it reads one from the terminal only, which is upstream's
  rule, and it goes last because it parks holding the keyboard.

## Files

| | |
| --- | --- |
| `zip.h` | the types, the state and the prototypes — upstream's `zip.h` with `tailor.h` written out |
| `zip.cpp` | the options, the help and the driver — upstream `zip.c` |
| `zipfile.cpp` | the headers, the scanners and `zipcopy` — upstream `zipfile.c` |
| `zipup.cpp` | one entry: read it, compress it, write it — upstream `zipup.c` |
| `fileio.cpp` | the names, the filter, the option parser and the splits — upstream `fileio.c` |
| `util.cpp` | pattern matching, the environment, number formatting — upstream `util.c` |
| `globals.cpp` | upstream's mutable state, and the message table |
| `deflate.cpp`, `trees.cpp` | the compressor, by Jean-loup Gailly |
| `crc32.cpp`, `crc32.h` | the CRC-32, by Mark Adler |
| `crypt.cpp`, `crypt.h` | traditional PKZIP encryption |
| `ttyio.cpp`, `ttyio.h` | the password prompt, on the raw keyboard |
| `unix.cpp` | what the OS answers about a file — upstream `unix/unix.c` |
| `braam.cpp`, `braam.h` | the porting layer: the two dates, the DOS stamp, the argv copy and the read a `^C` reaches |
| `revision.h`, `ziperr.h` | the version, and the error codes |
| `zipnote.cpp` | archive and entry comments — upstream `zipnote.c` |
| `zipsplit.cpp` | one archive into several — upstream `zipsplit.c` |
| `zipcloak.cpp` | encrypt or decrypt every entry — upstream `zipcloak.c` |
| `test/*.mjs` | what each of them has to do |
