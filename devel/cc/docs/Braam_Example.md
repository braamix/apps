# A program for Braam, worked through

Braam is a small operating system that runs in a
browser tab. Each program in it is a WebAssembly module, and every call it makes to
the system answers later, not at once. `vcc -t wasm32-braam` builds such a program
from C. The program is written with vcc's coroutines (see
[Coroutines_in_C.md](Coroutines_in_C.md)): a function that waits for the system is a
coroutine, and its callers `await` it.

This document follows one program from the source to a running process, in two
places: under node with a stand-in kernel, and on Braam itself. The program is
[examples/notes.c](examples/notes.c), a notebook kept in a file. [Braam.md](Braam.md)
is the reference for the target.

```
notes                list the notes, numbered
notes add WORDS...   add a note
notes del N          delete note N
notes ask            add the lines typed, one note each, until ^D
```

## 1. The source

Most of it is ordinary C. Three things differ.

**`main` is a coroutine.** On this target it must be written:

```c
coro(braam_call *) int main(int argc, char **argv)
```

Its yield type, `braam_call *`, is a request to the kernel. The runtime starts
`main`, hands each request it yields to the kernel, and resumes `main` with the
answer. What `main` returns is the exit status. A plain `int main(...)` is a
compile-time error on this target.

**A call that waits is awaited.** Any library call that has to wait for the system is
a coroutine. You call it with `await` in front, and any function that does so must be
a coroutine too:

```c
static coro(braam_call *) int list(void)
{
    FILE *f = await fopen(NOTES, "r");
    ...
    while (await fgets(line, sizeof line, f))
        printf("%3d  %s", ++n, line);
    await fclose(f);
```

The compiler checks this: an `await` outside a coroutine, or one whose yield type is
not `braam_call *`, is an error. Calling a coroutine without `await` is an error too.
So a port is mostly a matter of following the compiler's messages: put `await` in
front of each call that waits, then `coro(braam_call *)` on each function that makes
one, up to `main`.

**Output never waits; input does.**

- `printf`, `fprintf`, `fputs`, `fputc` and `fwrite` only append to the stream's
  buffer, so they are ordinary functions.
- The buffer is written out by `await fflush(f)` or `await fclose(f)`. When `main`
  returns, the runtime writes out every stream that is still open.
- Reading standard input writes out standard output first. That is how `notes ask`
  shows its prompt before it waits for the answer:

```c
    for (;;) {
        printf("note? ");
        if (!await fgets(line, sizeof line, stdin))
            break;
```

Two limits are worth knowing.

- **Cleanup that waits cannot be deferred.** `defer await fclose(f)` is an error:
  a deferred statement may not suspend. So `notes` closes its files explicitly.
- **Use `return` from `main`, not `exit()`.** `exit()` reports the status and stops
  the process at once. Nothing unwinds, and buffered output is lost.

## 2. Building

```sh
vcc -t wasm32-braam notes.c -o notes.wasm
```

The driver compiles `notes.c` against the Braam headers and runtime installed in
`share/vcc/wasm32-braam/`. It links the module the way a Braam process must be
linked:

- it imports its memory and has no entry point;
- it exports the five functions the kernel calls;
- it carries a `braam` section that gives the process ABI version and the starting
  memory, 4 pages unless `--initial-pages=N` says otherwise.

The result is about 33 KB.

## 3. Running without Braam

The runtime directory holds `run.mjs`, a stand-in kernel for node. It checks that
the module is a Braam process, then serves its calls from the host: files relative
to the current directory, standard input and output, and the clock. Each answer
arrives later, as Braam's would.

```
$ R=~/.local/share/vcc/wasm32-braam/lib/run.mjs
$ node $R notes.wasm
no notes
[exit 0]
$ node $R notes.wasm add buy milk
[exit 0]
$ node $R notes.wasm add call the plumber
[exit 0]
$ node $R notes.wasm ask
note? water plants
note? write letter
note? ^D
2 added
[exit 0]
$ node $R notes.wasm del 2
[exit 0]
$ node $R notes.wasm
  1  buy milk
  2  water plants
  3  write letter
(3 notes, 35 bytes)
[exit 0]
```

`[exit N]` is the exit status, written to standard error. A trap reports itself and
exits with 255. A process left waiting with no answer coming exits with 254.

## 4. Running on Braam

The same file runs on Braam unchanged. To get it there, run `fimport` in Braam's
shell and choose the file in the browser's picker; it lands in `/import`. Run it by
its path, `/import/notes.wasm`, or copy it into `/bin` to run it by name. Braam
replaces `/bin` when a new version of the system is opened, so the copy does not
last past that.

```
home $ cp /import/notes.wasm /bin/notes
home $ notes add buy milk
home $ notes add call the plumber
home $ notes ask
note? water plants
note? write letter
note?
2 added
home $ notes del 2
home $ notes
  1  buy milk
  2  water plants
  3  write letter
(3 notes, 35 bytes)
```

Here `notes ask` reads the terminal. The line editor is Braam's, and `^D` ends the
input.

## 5. Where this is tested

- **Under node:** `wasm32-tests`' `BraamTest.NotesExample` builds `examples/notes.c`
  with the in-tree driver and runs this session on `run.mjs`.
- **On Braam:** the ctest `braam-system` runs the same session, typed at the
  terminal, in a session of braam-core's own system harness. Before that, braam-core's
  `abi.mjs` checks the binary. This test is enabled when a built braam-core is at
  `../../Braam/braam-core` or wherever `-DBRAAM_CORE=` points.

## 6. What the library offers

[Braam.md](Braam.md) §3 lists the library: which calls wait and must be awaited,
which do not, and where it differs from Unix (`open`'s flags, `errno`'s numbers,
`stat`, signals, tasks). §4 there is a checklist for porting a program.
