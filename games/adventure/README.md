# adventure — Colossal Cave, the 1977 C re-coding

Find your way into Colossal Cave, collect the treasures hidden there and carry
them back out. The program tells you what it wants to; the rest of the rules
are part of the game.

**This is ADVENT — the original one.** Will Crowther wrote it in FORTRAN around
1975 out of his own maps of Mammoth Cave; Don Woods found it at Stanford in
1976 and added the treasures, the scoring and most of the puzzles. Jim Gillogly
re-coded that FORTRAN into C at RAND in July 1977, and it is Gillogly's C that
Berkeley shipped with BSD, that RetroBSD carries, and that this directory
ports. The 350-point version, 140 rooms, 298 words of vocabulary.

Public domain, as the original is. Upstream here is
`github.com/RetroBSD/retrobsd/tree/master/src/games/adventure`, whose
save/restore was rewritten in a portable way by Serge Vakulenko; the man page
is 4.4BSD's `adventure.6`, 6.2 of 6 May 1986.

## Playing it

```
adventure
```

One or two words a turn, and only the first five letters of each count — so
`northeast` has to be `ne` to be told apart from `north`. `help` gives general
hints, `info` explains how to end an adventure. `quit` ends the game and
scores it; `suspend` writes the game to a file you name, and

```
adventure <file>
```

resumes it. The cave will not take you back within fifteen minutes of a
suspend, which is the anti-abuse gate the original shipped with, magic word and
all.

```
Welcome to adventure!!  Would you like instructions?
> no

You are standing at the end of a road before a small brick building.
Around you is a forest.  A small stream flows out of the building and
down a gully.
> in

You are inside a building, a well house for a large spring.

There are some keys on the ground here.

There is a shiny brass lamp nearby.
```

At the prompt you get the keys `/bin/sh` binds — left and right, home and end,
`^A ^E ^B ^F ^K ^U ^W`, backspace and delete, `alt`-backspace for a word, `^L`
to clear — and **up and down walk a history of the last 32 commands**.

## What the port changed

There is no libc: no `stdio`, no `malloc`, no `signal`, no `time`, no `rand`,
and a program is a coroutine rather than a `main`. The game logic, the FORTRAN
statement labels it still carries, the `tr*` handlers that return label numbers
for the caller to `switch` back into a `goto`, and every line of its output are
upstream's. Comments are `//` rather than `/* */`, and `hdr.h`'s macro layer is
written out: the hundred and thirty-one `#define loc game.loc` aliases that
reached the state struct say `game.loc` now, and the two that kept upstream's
calls spelled `printf` and `putchar` name `adv_printf` and `adv_putc`. All of
it is mechanical — the binary is byte for byte the one the macros produced.

| Original | Here |
| --- | --- |
| `printf`, `putchar` | a variadic formatter over one buffer, flushed once a turn |
| `getchar` + the terminal driver | `LineEditor`, or `LineReader` over stdin |
| `malloc` for travel nodes | `heap_new<travlist>`, checked |
| `alloca` in `speak`/`pspeak` | a fixed 2 KB buffer; the longest message is 1460 bytes |
| `open`/`read`/`write`/`creat`/`close` | `open_at`, `read_file`, `write_all`, `close_fd` |
| `access` | `stat_of`, where `Err(NotFound)` is the good answer |
| `unlink` | `remove_path` |
| `srand`/`rand` | an xorshift32 seeded from `clock_now()`, or `ADVENTURE_SEED` |
| `time`/`localtime` | `clock_now()` and `civil()`, keeping the year-2066 joke |
| `atoi`, `strcmp` | one local `atoi`; `strcmp` left with the mode that used it |
| `exit(0)` | a status returned up the call chain |
| `signal(SIGINT, trapdel)` | nothing — see below |

**`ADVENTURE_SEED` is an addition**, and the only one upstream would not
recognise. Set it to a number and the dice start there instead of at the clock,
so a scripted game replays the same way every time. It exists for
`test/walkthrough.txt` and costs four lines in `adv_seed()`; unset, nothing
changes.

Four changes go deeper than a substitution:

- **`glorkz` is inside the binary.** Upstream is two-phase: `adventure glorkz`
  parses the 55,626-byte data file, writes the message text out XOR-encrypted
  to `adventure.dat`, appends the whole `struct game` after it, and normal play
  then `lseek`s into that file for every message. **Braam has no seek
  syscall** — the operation table in `sysabi.h` does not have one — so the text
  has to be resident either way. `mkdata.py` turns `glorkz` into a C++ array at
  build time, `rdata()` parses it at startup exactly as it always did, and what
  it used to write to a file it writes to a 46 KB heap block that `speak()`
  indexes. The encryption is kept: it costs nothing and it is upstream's. What
  is gone is `adventure.dat`, the bootstrap step that built it, and the
  version-bearing install path a data file would otherwise have to be found at.
- **Output is buffered.** A write is an asynchronous syscall, and `speak()`,
  `rspeak()` and the verb handlers are ordinary functions that cannot `co_await`
  one. They append to a buffer which the loop flushes before each read — one
  syscall for a whole room description rather than one per character. Only what
  reads, writes a file, or ends the game became a coroutine; most of `subr.c`
  and all of `vocab.c` are untouched.
- **`exit(0)` had nowhere to go.** It appeared at 21 sites, most of them deep
  inside the command loop. `done()`, `die()`, `start()` and `ciao()` now return
  `ADV_OVER` and the status travels up to `proc_main`. The handful that were
  can't-happens — `bug()`, a corrupt data section, a full hash table — trap
  instead, which is what the kernel reports as a crashed process.
- **A line editor, which upstream did not need.** Braam's console echoes what
  you type and hands over a line at Enter, but it understands no erase key
  (`src/user/console.cpp`); a mistyped `take lampp` could not be corrected. So
  the port claims the raw keyboard and runs the shell's own editor, copied from
  `braam-core`'s `src/cmd/sh/edit.{h,cpp}` and cut down to one prompt run. The
  green `> ` is an addition too: upstream printed no prompt at all, and a line
  editor needs an anchor to paint from. When stdin is not a console — a pipe, a
  file, a background job — none of this happens and lines come from stdin, so
  `adventure < script` still works.

**`^C` cannot be caught.** Upstream trapped `SIGINT` and turned a DEL into a
synthetic `quit`. On Braam the console cancels whatever process is in front
"whatever is claimed", so `^C` kills the game and the shell reports 130.
`delhit` and its handling in the command loop are kept as upstream wrote them,
and the editor still reports an abandoned line — but nothing reaches them while
the game is the foreground job. End of input does what DEL used to: it quits
and scores.

**One upstream bug is fixed**, because leaving it in would have been a
different program on a different day. `rtrav()` read the travel table with
`for (s=buf; *s; s++)`, testing `buf[0]` before anything had written it;
`rnum()` beside it gets this right with `for (s=nbf,*s=0;; s++)`. With the wrong
garbage the travel table comes out mangled — on the machine this was checked
against, `xyzzy` from inside the building went to the end of the road instead
of the debris room. The port initialises the buffer.

**Left exactly as upstream has it:** `poof()`, which sets the magic word and
the fifteen-minute latency, is called where the data is now built — upstream
called it only in the `adventure.dat` path, so in normal play `magic` stayed
null and the wizard's check dereferenced it. Everything else, including the
save format, is unchanged: `save()` still writes `struct game` raw and then
walks the travel lists node by node, and `restore()` still uses the pointers it
reads back only as "there was a node here", which works because `glorkz` is
parsed before either happens and the chains already have the right shape. A
save file is therefore this build's, and 32-bit: the same game suspended by the
BSD binary is 22,568 bytes and here 19,032.

## Building and packaging

From the top of this repository:

```
make            # build/games/adventure/adventure.wasm
make package    # build/games/adventure/adventure-1.0-r0.zip
```

The package holds `.PKGINFO` and `bin/adventure` and nothing else — the data is
in the binary. `bin/` is what reaches `PATH` once `/bin/pkg` installs it.

## Testing

```
make test       # from the top of this repository
```

`test/play.mjs` boots `../braam-core`'s kernel under node, plants the built
`adventure.wasm` in the image, and runs `test/walkthrough.txt` on stdin with
`ADVENTURE_SEED=36`. That walkthrough is a **complete game — 350 out of 350 in
330 turns**, and the test fails on anything less, on any word the parser
refuses, on a death, on a question that ate a command, on any of twenty
landmarks reached out of order, and finally on any difference at all from
`test/game.log`, the golden transcript beside it. The run is deterministic, so
that last check is exact; when a change to the game is meant, refresh it with

```
cp build/games/adventure/game.log games/adventure/test/game.log
```

The transcript is written to `build/games/adventure/game.log` whether the test
passes or not, and the whole run takes about twenty milliseconds.

The walkthrough was adapted from an `expect` script that plays the **Z-machine**
Adventure under `dfrotz` to the same 350 points, following
[the usual solution](https://ifarchive.org/if-archive/solutions/adventure-walkthrough.txt).
None of that script could be used as it stood. It waits on a `>` prompt that
only the line editor prints; it never answers `Would you like instructions?`,
`Do you indeed wish to quit now?` or `Do you really want to quit now?`, all of
which this version asks; and it uses `all`, `give` and `wait`, none of which are
among the 298 words here — so `get all` became the objects named one by one,
`give eggs` became `throw eggs`, and `give food` became `feed bear`. Its
`-s 123` dwarves fall on other turns than ours, so where the axe is picked up
and thrown is this seed's own, and so is the wait at Witt's End: the way out of
there is one chance in twenty a turn, and the game offers a hint every
twenty-five turns you fail, which has to be declined.

Its longer commands would in fact have parsed: `getin` keeps only the first two
words of a line, and only their first five letters, so `unlock grate with keys`
is `unlock grate` and `throw axe at dwarf` is `throw axe`. The walkthrough
writes the two words it means.

## Files

| | |
| --- | --- |
| `hdr.h` | `struct game` and the prototypes |
| `adventure.cpp` | the game: the command loop, the tables, the verbs, the scoring |
| `io.cpp` | the `glorkz` parser, the messages, the words typed, save and restore |
| `braam.cpp`, `braam.h` | the porting layer: output, input, the dice, the clock |
| `edit.cpp`, `edit.h` | the line editor, from `braam-core`'s `sh` |
| `mkdata.py` | `glorkz` to a C++ array, at build time |
| `glorkz` | the data file, verbatim |
| `test/play.mjs` | the headless test: kernel, image, walkthrough, assertions |
| `test/walkthrough.txt` | a whole game, 350 of 350, one command a line |
| `test/game.log` | what it prints, to the byte |
