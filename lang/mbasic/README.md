# mbasic — Microsoft BASIC 1.1 for the 6502, on Braam

Microsoft BASIC version 1.1 for the MOS 6502, Microsoft 1976-78, released under
MIT and rewritten here in C++20 for Braam. It is the interpreter that shipped on
the Apple II, the Commodore PET, the KIM-1 and the OSI — one codebase, six
targets, selected by a `REALIO` switch. This is a seventh.

Upstream is a single MACRO-10 file, `tmp/m6502.asm`, together with the design
document recovered from it in `tmp/doc/`. It cannot be assembled: MACRO-10 was
DEC's PDP-10 assembler used as a cross-assembler, and the universal file that
`SEARCH M6502` on line 2 pulls in — the one defining every opcode macro — is not
in the archive. The only way to run this program again is to rewrite it, which
is what this is.

```
$ mbasic
Memory size?
Terminal width?

Braam BASIC v1.1
Copyright 1978 Microsoft

Ok
10 for i=1 to 5:print i;sqr(i):next
run
 1  1
 2  1.41421356
 3  1.73205081
 4  2
 5  2.23606798

Ok
```

## What it is

The whole language, at the fullest switch setting the source supports short of
the Commodore's hardware-specific corners:

| Switch | Here | What it adds |
|---|---|---|
| `ADDPRC` | 1 | 9 significant digits rather than 6 |
| `INTPRC` | 1 | `%` integer variables and arrays |
| `GETCMD` | 1 | `GET` |
| `DISKO` | 1 | `LOAD` and `SAVE` |
| `EXTIO` | 1 | `INPUT#`, `PRINT#`, `CMD`, `SYS`, `OPEN`, `CLOSE`, and `?File data` |
| `LNGERR` | 1 | spelled-out error messages instead of two-letter codes |
| `NULCMD`, `TIME` | 0 | no `NULL`, no `TI`/`TI$` |

That gives 75 reserved words, `$80` (`END`) through `$CA` (`GO`). The four
tables that decide them — `RESLST`, `STMDSP`, `FUNDSP` and `OPTAB` — are in one
file, [tables.cpp](tables.cpp), because the connection between them is purely
positional: a token's value is `128 + its ordinal in RESLST`, and adding a word
under one switch renumbers everything after it in all four at once.

`SYS` and `USR` raise `?Illegal quantity`. There is no machine code to call, and
upstream's `USR` did exactly that until the user `POKE`d a vector into it.
`POKE`, `PEEK` and `WAIT` address 64 KiB of scratch of their own, so the
statements and `GETADR`'s `0..65535` check still mean something.

## What had to change, and why

### The interpreter is plain C++; the driver is the only coroutine

On Braam a `co_await` is a call and not a tail call, so a loop that awaits
without ever suspending grows the native stack until the process traps. An
interpreter's statement loop is exactly that shape.

So nothing below `Interp::step()` blocks. The tokenizer, the evaluator, variable
lookup, the string functions and `FOUT` are ordinary non-coroutine functions;
[braam.cpp](braam.cpp) holds `proc_main`, the driver loop and every `co_await`
in the program. `step()` runs a burst of statements and returns saying what it
wants — a line, a character, a file, or simply a breath. This is simbesm's
`cpu_burst()` contract, in
[emulators/simbesm/machine.h](../../emulators/simbesm/machine.h).

The burst is not an optimisation. It is the only thing that lets `SIG_INT` be
delivered, because a signal arrives where a process parks — without it
`10 GOTO 10` could never be broken.

### The error handler, and the blocking read, are the same unwind

`ERROR` reset the 6502 stack pointer and jumped to `READY`, unwinding from
whatever depth it was raised at. There is no `setjmp` here and none can be
written — wasm's call stack does not live in linear memory — and exceptions are
off. So `error()` records rather than unwinds, and the unwinding is done one
frame at a time by the macros in [err.h](err.h). That is
[editors/vi](../../editors/vi)'s answer to the same problem.

The one flag carries a suspension too, because a blocking read has to unwind for
exactly the same reason and to exactly the same place. A missed check must be
inert rather than corrupting, so the leaves are poisoned: `chrget` and `chrgot`
answer `0`, which is a statement terminator, so every scan loop ends; `outdo`
drops the character; `ptrget` answers a scratch reference and never a live
variable.

`INPUT` is the one statement that can want a line *midway* through itself, at
`GETNTH`'s `?? ` continuation prompt. Upstream held the rest of its state in
page zero and the 6502 program counter and got away with it because `QINLIN`
never returned until it had a line; a read that unwinds has to name every
field, which is what `InputState` is. Nothing is replayed on resume — the
resume point is a position in the *variable list*, so variables already assigned
are behind it.
`?Redo from start`, which deliberately does restart the whole statement, needs
no special case: it sets `TXTPTR` back to `OLDTXT` and `NEWSTT` re-dispatches.

### The keyboard changes hands once, at the right boundary

A key ring has exactly one receiver, and there is no non-blocking key read. So
the line editor and `ISCNTC` would be fighting over the keyboard, except that
they never want it at the same moment: at the prompt it is ours and the editor
reads it, and the moment a program runs long enough to yield it goes back to the
console, whose pump turns `^C` into `SIG_INT`. Neither loses a keystroke to the
other.

The editor itself is [games/adventure/edit.cpp](../../games/adventure/edit.cpp),
lifted and cut, with one change: the prompt is drawn by the interpreter through
`OUTDO`, so `TRMPOS` is maintained and a redirected run still shows it, and the
editor anchors where the cursor already is rather than asking for a row of its
own — which would put `INPUT`'s `? ` on the wrong line. `^D` on an empty line is
end of input; upstream had no way to leave BASIC at all, because it owned the
machine.

### `double` in place of the 5-byte float, with `FOUT` kept exactly

Upstream's format was 5 bytes: an excess-128 exponent, a 32-bit mantissa
normalised to `[0.5, 1)` with an implied leading 1 sharing the sign's bit
position, no NaN, no infinity, no denormals and no negative zero. Arithmetic is
IEEE `double` here.

But the *printed* form is language-visible, so [fout.cpp](fout.cpp) reproduces
it literally: 9 significant digits, a leading space when non-negative and `-`
when negative, a trailing space from `PRINT`, fixed notation for
`0.01 <= |x| < 1e9` and exponential outside it, `.5` and never `0.5`, trailing
zeros stripped along with the point, an always-present exponent sign and exactly
two digits, and `" 0"` for zero. What is *not* reproduced is the arithmetic:
upstream converted decimal in both directions by repeated `MUL10`/`DIV10` rather
than by a table, so `1E38` cost thirty-eight multiplications and accumulated the
rounding error of each. The last digits of a long computation will differ.

`INT` is still a floor and not a truncation, so `INT(-2.5)` is `-3`; `0^0` is
still `1`; an exponent past `2^127` is still `?Overflow`, and an underflow still
silently becomes zero.

### Containers in place of the arena, and what that keeps

Upstream had no allocator, no free list and no fragmentation — only four regions
between `TXTTAB` and `MEMSIZ`, kept adjacent and shuffled by block moves
whenever any of them grew. Program text, variables and arrays grew upward and
collided with string space growing downward, and the collision is what triggered
garbage
collection and, failing that, `?Out of memory`.

That is a `Vec` and a `String` here, and with it goes the mark-and-move
collector, the six-step string protocol, the copy-if-volatile rule and the whole
single-owner invariant they existed to serve — *"IT IS THE NATURE OF GARBAGE
COLLECTION THAT DISALLOWS HAVING TWO STRING DESCRIPTORS POINT TO THE SAME AREA
IN STRING SPACE"*. Absolute line links go too, and `LNKPRG` with them.

What is kept, because it is observable:

- arrays are **column-major** — the first subscript varies fastest — and an
  undimensioned one has extents of 11;
- a variable's name is still two bytes with the type in their high bits, so only
  the first two characters are significant and `A`, `A$` and `A%` are three
  variables;
- reading an undefined variable does not create it; assigning does;
- typing any program line still clears every variable;
- `?String too long` above 255 characters, and `?Formula too complex` at the
  fourth live string temporary, so the language does not silently gain capacity;
- `FRE` still answers a signed 16-bit count against the budget `Memory size`
  sets, and `?Out of memory` still fires — including from `GETSTK`'s `NUMLEV`,
  the 23 guaranteed levels of expression nesting.

The `FOR`/`GOSUB` stack could not go. Upstream's was a typed structure scanned
by tag byte, and it cannot be replaced by the host call stack because `FOR` and
`RETURN` unwind by *content*, not by depth: `RETURN` discards every `FOR` frame
above the `GOSUB` it finds, which is the language rule that a `GOSUB` leaving
loops open still returns correctly. It is an explicit `Vec<Frame>` in
[stmt.cpp](stmt.cpp), with the same search and truncate operations.

### The ten defects, fixed

Section 3 of
[tmp/doc/internals/13-porting-notes.md](tmp/doc/internals/13-porting-notes.md)
catalogues ten. All are fixed:

| | |
|---|---|
| `ISCNTC` compared a value `INCHR` had already masked with `ANDI 127` against the unmasked `^O203`, so on the Apple build **`^C` could not interrupt a running program at all** | `^C` breaks |
| `AYINT`'s `N32768` was declared with four bytes and read as five, so the comparison value was `-32768.00048828125` and exactly `-32768` was rejected | `A%=-32768`, `NOT 32767` and `-32768 AND -1` all work |
| `RMULZC` and `RADDZC` read a stray adjacent byte, so `RND`'s sequence was not the one designed | the intended constants |
| `INIT` copied four of the five seed bytes | one seed, fully initialised |
| the two copies of `CHRGET` disagreed at `QNUM` | one `CHRGET` |
| `ROMSW=0` never stored `TXTTAB` | not reachable |
| two different formulas for `NCMWID`, so answering `40` differed from the default `40` | one formula, used in both places |
| equal-exponent `FADD` added one to the guard byte | moot under `double` |
| `VAL` wrote one byte past the string and restored it | parses over a bounded copy |
| dead code in `TAN`, `SIN`, `REASON` and the Apple cassette routines | not written |

`RND` is still deterministic from a fixed seed, as upstream was. That is
language-visible — `RUN` twice gives the same numbers — and it is what makes a
golden transcript possible.

### Things that look like bugs and are not

Kept deliberately, because they are the language:

- `IF 2 > F OR T=5 THEN` mis-tokenizes, because the matcher sees `FORT` and
  `FOR` comes first. The source warns about it at `m6502.asm:1208-1216`.
- A `FOR` body always runs once: the test is at `NEXT`, so `FOR I=1 TO 0`
  prints once.
- `FOR`/`NEXT` never terminates on equality, and a zero step loops for ever.
- A false `IF` skips the **whole rest of the line**, not just to the next `:`,
  because it shares the `REM` path.
- `=<` and `><` are legal spellings; `<<` is a syntax error.
- True is `-1`, produced by masking, so `A<=B` and `(A<B) OR (A=B)` are the
  same value.
- Line numbers stop at 63999, and exceeding it is a *syntax* error.
- A blank line typed to `INPUT` is a silent `STOP` — `STPEND` with carry clear,
  so no `BREAK` is printed. It is *not* continuable in practice: `OLDTXT` is
  left pointing into the variable list rather than at a statement boundary, so
  `CONT` re-dispatches from mid-statement and raises `?Syntax error`. Upstream
  stored `TXTPTR` the same way and has the same wart.
- `SYSTEM` is `SYS` followed by `TEM`, and raises `?Illegal quantity`.

### Case is folded, which upstream's was not

Upstream was uppercase only, and had no reason not to be: the Apple II and the
PET had uppercase-only keyboards, so there was no case to fold and no bytes to
spend folding it on the hottest path in the interpreter. Here `print`, `PRINT`
and `PrInt` are one keyword and `x` and `X` are one variable.

The fold is in `CRUNCH` and nowhere else. It is the one chokepoint every path
reaches — the editor, direct mode and `load_line` all tokenize through it, and
everything downstream reads the crunched bytes through `chrgot` — so
[tables.cpp](tables.cpp) spells `RESLST` in lower case, `match_res` folds the
source byte before comparing, and the byte it stores for anything unmatched is
folded too. The stored line is therefore canonical, which keeps `LIST` the
exact inverse of `CRUNCH` and makes `SAVE` write one spelling whatever was
typed.

Three things never reach the matcher, and keep the case they were typed in: a
string literal, a `DATA` item, and a `REM` tail. So do `LOAD` and `SAVE`
filenames, which are string literals. `FIN` had to learn `1e5` beside `1E5`,
since a program's own exponent is folded on the way in; `FOUT` still prints
`E`.

**This is a departure from upstream, and so are the messages.** The tree's rule
is that a port keeps the program's output text, and these no longer are 1978's
bytes: `OK` is `Ok`, `?SYNTAX ERROR` is `?Syntax error`, and the free-memory
line above the banner is gone, the memory here being the kernel's rather than
the machine's. What is *not* touched is the part that makes it this BASIC — the
arithmetic, `FOUT`'s format to the digit, the four tables and every rule in
*Things that look like bugs and are not* below.

### Two questions asked only at a console

`Memory size?` and `Terminal width?` are asked when stdin is a terminal and not
otherwise: down a pipe there is nobody to answer them, and a run reading its
program from a file should not have the first two lines of it eaten. The width
otherwise comes from `tty_of`, and is zero — meaning no automatic wrap — for a
pipe. Answering `A` to `Memory size` still prints `Written by Weiland & Gates`.

## Files

Upstream is one file with 46 `SUBTTL` sections. The split follows them, and each
source names the chapter of `tmp/doc/internals/` it implements.

| | |
|---|---|
| [mbasic.h](mbasic.h) | the `Interp` object: every surviving page-zero cell as a field |
| [err.h](err.h) | the unwind — `Halt`, `ErrCode`, `CHK`/`ERR`/`SUSPEND` |
| [tables.cpp](tables.cpp) | `RESLST`, `STMDSP`, `FUNDSP`, `OPTAB`, `ERRTAB` |
| [crunch.cpp](crunch.cpp) | `CHRGET`, `CRUNCH`, `LINGET`, `FNDLIN`, the editor |
| [list.cpp](list.cpp) | `LIST`, the exact inverse of `CRUNCH` |
| [newstt.cpp](newstt.cpp) | the statement fetcher, dispatch, errors, the burst |
| [stmt.cpp](stmt.cpp) | the statements, and the `FOR`/`GOSUB` frames |
| [print.cpp](print.cpp) | `PRINT`, the column machinery, the output sink |
| [input.cpp](input.cpp) | `INPUT`, `INPUT#`, `READ`, `GET` — the mid-statement suspension |
| [frmevl.cpp](frmevl.cpp) | the evaluator, the operators, the relationals |
| [ptrget.cpp](ptrget.cpp) | variables, arrays, `DIM`, `DEF FN` |
| [string.cpp](string.cpp) | the string functions and `FRE` |
| [fout.cpp](fout.cpp) | `FOUT` and `FIN` — the one file that must match 1978 exactly |
| [math.cpp](math.cpp) | the arithmetic functions, over `braam::math` |
| [file.cpp](file.cpp) | `LOAD`, `SAVE`, and the channels |
| [sys.cpp](sys.cpp) | `POKE`, `PEEK`, `WAIT`, `POS`, and the two that lost their machine |
| [init.cpp](init.cpp) | the two questions and the banner |
| [braam.cpp](braam.cpp) | the driver. Everything that blocks is here, and only here |
| [edit.cpp](edit.cpp), [edit.h](edit.h) | `INLIN`, from `games/adventure` |
| [epath.cpp](epath.cpp), [epath.h](epath.h) | where the shipped examples are |

No `PORT`: this is a rewrite in Braam idiom, not a recompile of C, so the port
kit is not linked and `#include <string.h>` is still "file not found".
`braam::math` is, for the transcendentals and `ftoa`'s conversions.

## The language

[Manual.md](Manual.md) is the reference: every statement, function, operator
and error message, with the rules that surprise a modern reader marked. It
ships in the package as `share/Manual.md`, beside the examples.

## Examples

[examples/](examples/) holds nineteen programs, and the package ships them as
its `share/` payload. That lands them in `/pkg/store/mbasic-<version>/share/`,
a path carrying a version the binary does not know — so `LOAD` resolves a bare
name against it when the working directory has no such file:

```
load "wumpus.bas"
run
```

[epath.cpp](epath.cpp) finds the directory once at startup, by reading the
`/pkg/bin/mbasic` link `PATH` found and going two directories up, or failing
that by scanning `/pkg/store` for the name it is a prefix of. `SAVE` has no
such fallback: the store is read-only. A name with a `/` in it is a path of the
caller's own and is taken as given.

They are written in lower case, which the tokenizer folds (below); their
messages are ordinary English sentences.

They are new code rather than upstream's, and writing them found four things
this BASIC does that a modern eye does not expect. A reserved word matches
*anywhere*, so `money` is `m`, `on`, `ey` and `wrong` is `wr`, `on`, `g` —
case makes no difference to that. A variable name is significant in its first
two characters, so `pit1` and `pit2` are one variable and so are `feed` and
`fed`. There is no `else`, and no backslash escape inside a string. And a `for`
body always runs once, which is why [primes.bas](examples/primes.bas) has to
keep 2 and 3 away from its trial division — `for i = 2 to sqr(2)` would divide
2 by 2.

A fifth is not the language's: **string data is never folded**, so a program
that prompts `(y/n)` and tests `if a$ = "Y"` rejects a typed `y`. Each example
takes both, and [hangman.bas](examples/hangman.bas) upcases the letter it is
given with `asc`/`chr$`.

## Testing

`make test` at the top of the tree runs seven cases from [test/](test/). Six
drive a session through stdin and stdout redirected to files and compare the
transcript byte for byte against a golden beside the script — exact, because the
run is deterministic and down a pipe nothing echoes and no prompt is printed.
`interrupt.mjs` is on the grid, because a pipe has no keyboard.

`examples.mjs` is the fifth: it `LOAD`s and `RUN`s each of the nineteen in a
process of its own, so `RND` restarts from its fixed seed every time and one
example's stream cannot shift another's. Its answers are that particular
sequence's moves — the number is 54, the word is `MONITOR`, the wumpus is in
room 9 — so a change to an example that consumes a different number of `RND`
values means re-choosing them, not just re-blessing.

`case.mjs` is the case rules stated once: one keyword whatever the case, one
variable whatever the case, `LIST` canonical in lower case, and a string, a
`DATA` item, a `REM` tail and a filename keeping theirs. It also runs a program
typed the old way, since the point is that both spellings work.

Re-bless a golden with `--bless` after reading the diff.
