# em — uEmacs/PK, the MicroEMACS screen editor

`C-x C-f` to find a file, `C-k` and `C-y` to move text about, `M-x` for a
command by name, and a macro language with `!if` and `!while` in it. No modes:
you type and the characters go in.

**This is uEmacs/PK 4.0.15**, which is MicroEMACS 3.9e as Petri Kutvonen left
it at the University of Helsinki in 1991 and as Linus Torvalds has kept it
since. Dave Conroy wrote MicroEMACS in 1985 and Daniel Lawrence took it through
3.9e; the copyright notices are on every file and in [LICENSE](LICENSE).
Upstream's own README is honest about what it is:

> This is a *bad* editor. Really. Unless you have used it since the 1980s, and
> have untrainable fingers, you should absolutely walk away and never look back.

Which is a reason to port it, not a reason not to: it is 14 000 lines that
fit in 64 KB in 1985, and it is the shape of a screen editor with nothing left
out — buffers, windows, a kill ring, a search, a macro language, and an escape
to the shell.

## Using it

```
em file           # edit it
em                # a scratch buffer
em +12 file       # start on line 12
em -v file        # read only
em -spat file     # start at the first line matching pat, no space after -s
```

The keys are upstream's, and [emacs.hlp](emacs.hlp) — which `M-?` shows — is
the whole of them. The ones worth knowing before you start:

| | |
| --- | --- |
| `C-x C-f` | find a file, with TAB completing the name |
| `C-x C-s` | save it |
| `C-x 2`, `C-x o`, `C-x 1` | split the window, move between halves, close the others |
| `C-s`, `C-r` | search forward and back, ending the pattern with ESC |
| `M-r` | replace |
| `C-k`, `C-y` | kill to end of line, and yank it back |
| `M-space`, `C-w`, `M-w` | set the mark, then cut or copy to it |
| `C-x !` | run a shell command |
| `M-x` | a command by name |
| **`M-Z`** | **save everything and leave** — see below |

## What the port changed

No libc: no `malloc`, no `printf`, no `errno`, no `<string.h>`, no exceptions
and no `setjmp`. [braam.cpp](braam.cpp) supplies the string and character
routines uemacs calls, over `heap_alloc`, plus the six printf conversions the
message line uses; everything else was replaced rather than reimplemented.

| Upstream | Here |
| --- | --- |
| `tcap.c` — termcap, `tgetent`, `tputs` padding, `tgoto` | [screen.cpp](screen.cpp): the screen is an array of cells, so a capability is a constant and cursor addressing is indexing |
| `posix.c` — `termios` raw mode, `FIONREAD`, a 0.1 s wait to tell an arrow key from an ESC | the same file: a named key arrives whole, so there is nothing to tell apart and no line discipline to turn off |
| `read(0, …)` from wherever a key was wanted | one `next_key()`, at the bottom of `ttgetc()`, which is the only place the editor parks |
| `write(1, …)` from the painter | a back buffer of cells; the diff and the blit happen in `ttgetc()`, so a keystroke costs one syscall carrying the cells that changed |
| a `SIGWINCH` handler and `TIOCGWINSZ` | the geometry rides on every key read, and `checkwinsize()` acts on it from a place that may paint |
| `system()` — fork, exec, wait | `spawn()` with the screen and the keyboard handed over and taken back |
| a filter driven through `<fltinp >fltout` | the same two files, which is what upstream did too |
| `flock()` on a dev/ino key (`lock.c`) | gone. Braam has no advisory locking and no inode; `file_changed()` is what still catches somebody else's write |
| hunspell (`spell.c`) | built without it, which upstream's own Makefile supports: spell mode exists and never finds a misspelling |
| `stat()`'s `st_dev`, `st_ino`, `st_mtim` | `FileInfo`'s kind, size and millisecond mtime |
| `system("echo pat* >/tmp/meXXXXXX")` for TAB completion | `list_dir()` on the directory part — no shell, no temp file, and no `mkstemp` |
| `epath.h`'s `/usr/local/lib/` search path, and `$HOME/lib` | `/etc/emacs.rc` and `/etc/emacs.hlp`, with the package's own `share/` behind them — found by reading `/pkg/bin/em` ([epath.cpp](epath.cpp)) |
| `exit()` from wherever | a flag the command loop tests |
| K&R-era C with `int` command functions | C++20, where every bound command is a `Task<int>` |

## Structure

- **Every bound command is a coroutine, and there was no smaller cut.** The one
  blocking read sits under `ask_string()`, `ask_yesno()` and `getcmd()`, which
  nearly every command calls, and the commands are reached through two
  function-pointer tables — `names[]` and `keytab[]`, both `int (*)(int, int)`.
  Changing `fn_t` to `Task<int> (*)(int, int)` converts the table's hundred and
  seventy entries at once. 244 of the tree's 377 functions ended up as `Task`s.

- **The painter did not, and that is the point of the back buffer.** Only four
  of `display.cpp`'s thirty-three functions convert — `update()`, `update_now()`,
  `cmd_update_screen()` and `checkwinsize()` — because painting writes to
  memory. `ttputc()`, `paint_window()`, `modeline()` and `msg_printf()` are
  plain functions, exactly as they were.

- **A `co_await` is a call here, not a tail call, and that is a hard limit.**
  The wasm tail-call feature is off, so entering a task and returning from it
  each leave a frame on the native stack, and it is only given back when
  something *suspends* — a syscall unwinds the whole stack, and the kernel
  re-enters at `_resume`. A loop that awaits without ever suspending therefore
  grows the stack until it overflows, and the process traps. Three places had
  to answer it:

  - `fgetbyte()` in [fileio.cpp](fileio.cpp) is a plain function over the
    buffer, and `frefill()` is the awaited half. Awaiting once per byte grew
    the stack by the length of the file.
  - `forwchar`, `backchar`, `forwline` and `backline` in
    [basic.cpp](basic.cpp) are plain functions again, as they were named in
    MicroEMACS 3.9 before this fork renamed them; `cmd_forward_character()` and
    the other three are one-line `Task`s over them, and every word motion,
    paragraph motion and fill loop calls the plain ones a character at a time.
  - `dobuf()` in [exec.cpp](exec.cpp) calls `exec_yield()` once round its line
    loop, which parks on a zero-length sleep every 32 lines. Without it a macro
    file overflowed at about 450 lines; with it, 5000 is fine.

- **`exit()` is a flag.** After `tcap.c`, `posix.c` and `usage.c` went, the
  live `exit()` calls were `cmd_exit_emacs()` and `edinit()`'s allocation
  failure. A coroutine cannot be unwound from a depth, so `cmd_exit_emacs()`
  raises `quitting` and returns; `execute()` stops, and the `loop:` in
  `proc_main()` falls out to `display_close()` and the status. No mechanism,
  three checks.

- **`emacs.rc` and `emacs.hlp` ship in the package.** They land in
  `/pkg/store/uemacs-<version>/share/`, a path carrying a version the binary
  does not know — so at startup [epath.cpp](epath.cpp) reads the link `PATH`
  found, `/pkg/bin/em`, and takes the directory of its directory. `readlink`
  follows the directories on the way without following the leaf, so one
  syscall recovers the prefix; [converters/iconv](../../converters/iconv/)
  finds its character tables the same way. `$EMACS_PREFIX` overrides it, and a
  candidate is accepted only if `emacs.hlp` is under it, so a stale link
  cannot win.

  **The packaged copy is last**, where upstream's install directories were, so
  every copy on disk wins over it and none is shadowed:

  | the startup file, in order | the help file |
  | --- | --- |
  | `$HOME/.emacsrc` | `$HOME/emacs.hlp` |
  | `/etc/emacs.rc` | `/etc/emacs.hlp` |
  | `./.emacsrc` | `./emacs.hlp` |
  | each `$PATH` entry | the same |
  | `<store>/share/`[`emacs.rc`](emacs.rc) | `<store>/share/`[`emacs.hlp`](emacs.hlp) |

  `$HOME` is `/home` unless the environment says otherwise, and both the `/etc`
  and the packaged spelling drop the leading dot, because neither place is a
  home directory — upstream looked in `$HOME/lib` and then in the directories
  its Makefile installed into, and that is what this replaces. `em @file` and
  `execute-file` take a path and do not search at all.

  Two more files ride along as payload and nothing looks for them:
  `share/emacs.pdf`, upstream's manual, and `share/UTF-8-demo.txt`.

## Differences from upstream worth knowing

- **`C-x C-c` cannot be typed, and `M-Z` is the way out.** ^C reaches whatever
  is in front of the console whatever it has claimed — a full-screen program
  that stops answering must still be killable — so it arrives as `SIG_INT` and
  never as a key. The port makes that signal the abort key, which is what every
  other `^C` in the editor already means; `C-x C-c` is therefore unreachable,
  and quitting is `M-Z` (quick-exit, which saves) or `M-x exit-emacs`.
- **`C-x C-z` and `suspend-emacs` are gone.** `SIG_TSTP` is not in Braam's
  catchable set and nothing stops a process to give the shell its prompt back;
  the command says `(No job control)` rather than pretending.
- **`$typahead` is always zero, and every keystroke repaints.** Upstream asked
  the tty with `FIONREAD` and skipped a repaint while somebody was typing
  ahead. The keyboard queue is the kernel's and no syscall peeks at it, so only
  the pushback is known — and a repaint costs the cells that changed, which is
  not the bargain it was at 1200 baud.
- **The screen has colour**, which a termcap terminal did not: the message line
  is cyan. The mode line keeps the reverse video it always had, as a cell
  attribute rather than an escape sequence.
- **`emacs.rc` is UTF-8.** Upstream's was ISO 8859-1, for the `insert-string
  "ä"` macros on a Nordic keyboard; the whole system is UTF-8 here, so the file
  was converted and the macros still insert the letters they name.
- **`$TERM` is not set and `emacs.rc`'s Sun branch never runs.** There is no
  terminal type, because there is no terminal.
- **The shell escapes run Braam's `/bin`,** which is forty-odd commands and not
  a Unix: `C-x !` works, `sort` and `tr` are not there to be run.
- **A file replaced by rename is only caught if its size or mtime moved.**
  Upstream keyed the "changed under us" check on device and inode as well.
- **The screen is used up to 128 rows and 512 columns**, which is the widest
  grid the kernel will make and a back buffer of 512 KB.
- **`(End)` after a shell escape is written on the console, not through the
  message line.** The message line is in the back buffer, which is not sent
  while the screen claim is the child's — and taking the screen back is what
  paints over the output the pause exists to let you read. So the prompt is
  bytes on stdout, the keyboard is taken for the one keystroke and given
  straight back, and the screen is retaken after.

## Files

Upstream's file split and its names are kept; `.c` became `.cpp`.

| Here | Upstream |
| --- | --- |
| `main.cpp` | `main.c` — the entry point, the command loop, and the quit flag |
| `display.cpp` | `display.c` — the painter, which is still synchronous |
| `screen.cpp` | `tcap.c` + `posix.c` — the terminal, which is no longer a terminal |
| `fileio.cpp` | `fileio.c` — reading and writing, over the syscalls |
| `file.cpp` | `file.c` — find, read, insert, write |
| `buffer.cpp` `window.cpp` `line.cpp` | the same, unchanged but for the awaits |
| `basic.cpp` | `basic.c` — the motions, and the four plain ones under them |
| `word.cpp` `region.cpp` `random.cpp` | the same |
| `search.cpp` `isearch.cpp` | `search.c`, `isearch.c` — the patterns are untouched |
| `exec.cpp` `eval.cpp` | the macro language, and `exec_yield()` |
| `bind.cpp` `names.cpp` `ebind.cpp` | the tables. `ebind.c` was `ebind.h`, included once by `main.c`: a table of coroutine addresses has to live in a file that does not also call them |
| `spawn.cpp` | `spawn.c` — the escapes, over `spawn()` |
| `spell.cpp` `utf8.cpp` `globals.cpp` `version.cpp` | the same |
| `braam.cpp` / `braam.h` | — the C library uemacs calls |
| `epath.cpp` | — where the package's own `share/` is, found at startup |
| — | `lock.c`, `usage.c`, `wrapper.c` deleted |

`tmp/` holds the upstream tree the port was made from.

## Building and packaging

```
make                     # build/editors/uemacs/em.wasm, about 270 KB
make package             # uemacs-4.0-r1.zip: bin/em, and four files in share/
make test                # the seven cases below
```

## Testing

Seven cases, each booting the kernel, planting `em`, and driving it through
the grid — keys in, cells out, because uemacs has no command mode and no way
to be driven down a pipe. [test/emlib.mjs](test/emlib.mjs) is the shared half.

- `emkeys.mjs` — the first frame whole and its three colours, the mode line's
  reverse video, `C-f`/`C-b`/`C-n`/`C-p`/`C-a`/`C-e`, the arrow keys reaching
  the same commands, `M-<` and `M->`, `C-u` with and without a count, and the
  word motions.
- `emedit.mjs` — self-insert, `C-d`, `C-h`, `C-o`, `C-k` and `C-y` through the
  kill buffer, a region with `M-space`/`C-w`/`M-w`, `M-u`/`M-l`/`M-c`, and the
  two files `C-x C-s` and `C-x C-w` write.
- `emfiles.mjs` — `C-x C-f` on a file that is there and one that is not,
  `C-x b`, `C-x C-v`, `C-x C-r`, `C-x C-i`, and the TAB completion offering
  each match in turn and starting over when they run out.
- `emsearch.mjs` — `C-s`, `C-r`, a search that fails, the incremental search
  and the key that ends it running as a command, `M-r`, and a regexp under
  MAGIC.
- `emmacro.mjs` — that the packaged `emacs.rc` ran and that one on disk
  overrides it, `!while`/`!if`/`&add`/`&cat`, `store-macro` with
  `bind-to-key`, a keyboard macro, a `$` variable, `M-?` opening the packaged
  `emacs.hlp`, and what happens with no package at all.
- `emwindow.mjs` — `C-x 2`, `C-x o`, `C-x 1`, `C-x z` with one window, and a
  resize both ways with the buffer and the cursor kept.
- `embang.mjs` — `C-x !` with its output on the console under the pause, the
  key that resumes, `C-x @` opening a window on a command's output, and
  `C-x #` filtering the buffer.
