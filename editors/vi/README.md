# vi — the UCB screen editor, and ex under it

The editor Bill Joy wrote: `dd`, `cw`, `p`, `u`, and `:1,$s/old/new/g` behind
the colon. Two commands out of one set of sources, `vi` and `ex`, as upstream
shipped them.

**This is ex/vi version 3.6, dated 11/3/80.** Bill Joy wrote it at Berkeley in
1976 out of ed, and Mark Horton maintained it from 1979; this directory ports
the version that shipped with 4BSD, taken from the V7/x86 contrib tree at
`github.com/calmsacibis995/v7x86`, under `usr/contrib/ucb/ex`. It is the
ancestor of every vi in use since. The Berkeley copyright is on every file
and in [LICENSE](LICENSE); `:version` prints the version string.

## Using it

```
vi file          # the screen editor
ex file          # the same editor, one command per line, : is the prompt
ex - file        # no prompt and no autoprint, for a script
vi -R file       # read only
vi -t tag file   # start at a tag
vi +/pat file    # start at the first line matching pat
```

`vi` is the screen editor and `ex` is the same editor with the screen taken
away — one binary's worth of code either way, and `:visual` and `Q` cross
between them. The command half suits a browser tab better than it looks: it
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
| termcap's `cm`/`al`/`dl`/`ic` and a cost model over them | `vtube`, vi's own screen image, blitted into the Grid: one syscall a frame, carrying the cells that changed |
| a byte read from the tty, and a one-second wait to tell an arrow key from an ESC | `next_key()`; a named key arrives whole, so there is nothing to tell apart |
| `SIGTSTP`, `SIGHUP`, and re-arming `SIGINT` on every delivery | `sig_catch(SIG_INT)` once; `SIG_WINCH` arrives as `Err(Intr)` and is a repaint |
| `fork` and `execl` for `:!`, `:sh` and `:r !cmd` | `spawn()` with the console handed over and taken back |
| a filter driven through two pipes, one written by a **second forked copy of the editor** | a temp file on each side: one task cannot park on both ends of a pipeline |
| the errno message table, carried in the source | `error_name()`, because the kernel names its own errors |
| `expreserve` and `exrecover`, two setuid helpers | gone with the temp file they existed for |
| the `ex3.6strings` file and the `xstr` tool that built it | the messages are compiled in, as they were on VMUNIX |
| `-x` and `:set key`, the crypt mode | gone |
| K&R C throughout, no prototypes anywhere | C++20; [tools/knr.py](tools/knr.py) did the mechanical half and hand edits the rest |

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

- **`vtube` was already the screen.** vi kept an exact image of the terminal so
  that it could work out the fewest bytes to send. That image is the back
  buffer now: `vflush()` writes the cells that differ into the Grid, which
  keeps the damage, and one syscall sends the frame. It is the same economy
  `ex_put.c`'s cursor-cost model was after, arrived at from the other end — and
  it is why `vgoto()` is four assignments where it was two hundred lines.

- **`redraw` defaults on.** Upstream left `@` on a screen row it had decided
  not to repaint, because a repaint cost bytes at 300 baud; `:set redraw` was
  for people who would rather pay. A repaint costs the damaged cells here, so
  it is always worth it, and `@` lines do not appear.

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

[Keymap.md](Keymap.md) asks the other question — which of a modern vi user's
keystrokes work here, which could be made to, and which will not — and the list
below is what it draws on.

- **`:open` is gone.** Open mode edited one line where it stood, for terminals
  that could not address a cursor; a cell grid always can. The `state !=
  VISUAL` branches are left in place rather than excised — there are forty of
  them, and dead is cheaper than cut.
- **`showmatch` does not pause.** Upstream slept a second on the matching
  bracket. A sleep is a syscall and the pause happens inside insert mode, where
  nothing can await; the match is still checked, and an unmatched one beeps.
- **UTF-8, and the editing is by character.** Upstream dropped everything above
  0177, because `TRIM` was 0177 and `QUOTE` — the flag it passes to `putchar`
  — was the bit above it, so the flag and the data wanted the same bit. `QUOTE`
  is 0x200000 now and `TRIM` is a codepoint's worth, which separates them. A
  line is still a `char` array of UTF-8 bytes, the way the SDK's own `TextBuf`
  keeps text; what changed is the stepping. `h`, `l`, `x`, `X`, `r`, Backspace
  and the column arithmetic all move a whole codepoint, `nextchar`/`prevchar`
  in [ex_subr.cpp](ex_subr.cpp) being what they move by. A malformed sequence
  draws as U+FFFD and advances one byte, so bad input is visible and never eats
  the rest of the line; `:e` counts those where it used to count "non-ASCII".
  There is no `wcwidth` anywhere in Braam, so one codepoint is one cell here as
  it is in every other program.

  The regular expressions came with it. `Expbuf` holds codepoints rather than
  bytes, the way `vtube` and `rhsbuf` do, so `.` matches one character,
  `[а-я]` is a range and a class member can be multi-byte; `compile()` reads
  through a `getrune()` because `getchar()` hands back bytes. `advance()`'s
  greedy scanners and the `star:` backtrack step characters together — they
  have to, since the scanners overshoot by one for the backtrack to give back.
  Case is `rune_lower`/`rune_upper` from `kernel/text.h` throughout: `~`,
  `ignorecase`, and `\u` `\U` in a `:s` replacement, which used to spend their
  one conversion on a lead byte.

  `w`, `e`, `b` and the `\<` `\>` boundaries share one predicate, `rune_word`
  in [ex_subr.cpp](ex_subr.cpp) — they must not disagree about where a word
  ends. Above ASCII there are no tables to consult, so the rule is by
  exclusion: not a blank and not punctuation is a letter, with four ranges for
  the punctuation (Latin-1 symbols, General Punctuation, the CJK forms). That
  makes Cyrillic, Greek, CJK, Arabic and Hebrew word characters and leaves a
  dash, a guillemet or a CJK comma as punctuation, which is what stops `w`.

  One thing is still byte-wise, and is obscure enough to leave: a `:s`
  delimiter above ASCII is compared on its lead byte, so `:s§a§b§` does not
  work. Use a delimiter from ASCII, which is every delimiter anyone uses.
- **The screen image holds codepoints.** `vtube` was one byte per cell and is
  one `int`, which is what lets a cell carry a character and the `QUOTE` tag
  that marks the inside of an expanded tab. It costs 256 KiB of the 16 MB
  process cap, and `vutmp` — a line image, carved out of the tube's tail by
  upstream — has its own block now.
- **A key is a codepoint, and the line takes its bytes.** `key_byte()` used to
  answer `k.code < 0x80 ? k.code : 0`; it answers the codepoint, and `getbr()`
  hands back the UTF-8 bytes one at a time from a small queue below the
  pushbacks. Insert mode echoes the character rather than the byte, waiting for
  the last byte of a sequence, and `r` counts characters rather than keys.
- **The arrow keys answer `^P`, `^N`, `^H` and space,** not `hjkl`, so that a
  `:map` on them still reaches them. Home and End are `0` and `$`, the paging
  keys are `^B` and `^F`, and Delete is `x`. A modifier held with any of them
  changes nothing.
- **The escape key is not the byte 033.** It has a code of its own all the way
  into the decoder, because 033 has two other producers: `^[` types one, and
  `getbr()` answers one when replayed input runs out. The key always ends an
  insertion; the byte does not, when a repeat has more of itself to come.
- **The cursor keys work inside an insertion**, which upstream's did not:
  ←, →, Home, End, ↑ and ↓ move and leave you inserting where they land, and
  Delete takes the character under the caret. There is no cursor between two
  characters in vi's insert — the text being typed is in `genbuf` and the rest
  of the line is still in `linebuf` — so what actually happens is that the
  insertion ends, the motion runs on the whole line, and another insertion
  opens at the new cursor. Two consequences, both of them vim's rule as well:
  `u` undoes back to the last motion rather than to the `i`, and `.` repeats
  the text typed after it. `U` still restores the whole line. A paging key
  ends the insertion, `^V` still quotes the key after it, and on the `:` line
  a cursor key beeps and is dropped — ending *that* would run a half-typed
  command.
- **Backspace erases past the start of an insertion**, which upstream's did
  not: its `case CTRL('h')` carries the note `BUG: Can't back around line
  boundaries`, and it stopped at whatever the insertion itself had typed —
  which is the note's own subject. Three floors are below that, and each is
  answered where it lives. The autoindent is in `genbuf` too, so the floor is
  simply lowered onto it; `^T`, `^D` and `^^D` already move it. Below that is
  the line, which is in `linebuf`, and below the line is the break. Both take
  the route the cursor keys take: the insertion ends, `vdelete` rubs the
  character out or `op_join` takes the break, and another insertion opens where
  that left off. So the whole of vim's `backspace=indent,eol,start` is here,
  and `o` followed by a backspace undoes itself. The join inserts no space — it
  is `join(1)`, the `:j!` spelling — and only the first line of the file has
  nothing to back into. Two consequences beyond the cursor keys' own: the
  rubbed character lands in the unnamed register, because `vdelete` is what
  does the work, and a join reports itself on the echo line when `report` says
  to. `^H` is the same key and does the same thing; on the `:` line the echo
  area keeps its own floor, where backspacing off the prompt abandons the
  command.
- **`[Hit return to continue]` is written on the console, not through
  `vtube`.** Upstream paused after a `:` command that printed, so its output
  could be read before visual repainted over it, and it did so with `merror`
  and `getkey` — neither of which can be used for it here. A `:` command's
  output goes to the output buffer and then to stdout, which lands on the same
  cells the Grid does but is not in `vtube`; `getkey` begins with the
  `vflush()` whose diff would put `vtube` back over exactly what the pause
  exists to keep. So the prompt is bytes on stdout like the output above it,
  and the key is one `key_read()`. Around a shell escape there is no keyboard
  either — the claim went back before the child was spawned — so it is taken
  for the one keystroke and given straight back, and the pause happens before
  the screen is retaken, because taking it is what repaints. Any key answers;
  `:` opens the next `:` line, which pauses in its turn. Upstream paused from
  `fixol()`, which cannot await here — `excatch()` calls it, and `excatch()`
  has eight callers that are not coroutines — so the `:` line does it instead,
  which is the only place a command's output reaches the terminal.
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
- **The screen has colour**, which a termcap terminal did not: the echo line
  is cyan and the `~` of a row past the end of the file is blue. `vflush()`
  decides it per row, from where the row is and what is on it, so nothing
  upstream had to learn about it.
- **An insertion says so.** `-- INSERT --` in bright white, over the echo
  area, from the moment `i` is pressed until the insertion closes — which is
  vim's, not vi's: upstream gave no sign at all, and on a screen with no modes
  of its own that is a poor bargain. `vflush()` writes it over the echo row
  rather than into `vtube`, so the row underneath comes back on its own when
  the insertion ends. `r` is not a mode and the `:` line is not one either.
- **A motion sends a frame of its own.** The cursor rides in the header of a
  blit, and a blit with nothing damaged in it is not sent at all, so
  `vflush()` damages the cell under the cursor when it has moved. Without
  that, `h`, `j`, `k`, `l`, `$`, `G` and every other motion moved the cursor
  in the editor and left it where it was on the screen — which looks exactly
  like an editor that has stopped answering.
- **`^Z` and `:stop` are gone.** `SIG_TSTP` is not in Braam's catchable set.
- **`:preserve`, `:recover` and `-r` are gone** with the temp file.
- **`-x` is gone.** The crypt mode was asked to be left out.
- **`w300` and `w1200` are ignored.** They chose a window size from the line's
  speed; there is no line, so this is always the fast case: `window` is the
  whole screen but the status row, and `scroll` half of that. Both are read
  off the grid when visual claims it and again at every resize, so a value the
  user has set — in `EXINIT`, in `.exrc`, with `-w`, or with `:set` — stays
  put, and one left at its default follows the screen.
- **The screen is used up to 128 rows and 512 columns.** 512 is the widest
  grid the kernel will make; 128 is half the tallest, because `vlinfo`'s row
  number is a `char`. The image behind them is one 64 KiB heap block claimed
  on the first `vi`, and `TUBELINES`, `TUBECOLS` and `TUBESIZE` in `ex_tune.h`
  are the three constants that say so.
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

Each is about 220 KB.

[tools/knr.py](tools/knr.py) is the converter the port was made with: it reads
the prototypes out of `ex.h` and `ex_vis.h`, rewrites each K&R definition to
match, resolves the `#ifdef` configuration, quotes `CTRL()`'s argument and
inserts the `co_await`s. It ran once, over the upstream tree named at the top
of this file, and the `.cpp` files here are the source from here on -- it is
kept as the record of which half of the port was mechanical, not as a step you
can repeat.

## Testing

`make test` runs eleven cases, each driving `ex` with its input and output
redirected to files — a long transcript does not fit on 24 rows, and command
mode prints no prompt down a pipe, which is what makes a transcript assertable.

- `exscript.mjs` — the address grammar, `a`/`i`/`c`, `d`/`m`/`co`/`j`, every
  spelling of `delete` and the `dp`/`dl` forms, marks, undo, and a `:w` read
  back out of the store.
- `exerrors.mjs` — the gate on the error mechanism: twenty-two commands that
  must fail, each message exact and each buffer intact afterwards.
- `exregex.mjs` — every BRE construct, `:s` with groups and `&`, `:g`, `:v`,
  `ignorecase` and `nomagic`.
- `exfiles.mjs` — `:w` in its forms, `:r`, the two refusals that stop a buffer
  being lost, `readonly`, and the arg list.
- `exbang.mjs` — `:r !`, `:w !`, a range through a filter, and `:!` leaving the
  buffer alone.
- `vikeys.mjs` — the first frame whole and its three colours, `x`, counts, the
  word and arrow motions, `^F` and `^C`.
- `viinsert.mjs` — `i`/`A`/`o`/`O`, the operators, `yy`/`p`, `u`, `.`, a named
  buffer, `:map`, `:ab`, both shell escapes, the file `ZZ` writes, and the
  report line.
- `vikeypad.mjs` — the arrows, Home, End, Delete, Backspace and escape, in
  command mode, inside an insertion, after an operator and on the `:` line;
  and the mode line, which of the insert commands raise it and which do not.
- `viresize.mjs` — a resize mid-session: the screen re-cut, the buffer kept,
  the cursor where it was; and a session started on a 144x41 screen, which
  uses all of it.
- `viutf8.mjs` — a round trip through the store, the display, editing and
  typing by character, regexps, `~`, and `w`/`e`/`b`.
- `vibang.mjs` — the pause: `:!` under its output with the screen handed back,
  `:p` with both claims held, the key that resumes, and `:` at the prompt
  opening the next command.

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
| `ex_v.cpp` | `ex_v.c` — entering and leaving visual |
| `ex_vmain.cpp` | `ex_vmain.c` — the visual keystroke loop |
| `ex_voper.cpp` | `ex_voper.c` — operators, operands and word motions |
| `ex_vops.cpp` | `ex_vops.c` — delete, change, shift, yank, undo |
| `ex_vops2.cpp` | `ex_vops2.c` — insert mode |
| `ex_vops3.cpp` | `ex_vops3.c` — the `( ) { } [ ]` motions |
| `ex_vput.cpp` | `ex_vput.c` — the screen image, less the part that drove a terminal |
| `ex_vadj.cpp` | `ex_vadj.c` — logical screen control |
| `ex_vget.cpp` | `ex_vget.c` — single keys, `:map`, the echo area |
| `ex_vwind.cpp` | `ex_vwind.c` — window control and scrolls |
| `ex_screen.cpp` | `ex_tty.c` — the terminal, which is no longer a terminal |
| `braam.cpp` | — the C library ex calls |
| `ex_err.h` | — the `longjmp` that is not |
