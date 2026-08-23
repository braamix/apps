# dhrystone — the Dhrystone benchmark, version 2.1

A synthetic integer benchmark from 1984, and for a decade the way small
machines were compared. It exercises procedure calls, pointer and record
handling, string copying and comparison, and integer arithmetic, in a mixture
chosen to look like ordinary systems programming rather than like a kernel of
numeric code.

**The program does not compute anything meaningful**, but it is syntactically
and semantically correct, and every variable is assigned before it is read.
That is the point: what is measured is the shape of the work, not a result.

## Where it came from

- The original is **Ada**, published by **Reinhold P. Weicker** (Siemens AG,
  Erlangen) in *Communications of the ACM* vol. 27 no. 10 (Oct. 1984),
  pp. 1013–1030, together with the statistics the statement mixture is drawn
  from.
- **C version 2.0** appeared in *SIGPLAN Notices* vol. 23 no. 8 (Aug. 1988),
  pp. 49–62, alongside the paper "Dhrystone Benchmark: Rationale for Version 2
  and Measurement Rules". It was developed with **Rick Richardson** (PC
  Research Inc., Tinton Falls NJ) and takes many ideas from the "Version 1.1"
  he had distributed over Usenet, with comments from Chaim Benedelac
  (National Semiconductor), David Ditzel (SUN), Earl Killian and John Mashey
  (MIPS), Alan Smith and Rafael Saavedra-Barrera (UC Berkeley).
- **Version 2.1**, dated May 25 1988, is the one ported here. It differs from
  2.0 only in that a non-executed `else` was added to the `if` in `Func_3` and
  one removed from `Proc_3` — no change to execution time.

Version 2 exists because optimising compilers were deleting the benchmark.
Code was added at several places, but *within the measurement loop only in
branches that are not executed*, so the statement distribution still holds.
The loop counter check was made part of the benchmark, since subtracting loop
overhead correctly had proved impossible.

This port descends from the netlib distribution by way of
`github.com/sergev/dhrystone`, which replaced `times()`/`time()` with
`gettimeofday()` and added the nanoseconds, MDhrystones and DMIPS lines to the
report.

## The statement mixture

103 statements are executed per iteration:

| | number | |
| --- | ---: | ---: |
| assignments | 52 | 51.0 % |
| control statements | 33 | 32.4 % |
| procedure and function calls | 17 | 16.7 % |

Operands are 72.3 % integer, 18.6 % character, 5.0 % pointer, the rest string,
array and record; by locality, 47.1 % local, 22.7 % constant, 18.6 %
parameter, 9.1 % global. Calls take 1.82 parameters on average. There was no
explicit effort to account for cache effects.

## Measurement rules

From the Rationale paper, and they bind this port:

- **Separate compilation.** The division into units is deliberate: while
  compiling one, the compiler must know nothing of register allocation in
  another. Post-linkage optimisation defeats this and should not be used.
- **No procedure merging.** The proportion of calls is part of the
  distribution, so `Proc_*` and `Func_*` must not be inlined. The rule
  explicitly *does not* extend to the string functions, which C is allowed to
  implement inline.
- **Other optimisations are allowed but must be stated.**
- **Default results are those without `register` declarations.**

What this build does, stated as the rules require: **`-O3`**, four translation
units compiled separately, no LTO, no `register`, and `strcpy`/`strcmp` left
as real calls (below).

## What the port changed

Braam has no C library, so every line that reached one was replaced. The
measurement loop's statements are untouched; what encloses them moved, and the
last of the three notes below says why.

| Original | Here |
| --- | --- |
| `printf` | formatted into a `Buf` and written once |
| `printf`'s `%f` | `put_f64` from `math/ftoa.h`, which is `braam::math` |
| `scanf` prompt for the run count | the command line |
| `malloc` | `heap_new<Rec_Type>()`, and it is checked |
| `gettimeofday` | `proc_now()` — monotonic, milliseconds since boot |
| `strcpy`, `strcmp` from libc | `dhry_lib.cpp` |
| `#define true 1` / `false 0` | dropped; they are C++ keywords |

Three consequences worth knowing:

- **The clock counts whole milliseconds**, where `gettimeofday` counted
  microseconds. The two scale factors in the result computation moved by a
  thousand, and the "measured time too small" floor is 2000 ms rather than
  2000000 µs. A run must last seconds for the reading to mean anything, which
  is what the default run count is chosen for.
- **`strcpy` and `strcmp` are a translation unit of their own**, built with
  `-fno-builtin-str*`, so each stays a call across units exactly as the libc
  call was. "Understanding Variations in Dhrystone Performance"
  (*Microprocessor Report*, May 1989) measured 23 % of C Dhrystone time in
  those two functions on a VAX 11/785, and MIPS reported 34 % on an R3000 —
  so how they are reached moves the score. Folding them inline would be
  permitted by the rules; keeping them as calls keeps the number comparable to
  a libc build.
- **The measurement loop is entered in stretches.** The loop itself is
  upstream's, statement for statement, but Proc_0's initialisations are split
  out of it and the timer is started outside, so that `proc_main` can call it
  sixty-four times over rather than once. Between two stretches the process
  parks for zero milliseconds, which is the only moment a signal can reach it —
  see `^C` below. Sixty-four syscalls at 34-45 µs each is under 3 ms, inside a
  measured interval of seconds; the alternative, timing each stretch on its
  own, would quantise the total by a millisecond sixty-four times over and cost
  far more than it saved.

`Str_30` values are printed through `strlen`, which is in `dhry_lib.cpp` for
the same reason it has to exist at all — it is not part of the benchmark.

## Running it

```
dhrystone [runs]
```

With no argument it runs 100 000 000 iterations, which takes a few seconds.
The report prints every global and local with the value it should hold, then:

```
Nanoseconds for one run through Dhrystone: 15000.0
            Million Dhrystones per Second: 0.067
                                    DMIPS: 37.9
```

DMIPS is Dhrystones per second over 1757, the rate of a VAX-11/780 — the
machine that defined one MIPS.

**`^C` stops the run and reports it.** Between syscalls the kernel has no hold
on a process, so a signal reaches one only where it parks — which is why the
run is made in sixty-four stretches with a zero-length sleep between them. An
interrupted run prints the whole report, with the runs it managed and a line
saying so, and exits 130:

```
Interrupted after 2250000 runs
```

The rate is computed from the runs that were made, so it is still a rate; a run
cut short in the first two seconds falls into the "measured time too small"
branch as any short run does.

## Building

From the top of this repository:

```
make
```

which leaves `build/benchmarks/dhrystone/dhrystone.wasm`. Linking `braam::math`
for the report's three numbers costs 7 KB of the 37 KB binary, all of it musl's
printf engine — `--gc-sections` extracts nothing else, since the benchmark
itself never calls a mathematical function.

## Testing

```
make test
```

runs `test/interrupt.mjs`, which sends a running benchmark a `SIG_INT` and
checks that it stops at a stretch boundary and reports how far it got. The
timing cannot be tested there: the smoke harness freezes the clock, so a
headless run always takes the "measured time too small" branch. Numbers come
from a browser.

## Packaging

```
make package
```

builds `build/benchmarks/dhrystone/dhrystone-2.1-r1.zip`, a package in the
format `/bin/pkg` installs (Package_Formats.md §5):

```
.PKGINFO            P:dhrystone  V:2.1-r1  I:<unpacked size>  T:<description>
bin/dhrystone       the binary
```

`bin/` is the load-bearing part. Every flat entry of it becomes a link in the
installed generation's `bin/`, which `/pkg/bin` points at and the default
`PATH` — `/bin:/pkg/bin` — already names, plus a generated
`cmd:dhrystone=2.1-r1` provide. The entry's leaf is the command's name, which
is why it is `bin/dhrystone` and not `bin/dhrystone.wasm`.

**The zip alone cannot be installed.** `pkg` checks every package's size and
digest against a signed index, and the anchor naming the signing key ships
inside `rootfs.zip`; there is no `--force` and no local-file install. Publishing
means standing up a repository — keys, an anchor, an index — which is
Package_Formats.md §10 end to end, and `tools/mkrepo.py` in the core tree is a
worked example of all of it.

## Files

| | |
| --- | --- |
| `dhry.h` | the types and the prototypes (part 1) |
| `dhry_1.cpp` | `Proc_0`'s body, the measurement loop, `Proc_1`–`Proc_5` (part 2) |
| `dhry_2.cpp` | `Proc_6`–`Proc_8`, `Func_1`–`Func_3` (part 3) |
| `dhry_lib.cpp` | `strcpy`, `strcmp`, `strlen` (part 4) |
| `test/interrupt.mjs` | a run stopped by a signal, headless |
