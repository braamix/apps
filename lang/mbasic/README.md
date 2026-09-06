# mbasic — Microsoft BASIC 1.1 for the 6502, on Braam

Microsoft BASIC version 1.1 for the MOS 6502, Microsoft 1976-78, released under
MIT and rewritten here in C++20 for Braam. It is the interpreter that shipped on
the Apple II, the Commodore PET, the KIM-1 and the OSI — one codebase, six
targets, selected by a `REALIO` switch. This is a seventh.

Upstream is a single MACRO-10 file, [m6502.asm](m6502.asm), together with the
design document recovered from it — [Internals.md](Internals.md) and the
thirteen chapters in [internals/](internals/). It cannot be assembled:
MACRO-10 was DEC's PDP-10 assembler used as a cross-assembler, and the
universal file that `SEARCH M6502` on line 2 pulls in — the one defining every
opcode macro — is not in the archive. The only way to run this program again is
to rewrite it, which is what this is.

```
$ mbasic

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

That gives 75 reserved words, and `RENUM` below makes 76: `$80` (`END`) through
`$CB` (`GO`). The four
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
- `FRE` still answers a signed 16-bit count against a fixed budget,
  and `?Out of memory` still fires — including from `GETSTK`'s `NUMLEV`,
  the 23 guaranteed levels of expression nesting.

The `FOR`/`GOSUB` stack could not go. Upstream's was a typed structure scanned
by tag byte, and it cannot be replaced by the host call stack because `FOR` and
`RETURN` unwind by *content*, not by depth: `RETURN` discards every `FOR` frame
above the `GOSUB` it finds, which is the language rule that a `GOSUB` leaving
loops open still returns correctly. It is an explicit `Vec<Frame>` in
[stmt.cpp](stmt.cpp), with the same search and truncate operations.

### The ten defects, fixed

Section 3 of
[internals/13-porting-notes.md](internals/13-porting-notes.md)
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
- `SYS` and `USR` raise `?Illegal quantity`, having no machine to call.

### Text is UTF-8, and a token is not

A token is a byte `>= 0x80` and so is every byte of a UTF-8 sequence, and LIST
expanded any such byte through `RESLST` without asking where it stood. So
`10 PRINT "café"` listed back as `10 print "caflenstep"` — `C3 A9` being the
ordinals of `LEN` and `STEP` — and since SAVE walks the same bytes it *wrote
that to disk*. It was data loss, not a display fault.

The fix is that `detok` ([list.cpp](list.cpp)) now carries CRUNCH's own
regions, so inside a literal, a `DATA` item or a `REM` tail a byte is text and
never a token; SAVE shares the routine rather than keeping a second copy of the
loop. Outside those three, CRUNCH refuses a byte `>= 0x80` outright — it can
begin no name, number or reserved word, and storing one would collide with the
token space three ways over: LIST would expand it, `GONE2` would dispatch on
it, and PTRGET uses bit 7 of a name byte as the `$`/`%` type tag.

Above that, everything the user counts is a character. `LEN`, `LEFT$`,
`RIGHT$` and `MID$` step codepoints, so `MID$` cannot halve one; `ASC` and
`CHR$` carry Unicode, and `CHR$` reaching 1114111 rather than 255 is the
departure worth naming — it is also what stops `CHR$` building an invalid
sequence a byte at a time. The print column took one line: `OUTDO` skips a
continuation byte, which makes `TRMPOS` count runes and settles a mismatch that
was already there, `LINWID` having come from `tty_of` in cells all along.

**There is no width table, and that is the tree's rule rather than a shortcut.**
The grid is one codepoint per cell — `screen_put` writes one `Cell` and
advances the cursor by one, whatever the codepoint — so a character *is* a
column. `editors/vi` says the same thing; `editors/le` is the one port that
uses `wcwidth`, and `../braam-core/doc/Compat.md` records that it therefore
disagrees with the screen about a wide character.

### A reserved word is a word, which upstream's was not

Upstream matched a reserved word at every character position with no boundary
on either side, so `total = 1` was `TO` + `"tal"` and a bare `?Syntax error` —
and since `LIST` is the exact inverse it printed the line back as `total=1`, so
the fault could not be seen in a listing. Fourteen ordinary English words were
unusable as names: `total store money wrong month sort word land random letters
already sine using positive`. `m6502.asm:1209-1215` warns about it and draws
only the rule that the shorter word goes second in the table.

The fix is two guards on the same first-fit match ([crunch.cpp](crunch.cpp)): a
reserved word beginning with a letter may not follow one, and one ending in a
letter may not precede one. They are per end, so the entries that are not all
letters need no case of their own — `+` and `>` are punctuation at both ends,
`tab(` and `print#` at the last — and `OPTAB`'s `3*(token - PLUSTK)` arithmetic
and the `INPUT#`-before-`INPUT` ordering are untouched.

**It is a real trade and not a free win.** `TOTAL` and `1 TO X` are the same
shape — `TO` followed by letters — and no rule that does not parse can tell
them apart, so a keyword has to be free-standing now: `fori=1ton` is the
variable `fo`, and `printa`, `nexti` and `ifaandb` are gone with it. That is
the half of 1978 this gives up, and it is the same kind of departure as the
fold below. A **digit** is still a boundary, deliberately, so `1to10`,
`print1`, `goto100` and `5and3` are unchanged; and `FN` is exempt from the
trailing guard, a name always following it, so `DEF FNA(X)` still means `FN A`.

None of the examples tokenizes differently — they were written with
the old rule in mind, which is to say with spaces.

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

### A named file is a program, not a typed session

`mbasic prog.bas` runs it and prints what it prints — no banner, no `Ok`, and
the process exits when the program does, with 0, or 1 if an error was reported,
or 130 on `^C`. The lines are still fed as if typed, so an unnumbered one is a
direct command; the implicit `RUN` happens when they run out, unless a `RUN`
statement already fired (`ran_`).

Two streams, not one. The file supplies `Resume::Main` lines only and `INPUT`
reads stdin, which is what makes `mbasic guess.bas` playable at a terminal —
`input_init` therefore no longer hands `paths` to `Input`, and `console` is
simply "stdin is a terminal". The file itself is one `read_file` at startup,
with `epath_file`'s fallback, so a bare name finds a shipped example.

Redirected stdin is untouched: `mbasic <session` is still the whole typed
transcript, banner and every `Ok`, which is what nine of the eleven test cases
drive.

### Neither of upstream's two questions is asked

`MEMORY SIZE?` asked how much of the machine BASIC might take, and there is no
machine to take it from: `FRE` reports against a fixed 65535 and `?Out of
memory` still fires on it. `TERMINAL WIDTH?` asked what `tty_of` now answers.

`LINWID` is the terminal's width, or zero for a pipe — zero meaning no
automatic wrap, because wrapping a redirected transcript at some terminal's
width would be a hard thing to explain. The comma zones still need a number,
since a `PRINT` with commas has to line up somewhere, and that is `LINLEN`, 80.

Upstream's easter egg went with the first question: answering `A` to it printed
`WRITTEN BY WEILAND & GATES`. The authors are named in `--help` instead.

### `RENUM`, which upstream could not have had

The one statement here that is not in `m6502.asm`. It comes from the later
Microsoft releases, and the reason the 1978 version has no such thing is
structural rather than a matter of taste: a program was a linked list, each
line beginning with the absolute address of the next, so renumbering meant
rewriting every reference *and* relinking — and a reference whose digits grew
by one moved every line above it.

Neither half survives the port. `prog` is a sorted `Vec<Line>` with no links at
all, and a line-number **reference** is plain ASCII in the crunched text:
`CRUNCH` enters `0`–`9` straightaway without attempting a match
([crunch.cpp](crunch.cpp)), and `LINGET` re-parses the digits at execution
time. There is no `$0E` binary line-number token in this dialect — that is the
8080 lineage — so there is no fixed-width field to patch and nothing to
relocate. [renum.cpp](renum.cpp) is one pass rewriting digit runs into a fresh
`Vec<u8>` per line.

What it has to know is where a digit run is a reference and where it is data,
and that is `CRUNCH`'s own structure read backwards: a `"` copies through to
the closing quote, `REMTK` swallows the rest of the line, `DATATK` copies to
the next `:`, and only after `GOTO`, `GOSUB`, `THEN`, `RUN` and the `GO`+`TO`
pair is a number a reference. `GOTO` and `GOSUB` take a comma-separated list,
which is what serves `ON X GOTO a,b,c` without a case of its own.

Two behaviours are Microsoft's and are kept. It refuses — `?Illegal quantity`,
nothing changed — to reorder the program or to carry a line past `MAXLIN`. And
a reference to a line that does not exist is a *warning*, `Undefined line 999
in 500`, not an error: the reference is left as typed and the rest of the
program is renumbered anyway. Afterwards it does what the editor does on any
typed line, `runc()`, since every saved `TextPos` names a line whose text has
just moved, and exits to the prompt the way `LIST` does.

Adding the word cost the four tables their numbering: `renum` goes last among
the statements, so `RENUTK` is `$A2` and everything from `TAB(` up shifted by
one. That is invisible outside the binary — `LIST`, `SAVE` and `LOAD` all
detokenize by index into `RESLST`, and stored programs are text.

## Files

Upstream is one file with 46 `SUBTTL` sections. The split follows them, and each
source names the chapter of [internals/](internals/) it implements.

| | |
|---|---|
| [mbasic.h](mbasic.h) | the `Interp` object: every surviving page-zero cell as a field |
| [err.h](err.h) | the unwind — `Halt`, `ErrCode`, `CHK`/`ERR`/`SUSPEND` |
| [tables.cpp](tables.cpp) | `RESLST`, `STMDSP`, `FUNDSP`, `OPTAB`, `ERRTAB` |
| [crunch.cpp](crunch.cpp) | `CHRGET`, `CRUNCH`, `LINGET`, `FNDLIN`, the editor |
| [list.cpp](list.cpp) | `LIST` and `DETOK`, the exact inverse of `CRUNCH` |
| [renum.cpp](renum.cpp) | `RENUM` — the one statement that is not upstream's |
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
| [init.cpp](init.cpp) | the banner, and the widths it sets |
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

[examples/](examples/) holds twenty programs, and the package ships them as
its `share/` payload. That lands them in `/pkg/store/mbasic-<version>/share/`,
a path carrying a version the binary does not know — so a bare name is resolved
against it when the working directory has no such file, both on the command
line and in `LOAD`:

```
$ mbasic wumpus.bas
```

[epath.cpp](epath.cpp) finds the directory once at startup, by reading the
`/pkg/bin/mbasic` link `PATH` found and going two directories up, or failing
that by scanning `/pkg/store` for the name it is a prefix of. `SAVE` has no
such fallback: the store is read-only. A name with a `/` in it is a path of the
caller's own and is taken as given.

They are written in lower case, which the tokenizer folds (below); their
messages are ordinary English sentences.

They are new code rather than upstream's, and writing them found four things
this BASIC does that a modern eye does not expect. A reserved word used to
match *anywhere*, so `money` was `m`, `on`, `ey` and `wrong` was `wr`, `on`,
`g`; that is the one of the four that was fixed rather than documented, and it
is below. A variable name is significant in its first two characters, so `pit1`
and `pit2` are one variable and so are `feed` and `fed`. There is no `else`,
and no backslash escape inside a string. And a `for` body always runs once,
which is why [primes.bas](examples/primes.bas) has to keep 2 and 3 away from
its trial division — `for i = 2 to sqr(2)` would divide 2 by 2.

A fifth is not the language's: **string data is never folded**, so a program
that prompts `(y/n)` and tests `if a$ = "Y"` rejects a typed `y`. Each example
takes both, and [hangman.bas](examples/hangman.bas) upcases the letter it is
given with `asc`/`chr$`.

A sixth is `PRINT`'s, and [calendar.bas](examples/calendar.bas) is where it
bites: a printed number carries a sign column ahead of it and a space behind
it, so `print " "; day;` is four columns under ten and five over, and **a
numeric column cannot be aligned by printing it**. `str$` is the way out,
because it is the same conversion without the trailing space — so `str$(1)` is
`" 1"`, `str$(10)` is `" 10"`, and `right$(str$(day), 2)` is a right-aligned
day of the month with no case to test. The same free space separates the title
from its year in `m$(mo) + str$(yr)`. The week is then built up in a string and
printed once, the way maze.bas builds a row, since the leading blanks must not
be a `for` loop: a month beginning on a Sunday would gain a phantom cell.

[maze.bas](examples/maze.bas) is the one that could not have been written
before UTF-8 went in. It digs an 8×8 maze with a recursive backtracker — the
recursion written out over an array, there being none to be had here — and
draws it with the sixteen box-drawing glyphs, held in one string and picked by
a bitmask of the walls that touch a square:

```
70 b$ = " ╵╶└╷│┌├╴┘─┴┐┤┬┼"
...
650 l$ = l$ + mid$(b$, m + 1, 1)
```

Each of those glyphs is three bytes, so `mid$(b$, m + 1, 1)` is only the right
one because `mid$` counts characters; `len(b$)` answering 16 rather than 46 is
the same fact. The maze it prints is a `LIST` away from being unreadable under
the old rules, since `b$`'s bytes would have listed back as keywords.

## Testing

`make test` at the top of the tree runs eleven cases from [test/](test/). Ten
drive a session through stdin and stdout redirected to files and compare the
transcript byte for byte against a golden beside the script — exact, because the
run is deterministic and down a pipe nothing echoes and no prompt is printed.
`interrupt.mjs` is on the grid, because a pipe has no keyboard.

`renum.mjs` is every reference `RENUM` rewrites in one program, the three ways
it refuses, and the three places a digit run is data and not a reference — a
`DATA` item, a string and a `REM` tail. Each renumber is listed and run, since
a reference it missed shows up as a `?Undef'd statement` and not as a diff.

`examples.mjs` `LOAD`s and `RUN`s each of the twenty in a
process of its own, so `RND` restarts from its fixed seed every time and one
example's stream cannot shift another's. Its answers are that particular
sequence's moves — the number is 54, the word is `MONITOR`, the wumpus is in
room 9 — so a change to an example that consumes a different number of `RND`
values means re-choosing them, not just re-blessing.

`words.mjs` is the word rules stated once, the way `case.mjs` is the case
rules: the fourteen names that used to be unusable, the boundaries that are not
letters and so still crunch, the entries with punctuation at one end, and the
three forms the change gives up.

`utf8.mjs` is the round trip that used to lose text: a program holding `café`,
`naïve — dash` and `日本語` listed, run, saved, loaded and listed again, then
the character counts, `ASC`/`CHR$` over the whole range, the columns lining up,
and a raw `0xFF` planted in the store — which `put()` cannot express, since it
encodes — coming back as one U+FFFD.

`case.mjs` is the case rules stated once: one keyword whatever the case, one
variable whatever the case, `LIST` canonical in lower case, and a string, a
`DATA` item, a `REM` tail and a filename keeping theirs. It also runs a program
typed the old way, since the point is that both spellings work.

Re-bless a golden with `--bless` after reading the diff.
