# cmatrix — a Matrix screensaver on the terminal

Streams of characters fall down the screen, a white head on a green tail,
every other column, for ever. `q` or `^C` ends it; `-s` treats any key as
the end, which is the screensaver.

**This is CMatrix 1.2a by Chris Allegretta**, the curses program from 1999
that LiteBSD vendors as `games/cmatrix`. The rain, the flags and the keys
are upstream's; ncurses is a `ProcScreen`.

## Using it

```
cmatrix [-abBfhlnosxV] [-u delay] [-C color]
```

| | |
| --- | --- |
| `-a` | asynchronous scroll: each column has its own speed |
| `-b` | bold on some characters |
| `-B` | bold on all of them |
| `-n` | no bold, the default |
| `-o` | old-style scrolling, a whole column shifted a row a frame |
| `-s` | screensaver: the first keystroke exits |
| `-l` | Linux console glyph range (no font is loaded) |
| `-x` | the same range, as if the xterm used `mtx.pcf` |
| `-f` | accepted and ignored; there is no `$TERM` to force |
| `-u` | delay 0–10, default 4; each step is that many tens of milliseconds |
| `-C` | colour: green, red, blue, white, yellow, cyan, magenta, black |
| `-V` | version and exit |
| `-h` | usage and exit |

| key | |
| --- | --- |
| `q` | quit |
| `a` | toggle asynchronous scroll |
| `b` / `B` / `n` | bold some, all, or none |
| `0`–`9` | the delay |
| `! @ # $ % ^ &` | red, green, yellow, blue, magenta, cyan, white |

`^C` is `q`: the process catches it and leaves with status 0, which is
`finish()` on `SIGINT`. The man page also names `)` for black; the program
never did.

## What the port changed

**ncurses is a `ProcScreen`, and the frame is cells rather than `addch`.**
Upstream already thinks in `(y, x, ch, colour, bold)`, which is a Braam
`Cell`, so each even column is written through and only the cells that
differ go out. `curs_set(0)` is `cursor_on = false`. Odd columns stay blank,
as they did: the rain only ever occupied `j += 2`.

**The keyboard is a second task.** `wgetch` with `timeout(0)` is a
non-blocking read, and `napms` is a sleep, and those cannot share a task
here: `proc_spawn` parks the keyboard on `next_key()` and feeds a 64-deep
ring the frame loop pops one key from — one `wgetch` a frame. It has to be
that task that sees a resize: `ProcScreen::resize()` is private and only
`next_key()` calls it.

**`srand(time(NULL))` is POSIX `rand`, and can be pinned.** There is no
`time()`. `proc_random()` seeds it, and `CMATRIX_SEED` in the environment
replaces that, the way adventure's `ADVENTURE_SEED` does. The generator is
POSIX.1 (`state = state * 1103515245 + 12345`), not whoever libc CMatrix
was linked against, so a seeded run is ours rather than a native binary's.

**`malloc` is zeroed.** Upstream reads `bold` on cells that have never been
a head, so a native run is not deterministic even with a fixed seed. The
port clears each block, which is what makes the golden frames hold.

**`system()`, `consolechars`, `setfont` and `putenv("TERM=linux")` are
gone.** `-l` and `-x` still select the high-byte glyph set (`randmin` 166,
`randnum` 51) and draw a head as codepoint 183 rather than `&`; they do not
load a font, because there is no console font and no `mtx.pcf` here. `-f`
is accepted and does nothing. The unreachable `putenv TERM=` restore at the
bottom of `main` is gone with them.

**`handle_sigwinch` is the resized flag.** There is no `ioctl(TIOCGWINSZ)`.
The keyboard task sets the flag, the frame loop reads `LINES`/`COLS` off
the grid and runs `var_init()` again, which is what the handler did.

**`finish()` is a status.** `q` and `^C` return 0; a killed process never
got that far. The claims going back with the process are what put the
shell's screen back.

**The pointer table is `sizeof(cmatrix *)` a row.** Upstream allocated
`sizeof(cmatrix) * (LINES + 1)` for an array of pointers, which happens to
be the same size on a 64-bit host and is not on wasm32. The rows are freed
on a resize; upstream leaked them.

**`napms(0)` still parks.** `-u 0` and the `0` key ask for no delay; a loop
with no `co_await` cannot be interrupted, so the sleep is 1 ms instead of
zero.

**A screen shorter than four rows does not divide by zero.**
`rand() % (LINES - 3)` is clamped to a span of 1. 80 × 24 is unchanged.

**`step_and_draw` is `noinline` and not a coroutine.** The column walk is
the hot path, and inlining it into the frame loop would move its locals
into a heap-allocated coroutine frame.

## Differences from upstream worth knowing

- **The frames are this port's, not a native `cmatrix`'s.** The rain is
  chaotic in the small, and the generator is POSIX.1 rather than glibc's.
  `test/frames.log` is a seeded run of this binary.
- **Sixteen colours, and bold is `ATTR_BOLD`.** A Braam cell has
  `COLOR_BLACK`…`COLOR_WHITE` and `COLOR_BRIGHT`; the pair number is the
  colour, as `init_pair(COLOR_GREEN, COLOR_GREEN, …)` already was, and
  `A_BOLD` is the attribute bit rather than a bright colour.
- **Default colours are black, not the terminal's.** `use_default_colors`
  painted on `-1`; here the background is `COLOR_BLACK`.
- **The man page's `)` for black was never in the program.** `!` through
  `&` are.

## Files

| | |
| --- | --- |
| [cmatrix.cpp](cmatrix.cpp) | the port, as `cmatrix.c` is one file |
| [CMakeLists.txt](CMakeLists.txt) | the program, and the package |
| [test/](test/) | four headless cases under the SDK's harness |

## Building and packaging

From the top of this repository:

```
make            # build/games/cmatrix/cmatrix.wasm
make package    # build/games/cmatrix/cmatrix-1.2a-r0.zip
```

No `PORT` and no `LIBS`: nothing here wants a C library, and there is no
payload — the package is `.PKGINFO` and one `bin/cmatrix`.

## Testing

```
make test       # among the rest
```

- **frames.mjs** boots the kernel, plants the binary, runs
  `CMATRIX_SEED=1 cmatrix` and drives sixteen frames — `run(now)` advances
  the kernel's timer queue, which is what expires the 40 ms sleep. It
  asserts the rain fell in even columns, that two frames differ, and then
  every frame against `frames.log`.
- **colour.mjs** reads the foregrounds a hex digit a cell: the tails are
  green, a head is white, and `!` paints red.
- **keys.mjs** checks `-V` and `-h` and a bad `-C` on stdout, then `q`
  exits zero with the shell's screen back, and `-s` leaves on a keystroke.
- **interrupt.mjs** echoes a mark, starts the rain, presses `^C` between
  two frames — it parks on its sleep, so there is a window — and asserts
  status 0 and the mark back.

To refresh the golden after a deliberate change, run the case and copy
`build/games/cmatrix/frames.log` over the one in `test/`.

## License

GPL-1.0-or-later, upstream's.
