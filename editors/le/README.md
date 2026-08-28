# le — the LE text editor

Blocks, and a lot of them: stream and rectangular, copied, moved, shifted,
case-converted, filtered. Full undo and redo. A hex mode that is the same
editor over the same buffer. Syntax highlighting you can retune, a pull-down
menu you can rewrite, and a keymap you can respell — all three from files, all
three shipped in the package.

**This is LE 1.16.8**, which Alexander V. Lukyanov has maintained since 1993.
It is GPLv3; the notices are on every file and in
[tmp/le-editor/COPYING](tmp/le-editor/COPYING). Upstream calls it "slightly
similar to Norton Editor for DOS, but has more features", which undersells it:
it is a Norton Editor that grew regular expressions, a gap buffer that is
binary-clean, and thirty years of use.

## Using it

```
le file           # edit it
le +12 file       # start on line 12
le -r file        # read only
le -h file        # start in hex mode
le --dump-keymap  # the key bindings, in the format the keymap file takes
```

`F1` is the help and lists the rest. The ones worth knowing first:

| | |
| --- | --- |
| `F2` | save |
| `ESC` | leave, asking about a changed file |
| `F5`, `F6` | mark the start and the end of a block |
| `F11`, `F12`, `Shift-F12` | copy, move, delete the block |
| `F4` | the block menu — `F4 C`, `F4 D` and so on are the same commands |
| `^F`, `^B`, `^C` | search forward, back, again |
| `^R` | replace; `*` at the prompt does the rest of the file |
| `^U`, `^Z` / `M-z` | undo, redo |
| `^A H` | hex mode, and back |
| `^N`, `F10` | the menu bar |
| `^O` | the options form |
| `^G b`, `^G e` | to the beginning and the end of the file |

A pattern is a regular expression unless you turn that off in the options.

## What the port changed

No libc: no `malloc`, no `printf`, no `errno`, no `<string.h>`, no exceptions
and no `setjmp`. [braam.cpp](braam.cpp) supplies the string, character and
allocation routines LE calls, plus the printf conversions the status line and
the dialogs use; everything else was replaced rather than reimplemented.

| Upstream | Here |
| --- | --- |
| ncurses — `initscr`, `addch`, `attrset`, `refresh`, and terminfo behind them | [curses.cpp](curses.cpp): the screen is an array of cells with `fg`, `bg` and `attrs` as fields, so a colour is a number and cursor addressing is indexing |
| `getch()` blocking, with a SIGWINCH `siglongjmp` out of it | [getch.cpp](getch.cpp): the one place the process parks. A resize arrives as `Err(Intr)` from `next_key()`, which is the same thing without the jump |
| a key tree over terminfo escape sequences, with a timer to tell `ESC` from `ESC [ A` | [keymap.cpp](keymap.cpp): a key arrives whole, so there is nothing to time; the tree stays, keyed on the key |
| `fcntl` advisory locks on the edited file and the history | gone — the filesystem has no locking, and `DISABLE_FILE_LOCKS` is already upstream's own switch |
| `mmap` mode, and `FILE:BEGIN:LENGTH` to map a window of a device | gone — there is no `mmap`; `buffer_mmapped` is a compile-time `false` and about twenty-five guards fall with it |
| `fork` + `execl("/bin/sh")` to filter a block, `system()` for make and run | stubbed; see [What is not here yet](#what-is-not-here-yet) |
| SIGSEGV/SIGBUS handlers dumping the buffer to `~/.le/tmp/DUMP-*`, a SIGHUP rescue, SIGTSTP | gone. `SIG_INT`, `SIG_TERM` and `SIG_WINCH` are the whole catchable set, and none of them is a handler |
| `alarm(60)` driving the autosave | [signals.cpp](signals.cpp)'s `AutoSaveTick()`, asked from `Edit()`'s loop. It was already a resumable chunked state machine, which is what let it move |
| eight-bit Cyrillic codepages, a software keyboard layout, D211 and VTA2000 tables | gone. The browser sends the codepoint that was typed |
| `mbtowc` against the locale | [lewchar.cpp](lewchar.cpp): UTF-8, and only UTF-8 |
| `PKGDATADIR`, fixed at compile time | [epath.cpp](epath.cpp): `readlink("/pkg/bin/le")`, because a package's payload lands under a path carrying a version the binary does not know |
| `getpwuid`, `geteuid`, `chmod`, `utime`, `pathconf` | gone. There is no owner, no permission bit, and no way to set an mtime |

## Structure

**A `co_await` is where the work is.** Every command LE binds a key to can
reach a prompt, a dialog or the disk, and all three are syscalls — so
`ActionProc` is `Task<void>(*)()` and the 169 entries of the action table
became coroutines in one edit, as `fn_t` did in
[../uemacs](../uemacs/README.md). 238 functions are `Task`s.

**The error box records rather than blocks.** `ErrMsg`, `FError` and
`NoMemory` are called from sixty-eight places, including the buffer primitives
in [kern.cpp](kern.cpp) — which are pure arithmetic and run in tight loops. If
they had blocked on their modal Ok button, every one of those would have become
a coroutine. So the message is kept and `Edit()`'s loop puts the box up on its
next turn: the same box, the same text, one turn later. The result is that
[undo.cpp](undo.cpp) and [textpoin.cpp](textpoin.cpp) have no coroutines at all
and `kern.cpp` has six, which are the ones that really do I/O.

**The gap buffer needed nothing.** `buffer`, `BufferSize`, `GapSize`, `ptr1`
and `ptr2` are one `realloc`'d block and stayed exactly that. It is the reason
this port was less work than the two before it.

**A global with a constructor lives in storage of its own.** A namespace-scope
object with a non-trivial destructor wants `__cxa_atexit`, and there is none:
a process here is destroyed wholesale. [leglobal.h](leglobal.h) holds each of
the seventeen — the `TextPoint`s, the histories, the clipboard, the undo — in
a POD holder, built on first use and never destroyed, and a macro keeps every
use site as upstream wrote it.

**`new` returns null.** Every `new` and `delete` is `heap_new` and
`heap_delete`. This one is worth stating plainly because it is invisible until
it runs: `new KeyTreeNode` handed back null, so the key tree was never built
and not one binding fired.

**The keymap is respelled.** A terminfo capability like `$kcub1` named an
escape sequence, and there are none here, so `${Left}`, `${C-Left}` and
`${F4}` name the keys instead — resolved by a table in
[keynames.cpp](keynames.cpp) rather than by terminfo. Everything else in the
format survives, and most of the file did not have to change at all: **Ctrl on
a letter is still the control character** and **Alt is still an `ESC` in
front**, which is what upstream's own `\e|X` bindings always meant.

**A printable key is its UTF-8, one byte at a time.** Upstream read bytes and
inserted them one by one, which is how a multibyte character got into the
buffer; the decoder hands back the first byte and queues the rest, so
`StringTyped` and the self-insert path in `Edit()` are untouched.

**`fscanf` went into the SDK.** LE reads its config files with twelve `fscanf`
calls and twelve `sscanf` calls. `proc/file.h` had no scanning half, so one
went in — not a varargs `scanf`, but the conversions a format string would have
driven, one function each. See the 0.7 release notes in `../../braam-core`.

## Differences from upstream worth knowing

- **The file is not held open.** Upstream kept the descriptor for the whole
  session because the advisory lock lived on it. Here "readers share, a writer
  has the file to itself" (Concept.md §5.2), so holding it would make the first
  save fail; the text is in the buffer by then and the save opens by name.
- **A file is identified by its path.** There are no inode numbers, so
  `st_ino` is a hash of the path — which is what `SameFile()` means anyway.
  Two names for one file are two files to the position history.
- **A backup keeps the time it was written**, not the original's. Nothing can
  set an mtime here.
- **No mouse.** `WITH_MOUSE` was not defined by upstream's own CMake build
  either.
- **No `^Z`.** There is no job control to stop into.
- **The typeahead skip is gone.** `SyncTextWin` used to skip the redraw while
  a key was already queued and draw a spinner instead; that was a latency hack
  against a terminal it had to write bytes to, and the Grid sends only the
  cells that changed.
- **`--dump-colors` and `--dump-keymap` still work**, and the keymap they print
  is the format the keymap file takes.
- **`long double` is `double` in the calculator.** Quad is a compiler-rt call
  nothing provides here.
- **A `cchar_t` holds two codepoints, not ncurses' five.** A cell holds one and
  the renderer composes no combining marks, so the rest were never drawn.

## What is not here yet

- **Filtering a block through a command** (`pipe.cc`) and **Make, Run, Compile
  and the shell escape** (`cmd.cc`). Braam has `spawn`, `make_pipe`,
  `wait_child` and `screen_claim`, so both are possible and the choreography is
  settled — the screen goes back before the keyboard, and both before the
  spawn. `pipe.cc` used `poll()` over three descriptors at once, which one task
  cannot do, so a filter wants a temp file on each side. The actions say so
  until then.

## Files

Upstream's file split and its names are kept. What is new:

| | |
| --- | --- |
| [braam.cpp](braam.cpp) | the C library |
| [curses.cpp](curses.cpp) | all of ncurses this needs, over the Grid |
| [leio.cpp](leio.cpp) | the file syscalls, POSIX-shaped, with an `le_` prefix so a call that lost its `co_await` would not still compile |
| [lefile.cpp](lefile.cpp) | the four stdio calls the config parsers make, over `proc/file.h` |
| [lewchar.cpp](lewchar.cpp) | UTF-8 in place of the locale's encoding |
| [leglobal.h](leglobal.h) | a global that has a constructor |
| [epath.cpp](epath.cpp) | where `share` is |
| [cinc/](cinc/) | the handful of libc headers `regex.c` and `wcwidth.c` name, each forwarding to the shim; those two are the only files here that are C |

Gone: `mouse.cc`, `rus.cc`, `tables.cc`. `tables.cc`'s `ModifyKey` moved into
[chset.cpp](chset.cpp) as the identity it now is.

The four tables upstream generated with perl are generated with Python here,
byte for byte the same: [make-action-enum.py](make-action-enum.py),
[make-action-name-func.py](make-action-name-func.py),
[make-keymap.py](make-keymap.py), [make-mainmenu.py](make-mainmenu.py).

## Building and packaging

`make` at the top of the tree. The package is `le`, and it carries
`share/` — the keymap, the colours, the menu, the help and the syntax
rules — because `epath.cpp` finds them there.

## Testing

`make test`, or one case at a time:

```
node editors/le/test/leedit.mjs
```

| | |
| --- | --- |
| [leedit.mjs](test/leedit.mjs) | load, edit, save, and read the file back |
| [leblock.mjs](test/leblock.mjs) | stream blocks, undo and redo, and a `^G b` chord |
| [lesearch.mjs](test/lesearch.mjs) | literal and regexp search, and replace-all — which is `regex.c` and `re_search_2` reading across the gap |
| [lesigint.mjs](test/lesigint.mjs) | `^C` reaches its binding rather than killing the editor |
| [lescreen.mjs](test/lescreen.mjs) | hex mode, the menu bar and a resize |
| [lesyntax.mjs](test/lesyntax.mjs) | the syntax colours and the help, both out of `share` |

The harness plants the whole package link chain, because `readlink("/pkg/bin/le")`
is how the editor finds its own data.
