# eh — Edit Here

`vi(1)` the good parts version, by Anthony C Howe, ported to Braam from
v1.8.1 (`codeberg.org/SirWumpus/eh`). A full-screen modal editor descended
from *ae*, Anthony's Editor (1991), the canonical Buffer Gap worked example.

It is the smallest editor in this tree, and the reason is that its whole OS
surface sits in one place: the screen, ten calls for the files, one function
for the shell escape. There is no `setjmp`, no `stat`, no `dirent`, no
`termios`, no time, no stdio and **no floating point at all**.

The one hard part is that a blocking read is reached from a quarter of the
program, through a dispatcher that calls itself.

## Using it

    eh [file]

`h j k l` move, `i` inserts, `ESC` leaves insert, `W` writes, `Q` quits.
The full command list is upstream's.

## Structure

| file | what |
|---|---|
| `eh.cpp` | upstream's single file, the IOCCC arms stripped |
| `output.cpp` | the screen, over the Grid |
| `input.cpp` | the keyboard, and the one place the process parks |
| `globals.h` | what those three share |

The editor stays one file because that is what it is. Outside it are only the
two things upstream got from its libraries: the screen and the keyboard.

`assert()` is one `__builtin_trap()` at the head of `eh.cpp`, since the kit has
no `<assert.h>` and there is nothing to print to at that point. `<regex.h>` is
the SDK's `braam::regex`. Each file includes the standard headers it uses.

## The screen

**Painting sends nothing.** The five painters write cells into a `Grid`; the
one flush is in `getch()`, just before the process parks. That is what lets
`display()` and every other painter be an ordinary function, so only the
readers are coroutines.

A `Cell` carries `fg`, `bg` and `attrs` as fields, so cursor addressing is
indexing and there are no escape sequences anywhere. Every writer takes the
cell and the attribute it writes: there is no current position and no current
attribute to keep in step.

```cpp
void eh_erase();                                          // the whole screen
void eh_fill(int y, int x, u8 attr);                      // a row's tail
int  eh_put(int y, int x, const char *s, int n, u8 attr); // returns next column
void eh_put_rune(int y, int x, char32_t ch, u8 attr);
void eh_cursor(int y, int x);                             // once a frame
```

The names are `eh_*` because `kernel/screen.h` — where `Cell` and
`ATTR_REVERSE` come from — has a `screen_flush()`, `screen_put()`,
`screen_cursor()` and `screen_clear()` of its own.

`eh_put()` takes UTF-8 and writes one cell per **rune**, because `display()`
hands it a whole multibyte sequence and expects a single column. It returns the
column after the last, which is how the status line knows where its fill
starts. Three details it owes to `display()`:

- **LF and CR draw nothing.** A plain newline does not take `display()`'s
  control-character arm, so the writer is handed one at the end of every line
  and has to ignore it.
- **A TAB blanks to the next eight-column stop**, clipped to the right edge.
  `display()` advances its own column by the same amount.
- **A byte that starts no sequence draws as a reverse-video `~`**, one cell per
  byte.

`eh_open()` takes the keys and the screen; it is a `Task`, so `proc_main`
awaits it and exits 1 on failure. `beep()` is a no-op, because this system has
no bell.

Three Grid rules the flush depends on:

- A cursor-only move damages no cell, and a blit with no damage is never sent,
  so `eh_flush()` damages the cell under the cursor itself. Every `h`, `j`, `k`
  and `l` rides on it.
- The kernel blanks its own screen on a resize but leaves the Grid alone, so the
  whole frame is blitted after one, and after a flush the kernel refused.
- `Grid::damage` is a single bounding rect and `eh_erase()` touches every cell
  of every frame, so the one cell writer drops a write that would not change the
  cell. That is what keeps a frame down to what moved.

## Where it blocks

`getch()` and `mvgetnstr()` are the only two, and both are in `input.cpp`.
Closing the caller relation over them gives 27 functions, which are `Task`s;
everything else — the gap buffer, undo and redo, `display()` and every
motion — is an ordinary function.

`cmds[]` has two handler columns rather than one `Task` column:

```cpp
struct binding {
    int key;
    void (*func)(void);       // exactly one of these two is non-null
    Task<void> (*task)(void);
};
```

48 of its 71 entries are plain functions and 23 block. A single `Task` column
would put a heap frame behind every `l`. The order is upstream's, because the
capability model indexes into the table with `MOTION_CMDS` and `MOST_CMDS`.

`getcmd`'s repeat loop awaits, and *a `co_await` is a call and not a tail
call*, so `999999dd` would grow the native stack until it trapped.
`cmd_yield()` suspends every sixteen turns to give it back. This does not make
a long command interruptible: eh holds the raw keys, so a `^C` is a keystroke
and only `getch()` reads keystrokes.

## Keys

A Braam key carries no control characters — `^C` is `'c'` with `MOD_CTRL` —
so `input.cpp` folds them back into the bytes eh expects. Named keys pass
through as themselves: they sit above the Unicode range, which is what eh's
own `255 < ch` guard tests. Shift-Tab decodes to a tab.

`getch()` returns **bytes, not codepoints**: `insert()` reads a multibyte
character one byte at a time and the gap buffer is byte-oriented under it. A
printable key is encoded with `wctomb`, the first byte returned and the tail
pushed back.

`ungetch()` is a 1024-deep stack, deep enough for a whole line: upstream builds
a dozen commands out of it (`openo()` is `ungetstr("$a\n\033hi")`) and
`prompt()` primes a filename through it.

`mvgetnstr()` is a cooked line editor of about fifty lines, because there is no
line discipline here. It echoes the field itself, handles erase and `^U`, and
answers `ERR` on `ESC` or `^C`.

## Files

`open`/`creat`/`read`/`write`/`close` are `co_await b_*` over `compat/cio.h`.
`filewrite`'s partial-write loop and its `errno` handling are upstream's.

Its error message is `error_name(error_of(errno))`, not `strerror`, which here
answers `"ENOENT"` rather than prose.

`fileread` takes a pointer *into the gap* — every caller passes `gap` — and
`growgap()` reallocs it, so `fn` is dead after the open and must stay dead: a
copy would be a `PATH_MAX` array in a coroutine frame.

## `!`, the shell filter

There is no fork, and one task cannot park on both ends of a pipeline, so the
region goes out through a temp file and the output comes back through another.

`ChildIo` *moves* a descriptor out of this process's table, so stdout and
stderr need two descriptors onto the same file; that is what puts `!!WOOT`'s
"not found" into the buffer. The claims are not handed over: all three of the
child's streams are files, so the child can neither read the keyboard nor write
the screen.

## Unicode

`wchar_t` **is** UTF-32 here and there is no `<uchar.h>`, so upstream's
`mbrtoc32` and `c32rtomb` are spelled `mbrtowc` and `wcrtomb` at their six call
sites. Same semantics, including the `(size_t)-2` incomplete return that
`insert()` relies on. This system is UTF-8 throughout and has no locale.

`charwidth()` answers 1 for a wide character rather than `wcwidth()`'s 2: the
grid is one `Cell` per rune, so a 2 would drift the cursor along a CJK line.

`display()`'s printability test is `printable()`, not `0 < wcwidth(wc)`. The
kit's `wcwidth` is Markus Kuhn's, which answers 1 for a surrogate and for a
noncharacter; those draw as the invalid-byte `~`. The width itself is still
`wcwidth()`'s.

## Type widths

`off_t` is 64 bits while `long` and `size_t` are 32, which upstream's code does
not assume anywhere it matters except two places:

- the status line casts to `(long)`, or `%ld` would read four bytes of an
  eight-byte vararg and shift every field after it;
- `undo_redo()` casts to `(off_t)` before negating, or an unsigned negation
  widens by zero extension and moves every mark four gigabytes forward.

`row_start()` clips an offset past the end of the buffer, the way `bol()` clips
a negative one: `nextch()` stops advancing at EOF while `ptr()` stays inside
the buffer, so the walk would not terminate.

## Building and packaging

    make                # from the top of the tree
    make package

`PORT` for the C library, `NOFLOAT` because the only formats are
`%s %d %ld %lu %c %X %.*s`, and **`-funsigned-char`, which is not optional**:
`mblength()` asserts `0 <= ch < 256` and `charwidth()` compares
`127 < *s && *s < 194`.

## Testing

    node test/ehcases.mjs          # 121 of upstream's own cases
    node test/ehreplace.mjs        # $n backreferences, which upstream never has
    node test/ehtab.mjs            # a TAB in the last eight columns of a row

All of them pass. Each case boots the kernel, plants the binary, types a script
one key at a time, and asserts a 24×80 character image plus a reverse-video
mask, the file the run wrote, or both.

**Nothing in `test/golden/` was written by looking at what this port draws.**
The goldens are upstream's own, which it records as terminfo escape traces;
`test/extract.py` pulls the scripts out of its 1400-line test Makefile and
replays each trace into an image. Upstream ships every case twice, from NetBSD
Curses and from ncurses, and a case is only written out when both replay to the
same image — 116 of 132 do.

Four cases assert their text half only (`word0`–`word2`, `del14`): `textterm`
has `smso` and `rmso` but no `rev`, so the trace cannot record the reverse
video `display()` draws for a non-printable rune.

Not covered, each with its reason in `test/extract.py`: `term0` (no terminal
type here), `bang6`, `bang7`, `select5` (they pipe through `fmt(1)`, which
Braam has not got), `mark0`, `mark1` (two editor runs in one case), `bang3` (it
asserts upstream's `ls(1)` output, and Braam's `ls` marks a directory with a
trailing `/`), and `write1` (upstream skips it too, and its golden predates the
`L%lu` status line). `empty2`, which upstream disables over a tty-versus-pipe
backspace difference, runs here: keys are keys, and `mvgetnstr` is ours.

## Licence

`LICENSE.md` is upstream's, carried verbatim: 2-clause BSD plus a clause
forbidding use of the source as machine-learning training data. Clause 1
permits redistribution in source form provided the notice and the conditions
travel with it, which is why the file is here.
