# Visual entity catalog

Every entity the aquarium renders, grouped into classes, with its spawn path,
its motion arguments, its colour mask, its layer and its lifecycle. Upstream's
`Entities.md`, brought across with the port's names beside goquarium's and the
two places it disagreed with the code put right.

The scene is built by `setup_aquarium` (`animation.cpp`, upstream's `setup.go`):
environment, castle, seaweed, fish, then one random special. Specials chain from
there — nearly every one's death callback is `random_object`, so there is one
crossing the surface at a time and it starts the next as it leaves.

An item is an entity if it is made by `Animation::new_entity` and drawn by the
loop, composite helper parts — hit points and linked pieces — included.

---

## Class 1: environment and scenery

### the four water bands

- **Spawned in:** `add_environment` (`environment.cpp`), `AddEnvironment`
- **Type:** `ET_WATERLINE`
- **Form:** one frame each, a 33-character `~`/`^` segment tiled across the
  screen, at rows 5 to 8
- **Colour:** `COLOR_CYAN`, no mask
- **Motion:** static
- **Layer:** `DEPTH_WATER_LINE0`…`LINE3`, by band, falling back to `LINE0`
- **Life:** as long as the screen is that shape; a resize rebuilds them
- **Interactions:** `physical`, which is what pops a bubble at the surface

### the castle

- **Spawned in:** `add_castle` (`environment.cpp`), `AddCastle`
- **Type:** `ET_CASTLE`
- **Form:** one static thirteen-row sprite at `(width - 32, height - 13)`
- **Colour:** an explicit mask of `R`, `W`, `y` and `w` over a default of
  **dark grey** — `COLOR_BLACK | COLOR_BRIGHT`. Upstream's catalogue said
  `BLACK`; the code says `DARK_GREY`
- **Motion:** static
- **Layer:** `DEPTH_CASTLE` (22), the furthest back
- **Life:** rebuilt on a resize
- **Interactions:** decoration. Seaweed at 21 is drawn in front of it

### seaweed

- **Spawned in:** `add_seaweed` and `add_all_seaweed` (`environment.cpp`)
- **Type:** `ET_SEAWEED`
- **Form:** two alternating frames of `(` and `)`, a random three to six tall,
  one plant every fifteen columns
- **Colour:** `COLOR_GREEN`, no mask
- **Motion:** frames only, `frame_step` between 0.25 and 0.30
- **Layer:** `DEPTH_SEAWEED` (21)
- **Life:** `die_frame` of `(480 + rand(240)) × 10` ticks — eight to twelve
  minutes at ten frames a second, which is upstream's `DieTime` with the clock
  taken out of it. Its death callback is `add_seaweed`, so it replaces itself
- **Interactions:** decoration

---

## Class 2: the fish and what they leave behind

### fish

- **Spawned in:** `add_fish` and `add_all_fish` (`fish.cpp`)
- **Type:** `ET_FISH`
- **Form:** one frame, a directional sprite out of `OLD_FISH` (eight designs) or
  `NEW_FISH` (four). `--classic`, or eight draws in twelve, take the old set
- **Colour:** the design's directional mask, its digits rewritten by
  `rand_color`
- **Motion:** `fish_callback`, then `dx` of ±0.25 to ±2.0
- **Layer:** a random `DEPTH_FISH_START`…`DEPTH_FISH_END` (3 to 20)
- **Life:** `die_offscreen`; its death callback is `add_fish`, so the population
  holds
- **Interactions:** `physical`; `fish_collision` answers shark teeth and the
  hook point

### bubble

- **Spawned in:** `add_bubble` (`fish.cpp`), from `fish_callback` on three draws
  in a hundred
- **Type:** `ET_BUBBLE`
- **Form:** five frames — `.`, `o`, `O`, `O`, `O` — from the fish's mouth side
- **Colour:** `COLOR_CYAN`
- **Motion:** `dy = -1` a frame, `frame_step` 0.1, so it is usually still a `.`
  when it reaches the surface
- **Layer:** the fish's, minus one
- **Life:** `die_offscreen`
- **Interactions:** `physical`; `bubble_collision` kills it on a waterline

### splat

- **Spawned in:** `add_splat` (`fish.cpp`), from `fish_collision`
- **Type:** none
- **Form:** four frames of a burst, at the bite less four columns and two rows
- **Colour:** `COLOR_RED`
- **Motion:** none; `frame_step` 0.25
- **Layer:** the teeth's, minus two
- **Life:** `die_frame` 15
- **Interactions:** made when teeth reach a fish five rows tall or less

---

## Class 3: the specials

Each is `die_offscreen` with `random_object` as its death callback unless the
row says otherwise, and each picks its direction and its side with one draw.

| entity | spawned in | frames | motion | layer | colour |
| --- | --- | --- | --- | --- | --- |
| ship | `add_ship` | 1, directional | `dx` ±1, no frame step | `DEPTH_WATER_GAP1` | sail and hull mask over white |
| whale | `add_whale` | 12 built at run time: five idle, then seven spouts indented to the blowhole | `dx` ±0.5, `frame_step` 1 | `DEPTH_WATER_GAP2` | one mask reused across the twelve |
| monster, new | `add_monster` → `add_new_monster` | 2 | `dx` ±2, `frame_step` 0.25 | `DEPTH_WATER_GAP2` | eye highlight, over green |
| monster, old | `add_monster` → `add_old_monster` | 4 | `dx` ±2, `frame_step` 0.25 | `DEPTH_WATER_GAP2` | eye highlight, over green |
| big fish 1 | `add_big_fish` → `add_big_fish1`, one draw in three | 1, directional | `dx` ±3 | `DEPTH_SHARK` | a big mask through `rand_color`, over yellow |
| big fish 2 | `add_big_fish` → `add_big_fish2`, two draws in three | 1, directional | `dx` ±2.5 | `DEPTH_SHARK` | the same, over yellow |
| swan | `add_swan` | 1, directional | `dx` ±1, `frame_step` 0.25 | `DEPTH_WATER_GAP3` | accent mask over white |
| ducks | `add_ducks` | 3, one mask | `dx` ±1, `frame_step` 0.25 | `DEPTH_WATER_GAP3` | one mask for the three frames |
| dolphins | `add_dolphins`, three of them fifteen apart | 2 each | `dx` ±2, `frame_step` 0.5 | `DEPTH_WATER_GAP3` | **blue, magenta and cyan** by position — upstream's catalogue said blue, blue, cyan |

The dolphins are the one formation: they are `ET_DOLPHIN`, their callback is
`dolphin_delay_offscreen_death`, and `die_offscreen` is armed only once a
dolphin has overlapped the screen, so one starting fully outside is not removed
before it enters. Only the lead has a death callback, or three would chain.

---

## Class 4: the composites

### shark and teeth

- **Spawned in:** `add_shark` (`special.cpp`), as two entities
- **shark:** `ET_SHARK`, one directional frame, `dx` ±2, `DEPTH_SHARK`, cyan
  under its mask, `die_offscreen`, and `shark_death` for a callback
- **teeth:** `ET_TEETH`, a single `*`, the same speed, `DEPTH_SHARK + 1`,
  `physical`, no mask, and removed by `shark_death` — which then calls
  `random_object`
- **y:** 9, or `9 + rand(height - 19)` on a screen taller than nineteen rows,
  which keeps sharks in the deeper water

### fishline, fishhook and hook point

All three carry `fishhook_callback` and a `HookMode`, which is upstream's
`map[string]string{"mode": …}` written as an enum.

- **fishline:** `ET_FISHLINE`, `"|\n"` fifty times, at `x + 7`,
  `DEPTH_WATER_LINE1`
- **fishhook:** `ET_FISHHOOK`, a six-row hook, green, `DEPTH_WATER_LINE1`. Its
  death callback removes the hook point and the line and calls `random_object`
- **hook point:** `ET_HOOK_POINT`, a four-row marker, green, `physical`,
  `DEPTH_SHARK + 1` — this is the part a fish collides with

`lowering` moves the hook down two rows a frame to `int(height × 0.75)`;
`hooked` reels it up two a frame to `-10`. The line and the point do not move
themselves: each frame they find the hook, work out where it is about to be, and
snap to fifty above it and two below it. `retract` is what flips an entity to
`hooked`: it clears `physical`, and a caught fish also moves to
`DEPTH_WATER_GAP2` and takes `fishhook_callback` for its own.

---

## Appendix A: the layers

From `quarium.h`, upstream's `depth.go`:

- `DEPTH_GUI_TEXT` 0, `DEPTH_GUI` 1
- `DEPTH_SHARK` 2
- `DEPTH_FISH_START` 3 to `DEPTH_FISH_END` 20
- `DEPTH_SEAWEED` 21, `DEPTH_CASTLE` 22
- the bands and the gaps between them: `WATER_LINE3` 2, `WATER_GAP3` 3,
  `WATER_LINE2` 4, `WATER_GAP2` 5, `WATER_LINE1` 6, `WATER_GAP1` 7,
  `WATER_LINE0` 8, `WATER_GAP0` 9

The list is kept ascending by `z` and `draw_frame` walks it backwards, so the
highest `z` is painted first and **the lowest ends up in front**. `depth.go`'s
comment says the opposite; the code is what to trust.

## Appendix B: the rules every entity shares

- **Frames cycle:** `current_frame` indexes `shapes` and `colors` modulo their
  lengths, and the two need not be the same length — the ducks have three frames
  and one mask.
- **Movement arguments** are `dx`, `dy`, `dz` and `frame_step`; a `frame_step`
  of zero means the frame never advances and `frame_count` never rises.
- **Transparency:** with `auto_trans`, a space is skipped rather than painted, so
  what is behind shows through. Without it — the water bands — a space is a
  blank that erases.
- **Masks** are read line for line and character for character against the
  shape; a character the mask does not name leaves the entity's default colour.
- **A sprite's box is frame zero's**, measured once.
- **Death** is `alive`, `die_frame`, `die_offscreen`, and the callback that runs
  just before removal.

## Appendix C: where they come from

- `environment.cpp` — the water bands, the castle, the seaweed
- `fish.cpp` — the fish, the bubbles, the splat
- `special.cpp` — ship, whale, both monsters, both big fish, the shark system,
  the fishhook system, swan, ducks, dolphins, and `random_object` over them

with the layers, the model and the loop in `quarium.h`, `entity.cpp` and
`animation.cpp`.
