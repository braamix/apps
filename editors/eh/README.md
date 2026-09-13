# eh — Edit Here

`vi(1)` the good parts version, by Anthony C Howe, ported to Braam from
v1.8.1 (`codeberg.org/SirWumpus/eh`). A full-screen modal editor descended
from *ae*, Anthony's Editor (1991), the canonical Buffer Gap worked example,
by way of an IOCCC 28 entry that still lives in upstream's single file behind
`#ifdef IOCCC`.

It is the smallest editor here by an order of magnitude — 2,678 lines, of which
upstream's own file is 2,147, against vi's, le's and uemacs' tens of thousands — and the reason is that its whole OS
surface was already in one place: Curses for the screen, ten calls for the
files, one function for the shell escape. There is no `setjmp`, no `stat`, no
`dirent`, no `termios`, no time, no stdio and **no floating point at all**, so
none of the control-flow surgery the other three needed arises.

What is left is the one thing this tree has now solved four different ways and
never this cleanly: **a blocking read reached from 24 of 96 functions, through
a dispatcher that calls itself.**

## Using it

    eh [file]

`h j k l` move, `i` inserts, `ESC` leaves insert, `W` writes, `Q` quits. The
full command list is upstream's [README](../../editors/eh/tmp/eh/README.md);
nothing was added or removed except as noted below.

## Structure

| file | what |
|---|---|
| `eh.cpp` | upstream's single file, the IOCCC arms stripped |
| `ehscreen.h`, `ehscreen.cpp` | the screen it paints, over the Grid |
| `getch.cpp` | the one place the process parks |
| `braam.h` | what the port kit has not got |

The editor stays one file because that is what it is. What moved out is only
what upstream did not have: the library it linked, and the seam where the
process blocks.

**The POSIX ERE was the third file and is not here any more.** It was written
for this port, because the kit had no `<regex.h>`; when `/bin/grep` wanted one
too it moved into the SDK as `braam::regex`, and `#include <regex.h>` is what
eh says now. It gained a BRE arm, `REG_ICASE`, `REG_NOSUB`, `regerror` and
back-references on the way, none of which eh asks for — except that `\1` in an
ERE is a back-reference now where it used to be a literal `1`. See
`../braam-core/doc/Programming_Manual.md` §6.

## What the port changed

### The screen

**Painting sends nothing.** The one flush happens in `getch()` just before the
process parks. That is what keeps `display()` and every painter an ordinary
function, so only the readers became coroutines.

**There is no curses left.** `ehscreen.{h,cpp}` began as `editors/le`'s shim
and spent a while emulating one, which meant carrying two pieces of state eh
does not want: a current position and a current attribute. It wants neither.
Every write in `display()` already names the cell it writes — `mvaddch(i, j,
…)`, `mvaddnstr(i, j, p, mbl)` — and every attribute it sets is per-cell,
`standend()` in the loop's own condition clearing the last one. So the writers
take `(y, x, attr)` and the emulation goes: with it went twelve functions that
were `return OK;` and nothing else (`endwin`, `cbreak`, `noecho`, `echo`,
`nonl`, `nl`, `raw`, `noraw`, `keypad`, `delwin`, `refresh`, `beep`), five more
that no longer had a caller, and `WINDOW`, `stdscr`, `chtype`, the `A_*` word,
`move`, `getcurx` and `printw`. What is left is five painters over the Grid,
`eh_open`, `eh_flush`, and `beep()`, which stays a no-op because this system
has no bell.

The names are `eh_*` and the file is not `screen.cpp`: `kernel/screen.h` —
unavoidable, it is where `Cell` and `ATTR_REVERSE` come from — declares
`screen_flush()`, `screen_put()`, `screen_cursor()` and `screen_clear()` of its
own, and a `screen_flush()` returning a `Task` is a redeclaration conflict
rather than an overload.

`eh_put()` writes one cell per **rune**, because `display()` hands it a whole
multibyte sequence and expects a single column, and it returns the column after
the last — which is what `getcurx()` was for. LF and CR draw nothing:
`display()` does *not* route a plain newline through its control arm, so the
writer is handed one for every line on the screen and has to ignore it.

A TAB blanks to the next eight-column stop, which is what curses drew; upstream
expands tabs itself and only relies on curses not to move the cursor. **The
stop is clipped to the right edge now, and that fixes a hang**: the column used
to clamp at `COLS-1` while the stop did not, so a tab starting anywhere in
columns 72–79 of an 80-column screen never reached it and `display()` spun for
ever. Ten leading tabs were enough. Upstream's cases never get past column 56,
which is why the suite never saw it; `test/ehtab.mjs` does.

`initscr()` is `eh_open()`, a `Task`, so `proc_main` awaits it and checks
`is_err()` where upstream checked for null — which is still exit status 1.

Two Grid traps are handled and must stay handled: a cursor-only move damages no
cell, so the cell under the cursor is damaged deliberately in `eh_flush()` or
`h`/`j`/`k`/`l` would move nothing; and after a resize the **whole** frame is
blitted. A third is not a trap but pays for itself — the one cell writer drops
a write that would not change the cell, because `Grid::damage` is a single
bounding rect and `eh_erase()` touches every cell of every frame.

`timeout(100)` and the `case ERR:` arm went with them: there is no non-blocking
key read here, so the paste-coalescing heuristic had nothing to measure. **The
`input`/`pause` counters had to go too** — the `ERR` arm was the only thing
that reset `input`, so deleting one and keeping the other freezes the screen
after the third keystroke of every insert.

### Where it blocks

`getch()` and `mvgetnstr()` are the only two. Closing the caller relation over
them gives 24 functions, which are `Task`s; the other 72 — the whole gap
buffer, all of undo/redo, `display()` and every pure motion — are untouched.

`cmds[]` gained a second column rather than changing type:

```cpp
struct binding {
    int key;
    void (*func)(void);       // exactly one of these two is non-null
    Task<void> (*task)(void);
};
```

uEmacs converted its whole dispatch table at once; here that would put a heap
frame behind every `l`, and only 23 of the 71 entries block. **The order is
untouched** — it is the capability model, `MOTION_CMDS` and `MOST_CMDS` index
into it — and was diffed against upstream's to prove it.

`getcmd`'s repeat loop awaits, and *a `co_await` is a call and not a tail
call*: `999999dd` is four awaits a turn and would grow the native stack until
it trapped. `cmd_yield()` suspends every sixteen turns, uEmacs' `exec_yield()`
with a shorter period because the chain is deeper. It does not make a long
command interruptible — eh holds the raw keys, so a `^C` is a keystroke and
only `getch()` reads keystrokes. Upstream's `raw()` had the same property.

`getcmd`'s `static int this_cmd` needed no change: a block-scope static is not
part of a coroutine frame, and with one task tree the nesting is still LIFO.

### Keys

There are no control characters in a Braam key — `^C` is `'c'` with
`MOD_CTRL` — so `getch.cpp` folds them back. The named keys pass through as
themselves: they sit above the Unicode range, which is exactly what eh's own
`255 < ch` guard tests.

`getch()` returns **bytes, not codepoints**. `insert()` reads a multibyte
character one byte at a time and the gap buffer is byte-oriented under it, so a
printable key is encoded with `wctomb` and its tail pushed back.

**`KEY_BTAB` is gone, and that fixes an upstream bug.** Its `case KEY_BTAB:`
sits *below* `default:`'s `255 < ch` guard, so a real one reached `mblength()`
with a value that function's own assert forbids — and `-DNDEBUG`, which is the
shipping build, turns that into four garbage bytes in the buffer. Shift-Tab
decodes to a tab here and the arm is unreachable, which is what it was for.

`^Z` no longer raises `SIGTSTP`: there is no job control to stop for.

### Files

`open`/`creat`/`read`/`write`/`close` are `co_await b_*` over `compat/cio.h`.
`filewrite`'s partial-write loop and its `errno` handling are upstream's.

Its one error message reads differently: `strerror` answers `"ENOENT"` here and
not prose, so it is `error_name(error_of(errno))`, as vi's `syserror()` and
le's `FError()` are.

`fileread` takes a pointer *into the gap* — every caller passes `gap` — and
`growgap()` reallocs it. `fn` is therefore dead after the open, and must stay
dead: a copy would be a `PATH_MAX` array in a coroutine frame.

### `!`, the shell filter

Upstream forked and drove the child through two pipes. There is no fork, and
one task cannot park on both ends of a pipeline, so the region goes out through
a temp file and the output comes back through another — vi's `filter()`, which
is what vi always was.

`ChildIo` *moves* a descriptor out of this process's table, so stdout and
stderr need two descriptors onto the same file; that is upstream's third
`dup2`, and it is what puts `!!WOOT`'s "not found" into the buffer. The claims
are not handed over: all three of the child's streams are files, so it can
neither read the keyboard nor write the screen.

`!!cmd` and `!motion cmd` needed no further change. `deld` → `yanky` →
`getcmd(MOTION_CMDS)` reads the next key; for `!!` that key is `!`, which sits
outside the motion range, so nothing runs, the region is empty, and the child's
output is inserted.

`getenv("SHELL")` gained the NULL check upstream is missing — it called
`strrchr` on it unconditionally.

### Unicode

There is no `<uchar.h>`, but `wchar_t` **is** UTF-32 here, so `mbrtoc32` and
`c32rtomb` are `mbrtowc` and `wcrtomb` under another name (`braam.h`). Same
semantics, including the `(size_t)-2` incomplete return that `insert()` relies
on. `setlocale` is gone; this system is UTF-8 throughout.

**`charwidth()` no longer returns `wcwidth()`'s answer.** The kit's `wcwidth`
is Markus Kuhn's and reports 2 for East Asian Wide; the grid is one `Cell` per
rune, so a wide character occupies one column here and a 2 would drift the
cursor along a CJK line.

**`display()`'s printability test is `printable()`, not `0 < wcwidth(wc)`.**
Kuhn's function answers 1 for a surrogate and for a noncharacter; NetBSD's
answers -1, and upstream's goldens draw those as the invalid-byte `~`. The
width itself is still `wcwidth()`'s — the two questions were only ever spelled
the same. That was `word0`–`word2` and `del14`.

Case is the kit's and stops at Greek, so `~` on a Greek Extended letter was a
no-op: `iswalpha(U+1F0F)` was false because `rune_lower`/`rune_upper` had no
mapping for the block. Fixed in `braam-core`; `flip2` passes from the first SDK
that carries it.

### Three more upstream assumptions that do not hold here

`off_t` is 64 bits and `long` is 32, so the status line's `%ld` read four bytes
of an eight-byte vararg and shifted everything after it. Two sites take a
`(long)` cast; under a 16 MB process cap the buffer cannot approach 2 GiB.

**`off_t` is 64 bits and `size_t` is 32**, which is the other half of the same
assumption and the one that hurt. `undo_redo()` undoes an insert with
`adjmarks(-obj->size)`, and `-obj->size` is an *unsigned* negation: on LP64 it
is the same width as `off_t` and wraps to the right answer, here it widens by
zero extension, so undoing a ten-byte paste moved every mark past the gap
4294967286 forward instead of ten back. The mark then looked plausible — its
low 32 bits were exactly right — and only `` `a `` after a `u` went looking for
it, four gigabytes past EOF, where `line_number()` walked off the end of
memory. One `(off_t)` cast. That was `mark4` and `mark5`.

`row_start()` clips an offset past the end of the buffer, the way `bol()`
already clips a negative one and for the same reason: the walk never terminates
otherwise, because `nextch()` stops advancing at EOF while `ptr()` stays inside
the buffer. Upstream asserted it instead, and the assert is compiled out of a
release build. The clip is what turned that stale mark into a wrong frame
rather than a hang, and it stays.

## Building and packaging

    make                # from the top of the tree
    make package

`PORT` for the C library, `NOFLOAT` because the only formats are
`%s %d %ld %lu %c %X %.*s`, and **`-funsigned-char`, which is not optional**:
`mblength()` asserts `0 <= ch < 256` and `charwidth()` compares
`127 < *s && *s < 194`.

## Testing

    node test/ehcases.mjs          # 121 of upstream's own cases
    node test/ehreplace.mjs        # $n backreferences, which upstream never reaches
    node test/ehtab.mjs            # a TAB in the last eight columns of a row

The engine's own suite went with it: `../braam-core/test/unit/test_regex.cpp`,
which replays the cross product the differential harness here used to drive.

Upstream drives 138 cases as `printf 'keys' | eh file >a.out` from a 1400-line
make file, asserting two goldens each: a terminfo escape trace and the file the
run wrote. Braam emits no escapes, so the traces cannot be diffed — but they
**replay**. `test/extract.py` pulls the keystroke scripts out of upstream's
Makefile, copies its file goldens byte for byte, and replays each trace into a
24×80 character image plus a reverse-video mask.

That replay is checked, not trusted. Upstream ships every case twice, from
NetBSD Curses and from ncurses, whose escape streams differ substantially; a
case is only written out when **both replay to the same image**. 116 of 132
do. The rest are genuine differences between the two libraries — `imb0` draws
an invalid byte as `~~` in one and as blanks in the other — and have no single
right answer to assert.

Nothing in `test/golden/` was written by looking at what this port draws.

**`extract.py` expands make's variables, and that matters more than it sounds.**
A recipe's `$$` is a literal dollar, but a bare `$` names a variable and an
undefined one expands to nothing — so `replace4`'s `printf 'jjw/$/plugh!/a'`
runs an *empty* pattern, not `$`, and `replace1`'s `/butt/$0hole` runs
`/butt/hole`. Upstream's own goldens record exactly that. Reading those two
scripts literally is what made them look like port bugs.

It leaves one real gap: **`$0`..`$9` are typed by no case upstream has**, because
make eats every one of them. `test/ehreplace.mjs` covers that separately —
`replace_match()` is upstream's code carried unchanged, and nothing else would
notice if the port broke it.

**The mask of four cases is not asserted.** `textterm` has `smso` and `rmso`
and no `rev`, so a trace cannot record the `A_REVERSE` `display()` draws for a
non-printable rune: the golden's mask is blank where the port's is set.
`extract.py` names those cases (`word0`–`word2`, `del14`) and marks them
`"mask": false`; their text half is asserted as usual.

## Known differences

One of the 121 cases still differs. It is listed in `test/ehcases.mjs` with its
reason rather than dropped, so a regression anywhere else still fails the run
and this one starting to pass is reported too. The golden is upstream's:

- `flip2` — `~` on U+1F0F. The SDK's `rune_lower`/`rune_upper` stopped at
  Greek, so `iswalpha()` said a Greek Extended letter has no case. Fixed in
  `braam-core`; this passes from the first SDK that carries it.

Skipped, with the reason recorded in `test/extract.py`:

- `term0` — there is no terminal type here.
- `bang6`, `bang7`, `select5` — they pipe through `fmt(1)`, which Braam has
  not got.
- `mark0`, `mark1` — they run the editor twice in one case.
- `bang3` — it asserts the output of upstream's own `ls(1)` over the two
  directories of its test tree, and Braam's `ls` marks a directory with a
  trailing `/`. `bang1`, `bang2`, `bang4` and `bang5` cover the filter.
- `write1` — upstream skips it too (NetBSD lib/58151, a kill character over a
  pipe), and its golden predates the `L%lu` status line: it asserts a percent
  field this eh has not had since before 1.8.1.

`empty2`, which upstream disabled over a tty-versus-pipe backspace difference,
is **live again**: keys are keys here, and `mvgetnstr` is ours, so `^U` works
where upstream noted NetBSD's did not.

## Licence

`LICENSE.md` is upstream's, carried verbatim: 2-clause BSD plus a clause
forbidding use of the source as machine-learning training data. Clause 1
permits redistribution in source form provided the notice and the conditions
travel with it, which is why the file is here.
