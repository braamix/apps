# asciiclock — ASCII demo clock

A full-screen digital clock (HH:MM, blinking colon) over one of nine
animated backgrounds — snow, starfields, plasma, Matrix rain, fire, a
wandering maze, Game of Life, or a rotating 3D point cloud — with an
optional wireframe cube foreground. `q`, `x`, or ESC quit; `f` toggles
the cube and picks a new background, as upstream does.

**This is Deybacsi's [asciiclock](https://github.com/Deybacsi/asciiclock)
(2019).** The effect tables, layer compositor and keys are upstream's;
Linux `ioctl`, ANSI spam and `time()` are a `ProcScreen` and
`clock_now()`.

Upstream published no LICENSE file; the header comment in the port names
the author and the GitHub URL.

## Using it

```
asciiclock
asciiclock -h
```

| key | |
| --- | --- |
| `q`, `x`, ESC | quit |
| `f` | toggle the 3D cube; re-randomizes the background |

`-h` or `--help` prints that and exits. `^C` is quit with status 0, like
cmatrix's `finish()`.

## What the port changed

**The terminal is a `ProcScreen`.** Upstream painted ANSI per cell each
frame; here `mergelayers()` fills `FINAL` and `paint_final_to_grid()`
writes `Cell`s with damage tracking, then `flush()` once a frame.

**Wall time is `clock_now()` and `civil()`.** There is no `time()` or
`localtime()`. The harness clock is fixed, so headless tests see a
stable HH:MM.

**The keyboard is a second task**, on `next_key()` and a 64-deep ring,
the same shape as cmatrix and asciiquarium. Resize is handled there.

**`srand`/`rand` are a POSIX LCG** with `ASCIICLOCK_SEED` in the
environment, like `CMATRIX_SEED`.

**Layer buffers are 80×80** (upstream used 200×200). Wasm initial memory
is twelve pages so the BSS fits; geometry still follows the terminal.

**The labyrinth maze is iterative.** Upstream recursed while carving and
blew the native stack here; generation uses an explicit stack.

**Dropped:** `stty`, `price.txt`, the analog stub, live maze-carve
`printscreen`, and arrow-key debug prints.

**Snow and starfield** use signed coordinates; unsigned wrap on
`rand() % 3 - 1` used to trap.

## Files

| | |
| --- | --- |
| [asciiclock.cpp](asciiclock.cpp) | `proc_main`, keyboard, time, seed |
| [screen.cpp](screen.cpp) | layers, merge, grid paint |
| [clock_digital.cpp](clock_digital.cpp), [clock_digits.h](clock_digits.h) | digital face |
| [3d.cpp](3d.cpp) | projection and rotation |
| `backg_*.cpp`, [foreg_cube.cpp](foreg_cube.cpp) | effects |
| [CMakeLists.txt](CMakeLists.txt) | program and package |
| [test/](test/) | four headless cases |

## Building and packaging

From the top of this repository:

```
make            # build/games/asciiclock/asciiclock.wasm
make package    # build/games/asciiclock/asciiclock-1.0-r0.zip
```

`LIBS braam::math` for plasma, fire and 3D; no `PORT`.

## Testing

```
make test       # among the rest
```

- **frames.mjs** — `ASCIICLOCK_SEED=1`, sixteen frames at 40 ms against
  `frames.log`.
- **colour.mjs** — non-black foreground and background cells.
- **keys.mjs** — `-h` prints the keys; `f` changes the picture; `q` exits
  zero.
- **interrupt.mjs** — `^C` between frames, status 0, shell restored.

Refresh the golden from `build/games/asciiclock/frames.log` after a
deliberate visual change.
