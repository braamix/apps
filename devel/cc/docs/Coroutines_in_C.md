# Coroutines and `defer` in C

vcc extends C with two features:

- **`defer`** runs a cleanup statement when a block is left, whichever way it is left.
- **Coroutines** are functions that can stop in the middle, return to their caller,
  and later continue from where they stopped.

This is a tutorial. It assumes you know ordinary C and nothing else.

> **Status.** `defer` (section 1) works on every target; `<coro.h>` defines its short
> name. Coroutines (sections 2 to 7) work on every target but BESM-6, where the
> preprocessor defines `__vcc_coroutines__`. They were made for programs that run on
> [Braam](#8-coroutines-on-braam): section 8 is about `vcc -t wasm32-braam`,
> [Braam.md](Braam.md) is that target's reference, and
> [Braam_Example.md](Braam_Example.md) works a program through. Section 10 gives the
> frame ABI. How they are implemented, and why so, is in
> [Coroutines_Internals.md](Coroutines_Internals.md).

## Contents

1. [`defer`](#1-defer)
2. [A first coroutine](#2-a-first-coroutine)
3. [Frames and storage](#3-frames-and-storage)
4. [The operations](#4-the-operations)
5. [`await`: one coroutine calling another](#5-await-one-coroutine-calling-another)
6. [Stopping a coroutine early](#6-stopping-a-coroutine-early)
7. [Rules, errors and traps](#7-rules-errors-and-traps)
8. [Coroutines on Braam](#8-coroutines-on-braam)
9. [Quick reference](#9-quick-reference)
10. [The frame ABI](#10-the-frame-abi)

---

## 1. `defer`

### The problem

A function that acquires several things must release them on every way out:

```c
int work(void)
{
    char *a = malloc(100);
    if (!a)
        return -1;
    char *b = malloc(200);
    if (!b) {
        free(a);                /* don't forget */
        return -1;
    }
    if (step(a, b) < 0) {
        free(b);                /* don't forget either */
        free(a);
        return -1;
    }
    free(b);
    free(a);
    return 0;
}
```

Every new exit has to repeat the cleanup, and forgetting it once is a leak.

### The solution

```c
int work(void)
{
    char *a = malloc(100);
    if (!a)
        return -1;
    defer free(a);

    char *b = malloc(200);
    if (!b)
        return -1;              /* frees a */
    defer free(b);

    if (step(a, b) < 0)
        return -1;              /* frees b, then a */
    return 0;                   /* frees b, then a */
}
```

`defer S;` means: **when control leaves the block that contains this `defer`, run
`S`.** You write the cleanup once, next to the acquisition.

### The rules

**It belongs to the enclosing block.** The `{ }` that contains the `defer` is its
scope. In a loop body, the deferred statement runs at the end of every iteration:

```c
for (int i = 0; i < n; i++) {
    char *line = read_line(i);
    defer free(line);           /* runs at the end of each iteration */
    process(line);
}
```

A `defer` written as the body of an `if` without braces belongs to that body. It runs
straight away, because the body ends at once:

```c
if (x)
    defer puts("A");            /* prints A immediately: the body ends here */
```

**Every way out counts.** Falling off the end of the block, `return`, `break`,
`continue`, and a `goto` to a label outside the block all run it.

**Last in, first out.** Several `defer`s in one block run in reverse order:

```c
{
    defer puts("1");
    defer puts("2");
    defer puts("3");
}                               /* prints 3, 2, 1 */
```

That is the right order for resources that depend on each other: lock `a`, then
`b`; unlock `b`, then `a`.

**Nested blocks run inner first:**

```c
{
    defer puts("A");
    {
        defer puts("B");
    }                           /* prints B */
    defer puts("C");
}                               /* prints C, then A */
```

**Only what was reached.** A `defer` takes effect when control passes it. If a
`return` comes before the `defer`, nothing is run for it:

```c
char *p = malloc(10);
if (!p)
    return -1;                  /* the defer below was not reached: nothing runs */
defer free(p);
```

**Variables are read when the deferred statement runs**, not when the `defer` is
reached:

```c
{
    int x = 1;
    defer printf("%d\n", x);
    x = 2;
}                               /* prints 2 */
```

**A block of cleanup is allowed:**

```c
defer {
    unlock(m);
    log_unlock(m);
}
```

**What does not run it:** `exit()`, `abort()`, a trap, or `longjmp`. These do not
leave blocks in the ordinary way.

### What a deferred statement may not do

Control must leave a deferred statement only by reaching its end. So these are
compile-time errors inside a `defer`:

- `return`;
- `break` or `continue` that would leave the deferred statement (a loop entirely
  inside it may use them);
- `goto` to a label outside it;
- `yield` and `await` (section 5).

### Jumps into a block past a `defer`

You may not jump *into* a block to a point after one of its `defer`s. If you could,
the compiler could not tell whether that `defer` was reached.

```c
    goto inside;                /* error: jumps past the defer */
    {
        defer puts("x");
    inside:
        ...
    }
```

The same applies to `switch`: a `case` label that comes after a `defer` in the
switch's block is an error. Give the `case` its own block instead:

```c
switch (c) {
case 1: {
    char *p = malloc(10);
    defer free(p);              /* scope is this case only */
    use(p);
    break;                      /* frees p */
}
case 2:
    ...
}
```

### The cost

Almost none at run time. The compiler copies the deferred statements onto each exit
path. When they are long and a block has several exits, the exits share one copy
instead: each records where it goes next in a hidden variable and jumps to the copy,
which costs a store and a jump or two. Nothing is allocated and no list is kept while
the program runs.

---

## 2. A first coroutine

An ordinary function runs from start to finish in one go. A **coroutine** can stop
in the middle with `yield`, hand a value to whoever is running it, and continue from
the same point the next time it is resumed. All its local variables keep their
values in between.

```c
#include <coro.h>
#include <stdio.h>

coro(int) void count_to(int n)          /* yields ints, returns nothing */
{
    for (int i = 1; i <= n; i++)
        yield i;
}

int main(void)
{
    co_frame(int, void) *f = co_alloca(count_to, 0, 3);

    while (co_resume(f) == CO_SUSPENDED)
        printf("%d\n", co_value(f));    /* prints 1, 2, 3 */
    return 0;
}
```

Step by step:

1. `coro(int) void count_to(int n)` declares a coroutine. The `int` in `coro(int)` is
   the type of the values it yields. `void` is the type it returns at the end, as
   with any function.
2. `co_alloca(count_to, 0, 3)` prepares a run of `count_to(3)` in memory on the
   stack, and returns a pointer to it, `f`. (The `0` is extra room, explained in
   section 3.) **No code of `count_to` runs yet.** The memory is freed when the block
   containing the `co_alloca` ends — here, when `main` returns.
3. `co_resume(f)` runs the coroutine until it reaches a `yield` or finishes. It
   returns `CO_SUSPENDED` if it stopped at a `yield`, or `CO_DONE` if it finished.
4. `co_value(f)` is the value of the last `yield`.
5. The loop ends when `co_resume` returns `CO_DONE`.

You write the loop inside the coroutine as a plain loop. The caller sees a sequence
of values.

`#include <coro.h>` gives the short names `coro`, `yield`, `await`, `defer`,
`co_frame`, `co_alloca` and `co_init` through `co_alignof`, and declares `co_status` and
`co_signal`. Without the header the long spellings still work: `_Coro`, `_Yield`,
`_Await`, `_Defer`, `_Coro_frame`, `__co_init` and so on. So a program that already
uses a variable named `yield` or `defer` is not affected unless it includes
`<coro.h>`.

### A coroutine that yields nothing

`coro(void)` suspends without a value. Its `yield` is written alone:

```c
coro(void) void blink(void)
{
    for (;;) {
        led_on();
        yield;
        led_off();
        yield;
    }
}
```

Each `co_resume` runs one half-period. `co_value` cannot be used on such a
coroutine.

### A coroutine that returns a value

`return` works as usual. The caller reads the result with `co_result` after
`co_resume` has returned `CO_DONE`:

```c
coro(int) long sum_up(int n)            /* yields each term, returns the total */
{
    long s = 0;
    for (int i = 1; i <= n; i++) {
        s += i;
        yield i;
    }
    return s;
}

    ...
    while (co_resume(f) == CO_SUSPENDED)
        ;                               /* ignore the terms */
    long total = co_result(f);          /* 15 for n = 5 */
```

A coroutine whose return type is not `void` must end with a `return`, as any such
function must in vcc: the compiler reports one that may run off its end.

### What may be a coroutine

Any function except:

- a variadic one (`...`);
- an `inline` or `_Noreturn` one;
- one without a prototype: write `coro(int) void f(void)`, not `f()`;
- `main`.

A coroutine is not an ordinary function. You cannot call it as `count_to(3)`, and
you cannot take its address as a function pointer. You use its name only in
`co_alloca`, `co_init`, `co_sizeof`, `co_alignof` and `await` (section 5), or, when it
takes `(void)` or `(void *)`, as a `coro_ptr` (section 4).

A coroutine may call ordinary functions as usual. An ordinary function may run a
coroutine with `co_alloca` or `co_init` and `co_resume`, but it cannot `yield` or
`await` itself.

---

## 3. Frames and storage

Everything a coroutine must remember while it is stopped is kept in its **frame**:

- its arguments;
- the local variables still needed after a `yield`;
- where to continue;
- the last value yielded and, at the end, the result.

**The compiler never allocates memory for a frame by itself.** You say where it goes,
in one of two ways.

### On the stack: `co_alloca`

```c
co_frame(int, void) *f = co_alloca(count_to, 0, 3);
```

`co_alloca(g, extra, args...)` makes a frame for a run of `g` with these arguments,
of exactly the right size, and returns its pointer. The memory lives **until the end
of the block** that contains the `co_alloca`. This is the easy way, and usually the
right one.

`extra` is spare room for the coroutines that `g` itself awaits (section 5). Give
`0` when `g` awaits nothing.

When the block ends — by reaching `}`, or by `return`, `break`, `continue` or a
`goto` out of it — two things happen, as if you had written a `defer` (section 1)
at the place of the `co_alloca`:

1. If the coroutine has not finished, it is destroyed: its pending `defer`s run
   (section 6). So nothing it holds is leaked.
2. The memory is freed.

```c
void first_three(void)
{
    for (int k = 0; k < 10; k++) {
        co_frame(int, void) *f = co_alloca(count_to, 0, 100);
        for (int i = 0; i < 3 && co_resume(f) == CO_SUSPENDED; i++)
            printf("%d\n", co_value(f));
    }                                   /* f destroyed and freed, every time round */
}
```

A `co_alloca` in a loop body is freed at the end of every iteration, so the loop does
not use up the stack.

`co_alloca` works in coroutines too. There the memory comes from the coroutine's own
spare room (section 5) rather than the stack, and the rules are the same. This is how
a coroutine runs another coroutine of a different yield type:

```c
coro(char *) void report(void)          /* yields lines of text */
{
    static char line[32];
    co_frame(int, void) *n = co_alloca(count_to, 0, 3);
    while (co_resume(n) == CO_SUSPENDED) {
        snprintf(line, sizeof line, "got %d", co_value(n));
        yield line;
    }
}
```

Whoever starts `report` must give it spare room for that frame:
`co_alloca(report, co_sizeof(count_to))`.

You may not jump with `goto` into a block past a `co_alloca` in it, nor put a `case`
label after one; the same rule as for `defer`.

In an ordinary function, `co_alloca` takes the stack as `alloca` (`<alloca.h>`) does,
and the end of its block puts the stack pointer back to where it was. So memory that
`alloca` took after the `co_alloca`, in the same block, goes then too, not when the
function returns ([Standard_Include_Files.md](Standard_Include_Files.md), `<alloca.h>`).
`alloca` itself is an error in a coroutine, whose frame outlives the stack.

### In memory you own: `co_init`

```c
static _Alignas(16) char storage[256];
co_frame(int, void) *f = co_init(storage, sizeof storage, count_to, 3);
```

`co_init(mem, size, g, args...)` builds the frame in a block you supply: a static
array, a local array, or memory from `malloc`. Use it when the frame must outlive the
block that creates it — for example, a task a scheduler keeps in a table. The block
must be:

- **large enough**: at least `co_sizeof(count_to)` bytes, plus room for what the
  coroutine awaits;
- **aligned** to `co_alignof(count_to)`, which is never more than 16, so
  `_Alignas(16)` is always enough.

If either is wrong, `co_init` traps. Nothing is cleaned up for you at the end of a
block: finishing the coroutine, or calling `co_destroy`, is your job.

### `co_sizeof` and `co_alignof`

They are fixed when the coroutine is compiled, and read from a small table in its
unit (section 10), so they cost one load. But the compiler does not know them when it compiles a
program that uses the coroutine, so you cannot use them as an array
size: `char storage[co_sizeof(count_to)]` is an error. That is why `co_alloca`
exists. With `co_init`, pick a size with room to spare, or check it:

```c
if (co_sizeof(count_to) > sizeof storage)
    fatal("storage too small");
```

The compiler decides what goes into a frame only after optimizing the coroutine,
which keeps frames small, and only in the file that defines the coroutine — you can
run a coroutine from a library without seeing its source.

### The frame pointer

`co_alloca` and `co_init` return a `co_frame(Y, T) *`, where `Y` is the yield type and
`T` the return type. You use this pointer with every other operation. `co_frame(Y, T)` is an
incomplete type: you can only have pointers to it, never an object of it.

The pointer type mentions only `Y` and `T`, not which coroutine it is. So one
function can drive any coroutine that yields `int` and returns `void`.

### Rules for the storage

- **Do not move or copy** a frame while it is in use. Its address must stay the same
  from its creation to the end.
- **Do not use a `co_alloca` frame after its block has ended.** The pointer is dead
  then, like a pointer to a local variable.
- **Keep `co_init` storage alive** while the frame is in use. If a local array goes
  out of scope while its coroutine is stopped in the middle, anything the coroutine
  was holding is lost. Finish the coroutine, or use `co_destroy` (section 6), first.
- **Each run needs its own frame.** You can run the same coroutine several times at
  once, each in its own storage. The runs do not affect each other:

```c
static _Alignas(16) char s1[256], s2[256];
co_frame(int, void) *a = co_init(s1, sizeof s1, count_to, 2);
co_frame(int, void) *b = co_init(s2, sizeof s2, count_to, 2);
co_resume(a); co_resume(b);             /* a at 1, b at 1 */
co_resume(a);                           /* a at 2, b still at 1 */
```

### Locals and pointers

Local variables of a coroutine live in the frame, and the frame does not move. So a
pointer to a local stays valid while the coroutine is stopped. A coroutine can even
yield the address of its own local; section 5 has an example. The pointer stops
being valid when the coroutine leaves that variable's block.

The compiler decides which locals must live in the frame. You need not think about
it: a value computed before a `yield` and used after it is always still there.

---

## 4. The operations

All of them take a frame pointer `f`, except `co_alloca`, `co_init`, `co_sizeof` and
`co_alignof`.

| Operation | What it does |
|---|---|
| `co_alloca(g, extra, args...)` | Prepares a run of coroutine `g` with these arguments in memory that lives until the end of the block, plus `extra` bytes for what `g` awaits. At the end of the block, destroys `g` if unfinished and frees the memory. Runs no code of `g`. Returns the frame pointer. |
| `co_init(mem, size, g, args...)` | Prepares a run of coroutine `g` with these arguments in `mem`. Runs no code of `g`. Returns the frame pointer. |
| `co_resume(f)` | Runs until the next `yield` (returns `CO_SUSPENDED`) or until the coroutine finishes (returns `CO_DONE`). |
| `co_cancel(f)` | Like `co_resume`, but the `yield` the coroutine is stopped at returns `CO_CANCEL`, asking it to stop (section 6). |
| `co_destroy(f)` | Ends a stopped coroutine without running any more of its code, except its pending `defer`s. Returns `CO_DONE`. |
| `co_done(f)` | True once the coroutine has finished or been destroyed. |
| `co_value(f)` | The value of the last `yield`. Valid only after `co_resume` or `co_cancel` returned `CO_SUSPENDED`. |
| `co_result(f)` | The value the coroutine returned. Valid only after it finished with `CO_DONE`, not after `co_destroy`. |
| `co_sizeof(g)` | Bytes of memory a frame of `g` needs. Known when the program runs; not usable as an array size. |
| `co_alignof(g)` | Alignment a frame of `g` needs. At most 16. Known when the program runs. |

In `co_alloca`, `co_init`, `co_sizeof` and `co_alignof`, `g` may also be a `coro_ptr`
(below).

The types:

```c
typedef enum { CO_SUSPENDED, CO_DONE } co_status;      /* what co_resume returns */
typedef enum { CO_CONTINUE, CO_CANCEL } co_signal;     /* what yield returns */
```

`co_alloca` and `co_init` check the arguments against the coroutine's parameters as an ordinary
call does, and convert them the same way.

These operations look like function calls but are built into the compiler. You
cannot take their address.

### Pointers to coroutines: `coro_ptr`

A scheduler often keeps a list of coroutines to start, of different code but of one
shape. `coro_ptr(Y, T)` points to a coroutine that yields `Y`, returns `T`, and takes
either nothing, `(void)`, or one `void *`. Such a coroutine's name converts to one,
the way a function's name converts to a function pointer:

```c
coro(int) int count(void *arg);   /* counts up to *(int *)arg */
coro(int) int ticks(void);

coro_ptr(int, int) table[] = { count, ticks };

    int n = 3;
    co_frame(int, int) *f = co_alloca(table[0], 0, &n);   /* runs count(&n) */
```

`co_alloca`, `co_init`, `co_sizeof`, `co_alignof` and `await` take a `coro_ptr` where
they take a coroutine's name. The argument is optional and becomes a `void *`; a
coroutine that takes `(void)` ignores it. A `coro_ptr` can be stored, compared, passed
and returned like any pointer, and be null. The one thing you cannot do with it is
call it: `p(arg)` is only allowed after `await`, where it runs the coroutine to its
end as `await g(args)` does (section 5).

A coroutine with any other parameters has no `coro_ptr`. To put one in a table,
write a small coroutine that takes a `void *`, unpacks the arguments from it, and
awaits the real one.

---

## 5. `await`: one coroutine calling another

A coroutine often needs to call another coroutine and wait for its result. `await`
does that:

```c
coro(int) void walk(struct node *t)      /* yields the values of a tree, in order */
{
    if (!t)
        return;
    await walk(t->left);
    yield t->value;
    await walk(t->right);
}
```

`await g(args)` runs coroutine `g` to its end. **Each time `g` yields, the coroutine
doing the `await` yields the same value to its own caller.** When `g` returns, the
`await` expression has `g`'s result as its value. To the caller of `walk`, it looks
as if the values came from `walk` itself.

The example also shows that a coroutine may `await` itself recursively.

### Where the sub-coroutine's frame comes from

When you started the outermost coroutine, its frame got **spare room** after it:
the `extra` bytes of `co_alloca`, or the rest of the block you gave `co_init`. Every
`await` inside that run takes the frame of the coroutine it calls from this spare
room, and gives it back when that coroutine finishes. `await`s always finish in
reverse order of starting, so the spare room is used as a simple stack. A
`co_alloca` inside any coroutine of the run takes its memory from the same spare
room, and gives it back at the end of its block.

So the spare room must cover the deepest chain of `await`s: for `walk` on a tree of
depth 20, about 20 more frames of `walk`, that is `20 * co_sizeof(walk)`:

```c
co_frame(int, void) *t = co_alloca(walk, 20 * co_sizeof(walk), root);
```

If the room runs out, the program traps and names the coroutine that did not fit.
Give more.

A coroutine that awaits nothing, and uses no `co_alloca`, needs no spare room.

A frame made by `co_alloca` gets its own spare room, the `extra` you gave it. It does
not share its caller's.

### The second form: awaiting a frame you made

You can also make the sub-coroutine's frame yourself and `await` the pointer:

```c
static _Alignas(16) char buf[512];
co_frame(int, void) *sub = co_init(buf, sizeof buf, walk, root);
await sub;
```

Both forms mean the same. The first is shorter and needs no extra storage.
`co_alloca` makes the second form easy inside a coroutine too:
`await co_alloca(walk, 0, root)`.

### Yield types must match

An `await` passes the sub-coroutine's yielded values straight up, so its yield type
must be **exactly** the yield type of the coroutine that awaits it. A `coro(int)` can
await only `coro(int)` coroutines. A `coro(void)` can await only `coro(void)` ones.
The return types may differ: the result is the value of the `await` expression.

To use a coroutine with a different yield type, run it by hand with `co_alloca` (or
`co_init`) and `co_resume`, as `report` does in section 3. Any function or coroutine
may do that.

### `await` and `yield` inside expressions

Both are expressions and may appear inside larger ones:

```c
total = total + await read_number();
if ((yield x) == CO_CANCEL) ...
```

`await` binds as tightly as `sizeof`: `await f(x) + 1` means `(await f(x)) + 1`.
`yield` binds tighter than `==`: `yield i == CO_CANCEL` means `(yield i) ==
CO_CANCEL`. Write the parentheses anyway; the code is clearer with them.

### What `await` is, exactly

`await sub` behaves exactly like this loop:

```c
co_signal sig = CO_CONTINUE;
while ((sig == CO_CANCEL ? co_cancel(sub) : co_resume(sub)) == CO_SUSPENDED)
    sig = yield co_value(sub);
/* the value of the await is co_result(sub) */
```

So nothing in the language knows about input, output or scheduling. A yielded value
means whatever the program running the outermost coroutine decides it means.

### An example: requests to a scheduler

A coroutine can yield a *request*, and whoever is running it carries the request out
and resumes it with the answer:

```c
typedef struct {
    int fd;
    void *buf;
    size_t len;
    long out;                           /* filled in by the scheduler */
} io_req;

coro(io_req *) long read_exact(int fd, char *p, size_t n)
{
    size_t got = 0;
    while (got < n) {
        io_req r = { .fd = fd, .buf = p + got, .len = n - got };
        if ((yield &r) == CO_CANCEL)
            return -1;
        if (r.out <= 0)
            return got;                 /* end of input, or an error */
        got += r.out;
    }
    return got;
}

coro(io_req *) int read_header(int fd, struct header *h)
{
    long n = await read_exact(fd, (char *)h, sizeof *h);
    return n == sizeof *h ? 0 : -1;
}
```

The scheduler is ordinary C:

```c
static _Alignas(16) char mem[4096];
co_frame(io_req *, int) *t = co_init(mem, sizeof mem, read_header, fd, &hdr);

while (co_resume(t) == CO_SUSPENDED) {
    io_req *r = co_value(t);
    r->out = do_read(r->fd, r->buf, r->len);
}
int status = co_result(t);
```

`read_exact` yields the address of its local `r`. That is safe: `r` stays in place
while `read_exact` is stopped, and that is exactly when the scheduler uses it.

### Cost

An `await` chain *n* coroutines deep costs *n* resumes for each value that travels up
it. Keep chains reasonably short in hot code.

---

## 6. Stopping a coroutine early

Sometimes the caller no longer wants the rest: the user pressed `^C`, or a search
found what it needed. There are two ways to stop a coroutine before it finishes.

### `co_cancel`: ask it to stop

`yield` returns a `co_signal`. It is `CO_CONTINUE` after `co_resume`, and
`CO_CANCEL` after `co_cancel`. The coroutine decides what to do:

```c
coro(int) void numbers(void)
{
    for (int i = 0; ; i++)
        if ((yield i) == CO_CANCEL)
            return;                     /* clean up and finish */
}

    ...
    co_cancel(f);                       /* numbers returns; co_cancel returns CO_DONE */
```

Cancelling is a request, not a command. A coroutine that ignores the result of
`yield` carries on running until its next `yield`, and `co_cancel` then returns
`CO_SUSPENDED`. A coroutine that tests the signal at every `yield` can always be
cancelled. Nothing is thrown or unwound: the coroutine leaves by its own `return`,
so its `defer`s run as on any `return`.

`await` passes the cancel down: if the outer coroutine is cancelled while it awaits,
the `yield` the inner coroutine is stopped at returns `CO_CANCEL`.

### `co_destroy`: end it now

`co_destroy(f)` runs none of the coroutine's ordinary code. It runs only the
`defer`s that are active at the `yield` where it stopped, innermost first, and marks
the frame finished. If the coroutine is inside an `await`, the inner coroutine's
`defer`s run first. A coroutine it started with `co_alloca`, in a block it has not
yet left, is destroyed too, in its place among the `defer`s.

```c
coro(int) void reader(void)
{
    char *buf = malloc(4096);
    defer free(buf);
    for (;;)
        yield fill(buf);
}

    ...
    co_resume(f);
    co_resume(f);
    co_destroy(f);                      /* frees buf; nothing else of reader runs */
```

After `co_destroy`, `co_done(f)` is true and `co_result(f)` is not available. Do not
call `co_destroy` on a frame that has already finished: check `co_done(f)` first.

### `defer` across `yield`

A `yield` does not leave any block. The `defer`s of a stopped coroutine stay pending.
They run when the coroutine later leaves their block, or at `co_destroy`:

```c
coro(void) void task(void)
{
    lock(&a);
    defer unlock(&a);
    {
        lock(&b);
        defer unlock(&b);
        yield;                          /* a and b both still locked */
    }                                   /* unlock(&b) */
    yield;                              /* a still locked */
}                                       /* unlock(&a) */
```

So a resource held across a `yield` stays held while the coroutine is stopped. To
release it before stopping, close its block before the `yield`.

A deferred statement may not contain `yield` or `await`. Cleanup always runs to its
end without stopping.

---

## 7. Rules, errors and traps

### Compile-time errors

- `yield` or `await` outside a coroutine, or inside a deferred statement.
- `yield expr;` in a `coro(void)`, or a bare `yield;` in any other coroutine.
- `await` of a coroutine whose yield type is not exactly yours.
- `co_value` on a `coro(void)` frame; `co_result` on a frame whose return type is
  `void`.
- A coroutine that is variadic, `inline` or `_Noreturn`, that has no prototype, or that
  is `main`; `coro(Y)` on anything but a function; a yield type that is an array or a
  function.
- A coroutine whose return type is not `void` that may run off its end.
- A coroutine name used other than in `co_alloca`, `co_init`, `co_sizeof`, `co_alignof` or
  `await`: called directly, assigned, or converted to a function pointer. The one
  exception is a coroutine taking `(void)` or `(void *)`, whose name converts to a
  `coro_ptr`.
- A `coro_ptr` called other than after `await`, or with more than one argument; a
  `coro_ptr` given a coroutine of another `Y` or `T`.
- A jump into a block past one of its `defer`s or `co_alloca`s, including a `case`
  label.
- A `co_alloca` in the head of a loop (the condition of a `while` or `do`, or a clause
  of a `for`), which would run each time the head does. Put it in the body.
- `co_sizeof` or `co_alignof` used where a constant is required: an array size, a
  `case` label, a `_Static_assert`.
- `return`, `goto`, or a `break`/`continue` that leaves a deferred statement.
- Coroutines on BESM-6, the one target without them.

### Traps

These stop the program with a message naming the trap (`coroutine trap: CO_TRAP_…` on
standard output, and exit status 255), in every build:

| Trap | Cause |
|---|---|
| `CO_TRAP_STORAGE` | `co_init` given memory that is too small or badly aligned |
| `CO_TRAP_REENTRANT` | a coroutine resumes its own frame, directly or through others |
| `CO_TRAP_FINISHED` | `co_resume`, `co_cancel` or `co_destroy` on a finished frame |
| `CO_TRAP_NO_VALUE` | `co_value` when the last resume did not return `CO_SUSPENDED` |
| `CO_TRAP_NOT_DONE` | `co_result` before the coroutine finished, or after `co_destroy` |
| `CO_TRAP_NO_SPACE` | an `await`, or a `co_alloca` inside a coroutine, found no spare room left (section 5) |

A `co_alloca` in an ordinary function takes its memory from the stack, and running out
stops the program as any stack overflow does.

### Warnings

Two mistakes with memory given to `co_init` are common enough that the compiler warns
about the plain cases. A warning is printed and compilation goes on.

- **The frame outlives its memory.** The memory is a local array, or `&x` of a local,
  and the frame pointer is stored where it lives on after the block: in a global or
  static variable, a variable of an outer block, through a pointer, or returned.

  ```
  warning: f: the frame of 'gen' outlives its storage 'buf', an automatic object: use static or allocated storage
  ```

- **The frame may be left stopped.** Such a frame is resumed by a statement that
  ignores what `co_resume` returns. Nothing in the block destroys the frame, asks
  `co_done`, reads `co_result` or `await`s it. If the coroutine is still stopped at the
  end of the block, its `defer`s never run.

  ```
  warning: f: the frame of 'gen' in 'buf' may be left suspended at the end of the block, its defers never run: co_destroy it, or use co_alloca
  ```

### What the language does not catch

- Memory given to `co_init` that goes away while the coroutine is stopped, beyond the
  cases warned about above. Finish or `co_destroy` the coroutine first. (A `co_alloca`
  frame cannot be left behind: the end of its block destroys it.)
- A `co_alloca` frame pointer used after its block has ended.
- Copying or moving a frame's memory.
- Two threads resuming one frame. (vcc's runtimes have no threads.)
- A `longjmp` out of a block with a `co_alloca`: its frame is not destroyed, and off
  wasm32 its memory stays taken until an enclosing block with a `co_alloca` ends.

---

## 8. Coroutines on Braam

Braam is an operating system that runs in a browser tab. A Braam program may never
wait inside a call. When it needs input, it sends a request to the kernel, returns,
and is started again later with the answer. Coroutines let you write such a program
as ordinary C. Each blocking call is an `await`. The C runtime, not your code,
returns to the kernel and continues where you stopped.

Compile for Braam with `vcc -t wasm32-braam`. The output is a program Braam can
install and run. This section shows how coroutines are used there;
[Braam.md](Braam.md) is the full reference, with the library, a porting checklist and
how to run a program by hand.

### `main`

`main` is a coroutine that yields Braam requests:

```c
#include <coro.h>
#include <stdio.h>
#include <unistd.h>

coro(braam_call *) int main(int argc, char **argv)
{
    char buf[512];
    ssize_t n;
    while ((n = await read(0, buf, sizeof buf)) > 0)
        if (await write(1, buf, n) < 0)
            return 1;
    return n < 0;
}
```

That is `cat`. On Braam `main` must have this form: a `coro(braam_call *)` coroutine
taking `argc` and `argv`. The runtime builds `main`'s frame in a block from `malloc`. Every `await`
inside `main` takes its frame from that block (section 5). If a program needs a
bigger block, it defines its size:

```c
const unsigned __braam_task_bytes = 256 * 1024;
```

### Blocking calls are coroutines

Every function that may have to wait keeps its usual C name and arguments. It is
declared as `coro(braam_call *)`, so you put `await` in front of the call:

| Ordinary C | On Braam |
|---|---|
| `n = read(fd, buf, len);` | `n = await read(fd, buf, len);` |
| `n = write(fd, buf, len);` | `n = await write(fd, buf, len);` |
| `fd = open(path, O_RDONLY);` | `fd = await open(path, O_RDONLY);` |
| `close(fd);` | `await close(fd);` |
| `c = fgetc(f);` | `c = await fgetc(f);` |
| `fgets(s, n, f);` | `await fgets(s, n, f);` |
| `stat(path, &st);` | `await stat(path, &st);` |
| `fflush(stdout);` | `await fflush(stdout);` |
| `sleep(1);` | `await sleep_ms(1000);` |

A function of yours that calls any of these must itself be a
`coro(braam_call *)` coroutine, and its callers must `await` it, up to `main`.
Functions that never wait (`strlen`, `malloc`, `memcpy`, `qsort`, `snprintf`, …) are
ordinary and are called as usual.

### Output

`printf`, `puts` and `putchar` do not wait: they add to the output buffer. The buffer
is written out when you `await fflush(stdout)`, when a read from standard input
starts, and when `main` returns. So a prompt appears before the program reads the
answer, and nothing is lost at the end.

### Ending the program

Return from `main`. The return value is the exit status.

`exit()` cannot return through the coroutines that are running. On Braam it records
the status and stops the program as a crash. Return from `main` instead.

### `^C` and other signals

By default `^C` ends the program, with exit status 130, and so does `kill`. A program
that wants to be told instead asks once:

```c
#include <signal.h>

if (await sig_catch(SIGINT, 1) < 0)
    perror("sig_catch");
```

There are no signal handlers. A signal the program asked for is recorded, and each
call the program is waiting in gives up with `-1` and `errno` set to `EINTR`. That
covers a read of the terminal or of a pipe, `sleep_ms`, `braam_yield` and `poll`.
`sig_take(SIGINT)` then says whether it was `^C`, and forgets it:

```c
n = await read(0, buf, sizeof buf);
if (n < 0 && errno == EINTR && sig_take(SIGINT))
    ...                     /* interrupted, and still running */
```

`SIGINT`, `SIGTERM` and `SIGWINCH` (the terminal changed shape) can be asked for;
`SIGKILL` cannot.

The kernel can deliver a signal only while the program waits for something. A long
computation that never `await`s cannot be interrupted. Call `await braam_yield();`
from time to time inside such a loop. It gives the kernel a turn:

```c
while (!sig_take(SIGINT)) {
    work_a_little();
    await braam_yield();
}
```

### Two things at once

`main` runs as one task. A program that must wait for two things at once, say the
keyboard and a timer, starts a second task with `braam_spawn`. A task is a
`coro(braam_call *) int` coroutine with a frame of its own, from `co_init`:

```c
static coro(braam_call *) int clock_task(void)
{
    while (await sleep_ms(1000) == 0) {
        printf("tick\n");
        await fflush(stdout);
    }
    return 0;
}

    static char storage[4096];                 /* the task's frame and arena */
    braam_task *t = co_init(storage, sizeof storage, clock_task);
    braam_spawn(t);                            /* runs it to its first wait */
```

From then on the runtime resumes each task when the call it waits for is answered,
so the two run in turns. They share memory, but neither runs while the other does,
so no locks are needed. A process has at most `BRAAM_TASKS` (8) tasks, `main`
included; `braam_spawn` returns 0 when the table is full. A task that returns leaves
the table, and `co_done(t)` and `co_result(t)` tell its result. `co_destroy(t)` stops
a task that is waiting. The program ends when `main` returns, whatever the other
tasks are doing.

`poll` (`<poll.h>`) is the other way to wait for several descriptors at once.

### Cleanup on Braam

Use `defer` for memory, buffers and counters. A file descriptor is closed by `await
close(fd)`, which cannot be in a `defer`, because a deferred statement may not
`await`. Close descriptors yourself at the point you choose.

---

## 9. Quick reference

```c
#include <coro.h>

coro(Y) T g(params) { ... }             /* declare a coroutine: yields Y, returns T */

yield e;                                 /* stop, hand e to the caller (Y not void) */
yield;                                   /* stop (Y is void) */
co_signal s = (yield e);                 /* CO_CONTINUE or CO_CANCEL */

T r = await g(args);                     /* run g to its end, passing its yields up */
T r = await f;                           /* the same, on a frame you made */

defer statement;                         /* run statement when this block is left */

co_frame(Y, T) *f = co_alloca(g, extra, args);   /* until the end of this block */

static _Alignas(16) char mem[N];
co_frame(Y, T) *f = co_init(mem, sizeof mem, g, args);   /* in memory you own */

co_status st = co_resume(f);             /* CO_SUSPENDED or CO_DONE */
co_status st = co_cancel(f);             /* resume, yield returns CO_CANCEL */
co_destroy(f);                           /* run pending defers, finish */
int done = co_done(f);
Y v = co_value(f);                       /* after CO_SUSPENDED */
T r = co_result(f);                      /* after CO_DONE */
size_t n = co_sizeof(g), a = co_alignof(g);   /* not constants */

coro_ptr(Y, T) p = g;                    /* g takes (void) or (void *) */
co_frame(Y, T) *f = co_alloca(p, extra, arg);    /* any operation above, through p */
T r = await p(arg);
```

| Short name | Long name |
|---|---|
| `coro(Y)` | `_Coro(Y)` |
| `yield` | `_Yield` |
| `await` | `_Await` |
| `defer` | `_Defer` |
| `co_frame(Y, T)` | `_Coro_frame(Y, T)` |
| `coro_ptr(Y, T)` | `_Coro_ptr(Y, T)` |
| `co_alloca`, `co_init` … `co_alignof` | `__co_alloca`, `__co_init` … `__co_alignof` |

---

## 10. The frame ABI

This section is for code that meets a coroutine without going through the compiler's
operations: a scheduler written in assembly, a debugger, or another compiler's code
linked with vcc's. A C program never needs it.

### What a unit defines

For `coro(Y) T f(A a, B b)`, the unit that defines `f` emits three symbols, global
for a global `f` and local for a `static` one:

| Symbol | C type | What |
|---|---|---|
| `f$resume` | `int (char *fp)` | the body, resumed: runs to the next suspension and returns `CO_SUSPENDED` (0), or to the end and returns `CO_DONE` (1) |
| `f$init` | `void (char *fp, A a, B b)` | stores the arguments in a frame `__coro_setup` prepared |
| `f$co` | `size_t[2]` | the descriptor: the frame's size (a multiple of its alignment) and its alignment (up to 16) |

A coroutine that takes `(void)` or `(void *)` has a four-word descriptor instead: the
size, the alignment, an init function of type `void (char *fp, void *arg)` and
`f$resume`. The init function is `f$init` for `(void *)`; for `(void)` it is a thunk
`f$initp` that ignores `arg`. A `coro_ptr` is the address of this descriptor. The
`$` in the names keeps them out of C's name space.

Arguments and results follow the target's C calling convention, as for any function.

### The frame

A frame starts with a header that the runtime and every unit agree on, the C structure

```c
struct co_header {
    unsigned state;          /* 0 created; k >= 1 suspended at the k-th point;
                                UINT_MAX - 1 done; UINT_MAX destroyed */
    unsigned flags;          /* bit 0 running; bits 1-2 the signal of this resumption,
                                0 continue, 1 cancel, 2 destroy */
    int (*resume)(char *);   /* the coroutine's f$resume */
    char *task;              /* the root frame of the task this frame belongs to */
    char *top;               /* the next free byte of the task's arena (in a root frame) */
    char *limit;             /* the end of the arena (in a root frame) */
};
```

laid out as the target lays out structures: 24 bytes on wasm32 and the other ILP32
targets (`resume` at 8), 40 on LP64 (`resume` at 8), 12 on AVR and MSP430 (`resume`
at 4). Then, at offsets that depend only on `Y` and `T`:

- the **value** last yielded, after the header, rounded up to `Y`'s alignment, absent
  when `Y` is `void`;
- the **result**, after the value, rounded up to `T`'s alignment, absent when `T` is
  `void`.

Everything after that, the arguments and the locals that live across a suspension, is
laid out by the defining unit and may change whenever the coroutine is recompiled. So
a holder of a `co_frame(Y, T) *` can read the state, the value and the result without
knowing which coroutine it is, and nothing more.

### The runtime

The operations are calls of these routines, in `libc.a` (`libc/common/co.c`; on the
hosted targets, in `libvcc.a`):

```c
void *__coro_setup(void *storage, size_t bytes, const size_t *desc,
                   int (*resume)(void *), void *parent);
int   __coro_resume(void *frame, int signal);       /* 0 resume, 1 cancel, 2 destroy */
int   __coro_done(void *frame);
void *__coro_value(void *frame, unsigned offset);   /* checks the state, returns frame + offset */
void *__coro_result(void *frame, unsigned offset);
void *__coro_push(void *frame, size_t bytes, size_t align, const char *name);
void  __coro_pop(void *frame, void *p);
```

- `co_init(mem, size, f, a, b)` is `__coro_setup(mem, size, f$co, f$resume, 0)`, then
  `f$init(mem, a, b)`. With no parent the frame is the root of its own task, and the
  bytes of `mem` past the frame are that task's arena.
- `co_alloca(f, extra, ...)` does the same on `(size + extra)` rounded up to 16 bytes,
  taken in a function from the stack (`__builtin_alloca`, given back by
  `__builtin_stack_restore` to what `__builtin_stack_save` returned), or in a coroutine
  with `__coro_push` from the arena of the task.
- `co_resume`, `co_cancel` and `co_destroy` are `__coro_resume` with signal 0, 1 and 2.
  It checks the state and the running bit, sets the flags, calls the frame's
  `resume`, and clears the flags. A destroy of a frame that never started only marks
  it destroyed.
- `await g(args)` takes `g`'s frame with `__coro_push` (exactly `size` bytes), sets it
  up with the awaiting frame as `parent`, so it joins the same task, and gives it back
  with `__coro_pop` when `g` has finished or been destroyed.
- A trap prints `coroutine trap: CO_TRAP_…` and ends the program with status 255.

To run a coroutine from code of its own, a scheduler needs only `f$co`, `f$resume`,
`f$init`, the header and these routines.
