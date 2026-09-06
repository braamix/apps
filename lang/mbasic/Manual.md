# Microsoft BASIC 1.1 — Reference Manual

For `mbasic` on Braam. Microsoft BASIC for the 6502, 1976–78, the interpreter
that shipped on the Apple II, the PET, the KIM-1 and the OSI.

Everything here is what the interpreter actually does. Where a rule is
surprising it is marked **⚠**; those are the language, not mistakes.

---

## 1. Running it

| Command | What it does |
| --- | --- |
| `mbasic` | start, and type at the `Ok` prompt |
| `mbasic prog.bas` | **run that program**, then exit |
| `mbasic <script >out` | replay a whole typed session, banner and all |

`mbasic prog.bas` prints only what the program prints — no banner, no `Ok`. The
file's lines are read as if typed, so a numbered line is stored and an
unnumbered one runs at once; when the lines run out the program is `RUN`, unless
the file already ran it itself. `INPUT` reads the keyboard, not the rest of the
file, so a program that asks questions still works. A bare name that is not in
the current directory is looked for among the examples shipped with mbasic.

It exits 0 normally, 1 if any error was reported, and 130 on `^C`.

Output wraps at your terminal's width. Redirected output is never wrapped, and
`PRINT`'s comma zones then assume 80 columns.

Keys while it runs:

| Key | Effect |
| --- | --- |
| `^C` | stop the program — `Break in <line>`; `CONT` resumes |
| `^D` | on an empty line, leave BASIC |

`Ok` is the prompt. A line beginning with a digit is stored as part of the
program; anything else runs immediately (*direct mode*).

**Case does not matter.** `print`, `PRINT` and `PrInt` are the same word, and
`x` and `X` are the same variable. What you type is stored in lower case, so
`LIST` and `SAVE` always show lower case. Text inside `"quotes"`, after `REM`,
and after `DATA` keeps the case you typed.

---

## 2. Program lines

```
10 print "Hello"        store line 10
10                      delete line 10 (a bare number)
10 print "Goodbye"      replace line 10
```

Line numbers run **0 to 63999**. A bigger one is a *syntax* error, not an
overflow. Lines are kept in numeric order however you type them. An input line
may be up to 240 characters.

Several statements fit on one line, separated by `:`

```
10 a = 1 : b = 2 : print a + b
```

**⚠ Typing any program line erases all variables.** Add a line and everything
your program had computed is gone.

---

## 3. Numbers

Nine significant digits. Range roughly ±1.7e38; going past it is `?Overflow`.
Too small silently becomes 0.

Write them as `12`, `-3.5`, `.5`, `1E5`, `2.5e-3`. The `E` may be either case.

**How numbers print.** A non-negative number gets a leading space, a negative
one a `-`; every number printed gets one trailing space. Trailing zeros are
dropped, and so is a bare decimal point. `.5` prints as `.5`, never `0.5`.

| Size of the number | Printed as | Example |
| --- | --- | --- |
| 0.01 ≤ \|x\| < 1000000000 | plain digits | `123.456` |
| anything else, or 0 | `E` notation, sign and 2 exponent digits | `1.5E-07` |

---

## 4. Strings

Up to **255 characters**. Written between double quotes; there is no way to put
a `"` inside a string literal — use `CHR$(34)`.

`+` joins strings. Comparison is by character code, shortest-first on a tie, and
is **case-sensitive**: `"A"` does not equal `"a"`.

```
10 a$ = "Hello" : b$ = a$ + ", world"
20 print b$; len(b$)
```

---

## 5. Variables

A name starts with a letter and continues with letters and digits.

**⚠ Only the first two characters count.** `count` and `counter` are the same
variable.

A reserved word inside a name is fine: `total`, `money` and `positive` are
ordinary variables, though a name that *is* a reserved word — `to`, `len` — is
not. See §12.

Three types, and a suffix picks it. `a`, `a$` and `a%` are three *different*
variables.

| Written | Type | Holds |
| --- | --- | --- |
| `a` | number | any number |
| `a$` | string | up to 255 characters |
| `a%` | integer | −32768 to 32767, fractions floored |

A variable you have never assigned reads as 0 or as the empty string; reading
it does not create it.

---

## 6. Arrays

```
10 dim a(20), name$(5), grid(9, 9)
```

Subscripts start at **0**, so `dim a(20)` gives 21 elements, `a(0)` to `a(20)`.
Numeric elements start at 0, string elements empty.

Using an array without `DIM` gives every dimension 11 elements (0 to 10).
`DIM`ming an array that already exists is `?Redim'd array`. A subscript out of
range, or the wrong *number* of subscripts, is `?Bad subscript`.

---

## 7. Expressions

Operators, tightest first:

| Precedence | Operators | Notes |
| --- | --- | --- |
| 1 | `^` | power |
| 2 | `-` | negate |
| 3 | `*` `/` | |
| 4 | `+` `-` | `+` also joins strings |
| 5 | `=` `<>` `<` `>` `<=` `>=` | comparison |
| 6 | `NOT` | |
| 7 | `AND` | |
| 8 | `OR` | |

Brackets override, as usual.

**Comparisons give −1 for true and 0 for false.** That is why `NOT 0` is −1.

`AND`, `OR` and `NOT` work on whole numbers bit by bit; both operands must be
in −32768 to 32767. Because true is −1 (all bits set) they double as logic:

```
10 if a > 0 and b > 0 then print "Both positive"
```

Powers: `0^0` is 1, `x^0` is 1, `0^y` is 0. A negative base needs a whole-number
exponent, otherwise `?Illegal quantity`.

**⚠** `=<` and `><` are accepted spellings of `<=` and `<>`. `<<` is an error.

---

## 8. Statements

`LET` is optional: `let a = 1` and `a = 1` are the same. `?` is short for
`PRINT`.

### Assignment and data

| Statement | Meaning |
| --- | --- |
| `LET v = e` | assign; the variable's type decides the conversion |
| `DIM a(n), b(n,m)` | declare arrays |
| `DATA v, v, …` | constants for `READ`; quote a value containing `,` or `:` |
| `READ v, v, …` | take the next `DATA` values |
| `RESTORE` | make `READ` start again from the first `DATA` |
| `CLEAR` | erase all variables, keep the program |

### Input and output

| Statement | Meaning |
| --- | --- |
| `PRINT items` | print; see below |
| `INPUT v, v` | prompt `? ` and read |
| `INPUT "text"; v` | print `text`, then `? `, then read |
| `GET v` | read one keypress without waiting for Enter |

`PRINT` separators:

- `;` — items run together, no gap.
- `,` — move to the next 14-column zone.
- nothing at the end — a newline is printed.
- `;` or `,` at the end — **no newline**, the next `PRINT` carries on.

Inside `PRINT`, `TAB(n)` moves to column *n* and `SPC(n)` prints *n* spaces.
Both need the closing bracket. **⚠ `TAB` to a column already passed does
nothing.**

```
10 print "Name", "Score"        two zones
20 print "x = "; x              no gap
30 print "wait";                stay on this line
```

Reading rules:

- Values are separated by commas. Too few, and it asks again with `?? `.
- Too many, and it says `?Extra ignored`.
- A value of the wrong shape gives `?Redo from start` and **⚠ the whole `INPUT`
  statement runs again**, including variables it already assigned.
- **⚠ An empty line typed to `INPUT` stops the program silently** — no message,
  and `CONT` will not restart it cleanly, because it resumes inside the `INPUT`
  statement and reports `?Syntax error`. Press Enter only when you mean to stop
  for good.
- `INPUT` and `GET` are not allowed in direct mode.
- `GET` assigns the empty string if the key produced no character.

### Choices and loops

| Statement | Meaning |
| --- | --- |
| `IF e THEN stmts` | run them when `e` is non-zero |
| `IF e THEN n` | jump to line *n* |
| `IF e GOTO n` | the same |
| `FOR v = a TO b [STEP s]` | begin a loop; `s` defaults to 1 |
| `NEXT [v [, v …]]` | end a loop; bare `NEXT` closes the innermost |
| `GOTO n` | jump |
| `GOSUB n` | call; `RETURN` comes back |
| `RETURN` | return from `GOSUB` |
| `ON e GOTO n, n, …` | jump to the *e*-th line number |
| `ON e GOSUB n, n, …` | call the *e*-th |

There is **no `ELSE`**. Write a second `IF`.

**⚠ A false `IF` skips the whole rest of the line**, not just to the next `:`.
In `if x then a=1 : b=2`, `b=2` only runs when `x` is true.

**⚠ A `FOR` body always runs at least once** — the test is at `NEXT`. So
`for i = 1 to 0` executes once.

**⚠ A loop never ends on equality, and `STEP 0` never ends at all.**

`ON` with 0, or with a number past the end of the list, simply carries on to the
next statement — no error.

A `GOSUB` that leaves loops open still returns correctly.

### Running and stopping

| Statement | Meaning |
| --- | --- |
| `RUN` | clear variables and start at the first line |
| `RUN n` | clear variables and start at line *n* |
| `STOP` | stop, print `Break in <line>` |
| `END` | stop silently |
| `CONT` | carry on after `STOP`, `^C` or an empty `INPUT` |
| `NEW` | erase the program and the variables |
| `LIST` | show the program |
| `RENUM` | renumber the program |
| `REM text` | a comment, to the end of the line |

`LIST` forms: `LIST`, `LIST 100`, `LIST 100-`, `LIST -200`, `LIST 100-200`.

`RENUM [new][,[old][,inc]]` renumbers from line *old* onwards, giving the first
of them the number *new* and stepping by *inc*. Left out, *new* and *inc* are
both 10 and *old* is the first line of the program.

```
renum                  10, 20, 30, …
renum 100              100, 110, 120, …
renum 1000, 500, 5     lines below 500 keep their numbers
```

Every line number a statement refers to is renumbered with it — after `GOTO`,
`GO TO`, `GOSUB`, `THEN`, `ON … GOTO`, `ON … GOSUB` and `RUN`. Numbers inside
`DATA`, inside strings and after `REM` are left alone.

**⚠ `RENUM` cannot reorder the program**, and cannot carry a line past 63999.
Either is `?Illegal quantity`, and nothing is changed.

A reference to a line that does not exist is reported as `Undefined line 999 in
500` — naming the new number of the line it is in — and left as you typed it.
The rest of the program is renumbered anyway.

**⚠ `CONT` does not work after an error, or after you edit the program.**
`RENUM` counts as editing: it erases the variables too.

### Definitions

```
10 def fna(x) = x * x + 1
20 print fna(3)                    prints  10
```

One numeric argument, a numeric result. The function must be defined before it
is used. The argument name is an ordinary variable whose value is put back
afterwards, so using `x` elsewhere is safe.

### Files

| Statement | Meaning |
| --- | --- |
| `SAVE "path"` | write the program as text |
| `LOAD "path"` | replace the program with one read from a file |
| `OPEN n, "path"` | open channel *n* (1–255) for reading |
| `OPEN n, "path", "W"` | open channel *n* for writing |
| `PRINT# n, items` | write to channel *n* |
| `INPUT# n, v, v` | read from channel *n* |
| `CLOSE n` | close channel *n* |

A saved program is ordinary text — the same lines `LIST` shows.

Filenames keep the case you type. A plain name with no `/` that is not in the
current directory is also looked for among the examples shipped with mbasic, so
`load "wumpus.bas"` works.

### Machine statements

`POKE a, b`, `PEEK(a)` and `WAIT a, m [, x]` address 64 KiB of scratch memory
that is theirs alone — it is not the interpreter's storage and not the
computer's. Addresses run 0 to 65535.

`SYS` and `USR` always give `?Illegal quantity`: there is no machine code to
call. `SYSTEM` is an ordinary variable — a keyword must stand alone (§12).

---

## 9. Functions

### Numbers

| Function | Gives |
| --- | --- |
| `ABS(x)` | absolute value |
| `SGN(x)` | −1, 0 or 1 |
| `INT(x)` | largest whole number ≤ x — **⚠ `INT(-2.5)` is −3** |
| `SQR(x)` | square root; negative x is `?Illegal quantity` |
| `EXP(x)` | e to the x |
| `LOG(x)` | natural logarithm; x ≤ 0 is `?Illegal quantity` |
| `SIN(x)` `COS(x)` `TAN(x)` | radians |
| `ATN(x)` | arctangent, in radians |
| `RND(x)` | random number, 0 ≤ r < 1 |

`RND(1)` (any positive) gives the next number, `RND(0)` repeats the last one,
and a negative argument restarts the sequence from that value. **⚠ The sequence
is the same every run** unless you reseed it.

```
10 d = int(rnd(1) * 6) + 1        a dice roll
```

### Strings

| Function | Gives |
| --- | --- |
| `LEN(a$)` | length |
| `LEFT$(a$, n)` | first *n* characters |
| `RIGHT$(a$, n)` | last *n* characters |
| `MID$(a$, p)` | from position *p* to the end |
| `MID$(a$, p, n)` | *n* characters from position *p* |
| `CHR$(n)` | the character with code *n* (0–255) |
| `ASC(a$)` | code of the first character; empty is `?Illegal quantity` |
| `STR$(x)` | the number as text, with its leading space |
| `VAL(a$)` | the number at the front of the text, else 0 |

Positions start at **1**. `MID$(a$, 0, …)` is `?Illegal quantity`; a position
past the end gives the empty string. Asking for more characters than there are
gives what there is.

```
10 u$ = chr$(asc(c$) - 32)        lower case to upper
```

### Housekeeping

| Function | Gives |
| --- | --- |
| `POS(x)` | current print column; the argument is ignored |
| `FRE(x)` | bytes left of a fixed 65535-byte budget |

**⚠ `FRE` is a *signed* 16-bit count**, so anything above 32767 free shows as a
negative number — an empty program reports `-1`. That is how the original
behaved. The budget is fixed here; upstream asked for it at startup.

---

## 10. Errors

Every message reads `?<message> error`, plus ` in <line>` when a program was
running.

| Message | Usual cause |
| --- | --- |
| `Syntax` | mistyped; or a keyword with a letter against it |
| `Next without for` | `NEXT` with no matching `FOR` |
| `Return without gosub` | `RETURN` with no matching `GOSUB` |
| `Out of data` | `READ` past the last `DATA` |
| `Illegal quantity` | argument out of range |
| `Overflow` | number too big |
| `Out of memory` | too many variables, or an expression nested past 23 deep |
| `Undef'd statement` | `GOTO`/`GOSUB` to a line that does not exist |
| `Bad subscript` | subscript out of range, or the wrong number of them |
| `Redim'd array` | `DIM` on an array that already exists |
| `Division by zero` | |
| `Illegal direct` | `INPUT` or `GET` typed outside a program |
| `Type mismatch` | number where a string belongs, or the reverse |
| `String too long` | a result past 255 characters |
| `File data` | no such channel, or the file could not be read |
| `Formula too complex` | over three string parts at once — split the line |
| `Can't continue` | `CONT` after an error or an edit |
| `Undef'd function` | `FN` used before `DEF` |

---

## 11. Limits

| | |
| --- | --- |
| Line numbers | 0 – 63999 |
| Input line | 240 characters |
| String length | 255 characters |
| Integer variables (`%`) | −32768 – 32767 |
| Number range | about ±1.7e38, 9 significant digits |
| Expression nesting | 23 levels |
| Open `FOR`/`GOSUB` | 46 |
| String parts in one expression | 3 |
| `PRINT` comma zones | 14 columns |

---

## 12. Reserved words

A variable may not **be** one of these, though it may **contain** one: `total`
and `positive` are names, `to` and `pos` are not.

```
abs   and   asc   atn    chr$   clear close cmd     cont   cos
data  def   dim   end    exp    fn    for   fre     get    go
gosub goto  if    input  input# int   left$ len     let    list
load  log   mid$  new    next   not   on    open    or     peek
poke  pos   print print# read   rem   renum restore return right$
rnd   run   save  sgn    sin    spc(  sqr   step    stop   str$
sys   tab(  tan   then   to     usr   val   wait
```

Also the operators `+ - * / ^ > = <`.

**⚠ A keyword must stand alone.** It is recognised only where a letter does not
touch it on either side, so `for i = 1 to n` works and `fori=1ton` does not —
that is the variable `fo`. A digit is not a letter, so `1to10`, `print1`,
`goto100` and `5and3` are still read as keywords.

`fn` is the exception, because a name always follows it: `fna(x)` is `fn a(x)`,
so no variable may begin with `fn`.

---

## 13. A worked example

```
10 rem Guess the number
20 n = int(rnd(1) * 100) + 1
30 t = 0
40 input "Your guess"; g
50 t = t + 1
60 if g < n then print "Too low!" : goto 40
70 if g > n then print "Too high!" : goto 40
80 print "Got it in"; t; "tries"
90 end
```

Note `goto 40` inside the `IF`: everything after `THEN` on that line belongs to
the `IF`.

---

## 14. Differences from the 1978 original

- Case is folded; the original was upper case only.
- A keyword must stand alone, so `total` is a variable. The original matched a
  reserved word anywhere and read it as `to` followed by `tal`; the cost of the
  change is that `fori=1ton` no longer works.
- Messages are in sentence case — `Ok`, `?Syntax error` — not capitals.
- Arithmetic is 64-bit, so long calculations are more accurate in their last
  digits. The printed format is unchanged.
- `RENUM` is here, from the later Microsoft releases; the 1978 original had no
  way to renumber a program.
- `LOAD` and `SAVE` use named files instead of cassette tape.
- `SYS` and `USR` have no machine to call.
- `WAIT` tests its condition once instead of spinning for ever.
