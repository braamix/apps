# asciiquarium — an aquarium on the terminal

Fish swim across the screen at their own speeds and blow the occasional bubble,
seaweed sways on the bottom, a castle stands in the corner, and above the water
one event at a time crosses the surface: a ship, a whale that spouts, a sea
monster, a big fish, a shark with teeth that eat a small fish, a fishhook that
lowers and reels one up, a swan, three ducks, three dolphins. Each of them, on
leaving the screen, spawns the next.

**This is goquarium**, Serge Vakulenko's Go rewrite of Kirk Baucom's Perl
[asciiquarium](http://robobunny.com/projects/asciiquarium) by way of Mohammad
Abu Mattar's Python port. Upstream draws with `termbox-go`; here it is a
`ProcScreen`, and the rest is the same aquarium — the same sprites, the same
masks, the same dice, the same ten frames a second.

## Using it

```
asciiquarium [--classic]
asciiquarium --info
asciiquarium --version
```

| key | |
| --- | --- |
| `q` | quit |
| `p` | pause and unpause |
| `r` | reset: clear the scene and spawn it again |
| `i` | the info overlay, which pauses under it |
| `ESC` | close the overlay |

`--classic` draws from the eight classic fish alone; without it four in twelve
are the newer designs. `^C` also ends it, with 130, since nothing catches it —
upstream drops `Ctrl-C` silently and leaves `q` as the way out.

The screen must be at least 40 × 15, which is upstream's rule; below that it
says so and exits, at startup or on a resize.

## What the port changed

**`termbox` is a `ProcScreen`, and the frame is cells rather than escapes.**
Upstream already thinks in `(x, y, ch, fg)` with a per-character colour mask,
which is exactly a Braam `Cell`, so the frame is built in a shadow grid and only
the cells that differ are written through — the damage that follows is what
`flush()` sends. A paused frame is not repainted at all, so nothing goes out.

**The keyboard is a second task.** `key_read` blocks and there is no "a key or a
deadline" to await, so the clock and the keyboard cannot live in the same task:
`proc_spawn` puts the keyboard in one of its own, parked on `next_key()` and
feeding a 64-deep ring the frame loop drains — which is upstream's goroutine and
its `chan termbox.Event`. It has to be that task and not the sleeping one for a
second reason: `ProcScreen::resize()` is private and only `next_key()` calls it,
so the grid is reshaped where the keys arrive. `Err(Intr)` there is a resize with
no key behind it; the grid is already the new shape and the next frame is
repainted whole, because the kernel blanks its own screen on a resize and a
cell-by-cell diff would send nothing.

**`CallbackArgs any` is fields.** Upstream's callback arguments are an interface
holding either `[]float64{dx, dy, dz, frameStep}` or
`map[string]string{"mode": …}`, told apart by a type switch in `MoveEntity` and
in `FishhookCallback`. Here they are named fields and a `HookMode`; `HOOK_NONE`
is the arm that carries movement, and the other is the fishhook's.

**The closures are functions.** `AddFish`'s death callback captured
`classicMode`, which is now a field on the `Animation`; `GroupDeath`'s captured
a list of type names, which is now the one function that cleans up a hook's rig;
the dolphins' lead callback is `random_object` itself, which ignores the entity
it is handed.

**A frame is split into lines once**, at construction, rather than by
`strings.Split` on every entity on every frame — which upstream's `drawEntity`
does, and it is the hot path.

**Seaweed dies by frames, not by a wall clock.** Upstream's `DieTime` is an
absolute `time.Now().Add(8..12 minutes)` and is the only clock reading in the
whole simulation. `die_frame` already existed for the splat, so seaweed takes a
deadline in ticks — `(480 + rand(240)) × 10`, the same duration at upstream's own
ten frames a second. The program then reads no clock at all, which is what makes
a seeded run reproducible and testable: `proc_now()` is frozen under the test
harness.

**The dice are xorshift32, and can be pinned.** Upstream never seeds
`math/rand`, so a Go 1.22 run is different every time and there is no seed to
port. `proc_random()` seeds this one, and `ASCIIQUARIUM_SEED` in the environment
replaces it, the way adventure's `ADVENTURE_SEED` does.

**`GOQUARIUM_STDERR` and `debugf` are gone.** They reassign `os.Stderr` to an
opened file, which is not a thing here, and they are a debugging aid rather than
the program.

**`--version` names Braam** where upstream names the Go runtime and the host
architecture.

**The bottom row is drawn on.** Upstream keeps `height = h - 1` back to dodge a
terminal's bottom-row scroll; a cell written on the last row of a grid moves
nothing, so the aquarium is a row deeper here and the castle stands on the
bottom of the screen.

## Differences from upstream worth knowing

- **`w` is brighter than `W`.** termbox's `ColorWhite` is terminal colour 7 and
  its `ColorLightGray` is 15, so upstream's map — `'w'` to `ColorLightGray`,
  `'W'` to `ColorWhite` — makes lowercase the bright one, alone among `r`/`R`,
  `k`/`K` and the rest of the table. It is reproduced as written.
- **Lower depth is in front.** `depth.go`'s comment says the opposite; the code
  sorts descending by `Z` and paints in that order, so seaweed at 21 is drawn
  over the castle at 22. That is what you see, and it is what the port does.
- **A collision handler acts on the previous frame's list.** `animate()` runs
  every `Update` — which is what fires `CollHandler` — before
  `checkCollisions()`. The one-frame lag is visible, so it is kept.
- **A sprite's box is frame zero's**, computed once and never recomputed, so a
  multi-frame entity keeps the first frame's width and height for collisions and
  for dying off-screen however wide the later frames are.
- **The sprites are upstream's bytes.** They were lifted out of the Go sources by
  a parser over `go/ast` rather than by hand, and every literal in the port was
  compared against that dump before this was committed — trailing spaces on a
  big fish's fin included, since they are what its width is measured from.

## Files

| | |
| --- | --- |
| [asciiquarium.cpp](asciiquarium.cpp) | `main.go`, `cli.go`: the entry point and the flags |
| [animation.cpp](animation.cpp) | `animation.go`: the loop, the keyboard task, the draw |
| [entity.cpp](entity.cpp) | `entity.go`: the entity model and its lifecycle |
| [environment.cpp](environment.cpp) | `environment.go`: water, castle, seaweed |
| [fish.cpp](fish.cpp) | `fish.go`: the twelve fish designs, bubbles, the splat |
| [special.cpp](special.cpp) | `special.go`: everything that crosses the surface |
| [info.cpp](info.cpp) | `info.go`, `version.go`: the help text |
| [quarium.h](quarium.h) | `depth.go`, and what the other files share |
| [Entities.md](Entities.md) | every entity, with its spawn rule and its layer |
| [test/](test/) | five headless cases under `../braam-core`'s harness |

## Building and packaging

From the top of this repository:

```
make            # build/games/asciiquarium/asciiquarium.wasm
make package    # build/games/asciiquarium/asciiquarium-2.2.0-r1.zip
```

No `PORT` and no `LIBS`: nothing here wants a C library, the arithmetic is plain
`double`, and the sprites are compiled in — so the package is `.PKGINFO` and one
`bin/asciiquarium`, and there is nothing to unpack or find at run time.

## Testing

```
make test       # among the rest
```

- **frames.mjs** boots the kernel, plants the binary, runs
  `ASCIIQUARIUM_SEED=1 asciiquarium` and drives twelve frames — `run(now)`
  advances the kernel's timer queue, which is what expires the 100 ms sleep. It
  asserts the water bands and the castle standing on the bottom row, that no two
  frames are alike, and then every frame against `frames.log`.
- **colour.mjs** reads the foregrounds a hex digit a cell: the water band is
  cyan, the castle is dark grey with white and yellow through it, and a fish's
  mask is more than one colour, which is `randColor` having rewritten its digits.
- **keys.mjs** presses each key in turn — `p` freezes the frame and `p` releases
  it, `i` puts the overlay up and `ESC` takes it down, `r` respawns the scene,
  `q` exits zero with the shell's screen back. A key takes two frames to land:
  the first wakes the keyboard task and the second is the frame drawn after it.
- **resize.mjs** goes to 40 × 15 and back: the band re-tiles, the castle follows
  the corner, nothing is left below the new height, and 39 × 14 is refused with
  upstream's message — on the real stderr, since the claims are given back
  before it is written or it would die with the screen.
- **interrupt.mjs** echoes a mark, starts the aquarium, presses `^C` between two
  frames — it parks on its sleep, so there is a window — and asserts 130 and the
  mark back.

To refresh the golden after a deliberate change, run the case and copy
`build/games/asciiquarium/frames.log` over the one in `test/`.

## License

GPL-3.0-or-later, upstream's.
