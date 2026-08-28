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
| `Home`, `End`, `Delete` | the same as `C-a`, `C-e` and `C-d` |
| `F1` | the help browser — `M-?` is the same key |
| `M-space`, `C-w`, `M-w` | set the mark, then cut or copy to it |
| `C-x !` | run a shell command |
| `M-x` | a command by name |
| `C-x C-c` | leave, asking about a changed buffer |
| **`M-Z`** | **save everything and leave** |

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

- **`^C` is a signal, and the port hands the keystroke back.** ^C reaches
  whatever is in front of the console whatever it has claimed — a full-screen
  program that stops answering must still be killable — so it arrives as
  `SIG_INT` and never as a key. `ttgetc()` catches it and returns the ^C
  keystroke, so upstream's bindings all work: `^C` is insert-space, `C-x C-c`
  is the hard quit, and `C-g` is abort as it is everywhere else. After a shell
  escape the foreground set is empty and ^C arrives as an ordinary key instead,
  which is the same thing by the other route. `SIG_TERM` raises the quit flag.
- **The named keys speak uEmacs's PC vocabulary, not the VT220's.** A named
  key arrives whole, so the port picks the `FN` name itself, and it picks the
  one uEmacs uses on a PC — the DOS scan code as a character, which is the set
  [emacs.rc](emacs.rc) lists. The arrows had the ANSI `FNA`–`FND` the terminal
  decoder used to produce, and those are F7 to F10, so they moved to `FNH`,
  `FNK`, `FNM` and `FNP` and [ebind.cpp](ebind.cpp) moved with them. Home, End,
  Insert and Delete are `FNG`, `FNO`, `FNR` and `FNS`, bound to
  beginning-of-line, end-of-line, yank and delete-next-character; upstream
  leaves them to a startup file, which is not good enough here, because a
  user's own `.emacsrc` must not be what decides whether Delete works. What
  they were decoded as before — `SPEC|'1'` through `SPEC|'4'`, the VT220
  keypad's Find, Insert Here, Remove and Select — made Home start an
  incremental search and Delete kill the region. `PgUp` and `PgDn` keep `FN5`
  and `FN6`: paging means paging in both vocabularies, and that is the pair
  `emacs.rc`'s help viewer rebinds.

  **F1 to F12 arrive too**, as `FN;` through `FND` and `FNT` through `FN]` with
  shift. Nothing binds them but the startup file, and [`emacs.rc`](emacs.rc)
  binds the two upstream's own PC branch did: help on F1 and exit on F10. F11
  and F12 have no `FN` spelling, so `M-K` is the way to bind them; it takes the
  keystroke rather than a name.

  **Control on a motion key** is the word and buffer motions: `C-Left` and
  `C-Right` are `M-B`/`M-F`, `C-Home` and `C-End` are `M-<`/`M->`, and
  `C-Backspace` and `C-Delete` are `M-^H`/`M-D`. Every other modifier on a
  named key is still dropped.

  **Command is the browser's, and `Alt` is the only Meta.** The host does not
  consume a chord carrying ⌘ or Super — `Meta is the system's`, says
  `keys.js` — so the page acts on it *and* delivers it, and a program is meant
  to find nothing bound to it. Reading it as uemacs's Meta prefix made every
  system chord run a command underneath the browser's: ⌘V paged backwards
  before the pasted text arrived, so a paste landed a screen away from the
  cursor, and ⌘Z — undo, on the machine most likely to send it — was
  quick-exit, which writes every buffer out and leaves. Any key carrying it is
  now dropped, which is what vi and the shell's line editor already did.

- **`&lef`, `&mid` and `&rig` terminate what they return.** Upstream's are
  `strncpy` and `strcpy` into a static buffer the last call also used, so
  `&lef $line 2` came back as `..` with the tail of an earlier answer stuck to
  it — which is why the help viewer's `".."` test never matched and a topic
  could not be opened. The three now copy, clamp and terminate.
- **`C-x C-z` and `suspend-emacs` are gone.** `SIG_TSTP` is not in Braam's
  catchable set and nothing stops a process to give the shell its prompt back;
  the command says `(No job control)` rather than pretending.
- **`$typahead` is always zero, and every keystroke repaints.** Upstream asked
  the tty with `FIONREAD` and skipped a repaint while somebody was typing
  ahead. The keyboard queue is the kernel's and no syscall peeks at it, so only
  the pushback is known — and a repaint costs the cells that changed, which is
  not the bargain it was at 1200 baud.
- **The screen has colour**, which a termcap terminal did not: the message line
  is bright yellow and the mode line is black on cyan. Upstream marked the mode
  line with reverse video, which is what a terminal had instead of a palette;
  that becomes the colour pair here rather than riding along with it, since the
  renderer swaps foreground and background for the attribute and would undo
  them.
- **[`emacs.rc`](emacs.rc) is two thirds shorter**, because most of upstream's
  was addressed to a DOS box or a serial terminal and none of it ran. `$sres`
  answers `"NORMAL"` and nothing can set it, so the `%system` kludge always
  said `OTHER` and the whole PC branch under it was skipped: the `CGA` line,
  the `$flicker`/`$scroll` snow workaround, the F-key and Alt-key bindings and
  the colour modes. Just as well — there are no colour modes (`modename[]` has
  nine entries and `blue`, `HIGH` and `red` are not among them) and no
  `$scroll` variable, and an unforced FALSE ends a startup file, so either
  would have stopped the rest of it dead. `$progname` is `uEmacs/Pk` rather
  than the `uEmacs/PK` upstream tests for, so the Sun `$TERM` branch and the
  `.emrc` files never ran either; there is no `$TERM` and no `$LANG` in the
  environment to test. Gone with them: the Latin-1 macros that made `{|}[\]`
  insert `äöåÄÖÅ` on a Nordic keyboard that could not send the letters — this
  one sends them, and the whole system is UTF-8 — and `bind-to-key newline
  ^J`, which was for pasting into an xterm. A paste arrives here as Enter
  keystrokes, never as `^J`, so `^J` goes back to newline-and-indent.
- **The mode line no longer says `Spell`.** Upstream's `emacs.rc` turned spell
  mode on for every file it read, and this build has no hunspell behind it —
  the mode existed and never found a misspelling. What is left in the file is
  the help browser, `CMODE` and `WRAP` by file extension, `utf-8`, and F1 and
  F10.
- **The shell escapes run Braam's `/bin`,** which is forty-odd commands and not
  a Unix: `C-x !` works, `sort` and `tr` are not there to be run.
- **A file replaced by rename is only caught if its size or mtime moved.**
  Upstream keyed the "changed under us" check on device and inode as well.
- **The screen is used up to 256 rows and 500 columns**, a back buffer of
  1 MB. The rows are the tallest grid the kernel will make; the columns are
  twelve short of its 512, because `MAXCOL` is what the mode line and the
  message line are built in and a wider terminal would overrun them.
- **A resize sends the whole frame, not the difference.** The kernel
  reallocates its screen on every resize and keeps only the rows above the
  cursor, blanking the rest — but it leaves the process's own Grid alone when
  the geometry has not changed, and that Grid is what `vflush()` diffs
  against. A drag is a burst of resizes the process sees none of until it is
  next scheduled, so the geometry it wakes to is often the one it already had:
  every cell then compares equal, nothing is sent, and the screen stays black
  below wherever the cursor was. So a `SIG_WINCH`, a screen handed back after
  a shell escape, and a blit the kernel refuses each arm one unconditional
  frame.
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
make package             # uemacs-4.0.15-r0.zip: bin/em, and four files in share/
make test                # the seven cases below
```

## Testing

Seven cases, each booting the kernel, planting `em`, and driving it through
the grid — keys in, cells out, because uemacs has no command mode and no way
to be driven down a pipe. [test/emlib.mjs](test/emlib.mjs) is the shared half.

- `emkeys.mjs` — the first frame whole and its colours, the mode line black on
  cyan, `C-f`/`C-b`/`C-n`/`C-p`/`C-a`/`C-e`, the arrow keys reaching
  the same commands, `Home`/`End`/`Delete`, `C-Left`/`C-Right`/`C-Home`/`C-End`,
  `M-<` and `M->`, `C-u` with and without a count, the word motions, and a
  ⌘V paste landing at the cursor.
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
  overrides it, a `.c` file coming up in `CMODE` from the file-read hook,
  `!while`/`!if`/`&add`/`&cat`, `store-macro` with
  `bind-to-key`, a keyboard macro, a `$` variable, `M-?` opening the packaged
  `emacs.hlp` and `F1` opening a topic out of its index, and what happens with
  no package at all.
- `emwindow.mjs` — `C-x 2`, `C-x o`, `C-x 1`, `C-x z` with one window, a
  resize both ways with the buffer and the cursor kept, and a burst of them
  ending at the size the editor already had.
- `embang.mjs` — `C-x !` with its output on the console under the pause, the
  key that resumes, `C-x @` opening a window on a command's output, and
  `C-x #` filtering the buffer.
