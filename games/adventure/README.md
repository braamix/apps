# adventure — Colossal Cave

Find your way into Colossal Cave, collect the treasures hidden there and carry
them back out. The 350-point version: 140 rooms, 298 words of vocabulary.

**This is ADVENT, the original.** Will Crowther wrote it in FORTRAN around 1975
from his own maps of Mammoth Cave; Don Woods found it at Stanford in 1976 and
added the treasures, the scoring and most of the puzzles; Jim Gillogly re-coded
it into C at RAND in July 1977. Berkeley shipped that C with BSD, which is what
this directory ports. Public domain, as the original is.

## Playing

```
adventure           # a new game
adventure <file>    # resume one written by `suspend`
```

One or two words a turn, and only the first five letters of each count — so
`northeast` has to be `ne` to be told apart from `north`. `help` gives hints,
`info` explains how to end a game, `quit` scores it, `suspend` writes it to a
file you name. A resume within fifteen minutes of the suspend is refused; that
gate is upstream's, magic word and all.

The prompt takes the keys `/bin/sh` binds — left and right, home and end,
`^A ^E ^B ^F ^K ^U ^W`, backspace and delete, `alt`-backspace for a word, `^L`
to clear — and up and down walk the last 32 commands. `^C` is upstream's DEL:
it pretends you typed `quit`, and a second one answers the confirmation.

With stdin redirected there is no prompt, no echo and no editor, so
`adventure < script` works.

## What the port changed

No `stdio`, `signal`, `time` or `rand`, and a program is a coroutine rather
than a `main`. `atoi` and `strlen` come from the port kit
(`braam_add_program(... PORT)`), which is the whole of the C library this game
asks for. The game logic and every line of its output are upstream's. Comments
are `//`, and `hdr.h`'s macro layer is written out — the aliases for `printf`
and `putchar` are `adv_printf` and `adv_putc`.

| Upstream | Here |
| --- | --- |
| `printf`, `putchar` | a variadic formatter over one buffer, flushed once a turn; `%d %u %c %s` and no float, so it stays hand-written rather than costing the kit's `snprintf` |
| `getchar` + the terminal driver | `LineEditor`, or `LineReader` over stdin |
| `malloc` for travel nodes | a `Vec<Travel>` per location, filled as `glorkz` is read |
| `alloca` in `speak`/`pspeak` | a fixed 2 KB buffer; the longest message is 1460 bytes |
| `open`/`read`/`write`/`creat`/`close` | `open_at`, `read_file`, `write_all`, `close_fd` |
| `access` | `stat_of`, where `Err(NotFound)` is the good answer |
| `unlink` | `remove_path` |
| `srand`/`rand` | an xorshift32 seeded from `clock_now()`, or `ADVENTURE_SEED` |
| `time`/`localtime` | `clock_now()` for the epoch and the zone, then the port kit's `gmtime_r` |
| `atoi`, `strcmp` | the port kit's `<stdlib.h>` |
| `exit(0)` | a status returned up the call chain |
| `signal(SIGINT, trapdel)` | `sig_catch(SIG_INT)`, and `Err(Intr)` is the DEL |

The kit's Group B has a `b_*` for each of the four file rows above, and this
port does not take them: they spell a path as `const char *` where every path
here is a `Str`, so each call site would grow a NUL-terminated `char[PATH_MAX]`
that has to live in a heap block rather than a coroutine frame. `vi` took Group
B because `vi` is still C and its paths are already `char *`.

Structure:

- **The state is a class.** Upstream's globals are `Game`'s members, its
  routines `Game`'s methods; a field carries a trailing underscore so neither
  hides the other. The fixed tables are `Vec`s, sized once in `alloc()` and
  unchecked afterwards as the C arrays were.
- **The turn is a state machine.** Upstream's 750-line command loop carried 37
  FORTRAN statement labels and switched on the number a handler returned. Each
  label is a method returning the next one, `play()` dispatches, and `main.cpp`
  has no `goto` left. `Phase` names 20 states; `Move`, `Done` and `Death` name
  what the routines below it return and take. The forward jumps inside
  `fdwarf()`, `march()`, `checkhints()`, `trtake()`, `trtoss()` and `vocab()`
  are upstream's own and stay, with named labels.
- **`glorkz` is inside the binary.** Braam has no seek syscall, so the
  two-phase build that wrote `adventure.dat` and `lseek`ed into it is gone.
  `mkdata.py` turns `glorkz` into a C++ array at build time, `rdata()` parses it
  at startup as it always did, and the XOR-encrypted text goes to a 46 KB heap
  block that `speak()` indexes.
- **Output is buffered.** `speak()`, `rspeak()` and the verb handlers cannot
  `co_await` a write, so they append to a buffer the loop flushes before each
  read — one syscall per room description rather than one per character. Only
  what reads, writes a file or ends the game is a coroutine.
- **`exit(0)` had nowhere to go.** It appeared at 21 sites. `done()`, `die()`,
  `start()` and `ciao()` return `ADV_OVER` and the status travels up to
  `proc_main`; the can't-happens trap through `bug()` instead.
- **A line editor.** Braam's console understands no erase key, so the port
  claims the raw keyboard and runs the shell's editor, cut down to one prompt
  run. The green `> ` is an addition: upstream printed no prompt, and an editor
  needs an anchor to paint from. `SIG_WINCH` repaints the line on a resize.
- **`ADVENTURE_SEED`** pins the dice for a scripted game. Unset, nothing
  changes.

## The numbers are named

`glorkz` decides the numbers; the enums only name them, all in `hdr.h` beside
`class Game` except `Msg`/`Magic` in `msg.h` and `Section` in `data.h`.

| | |
| --- | --- |
| `Verb`, `Motion` | the 31 verbs and 75 motion words |
| `Msg`, `Magic` | what `rspeak()` and `mspeak()` say, text beside each name |
| `Hint`, `HintRule` | the 8 hints, and the 4 columns of their table |
| `Loc` | the 23 rooms the code names, and three of them as thresholds |
| `Phase`, `Move`, `Done`, `Death` | the statement labels, as return values |
| `WordClass`, `word_of()` | glorkz's `class * 1000 + n` word numbering |
| `CondBit`, `COND_FORCED` | `cond_`'s low four bits, under the `Hint` bits |
| `Special`, `TRAV_*`, `COND_*` | what `march()` decodes out of a travel entry |
| `Dwarves`, `NDWARVES`, `PIRATE` | dwarf six is the pirate; `dflag_`'s states |
| `MAXOBJ`, `FIXED`, `TREASURE`, `CARRIED`, `PINNED` | the object space |
| `Section` | glorkz's twelve data sections |

`Loc` and `Dwarves` are scoped plain enums rather than `enum class`, because
both are arithmetic as much as identity: `Loc::Alcove + Loc::PloverRoom - loc_`
is how the plover tunnel swaps ends.

Still numeric on purpose: the `prop_` values, which mean something different for
every object and index `pspeak()`'s rows in `glorkz`; the scoring weights; the
clocks and the lamp's life; every `pct(n)`.

Four checks at startup keep the naming honest, and trap on disagreement:

- `check_vocab()` looks up the word behind every `Verb` and `Motion` the code
  names.
- `check_msgs()` asserts the message table has not shifted under the names, gap
  at 87–89 included. Nothing can compare message text at runtime.
- `check_hints()` ties each hint to the question and answer its name claims.
- `check_locs()` checks where glorkz places the grate, keys and lamp, that every
  hint and liquid bit is set on the room `Loc` names, and that each named room
  has a description and is not a forced move.

## Differences from upstream worth knowing

- **One bug is fixed.** `rtrav()` read the travel table with
  `for (s=buf; *s; s++)`, testing `buf[0]` before anything wrote it, which
  mangles the table; `rnum()` beside it gets this right. The port initialises
  the buffer.
- **`poof()` is called where the data is built.** Upstream called it only in
  the `adventure.dat` path, so `magic` stayed null and the wizard's check
  dereferenced it.
- **Two dead fields are gone.** `k2_` was never read or written. `spices` was
  never assigned, so `trbridge()`'s `prop[spices]` test read `prop[0]`, which
  no object owns — the `tally2_` bump it guarded could not fire on any turn.
- **The save file is a field list.** `serial.h` names every field once and
  visits it twice, `Ser` to write and `De` to read, so the halves cannot drift;
  a magic word means a stale file is refused rather than mis-parsed. The travel
  lists are not in it — `rdata()` rebuilds them before `restore()` runs. 9,492
  bytes, where the raw struct took 19,032.
- **`mback()`'s fallback cannot fire** as this port has it: the cursor starts at
  the head of the list, so `k_ == travel_[loc_][0].tloc` would already have
  matched. Upstream reads `travel[ll]`, by the sense of it. Left as it is,
  because changing it would change the game.

## Building and packaging

From the top of this repository:

```
make            # build/games/adventure/adventure.wasm
make package    # build/games/adventure/adventure-1.0-r3.zip
```

The package holds `.PKGINFO` and `bin/adventure` and nothing else — the data
is in the binary. `bin/` is what reaches `PATH` once `/bin/pkg` installs it.

## Testing

```
make test       # from the top of this repository
```

Each case boots `../braam-core`'s kernel under node and plants the built
`adventure.wasm` in the image.

- `test/play.mjs` runs `test/walkthrough.txt` on stdin with
  `ADVENTURE_SEED=36`: a complete game, 350 of 350 in 322 turns, with nothing
  spare — drop any one of its 329 commands and it ends short. It fails on a
  refused word, a death, a question that ate a command, twenty landmarks
  reached out of order, and on any difference from `test/game.log`.
- `test/interrupt.mjs` plays on the grid, because an interrupt is a property of
  the console: `^C` at the prompt must ask rather than die, a second must score
  and exit 0, and the shell reading a command afterwards proves the keyboard
  came back.
- `test/suspend.mjs` suspends a prefix twice to check the bytes match, resumes
  from it, and checks a file of `A`s is refused. The resume stops at the
  fifteen-minute gate — the harness clock is frozen — which is already proof
  that `restore()` read the file.
- `test/back.mjs` replays the walkthrough's opening with `back` after every
  command, which the walkthrough itself never types.

The run is deterministic, so the transcript checks are exact. When a change to
the game is meant, refresh the golden:

```
cp build/games/adventure/game.log games/adventure/test/game.log
```

The transcript is written to `build/games/adventure/game.log` pass or fail, and
the whole run takes about twenty milliseconds.

## Files

| | |
| --- | --- |
| `hdr.h` | `class Game`: the state, the methods, and every named number |
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
| `test/play.mjs` | the walkthrough, and the assertions on it |
| `test/interrupt.mjs` | `^C` at the prompt, on the grid |
| `test/suspend.mjs` | suspend to a file, and resume from it |
| `test/back.mjs` | `back`, which the walkthrough never types |
| `test/walkthrough.txt` | a whole game, 350 of 350, one command a line |
| `test/game.log`, `test/back.log` | what those print, to the byte |
