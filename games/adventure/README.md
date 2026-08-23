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
and a program is a coroutine rather than a `main`. The game logic and every
line of its output are upstream's. Comments are `//` rather than `/* */`, and
`hdr.h`'s macro layer is written out: the two aliases that kept upstream's
calls spelled `printf` and `putchar` name `adv_printf` and `adv_putc`.

**The state is a class.** Upstream's globals — the hundred and thirty-one
`#define loc game.loc` aliases reached one struct — are `Game`'s members, and
its routines are `Game`'s methods; a field carries a trailing underscore so
neither hides the other. The fixed tables are `Vec`s, sized once in `alloc()`
and unchecked from then on as the C arrays were, and the travel lists are a
`Vec` of `Vec` where upstream had a hand-rolled linked list.

**The turn is a state machine.** Upstream's command loop is one function of
750 lines carrying thirty-seven FORTRAN statement labels, with the `tr*`
handlers returning the label number their caller switched back into a `goto`.
Each label is a method here, returning the label to go to next, and `play()`
dispatches; `main.cpp` has no `goto` left. Twenty states cover the
thirty-seven labels, because a fallthrough chain stays a chain of calls and
nine of the twelve `9xxx` labels are only ever entered from `4000` with the
matching verb already in `verb_`. The local jumps inside `fdwarf()`,
`march()`, `checkhints()`, `trtake()`, `trtoss()` and `vocab()` are upstream's
own and stay: each is a forward jump within one routine. Their labels are
named, though — `march()` reads `find_exit` → `try_entry` → `take_exit`,
with `next_entry` when a probability move fails — and each keeps its
statement number as a comment.

**The vocabulary is two enums.** `glorkz` maps a word to a number — under
1000 a motion word, 1000 an object, 2000 a verb, 3000 a message — so the two
dispatch switches read `case 1:` through `case 31:` with a comment for each,
and `badmove()` chose its complaint with `if (k_ >= 43 && k_ <= 50)`. `Verb`
names all thirty-one and `Motion` all seventy-five, which is the first place
that numbering is written down; `verb_` is typed, `k_` is not, because `k_`
holds a motion, an object, a location, a message or plain scratch depending on
where you are. The travel table's `tverb` is a `Motion`, which also names the
1 that no word carries and that `linkdata()` and `march()` both test — the
exit that fires whatever the player typed.

**The messages are named too**, in [msg.h](msg.h): `Msg` for the 198 that
`rspeak()` prints and `Magic` for the 32 that `mspeak()` does. 139 of them
appeared as bare numbers, and nine more sites computed one — every one of
those turns out to index a run of consecutive messages, which is what
`Msg::PutDownClam + k_` and `Msg::Killed + numdie_ * 2` now say. `msg.h` is
generated from `glorkz`, so the numbers are the file's; the names are a
summary of the text, which each one carries beside it because that is the
only way to check one.

**The hints are named too.** `Hint` covers the eight, and `HintRule` names the
four columns of the table `checkhints()` reads — a turn threshold, a point
cost, a question and an answer. Their numbers cannot move: **a hint's number is
also its bit in `cond_`**, which is what `bitset(loc_, hint)` tests, and glorkz
sets bit 4 on the room outside the grate, 5 on the bird chamber, 6 on the hall
of the mountain king, 7 across the all-alike maze, 8 on the plover rooms and 9
on Witt's End. Bits 0 to 3 are the lit and liquid flags, which is why the loop
starts at 4 — and why the oyster's clue and the offer of instructions, neither
of them triggered by a room, can sit below it.

The numbers are still the data file's, and three checks keep the naming honest.
`check_vocab()` looks up the word behind every vocabulary constant the code
uses by name and traps if `glorkz` disagrees. `check_msgs()` cannot do that —
nothing compares message text at runtime — so it asserts the weaker thing that
still matters: that the message table has not shifted under the names, gap at
87–89 included. `check_hints()` ties each hint to the question and answer its
name claims, so the two tables cannot drift apart.

The guarantee that used to be "the binary is byte for byte the one the macros
produced" is now the transcript. `test/play.mjs` diffs 41,777 bytes of output
from a 322-turn game, and `test/back.mjs` and `test/suspend.mjs` cover what it
does not reach; none of the above moved a byte of any of them.

| Original | Here |
| --- | --- |
| `printf`, `putchar` | a variadic formatter over one buffer, flushed once a turn |
| `getchar` + the terminal driver | `LineEditor`, or `LineReader` over stdin |
| `malloc` for travel nodes | a `Vec<Travel>` per location, filled as `glorkz` is read |
| `alloca` in `speak`/`pspeak` | a fixed 2 KB buffer; the longest message is 1460 bytes |
| `open`/`read`/`write`/`creat`/`close` | `open_at`, `read_file`, `write_all`, `close_fd` |
| `access` | `stat_of`, where `Err(NotFound)` is the good answer |
| `unlink` | `remove_path` |
| `srand`/`rand` | an xorshift32 seeded from `clock_now()`, or `ADVENTURE_SEED` |
| `time`/`localtime` | `clock_now()` and `civil()`, keeping the year-2066 joke |
| `atoi`, `strcmp` | one local `atoi`; `strcmp` left with the mode that used it |
| `exit(0)` | a status returned up the call chain |
| `signal(SIGINT, trapdel)` | `sig_catch(SIG_INT)`, and `Err(Intr)` is the DEL |

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

**`^C` is upstream's DEL.** Upstream trapped `SIGINT` and turned a DEL into a
synthetic `quit`, and `delhit` and its handling in the command loop were kept
as upstream wrote them from the first version of this port — but until Braam
had signals nothing reached them, because the console cancelled whatever
process was in front. `adv_input_init` now asks for `SIG_INT`, which abandons
the read the game is parked on with `Err(Intr)` instead of killing it, and both
input paths answer that as an interrupted line. So `^C` pretends the player
typed `quit`, and the game asks before doing it — a second `^C` answers that
question, as an abandoned line answers any of them. End of input still does the
same thing, which is what a script that stops mid-game gets.

That the game is told rather than killed also means it hands the keyboard back
on the way out: `keys_claim(false)` runs where a cancellation used to skip it.

**A resize repaints the line being typed**, which upstream had no notion of.
The editor asks for `SIG_WINCH` too, and a resize with no keystroke behind it
abandons the read the same way; the repaint that follows carries the new
geometry back. This is what `ProcScreen` does for a full-screen program, done
by hand because the editor drives `key_read()` itself.

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
null and the wizard's check dereferenced it.

**The save file is a field list.** Upstream wrote `struct game` raw and then
walked the travel lists node by node. `serial.h` names every field once and
visits it twice — `Ser` writes, `De` reads — so the two halves cannot drift
apart, and a magic word at the front means a stale file is refused rather than
mis-parsed. The travel lists are not in it at all: `rdata()` rebuilds them
before `restore()` runs and play never changes them, which is the same reason
the old format could treat the pointers it wrote as "there was a node here". A
save file is this build's, and 9,780 bytes where the raw struct took 19,032.

**One thing worth checking against upstream.** `mback()` has a fallback for
getting back through a forced location, and as this port has it the branch
cannot fire: the cursor starts at the head of the list, so
`k_ == travel_[loc_][0].tloc` would already have matched `ll == k_` and
returned. The line reads `travel[loc]` where upstream, by the sense of it,
reads `travel[ll]` — the location you would pass through. It is left as it is,
because changing it would change the game.

## Building and packaging

From the top of this repository:

```
make            # build/games/adventure/adventure.wasm
make package    # build/games/adventure/adventure-1.0-r2.zip
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
322 turns**, with nothing spare in it: drop any one of its 329 commands and the
game ends short of 350. The test fails on anything less, on any word the parser
refuses, on a death, on a question that ate a command, on any of twenty
landmarks reached out of order, and finally on any difference at all from
`test/game.log`, the golden transcript beside it. That transcript has the
commands put back into it, one per turn — the game echoes nothing when its
input is a file, so `play.mjs` recovers the turn boundaries from the writes and
interleaves them, and the log reads like a session someone sat through.

The run is deterministic, so that last check is exact; when a change to the
game is meant, refresh it with

```
cp build/games/adventure/game.log games/adventure/test/game.log
```

`test/interrupt.mjs` is the second case and the only one that plays on the
grid: an interrupt is a property of the console, and over a pipe there is no
keyboard to press. It starts a game, sends `^C` at the prompt, checks that the
game asks whether to quit rather than dying, sends another, and checks that it
scores itself, exits to a status-0 prompt and gives the keyboard back — the
shell reading a command afterwards is what proves the last of those.

`test/suspend.mjs` is the third, and covers what `make test` used to reach not
at all: it plays a prefix, `suspend`s it to a file, checks that the same prefix
suspended twice writes the same bytes, resumes from it, and checks that a file
of `A`s is refused as a forgery. The resume does not get to play on — the
harness clock is frozen, so no time passes and `start()`'s fifteen-minute gate
turns it away, which is upstream's rule rather than this port's. Reaching that
refusal is already the proof that `restore()` read the file.

`test/back.mjs` is the fourth. The walkthrough never types `back`, so
`mback()` — the one routine that compared travel-list pointers for
identity — had no coverage when the lists became `Vec`s. It replays the walkthrough's
opening with a `back` after every command and diffs `test/back.log` byte for
byte; that golden was generated from the binary as it stood before the change.

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
there is one chance in twenty a turn, and the game offers a hint every twenty
turns you fail, which has to be declined.

Its longer commands would in fact have parsed: `getin` keeps only the first two
words of a line, and only their first five letters, so `unlock grate with keys`
is `unlock grate` and `throw axe at dwarf` is `throw axe`. The walkthrough
writes the two words it means.

## Files

| | |
| --- | --- |
| `hdr.h` | `class Game`: the state, the methods, the phases, the words |
| `msg.h` | what `rspeak()` and `mspeak()` say, named |
| `main.cpp` | closing the cave, and the command loop — upstream `main.c` |
| `init.cpp` | the shared state and its setup — upstream `init.c` |
| `vocab.cpp` | the objects and the words — upstream `vocab.c` |
| `subr.cpp` | the statement functions — upstream `subr.c` |
| `move.cpp` | the dwarves and the travel table — `subr.c` too |
| `verbs.cpp` | the hints and the verb handlers — `subr.c` too |
| `wizard.cpp` | suspend, resume and the magic word — upstream `wizard.c` |
| `done.cpp` | dying, quitting and the scoring — upstream `done.c` |
| `io.cpp` | the messages and the words typed — upstream `io.c` |
| `data.cpp`, `data.h` | the `glorkz` parser — `io.c`'s `rdata()` and below |
| `save.cpp`, `serial.h` | the save file: one field list, written and read |
| `braam.cpp`, `braam.h` | the porting layer: output, input, the dice, the clock |
| `edit.cpp`, `edit.h` | the line editor, from `braam-core`'s `sh` |
| `mkdata.py` | `glorkz` to a C++ array, at build time |
| `glorkz` | the data file, verbatim |
| `test/play.mjs` | the headless test: kernel, image, walkthrough, assertions |
| `test/interrupt.mjs` | the second one: `^C` at the prompt, on the grid |
| `test/suspend.mjs` | the third: suspend to a file, and resume from it |
| `test/back.mjs` | the fourth: `back`, which the walkthrough never types |
| `test/back.log` | what that prints, to the byte |
| `test/walkthrough.txt` | a whole game, 350 of 350, one command a line |
| `test/game.log` | what it prints, to the byte |
