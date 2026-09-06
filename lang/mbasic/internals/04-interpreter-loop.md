# 04 — The interpreter loop, dispatch, and errors

---

## 1. `NEWSTT` — the statement fetcher

[m6502.asm:2131-2192](../m6502.asm#L2131-L2192). The source describes the contract at
[m6502.asm:2132-2137](../m6502.asm#L2132-L2137):

> BACK HERE FOR NEW STATEMENT. CHARACTER POINTED TO BY TXTPTR IS ":" OR END-OF-LINE. THE ADDRESS OF
> THIS LOC IS LEFT ON THE STACK WHEN A STATEMENT IS EXECUTED SO THAT IT CAN MERELY DO A RTS WHEN IT
> IS DONE.

```
NEWSTT: JSR ISCNTC             ; IFN REALIO - listen for ^C
        LDWD TXTPTR            ; A = TXTPTR, Y = TXTPTR+1
        CPYI BUFPAG            ; IFN BUFPAG
        BEQ DIRCON             ; direct mode => do NOT save OLDTXT
        STWD OLDTXT
DIRCON: LDYI 0
        LDADY TXTPTR
        BNE MORSTS             ; not NUL: must be ':' or a syntax error
        LDYI 2
        LDADY TXTPTR           ; the link's high byte
        CLC
        JEQ ENDCON             ; zero => ran off the end of the program
        INY / LDADY TXTPTR -> CURLIN
        INY / LDADY TXTPTR -> CURLIN+1
        TYA                    ; Y = 4
        ADC TXTPTR -> TXTPTR   ; step past link + line number
        (INC TXTPTR+1 on carry)
GONE:   JSR CHRGET
        JSR GONE3
        JMP NEWSTT
```

### 1.1 What happens once per statement, and what once per line

Per **statement**: the ^C check, and the `OLDTXT` save. Per **line**: loading `CURLIN` and stepping
`TXTPTR` over the 4-byte header.

`OLDTXT` is the anchor for "restart this statement". It is written at every statement boundary but
**only in program mode**, which is what makes `CONT` and `INPUT`'s `?REDO FROM START` work.

### 1.2 Direct versus program mode — two different tests

The interpreter decides "am I running a typed-in line?" two different ways in two different places,
and a port must keep both:

| Test | Where | Used by |
|---|---|---|
| `TXTPTR+1 == BUFPAG` (or `== 0` when `BUFPAG=0`) | `NEWSTT`, [m6502.asm:2141-2143](../m6502.asm#L2141-L2143) | deciding whether to save `OLDTXT` |
| `CURLIN+1 == 255` | `ERROR`, `STOP`/`END`, `ERRDIR` | deciding whether to print `IN <line>`, whether `CONT` is possible, whether `DEF`/`GET` are legal |

`MAIN` sets `CURLIN+1 = 255` the moment it sees a non-empty line
([m6502.asm:1562-1563](../m6502.asm#L1562-L1563)), before it even knows whether the line is a program
line or a direct statement.

The `DIRCON:` label is placed *after* `LDYI 0` when `BUFPAG=0` and *before* it otherwise
([m6502.asm:2145-2147](../m6502.asm#L2145-L2147)), because in the `BUFPAG=0` case `Y` is already zero
from the comparison.

The 1977-12-01 and 1978-07-27 revision notes
([m6502.asm:215-217](../m6502.asm#L215-L217), [m6502.asm:235-236](../m6502.asm#L235-L236)) both concern
this area: code that assumed `TXTPTR+1 == 0` iff direct, and a check of `CURLIN` made before
`CURLIN` was set up.

---

## 2. Dispatch

```
GONE3:  BEQ ISCRTS              ; terminator => nothing to do, back to the loop
GONE2:  SBCI ENDTK              ; carry is already 1 (CHRGET sets it for non-numerics)
        BCC GLET                ; below the first statement token => implied LET
        CMPI SCRATK-ENDTK+1     ; = 26 here
        BCS SNERRX              ; a reserved word, but not a statement one
        ASL A / TAY
        LDA STMDSP+1,Y / PHA
        LDA STMDSP,Y   / PHA
        JMP CHRGET
```

### 2.1 The `RTS`-to-`address-1` trick

`STMDSP` stores each handler's address **minus one** (`ADR(END-1)`, `ADR(FOR-1)`, …,
[m6502.asm:997](../m6502.asm#L997)). The dispatcher pushes that value and then jumps to `CHRGET`.
`CHRGET`'s own `RTS` pops it and, because 6502 `RTS` sets `PC = popped + 1`, lands on the handler's
first instruction.

Two things fall out of this that handlers depend on:

- Every statement handler is entered with `A` = the first character **after** its token, and with
  `CHRGET`'s full flag contract already set.
- The stack at handler entry holds `NEWSTT`'s return address (from the `JSR GONE3` at
  [m6502.asm:2166](../m6502.asm#L2166)) and nothing else belonging to the dispatcher.

### 2.2 Implied `LET`

Any byte below `ENDTK` — a letter, a digit, anything that is not a reserved word — falls through to
`GLET` and is handed to `LET` ([m6502.asm:2184](../m6502.asm#L2184)). So `A=1` and `LET A=1` take the
same path, and a stray character produces a `LET`-shaped syntax error rather than a distinct one.

### 2.3 The `GO TO` special case

`SNERRX` ([m6502.asm:2188-2192](../m6502.asm#L2188-L2192)):

```
SNERRX: CMPI GOTK-ENDTK
        BNE SNERR1
        JSR CHRGET
        SYNCHK TOTK
        JMP GOTO
```

`GO` is a reserved word with a token (194) but no `STMDSP` entry, so it lands in the "reserved word
that is not a statement" arm. That arm checks for it specifically, consumes the following `TO`, and
enters `GOTO`. This exists purely because the tokenizer cannot match across the space — see
[03-tokenizer-editor.md](03-tokenizer-editor.md#23-why-spaces-cannot-appear-inside-reserved-words).

### 2.4 Statement termination

`MORSTS`: a byte that is neither NUL nor `:` where a terminator was expected is a syntax error. A
`:` loops back to `GONE` for the next statement on the line.

Because `NEWSTT` re-checks the terminator after every handler returns, a statement that reads its
arguments and then simply `RTS`es without consuming the rest of its text produces a syntax error
automatically. Several statements use this deliberately (`NEW`, `CLEAR`, `CONT`, `RETURN`) as a way
of saying "only act if the syntax was perfect".

### 2.5 Handlers that must not return normally

A handler that pushes a semi-permanent stack frame, or that abandons the current position, has to
discard `NEWSTT`'s return address itself and jump back to `NEWSTT` (or elsewhere):

| Handler | Why | Where |
|---|---|---|
| `FOR` | pushes an 18-byte frame that must sit directly above `NEWSTT`'s return address | [m6502.asm:2092-2093](../m6502.asm#L2092-L2093) |
| `NEXT` | truncates the stack with `TXS` | [m6502.asm:3120](../m6502.asm#L3120) |
| `LIST` | exits to `READY`, not to the program | [m6502.asm:1988-1989](../m6502.asm#L1988-L1989) |
| `STOP` / `END` | exits to `READY` | [m6502.asm:2240-2241](../m6502.asm#L2240-L2241) |
| `RUN` with an argument | `CLEARC` has already reset the stack | [m6502.asm:2366-2368](../m6502.asm#L2366-L2368) |
| `DDT` | `REALIO=0` only | [m6502.asm:2250-2254](../m6502.asm#L2250-L2254) |

The invariant they are preserving is the one `FNDFOR` relies on: **`NEWSTT`'s return address sits
immediately below the topmost `FOR`/`GOSUB` frame** — see
[02-data-structures.md](02-data-structures.md#44-frame-discovery-geometry).

---

## 3. The ^C check

`ISCNTC` is called once per statement from `NEWSTT` and once per line from `LIST`. It is absent
entirely when `REALIO=0`.

Each target supplies its own ([m6502.asm:2205-2226](../m6502.asm#L2205-L2226)); the Commodore uses an
external vector. All of them share one structural feature: **`ISCNTC` falls through into `STOP`**.
There is no "return true"; the break is implemented by dropping into the `STOP` path with the carry
and zero flags arranged so that `STOP` prints `BREAK`.

```
STOP:   BCS STOPC        ; carry set => print BREAK
END:    CLC
STOPC:  BNE CONTRT       ; not ^C, or no terminator after STOP/END => just return
```

> **The Apple implementation does not work as written.** `ISCCAP` calls `INCHR`, which does
> `ANDI 127` ([m6502.asm:1756-1757](../m6502.asm#L1756-L1757)), so the character in `A` is 3. It then
> compares against 131 (`^O203`). The comparison fails with both `C=0` and `Z=0`, so `STOP`'s `BCS`
> is not taken, `END`'s `CLC` runs, and `STOPC`'s `BNE` returns. The ^C is consumed from the
> keyboard but never interrupts. See
> [13-porting-notes.md](13-porting-notes.md#3-defects-in-the-1978-code).

---

## 4. The error handler

### 4.1 `ERROR`

[m6502.asm:1516-1547](../m6502.asm#L1516-L1547). Entered with **`X` = the error code**, which is a byte
offset into `ERRTAB`. `OMERR` is a labelled entry that presets `ERROM` first.

```
OMERR:  LDXI ERROM
ERROR:  LSR CNTWFL              ; force output back on
        [EXTIO: close the current channel]
ERRCRD: JSR CRDO / JSR OUTQST   ; newline, then '?'
        print the message (two characters, or the long form)
TYPERR: JSR STKINI              ; reset stack, temporaries, SUBFLG; OLDTXT+1 = 0
        LDWDI ERR               ; " ERROR"
ERRFIN: JSR STROUT
        LDY CURLIN+1 / INY / BEQ READY    ; direct mode => no " IN n"
        JSR INPRT                          ; " IN " + line number
READY:  LSR CNTWFL
        LDWDI REDDY
        JSR RDYJSR              ; patched to JMP STROUT by INIT
MAIN:   ...
```

`STKINI` is what makes an error unrecoverable: it resets the stack pointer, discarding every `FOR`
and `GOSUB` frame, and zeroes `OLDTXT+1` so `CONT` refuses. Variables and the program survive; all
control-flow context does not.

`ERRFIN` is shared with the `BREAK` path ([m6502.asm:2242-2247](../m6502.asm#L2242-L2247)), entered
with `[Y,A]` pointing at `BRKTXT` instead of `ERR`.

`RDYJSR` is a page-zero `JMP` that `INIT` leaves pointing at `INIT` until startup completes. An
error raised *during* initialisation therefore restarts initialisation rather than trying to print
a prompt with half-configured state.

### 4.2 Error codes, `LNGERR = 0` (this build)

[m6502.asm:1247-1287](../m6502.asm#L1247-L1287). Two characters each; the symbol's value is its byte
offset, stepping by 2.

| Value | Code | Meaning |
|---|---|---|
| 0 | `NF` | `NEXT` without `FOR` |
| 2 | `SN` | Syntax |
| 4 | `RG` | `RETURN` without `GOSUB` |
| 6 | `OD` | Out of data |
| 8 | `FC` | Illegal quantity |
| 10 | `OV` | Overflow |
| 12 | `OM` | Out of memory / out of string space |
| 14 | `US` | Undefined statement |
| 16 | `BS` | Bad subscript |
| 18 | `DD` | Redimensioned array |
| 20 | `/0` | Division by zero |
| 22 | `ID` | Illegal direct |
| 24 | `TM` | Type mismatch |
| 26 | `LS` | String too long |
| (28) | `FD` | File data — **`IFN EXTIO` only** |
| 28 (30) | `ST` | String formula too complex |
| 30 (32) | `CN` | Can't continue |
| 32 (34) | `UF` | Undefined function |

### 4.3 Error codes, `LNGERR = 1`

[m6502.asm:1289-1346](../m6502.asm#L1289-L1346). The messages are spelled out and concatenated, with
bit 7 set on the last character of each; the symbol's value is its byte offset into the blob. The
source warns at [m6502.asm:1291-1292](../m6502.asm#L1291-L1292) that the technique breaks if the total
text exceeds 256 characters.

Offsets (with `EXTIO=0`): `NEXT WITHOUT FOR` 0, `SYNTAX` 16, `RETURN WITHOUT GOSUB` 22,
`OUT OF DATA` 42, `ILLEGAL QUANTITY` 53, `OVERFLOW` 69, `OUT OF MEMORY` 77,
`UNDEF'D STATEMENT` 90, `BAD SUBSCRIPT` 107, `REDIM'D ARRAY` 120, `DIVISION BY ZERO` 133,
`ILLEGAL DIRECT` 149, `TYPE MISMATCH` 163, `STRING TOO LONG` 176, `FORMULA TOO COMPLEX` 191,
`CAN'T CONTINUE` 210, `UNDEF'D FUNCTION` 224.

> **The numeric value of every error code differs between `LNGERR=0` and `LNGERR=1`.** They are byte
> offsets into two unrelated tables. Any reimplementation must key on the symbolic names, never on
> the numbers.

### 4.4 Messages

[m6502.asm:1348-1364](../m6502.asm#L1348-L1364):

```
ERR:    " ERROR", 0
INTXT:  " IN ", 0
REDDY:  CR, LF, "OK", CR, LF, 0      ; "READY." on the Commodore
BRKTXT: CR, LF, "BREAK", 0
```

All are NUL-terminated and **must live above page zero**. `STROUT` goes through `STRLIT`, which
copies any string whose data is on page 0 into string space — see
[08-strings-gc.md](08-strings-gc.md#33-the-copy-if-volatile-rule). The source states the constraint
at [m6502.asm:382-387](../m6502.asm#L382-L387).

### 4.5 `CONT`

[m6502.asm:2255-2266](../m6502.asm#L2255-L2266). Requires a terminator (`BNE CONTRT`); raises `?CN` if
`OLDTXT+1 == 0`; otherwise restores `TXTPTR` from `OLDTXT` and `CURLIN` from `OLDLIN` and returns
into `NEWSTT`.

`OLDTXT` / `OLDLIN` are written by:

- `NEWSTT`, at every statement boundary in program mode;
- `STOP`, `END` and ^C ([m6502.asm:2232-2239](../m6502.asm#L2232-L2239)), which store the pointer to
  their own terminating character;
- a bare carriage return typed to `INPUT`.

And `OLDTXT+1` is zeroed by `STKINI`, which runs on every error and on every `CLEAR`, `RUN`, `NEW`
and program edit. So the rule "you can continue after `STOP` or ^C, but not after an error or after
touching the program" is a direct consequence of a single byte.

---

## 5. The two memory guards

Every allocation in the system passes through one of these two checks. Neither is an allocator; both
just assert that two pointers have not crossed.

### 5.1 `GETSTK` — is there enough 6502 stack?

[m6502.asm:1472-1480](../m6502.asm#L1472-L1480).

```
GETSTK: ASL A                              ; A = number of 2-byte entries needed; x2, clears C
        ADCI 2*NUMLEV+<3*ADDPRC>+13        ; = 46 + 3 + 13 = 62
        BCS OMERR
        STA INDEX
        TSX / CPX INDEX / BCC OMERR        ; require S >= 2n + 62
```

`NUMLEV` = 23 ([m6502.asm:238](../m6502.asm#L238)) is the number of guaranteed nesting levels. The
extra 13 (16 with `ADDPRC`) protects `FBUFFR`, which sits immediately below the stack — a stack
overflow would otherwise silently corrupt the number-formatting buffer.

| Caller | `A` | Effective requirement |
|---|---|---|
| `FRMEVL`, at every nesting level | 1 | `S > 64` |
| `GOSUB` | 3 | `S > 68` |
| `FOR` | 9 | `S > 80` |

`NUMLEV` was raised from 19 to 23 on 1978-02-25 ([m6502.asm:227-228](../m6502.asm#L227-L228)).

### 5.2 `REASON` — is there enough heap?

[m6502.asm:1486-1512](../m6502.asm#L1486-L1512). Asserts `[Y,A] < FRETOP`. If not:

1. push `A`, then push the 9 bytes `HIGHDS`..`LOWTR+1` and `Y`;
2. `JSR GARBA2` — force a garbage collection;
3. pop everything back;
4. re-test, and raise `?OM` if it still fails.

Saving those nine consecutive page-zero bytes across the collection is why their adjacency is a hard
constraint (see [01-memory-map.md](01-memory-map.md#3-adjacency-constraints)). The restore loop
starts at `LDXI 256-8-ADDPRC` and counts up with `INX/BMI`.

`BLTU` calls `REASON` before every block move, which is what makes "insert a program line" and
"create a variable" able to trigger a string garbage collection.
