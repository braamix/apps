# ex — the UCB line editor, and vi beside it

Edit a file by naming lines and telling the editor what to do to them:
`:1,$s/old/new/g`, `:g/TODO/p`, `:5,10m$`. Two commands out of one set of
sources, `ex` and `vi`, as upstream shipped them.

**This is ex/vi version 3.6, dated 11/3/80.** Bill Joy wrote it at Berkeley in
1976 out of ed, and Mark Horton maintained it from 1979; this directory ports
the version that shipped with 4BSD, taken from the V7/x86 contrib tree. It is
the ancestor of every vi in use since. The Berkeley copyright is on every file
and in [LICENSE](LICENSE); `:version` prints the version string.

**Only command mode is built.** The visual half — `vi` proper, the full-screen
editor — is not ported yet, and both binaries currently start in command mode;
`:visual` says so and stops. What is here is `ex`, complete: everything you can
do at a `:` prompt, which is the whole of the editor's vocabulary and rather
more of it than the screen ever showed.

## Using it

```
ex file          # the editor, one command per line, : is the prompt
ex - file        # no prompt and no autoprint, for a script
ex -R file       # read only
ex -t tag file   # start at a tag
ex +/pat file    # start at the first line matching pat
```

`ex` is the shape of editor that suits a browser tab better than it looks: it
needs no terminal, so it works down a pipe, and a session is a script you can
keep. `ed`-style batch editing —

```
$ printf '%s\n' 'g/^#/d' '1,$s/  */ /g' 'w' 'q' | ex notes
```

— is something Braam had no way to do at all before this.

## What the port changed

No libc: no `malloc`, no `printf`, no `errno`, no `<cstring>`, no exceptions,
no `setjmp`. `braam.cpp` supplies the string and character routines ex calls,
over `heap_alloc`; everything else was replaced rather than reimplemented.

| Upstream | Here |
| --- | --- |
| a temp file in `/tmp`, three `BUFSIZ` buffers and a two-buffer LRU over them | one growable arena in memory; a `line` handle is already a byte offset, so `getline()` and `putline()` are untouched |
| `sbrk` for the line pointer array | one `heap_alloc` at startup, taken once and never moved |
| `setjmp`/`longjmp` to the command loop and to a visual catch | a sticky flag and four return macros, and a dozen poisoned leaf routines so that a missed check is inert (`ex_err.h`) |
| termcap, `tputs` padding, and a cursor-motion cost model | nothing: the screen is an array of cells, so a capability is a constant and cursor addressing is indexing |
| `read(0, …)` from the bottom of the address parser | one read at the top of the command loop, because a syscall must be awaited and that is a plain function |
| `write(1, …)` from wherever output was formatted | a growable buffer, drained where the editor stops to read |
| `fork` and `execl` for `:!`, `:sh` and `:r !cmd` | `spawn()` with the console handed over and taken back |
| a filter driven through two pipes, one written by a **second forked copy of the editor** | a temp file on each side: one task cannot park on both ends of a pipeline |
| the errno message table, carried in the source | `error_name()`, because the kernel names its own errors |
| `expreserve` and `exrecover`, two setuid helpers | gone with the temp file they existed for |
| the `ex3.6strings` file and the `xstr` tool that built it | the messages are compiled in, as they were on VMUNIX |
| `-x` and `:set key`, the crypt mode | gone |
| K&R C throughout, no prototypes anywhere | C++20; `tools/knr.py` did the mechanical half and `tools/patch/` the rest |

## Structure

- **The buffer is in memory and the packing is unchanged.** Upstream kept text
  in a temp file and memory held only handles. On the VMUNIX arm `SHFT` is 0
  and `OFFBTS` is 10 against a `BUFSIZ` of 1024, so the block-and-offset packing
  degenerates to the identity: a handle *is* a byte offset. That is what let
  `getline()` and `putline()` stay upstream's own source, and with them the
  property they rest on — `tline` only ever increases, so a handle names one
  line forever, which is what makes marks and the general undo correct.

- **The line pointer array may not move.** `dot`, `dol`, `addr1`, `addr2`,
  `unddol`, `truedol`, the undo bounds and `names[]` are all raw pointers into
  it, and `sbrk` never moved. So it is taken once, and `morelines()` bumps a
  limit rather than growing anything.

- **error() records rather than unwinds.** This is the part of the port that
  had to be invented. Upstream's `error()` never returned: it `longjmp`ed, to
  the top of the command loop or to a catch inside visual. wasm keeps its call
  stack outside linear memory, so `setjmp` cannot be written here and
  exceptions are off. `error()` therefore sets a flag and returns, and
  `THROW`/`THROWC` unwind one frame at a time. A missed check has to be inert
  rather than corrupting, so a dozen leaf routines are poisoned while the flag
  is up: the input routines answer `EOF`, `putchar()` drops, and `putline()`
  appends nothing and answers a sentinel — never a live handle, because two
  lines sharing one is precisely what breaks a mark.

- **The order in `error_end()` is load-bearing.** The flag goes up last,
  because `putchar()` is poisoned on it and the message has to get out first.

- **Reading happens in one place.** `getach()` is called from the bottom of the
  address parser and cannot await a syscall, so it answers `EOF` when its
  buffer runs dry and the command loop fills it. That is safe because a whole
  line is always buffered before any of it is parsed: a command stops at its
  newline and never asks for the one after it. The consequence is the good one
  — the address parser, the regular expressions, the substitute and the undo
  are ordinary synchronous code, exactly as written.

- **Off a terminal, input is read a byte at a time.** Upstream did the same and
  for the same reason: what is not read stays readable, so a child spawned by
  `:!` sees the rest of the script rather than finding it eaten.

## Differences from upstream worth knowing

- **`:visual`, `:open` and `:vi` are not built.** Open mode is out for good —
  it existed for terminals that could not address a cursor, and a cell grid
  always can — and visual is not written yet.
- **`:!cmd` runs, `sort` may not.** The escapes work, but they run Braam's
  `/bin`, which is forty-odd commands and not a Unix.
- **`:e *.c` does not glob.** Upstream forked a shell to expand the argument
  words; the shell here has already done that before ex was entered. `%`, `#`
  and `$` still expand.
- **The error text for a failed system call is the kernel's.** A missing file
  is `not found`, not `No such file or directory`.
- **There is no `TERM` and `:set term=` is an error.** The screen is an array
  of cells, so a terminal type would name nothing; `:set all` prints
  `term=braam`.
- **`^Z` and `:stop` are gone.** `SIG_TSTP` is not in Braam's catchable set.
- **`:preserve`, `:recover` and `-r` are gone** with the temp file.
- **`-x` is gone.** The crypt mode was asked to be left out.
- **`w300` and `w1200` are ignored.** They chose a window size from the line's
  speed; there is no line, so this is always the fast case.
- **A file is limited by memory**, not by the temp file: about 2 MB of text,
  and the buffer grows by everything ever typed rather than by what is in it,
  because nothing is ever reused. Both limits are one constant each in
  `ex_buf.h`.
- **Only `%s %c %d %u %o %x` reach `printf`.** `%D` was the v6 library's long
  decimal and is `%ld` here.

## Building and packaging

```
make                     # both binaries
make package             # vi-3.6-r0.zip, holding bin/ex and bin/vi
```

Each is about 130 KB. `tools/regen.sh` regenerates the ported `.cpp` files from
`tmp/ex/`: `tools/knr.py` does the K&R conversion, resolves the `#ifdef`
configuration and inserts the `co_await`s, and one script per file under
`tools/patch/` does the edits that needed a decision. It ran once; the `.cpp`
files are the source from here on. It is kept because it says exactly which
half of the port was mechanical.

## Testing

`make test` runs five cases, each driving `ex` with its input and output
redirected to files — a long transcript does not fit on 24 rows, and command
mode prints no prompt down a pipe, which is what makes a transcript assertable.

- `exscript.mjs` — the address grammar, `a`/`i`/`c`, `d`/`m`/`co`/`j`, marks,
  undo, and a `:w` read back out of the store.
- `exerrors.mjs` — the gate on the error mechanism: twenty-two commands that
  must fail, each message exact and each buffer intact afterwards.
- `exregex.mjs` — every BRE construct, `:s` with groups and `&`, `:g`, `:v`,
  `ignorecase` and `nomagic`.
- `exfiles.mjs` — `:w` in its forms, `:r`, the two refusals that stop a buffer
  being lost, `readonly`, and the arg list.
- `exbang.mjs` — `:r !`, `:w !`, a range through a filter, and `:!` leaving the
  buffer alone.

## Files

| Here | Upstream |
| --- | --- |
| `ex.cpp` | `ex.c` — entry, argument handling, `.exrc` |
| `ex_addr.cpp` | `ex_addr.c` — address parsing |
| `ex_cmds.cpp` | `ex_cmds.c` — the command loop and its switch |
| `ex_cmds2.cpp` | `ex_cmds2.c` — command decoding, and the error machinery |
| `ex_cmdsub.cpp` | `ex_cmdsub.c` — append, delete, join, move, undo, `:map`, tags |
| `ex_data.cpp` | `ex_data.c` — the option table, and every global's one definition |
| `ex_get.cpp` | `ex_get.c` — command mode input |
| `ex_io.cpp` | `ex_io.c` — files, `:source`, argument words |
| `ex_re.cpp` | `ex_re.c` — the regular expressions, `:g` and `:s` |
| `ex_set.cpp` | `ex_set.c` — `:set` |
| `ex_subr.cpp` | `ex_subr.c` — sixty subroutines |
| `ex_unix.cpp` | `ex_unix.c` — `:!`, `:sh`, filters |
| `ex_buf.cpp` | `ex_temp.c` — the buffer, which is no longer a temp file |
| `ex_out.cpp` | `ex_put.c` — the half of it that formats |
| `ex_file.cpp` | — the system calls, in the shapes ex expects them |
| `ex_screen.cpp` | `ex_tty.c` — the terminal, which is no longer a terminal |
| `braam.cpp` | — the C library ex calls |
| `ex_err.h` | — the `longjmp` that is not |
