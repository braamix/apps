# duremark — the DureMark benchmark, version 1.0

A small integer benchmark inspired by EEMBC's CoreMark, keeping the spirit of
its workloads at a fraction of the complexity and memory. It was written to
compare 8-, 16- and 32-bit processors against each other, so it is C89, it
assumes nothing about the width of `int`, and its whole working set is about
one and a half kilobytes of static arrays.

**One DureMark is roughly the speed of a classic IBM PC XT.**

Copyright (c) 2025 Serge Vakulenko, MIT. Upstream is
`github.com/sergev/duremark`.

## What it measures

Three workloads, timed separately and reported as percentages of the total:

| | Operations | What it exercises |
| --- | --- | --- |
| **Linked list** | find by index, reverse, bubble sort by data, sort back by index | pointer chasing and memory access patterns |
| **Matrix** | add a constant, multiply by a constant, 10×10 matrix multiply | arithmetic and tight computational loops |
| **State machine** | recognise numbers in a comma-separated string, corrupt it, run again | conditional branches and state transitions |

The iteration count starts at 3 and climbs the 3/10/30/100 ladder until the
measured work reaches three seconds. The score is **iterations per second**.

```
Try 3 iterations...
Try 10 iterations...
...

DureMark 1.0 Results
=======================
Iterations      : 1000000
Execution Time  : 6.9 sec
List Workload   : 45.8%
Matrix Workload : 11.7%
State Workload  : 42.4%
-----------------------
Total Score     : 145896.12 DureMark
```

## What the port changed

Upstream already carries a porting layer — `duremark.h` picks one with `#if`,
and `unix.c` and `dos.c` are two implementations of the same four things. Braam
is the third, `braam.cpp`, and the workloads are upstream's. The whole C
library surface of the benchmark is `printf` and `clock`; there is no `malloc`,
no string or memory function, no `<math.h>`, no file I/O and no argument
parsing to replace.

| Original | Here |
| --- | --- |
| `clock()`, `CLOCKS_PER_SEC` | `proc_now()`, so a tick is a millisecond |
| `#define du_printf printf` | a small variadic formatter over `Buf` |
| `int main()` | `Task<i32> du_main()`, because writing is a coroutine |

Two changes go deeper than the porting layer, and both are forced by the
platform:

- **The timer moved outward.** Upstream starts and stops the clock around
  *each workload call, inside the innermost loop* — three pairs per iteration.
  This clock counts whole milliseconds, so each of those intervals measures
  zero, the total stays zero, the ladder runs to its twenty-step limit and the
  report divides by zero. (Turbo C's 18.2 Hz clock fails the same way, so the
  DOS path can never have been run either.) Each workload is now timed over its
  whole loop, selected with the `execs` mask upstream already has for the
  purpose. The percentages and the score are computed from exactly the same
  quantities; what is lost is that the three no longer interleave within one
  iteration.
- **Output is buffered.** A write is an asynchronous syscall and `du_printf` is
  called from ordinary functions, which cannot await one. So `du_printf`
  appends to a buffer and `du_main` flushes it — after each
  `Try N iterations...` line, so progress is visible, and once at the end.

Smaller things: the matrix block is `alignas`-ed, since it is cast from
`uint8_t[]` to wider types; and one comparison gained a `(double)` cast that
C++20 wants and C did not.

**Left exactly as upstream has it:** the state workload's "restore" pass XORs
with `0x43` where the corruption used `0x12`, so the buffer is not in fact
restored and drifts from iteration to iteration. That changes the work each
iteration does and therefore the score, so it is copied rather than corrected.

## Running it

```
duremark
```

No arguments — the iteration count is chosen by the ladder. A run takes several
seconds and **cannot be interrupted**: between syscalls the kernel has no hold
on a process, so `^C` cannot reach a running benchmark.

## Building and packaging

From the top of this repository:

```
make            # build/benchmarks/duremark/duremark.wasm
make package    # build/benchmarks/duremark/duremark-1.0-r0.zip
```

The package holds `.PKGINFO` and `bin/duremark`; `bin/` is what reaches `PATH`
once `/bin/pkg` installs it.

## Files

| | |
| --- | --- |
| `duremark.h` | types, structures and prototypes; picks the porting layer |
| `main.cpp` | the ladder, the timing and the report |
| `list.cpp` | the linked-list workload |
| `matrix.cpp` | the 10×10 matrix workload |
| `state.cpp` | the number-recognising state machine |
| `braam.cpp`, `braam.h` | the porting layer: the clock and the formatter |
