# cc on Braam

`cc` is vcc, a C11 compiler with `defer` and coroutines, running on Braam
and building Braam programs. This is its user's guide: how to run it, what
a program for Braam looks like, what the C library offers, and how to port
a program. [Coroutines_in_C.md](Coroutines_in_C.md), beside this file, is
the tutorial and reference for the language extension.

## 1. A first program

In `hello.c`:

```c
#include <stdio.h>

coro(braam_call *) int main(int argc, char **argv)
{
    await printf("Hello, %s!\n", argc > 1 ? argv[1] : "world");
    return 0;
}
```

```sh
cc hello.c
./a.out
```

`main` is a coroutine: a Braam program never waits inside a call, so every
call that could wait — a read, a write, a `printf`, an open, a sleep — is
a coroutine and is called with `await`. Everything else is ordinary C11.

The package installs the command `cc` and needs `wasm`, whose `as` and
`ld` do the last two stages. The examples are in the package's `share/`,
`/pkg/store/cc-<version>/share/`: `hello.c`, and `notes.c`, a notebook
kept in a file.

## 2. Running the compiler

```text
cc [options] file...
```

It is a driver, as Unix `cc` is: it runs one program per stage and passes
files between them.

```text
vcpp       preprocess    .c   -> .i
vparse     parse         .i   -> .ast
vlower     lower + opt   .ast -> .tac
vgenwasm   code gen      .tac -> .s
as         assemble      .s   -> .o
ld         link          .o   -> a.out
```

| Option | Meaning |
| --- | --- |
| `-c` | compile and assemble, do not link |
| `-S` | compile only, to assembly (`.s`) |
| `-E` | preprocess only; `-P` drops the line markers |
| `-o file` | the output (default `a.out` for a link) |
| `-v` | echo each stage's command |
| `-Dname[=v]`, `-Uname`, `-Ipath` | for the preprocessor |
| `-Lpath`, `-lname` | for the linker, after the objects |
| `--initial-pages=N` | the process's initial memory, in 64 KiB pages (default 4) |
| `-nostdinc`, `-nostdlib` | no standard headers; no `crt0.o` and `libc.a` |
| `-O`, `-g`, `-W…`, `-std=…` | accepted and ignored |

Inputs are `.c`, `.S` (preprocessed assembly), `.s`, `.o` and `.a`. The
status is 0 on success and 1 on any failure. A compile error is reported
as `file:line:column: error: ...`, and on Braam the first one ends the
compile.

The intermediate files are in `/tmp`. The passes are in the package's
`libexec/`, the headers in `share/vcc/wasm32-braam/include/`, and
`crt0.o` and `libc.a` in `share/vcc/wasm32-braam/lib/`; `cc` finds them
through the link `/pkg/bin/cc` leads to. `VCC_CPP`, `VCC_PARSE`,
`VCC_LOWER`, `VCC_GEN`, `VCC_AS` and `VCC_LD` name another program for a
stage.

## 3. How a program runs

The kernel *steps* a process: it calls the program, which runs until some
call has to wait, hands that request to the kernel and returns. When the
answer comes the program is called again and goes on from where it was.
The runtime turns this into coroutines: `main` and every call that waits
are coroutines, and `await` is the point a process can stop at.

**Ending.** `main`'s return value is the exit status. When it returns, the
`atexit` functions run and every stream is written out. `exit(n)` does the
same from anywhere, but nothing unwinds on the way: no `defer` runs.

**The arena.** Every `await` takes its callee's frame from a 64 KiB arena
behind `main`'s. A program whose `await` chains go deep, recursion for
instance, makes it larger by defining

```c
const unsigned __braam_task_bytes = 256 * 1024;
```

When it runs out, the program stops with `coroutine trap:
CO_TRAP_NO_SPACE:` and the name of the coroutine that did not fit.

**Tasks.** A process has up to 8 tasks, `main`'s included, each waiting on
its own request. `braam_spawn(frame)` starts one: `frame` is a
`braam_task *`, usually made by `co_init` on storage that outlives it; the
storage after the frame is that task's arena. Tasks run in turns and need
no locks. `co_done` and `co_result` on its frame tell how one ended, and
`co_destroy` stops one. The process ends when `main` returns.

**Signals.** There are no signal handlers. A signal the program did not
ask for ends it with status 130: `^C`, or `kill`. `await sig_catch(SIGINT,
1)` asks for one instead (`SIGINT`, `SIGTERM` or `SIGWINCH`); then a call
waiting for ever gives up with -1 and `errno` `EINTR`, and `sig_take(sig)`
says whether `sig` came. A signal reaches a process only while it waits,
so a long computation should `await braam_yield()` now and then.

## 4. The library

| Header | Calls that wait (`await` them) | Calls that do not |
| --- | --- | --- |
| `<stdio.h>` | `printf`, `fprintf`, `vprintf`, `vfprintf`, `puts`, `putchar`, `fputs`, `fputc`, `putc`, `fwrite`, `perror`, `fopen`, `fclose`, `fflush`, `fgetc`, `getc`, `getchar`, `fgets`, `fread`, `fseek`, `ftell`, `rewind`, `remove`, `rename` | `sprintf`, `snprintf`, `vsprintf`, `vsnprintf`, `ungetc`, `feof`, `ferror` |
| `<unistd.h>` | `read`, `write`, `close`, `lseek`, `unlink`, `rmdir`, `chdir`, `getcwd`, `readlink`, `isatty`, `access` | `getpid`, `getopt` |
| `<fcntl.h>` | `open` | |
| `<sys/stat.h>` | `stat`, `lstat`, `fstat`, `mkdir` | `S_ISDIR`, `S_ISREG`, `S_ISLNK` |
| `<stdlib.h>` | `realpath`, `mkstemp`, `mkstemps` | `malloc` and the rest, `strtol` and `strtod` families, `qsort`, `bsearch`, `getenv`, `atexit`, `exit` |
| `<spawn.h>`, `<sys/wait.h>` | `posix_spawn`, `posix_spawnp`, `waitpid`, `wait` | |
| `<time.h>` | `time` | `gmtime`, `localtime`, `strftime` |
| `<poll.h>` | `poll` | |
| `<braam.h>` | `braam_sys`, `sleep_ms`, `braam_yield`, `sig_catch` | `braam_now`, `braam_spawn`, `sig_take`, `sig_pending` |

`<string.h>`, `<ctype.h>`, `<math.h>` (musl's), `<getopt.h>`'s
`getopt_long` and the rest of C11's headers are there too, and none of
them waits.

Every call that waits keeps C's name and parameters, so
`n = read(fd, buf, len)` becomes `n = await read(fd, buf, len)`. Where it
differs from Unix:

- **Output waits.** `printf` and the rest put their text into the
  stream's buffer of `BUFSIZ` bytes, and wait while it is written out when
  it is full. It is also written out by `await fflush(f)`, `await
  fclose(f)`, at the end, and, for standard output, before every read of
  standard input, so a prompt shows. `stderr` is written out at the end of
  each call. `sprintf` and `snprintf` only write memory, and do not wait.
- **`open`'s flags** are Braam's bits; use the names. A mode argument is
  accepted and ignored.
- **`errno`.** Braam's errors are numbered from 33 up; `strerror` and
  `perror` know them.
- **`stat`** gives a kind, a size and a time. The mode is 0644 for a file
  and 0755 for a directory, and `st_ino` is a hash of the path.
- **`sleep_ms(ms)`** is the sleep; `braam_now()` is milliseconds since
  boot.
- **`long` is 32 bits**, as are `size_t` and pointers; `long long` is 64.
  `long double` is binary128.

Not available: `scanf` and its family, `tmpfile`, `signal()` and
`raise()`, `setjmp` and `longjmp`, `fork` and `exec` (`posix_spawn` is
the way), and reading directories.

## 5. Porting a program

The compiler names most of the work. It rejects a call of a coroutine
without `await`, and an `await` in a function that is not a coroutine. So:

1. Change `main` to `coro(braam_call *) int main(int argc, char **argv)`.
2. Compile, and put `await` in front of each call it names.
3. Make each function that now holds an `await` a `coro(braam_call *)`
   coroutine, with the same parameters and return type, and compile again.
   Its callers are named next. Repeat until it compiles.

Then what the compiler cannot see:

- **Cleanup that waits.** A deferred statement may not `await`, so `defer
  await fclose(f)` is an error. Close files explicitly; `defer` is still
  right for memory.
- **Function pointers.** A coroutine has none. A table of handlers that
  wait becomes a table of `coro_ptr(braam_call *, int)`, or a `switch`.
- **Callbacks.** A function called through an ordinary pointer, as
  `qsort`'s comparison is, cannot wait.
- **Plain code cannot print.** A function called through a pointer, an
  `atexit` function, a deferred statement and a coroutine of another
  yield type cannot `await printf`. Format with `snprintf` and print the
  text from a coroutine that can.
- **Variadic functions** can be coroutines, as `printf` is. A structure
  cannot be one of their variable arguments.
- **`alloca`** is an error in a coroutine, since its frame outlives the
  stack.
- **A loop that never waits** cannot be interrupted; give it an `await
  braam_yield()`.
- **Deep recursion through `await`** needs a larger `__braam_task_bytes`.
- **Names.** A local variable may not have the name of a function in
  scope; rename the variable.

A program that is plain C everywhere but at its edges — read the input
whole, work, write the output — needs coroutines only in `main` and the
two edges. That is how `cc` itself was ported: its passes read their input
whole, and every function that writes output is a coroutine.
