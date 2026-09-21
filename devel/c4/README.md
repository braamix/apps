# c4 — C in four functions

A tiny C compiler and a bytecode VM that runs what it just compiled, together
about five hundred lines. **This is Robert Swierczek's [c4](
https://github.com/rswier/c4)** (2014), GPL-2.0. It is enough C to compile
itself: `char`, `int` and pointers; `if`, `while`, `return` and expressions;
and the handful of library calls a self-hosted run needs.

```
c4 hello.c
c4 -s hello.c
c4 c4.c hello.c
```

The package ships the original `hello.c` and `c4.c`, and a name with no `/`
that is not in this directory is looked for among them, so after `pkg install
c4` the three lines above are what they look like.

`[-s]` dumps each source line with the assembly it produced and does not run.
`[-d]` traces every instruction as it runs. Remaining arguments after the
source file are the guest's `argv`, which is how `c4 c4.c hello.c` hands
`hello.c` to the copy of c4 that just compiled.

## What the port changed

**The VM is a driver, not a coroutine.** `open`, `read`, `close` and `printf`
block here, and they are reached from the instruction loop — which cannot
await: a `co_await` is a call and not a tail call, so a loop that awaited
would grow the native stack until the process trapped. So `c4_burst()` runs
plain C++ until it has something for its caller to do, and only
[braam.cpp](braam.cpp) awaits. This is simbesm's `cpu_burst()` contract.

The four functions — `next`, `expr`, `stmt`, and the compile walk — stay
ordinary C. Compiler diagnostics and `-s`/`-d` listings accumulate in a
buffer the driver drains, because a variadic `printf` cannot be a coroutine
and those functions are not Tasks. `exit(-1)` on a compile error is a sticky
flag and a return, one frame at a time: there is no `longjmp`.

`#define int long long` is kept, so the VM word is eight bytes as on
upstream's 64-bit hosts. Pointers here are four; they sit in the low half of
a word. Guest `argv` is therefore an array of eight-byte slots, because
`++argv` adds `sizeof(int)`. Guest `open` passes Unix flag values (`0` is
read), which the driver maps onto the kit's `O_*`.

A burst that does nothing but arithmetic parks every few thousand
instructions, which is the only way `^C` can reach a `while (1)`.

Dropped: nothing of the language. The host `open`/`read`/`close`/`printf` of
the compiler itself moved into the driver; the guest still sees the same
nine library calls.

## Files

| | |
| --- | --- |
| [c4.cpp](c4.cpp) | the four functions, the compile walk, `c4_burst` |
| [braam.cpp](braam.cpp) | `proc_main`, the driver, share lookup |
| [c4.h](c4.h) | what those two share |
| [share/c4.c](share/c4.c) | upstream's compiler, for self-host |
| [share/hello.c](share/hello.c) | the first program it runs |
| [test/](test/) | hello, self-host, and a guest `^C` |
| [LICENSE](LICENSE) | GPL-2.0 |

## Building

From the top of this repository:

```
make            # build/devel/c4/c4.wasm
make package    # build/devel/c4/c4-1.0-r0.zip
```
