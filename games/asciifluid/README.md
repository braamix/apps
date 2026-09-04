# asciifluid — ASCII fluid dynamics

A smoothed-particle-hydrodynamics simulator that reads a text file and pours
it. Every non-space character becomes a particle, `#` becomes a wall, and a
marching-squares renderer draws the result on the terminal, one frame every
twelve milliseconds, for ever.

**This is IOCCC 2012/endoh1 by Yusuke Endoh**, the winner of "Most complex
ASCII fluid — Honorable mention". Twenty lines of `double complex` arithmetic
that are also, read another way, a picture of a fountain. The port is of
`endoh1.alt2.c`, upstream's own `indent(1)`ed rendering of `endoh1.c`; `-c` is
the density colouring of `endoh1_color.c`, which the author added at the
judges' request after the contest. The seventeen configurations under `data/`
are the entry's, from the author, the judges and HAMANO Tsukasa.

## Using it

```
asciifluid [-cG] [-g <n>] [-p <n>] [-v <n>] [-d <ms>] [<file>]
asciifluid -l
```

The package ships the configurations, and a name with no `/` that is not a file
is looked for among them, so there is nothing to unpack:

```
asciifluid -l              # clock column column2 ... tanada
asciifluid column          # a water column collapsing, the SPH demo
asciifluid pour-out        # pours you a cup of tea
asciifluid tanada          # terraced rice fields
asciifluid logo            # the entry's own logo, falling apart
asciifluid /home/mine.txt  # a path, or nothing, which is stdin
```

`^C` ends it; there is no other way out, and there was none upstream either.

Upstream fixes the three factors at compile time — `-DG=1 -DP=4 -DV=8` — and a
browser cannot recompile, so they are flags whose defaults are those numbers:
`-g` gravity, `-p` pressure, `-v` viscosity, `-d` the milliseconds between
frames. `-G` is upstream's documented `-DG=I`: gravity along the imaginary
axis, which is sideways. `-c` colours each cell by the density around it.

## What the port changed

**`<complex.h>` is gone, and was never going to be here.** A C complex multiply
or divide is a call to `__muldc3` / `__divdc3`, which are compiler-rt, and
nothing provides one — so `Cx` is two doubles with the four operators written
out. The division is the conjugate form, `(a·b̄)/|b|²`, because that is what
clang inlines a complex division to; Smith's method, which `__divdc3` uses in
the cold path, differs in the last bit and the simulation is chaotic enough to
notice.

**`cabs` is `sqrt(x² + y²)`** rather than a correctly rounded `hypot`. It is one
wasm instruction, and it is also the closer of the two: against a native build
of upstream, the port's frames are identical for forty on most configurations,
where musl's `hypot` parts company at five.

**Upstream's `a[97687]` is a heap block sized from the input.** The array was a
big number, not a bound — two particles a non-space character is what the
reader actually makes — and sizing it means the binary keeps the default four
pages of initial memory instead of carrying 1.6 MB of BSS.

**`usleep(12321)` is `co_await sleep_for(12)`.** There is no microsecond sleep,
and this is the only place the program can be interrupted: the physics has no
`co_await` in it, and a loop without one cannot be killed.

**`puts(o)` is `write_all(SYS_STDOUT, ...)` and the escapes are unchanged.**
Braam's grid parses `ESC [ 2 J` and `ESC [ 1 ; 1 H` on the way in
(`../braam-core/doc/ANSI_Escape_Codes.md` §4.3), so upstream's frame buffer —
the escape prefix, a byte a cell for the marching square's bits, then the
glyphs those bits choose — survives whole. `screen_claim(true)` is taken first,
which is what puts the shell's screen back: `^C` kills the process, and a
killed process runs no destructor of its own.

**The field is the terminal's**, not 79 × 23. `tty_of(SYS_STDOUT)` sizes it,
`sig_catch(SIG_WINCH)` and an `Err(Intr)` out of the sleep resize it, and on
the 80 × 24 Braam boots with the arithmetic is upstream's exactly.

**The frame is trimmed to the screen.** Upstream paints twenty-five rows and
homes the cursor, which is why its remarks say to make the window "larger than
80 x 25"; here the last row's newline is left off instead, so an 80 × 24 screen
does not scroll a row a frame.

**`step()` is `noinline` and not a coroutine.** It is two O(n²) sweeps, and
inlining it into the frame loop would move its locals into a heap-allocated
coroutine frame.

## Differences from upstream worth knowing

- **The frames are upstream's, until they are not.** `test/frames.log` came out
  of upstream's own `endoh1` binary, and the port reproduces it byte for byte.
  Further out the two part company: this is a chaotic simulation, and a
  last-bit difference in one `libm` becomes a different glyph a few frames
  later. `glass-half-empty` diverges at frame 6, `funnel3` at 38, and most of
  the rest not within the forty that were compared. A native C++ transcription
  of upstream diverges from upstream in the same way, so this is the
  simulation, not the port.
- **`-c` colours upstream's black-and-white simulation.** `endoh1_color.c` is a
  second program, not a flag, and it differs from `endoh1.c` in two incidental
  ways: it marks the bottom-right corner of each square with `|=` where the
  black-and-white one *assigns*, and it moves each particle before drawing it
  rather than after. Those change the picture, so they are not reproduced —
  what `-c` takes is the colouring: the density each cell's neighbours
  contribute, accumulated in a byte that wraps, compressed by
  `tanh(9 - |v| / 2) * 3 + 3` into three components twenty apart, and written
  as `ESC [ 48 ; 5 ; NNN m` over a 6 × 6 × 6 cube. The mapping was checked
  against the upstream binary: the set of colour indices it emits is exactly
  the set this produces.
- **Sixteen colours, not 256.** A Braam cell has `COLOR_BLACK`…`COLOR_WHITE`
  and `COLOR_BRIGHT`, and the grid quantises a `48;5;NNN` to the nearest of
  them, so the density banding is coarser than on an xterm. Nothing was done
  about it: the escape upstream writes is the escape that is written.
- **`-c` builds the escapes in a second buffer** rather than `sprintf`ing each
  cell's escape into the twelve bytes that held its accumulator, which is what
  upstream does and what makes its offsets what they are.
- **Reading stops at a NUL**, as `0 < (x = getc(stdin))` does upstream, so a
  binary file is read as far as its first zero byte.

## Files

| | |
| --- | --- |
| [asciifluid.cpp](asciifluid.cpp) | the port, as `endoh1.alt2.c` is one file |
| [CMakeLists.txt](CMakeLists.txt) | the program, and the package with `share/` |
| [data/](data/) | the seventeen configurations, upstream's `*.txt` |
| [test/frames.mjs](test/frames.mjs) | twelve frames of `column`, against the golden |
| [test/frames.log](test/frames.log) | the golden, from upstream's own binary |
| [test/colour.mjs](test/colour.mjs) | `-c`: the same glyphs, plus backgrounds |
| [test/colour.log](test/colour.log) | one frame, a glyph row and a colour row |
| [test/interrupt.mjs](test/interrupt.mjs) | `^C` ends it, 130, screen restored |

## Building and packaging

From the top of this repository:

```
make            # build/games/asciifluid/asciifluid.wasm
make package    # build/games/asciifluid/asciifluid-1.0-r0.zip
```

The package holds `.PKGINFO`, `bin/asciifluid` and `share/*.txt`.
`bin/` is what reaches `PATH` once `/bin/pkg` installs it; `share/` is nested,
so it is payload, and `/pkg/bin/asciifluid` is the symlink the program reads
back to find it. `ASCIIFLUID_PREFIX` overrides that, which is how a hand-built
tree and the tests find the configurations.

## Testing

```
make test       # among the rest
```

- **frames.mjs** boots the kernel, plants the binary and `column.txt`, and
  drives twelve frames by hand — `run(now)` advances the kernel's timer queue,
  which is what makes the sleep expire; it is `proc_now()` that is frozen in
  the harness, and nothing here reads it. It asserts that the fluid moved and
  then the whole screen, every frame, against `frames.log`.
- **colour.mjs** runs the same configuration with `-c` and asserts that the
  glyphs are still the black-and-white run's, that the backgrounds are painted
  at all, and then the frame against `colour.log` — a glyph row and a row of
  one hex digit a cell, since a Braam cell has sixteen colours.
- **interrupt.mjs** echoes a mark, starts the fluid, checks the mark is gone,
  presses `^C` between two frames — the program parks on its sleep, so unlike
  a compute-bound one there is a window — and asserts the shell reports 130 and
  the mark is back.

To refresh a golden after a deliberate change, run the case and copy
`build/games/asciifluid/frames.log` or `colour.log` over the one in `test/`.
