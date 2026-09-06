# 05 — Statement semantics

Every statement handler is entered with `A` = the character following its token and `CHRGET`'s flag
contract set, and returns by `RTS` to `NEWSTT` unless noted. See
[04-interpreter-loop.md](04-interpreter-loop.md#2-dispatch).

---

## 1. `LET`

[m6502.asm:2539-2653](../m6502.asm#L2539-L2653).

```
LET:  JSR PTRGET / STWD FORPNT
      SYNCHK EQULTK
      LDA INTFLG / PHA          ; INTPRC: remember the variable's integer-ness
      LDA VALTYP / PHA          ; and its type
      JSR FRMEVL
      PLA / ROL A               ; carry := "the variable is a string"
      JSR CHKVAL                ; type-check, and leave Z=1 for numeric
      BNE COPSTR
```

Three stores, chosen by the *variable's* type, not the expression's:

| Target | Action |
|---|---|
| float | `MOVVF` — round and pack 5 bytes to `(FORPNT)` |
| integer `%` | `ROUND`, `AYINT`, then store `FACMO` at offset 0 and `FACLO` at offset 1 — **big-endian** ([m6502.asm:2558-2571](../m6502.asm#L2558-L2571)) |
| string | `COPSTR` → `INPCOM` → `GETSPT` |

### 1.1 The copy decision

`GETSPT` ([m6502.asm:2616](../m6502.asm#L2616)) decides whether the string body must be duplicated:

```
copy  <=>  (body pointer >= FRETOP)  AND  (descriptor address >= VARTAB)
```

That is: copy only when the source data lives in dynamic string space **and** the source descriptor
belongs to a real variable or array element. Temporaries live in `TEMPST` on page zero, below
`VARTAB`, so a value that is already a temporary is simply adopted.

The reason is the garbage collector's single-owner invariant — see
[08-strings-gc.md](08-strings-gc.md#5-garbage-collection). `A$=B$` where `B$`'s data is in string
space must copy, because otherwise two descriptors would point at one body. `A$=CHR$(7)` need not,
because the only pointer to that one-byte body is a temporary that is about to disappear.

Either way `COPYZC` calls `FRETMS` (freeing the temporary *slot* but not the space) and then copies
the three descriptor bytes into `(FORPNT)`.

`INPCOM` is shared verbatim by `INPUT`, `READ` and `GET`, which is why those statements must also
set `FORPNT`.

---

## 2. `PRINT`

[m6502.asm:2655-2849](../m6502.asm#L2655-L2849).

### 2.1 State

| Name | Meaning |
|---|---|
| `TRMPOS` | current column |
| `LINWID` | line width; a RAM byte initialised to `LINLEN` (40) and settable at startup |
| `NCMWID` | column beyond which there are no more comma fields; initialised to `NCMPOS` |
| `CLMWID` | comma field width — an **assembly constant**, 14 here, not a variable |

`NCMPOS == ((LINLEN/CLMWID)-1)*CLMWID` ([m6502.asm:2749](../m6502.asm#L2749)) = 14 for this build.

### 2.2 The item loop

```
STRDON: JSR STRPRT
NEWCHR: JSR CHRGOT
PRINT:  BEQ CRDO           ; terminator reached normally => newline
PRINTC: BEQ PRTRTS         ; terminator after , ; or TAB() => NO newline
        CMPI TABTK / BEQ TABER        ; carry SET   => TAB
        CMPI SPCTK / CLC / BEQ TABER  ; carry CLEAR => SPC
        CMPI 44 / BEQ COMPRT          ; comma
        CMPI 59 / BEQ NOTABR          ; semicolon
        JSR FRMEVL
        BIT VALTYP / BMI STRDON       ; string => print it
        JSR FOUT / JSR STRLIT         ; number => format, build a descriptor
        ...width check...
LINCHK: JSR STRPRT / JSR OUTSPC / BNEA NEWCHR
```

The whole `;`/`,`-suppresses-the-newline rule is the two-instruction difference between the `PRINT`
and `PRINTC` entry points ([m6502.asm:2669-2670](../m6502.asm#L2669-L2670)). Separators jump to code
that re-enters at `PRINTC`, so a trailing separator leaves the cursor where it is.

Numeric items get a **leading** sign character from `FOUT` (a space when positive) and a **trailing**
space from `OUTSPC`. Before printing a number, `TRMPOS + length` is compared against `LINWID` and a
newline is issued first if it would not fit ([m6502.asm:2687-2695](../m6502.asm#L2687-L2695)) — this
check is `IFN REALIO-3`, so the Commodore does not do it.

### 2.3 Comma zones

`COMPRT` ([m6502.asm:2748-2762](../m6502.asm#L2748-L2762)):

```
COMPRT: LDA TRMPOS
        CMP NCMWID / BCC MORCOM
        JSR CRDO / JMP NOTABR      ; past the last field: just move to the next line
MORCOM: SEC
MORCO1: SBCI CLMWID / BCS MORCO1   ; A mod CLMWID, by repeated subtraction
        EORI 255 / ADCI 1          ; => CLMWID - (TRMPOS mod CLMWID)
        BNE ASPAC                  ; print that many spaces
```

### 2.4 `TAB(` and `SPC(`

`TABER` ([m6502.asm:2764-2780](../m6502.asm#L2764-L2780)) handles both. `PHP` saves the carry that
distinguishes them, `GTBYTC` reads the argument into `X`, the closing `)` is checked explicitly
(it is not part of the token), then `PLP`:

- carry clear (`SPC`): print `X` spaces.
- carry set (`TAB`): `TXA / SBC TRMPOS`; if the result is negative, print nothing. So `TAB` to a
  column already passed does nothing rather than wrapping.

`NOTABR` re-enters at `PRINTC`, which is how separators avoid the newline.

### 2.5 `CRDO` and `OUTDO`

`CRDO` ([m6502.asm:2711-2746](../m6502.asm#L2711-L2746)) sets `TRMPOS = 13` *before* emitting anything
— a trick to make `TRMPOS` less than the line width so the automatic-wrap check in `OUTDO` does not
fire while the newline itself is being written. It then emits 13 and 10 and falls into `CRFIN`.

`CRFIN` with `NULCMD=0` just zeroes `TRMPOS`. With `NULCMD=1` it first emits `NULCNT` NUL characters
— padding for mechanical terminals that need time for the carriage to return.

`OUTDO` ([m6502.asm:2818-2848](../m6502.asm#L2818-L2848)) is the single character-output choke point:

1. If `CNTWFL` bit 7 is set (the user typed ^O), output is suppressed entirely.
2. Characters below 32 do not advance `TRMPOS`.
3. If `TRMPOS == LINWID`, a `CRDO` is forced **before** the character — automatic line wrap.
4. `INC TRMPOS`.

On the Apple, `OUTDO` sets bit 7 of the character before calling the monitor and clears it
afterwards ([m6502.asm:2838](../m6502.asm#L2838), [m6502.asm:2844](../m6502.asm#L2844)).

### 2.6 `STROUT` / `STRPRT`

[m6502.asm:2785-2801](../m6502.asm#L2785-L2801). `STROUT` takes a NUL-terminated string pointer in
`[Y,A]`, runs it through `STRLIT` to build a descriptor, and falls into `STRPRT`. `STRPRT` frees the
temporary and prints the bytes; **a character equal to 13 inside the string triggers `CRFIN`**, so
embedded carriage returns reset `TRMPOS` and emit any null padding.

### 2.7 `PRINT#` (`EXTIO` only)

[m6502.asm:2656-2666](../m6502.asm#L2656-L2666). `CMD` reads a channel number, opens the output
channel, stores it in `CHANNL`, and falls into `PRINT`. Throughout the printer a non-zero `CHANNL`
suppresses `TRMPOS` maintenance and automatic newlines.

---

## 3. `INPUT`, `READ`, `GET`, `DATA`, `RESTORE`

[m6502.asm:2852-3092](../m6502.asm#L2852-L3092). All three input statements share one loop, selected by
`INPFLG`.

### 3.1 `INPFLG` encoding

| Value | Statement | How it is tested |
|---|---|---|
| `$00` | `INPUT` | `BEQ` |
| `$40` | `GET` | `BVS` / `BVC` — the overflow bit |
| `$98` | `READ` | `BMI` / `BPL` — the negative bit |

The three values are chosen so that one byte answers all three questions with single-bit branches.
The `READ` value is synthesised by the `XWD ^O1000,^O251` opcode trick at
[m6502.asm:2948-2952](../m6502.asm#L2948-L2952) rather than written as `LDA #$98`.

### 3.2 The shared loop

```
INLOOP: JSR PTRGET / STWD FORPNT       ; which variable are we filling?
        LDWD TXTPTR / STWD VARTXT      ; save the program position
        LDXY INPPTR / STXY TXTPTR      ; switch TXTPTR to the data source
        JSR CHRGOT / BNE DATBK1
        ; source exhausted:
        ;   GET   -> read one character via CZGETL into BUF
        ;   READ  -> DATLOP: find the next DATA statement
        ;   INPUT -> print '?' and read another line
DATBK1: JSR CHRGET
        BIT VALTYP / BPL NUMINS
```

The trick is that `TXTPTR` is *temporarily repointed at the data*, so the same `FIN` and `STRLT2`
routines that parse program text parse input. `VARTXT` holds the program position meanwhile — and
`VARTXT` aliases `OPPTR`, which is why an `INPUT` cannot be re-entered from inside an expression.

String values ([m6502.asm:3008-3024](../m6502.asm#L3008-L3024)): if the first character is `"`, the
delimiters become quote/quote and the value starts after it; otherwise the delimiters are `:` and
`,` and the value starts where it is. Numeric values ([m6502.asm:3025-3030](../m6502.asm#L3025-L3030))
go through `FIN`, then `QINTGR` if the target variable is an integer.

`STRDN2` requires the value to be followed by NUL or `,`.

### 3.3 The error paths

`TRMNOK` ([m6502.asm:2858-2878](../m6502.asm#L2858-L2878)) — a badly formed value:

| Statement | Response |
|---|---|
| `INPUT` | print `?REDO FROM START`, set `TXTPTR = OLDTXT`, `RTS` — **the entire `INPUT` statement re-executes**, including any already-assigned variables |
| `READ` | set `CURLIN = DATLIN` so the syntax error is reported against the offending `DATA` line, then raise it |
| `GET` | force `CURLIN+1 = 255` so it looks direct, then raise a syntax error |

`VAREND` ([m6502.asm:3073-3086](../m6502.asm#L3073-L3086)) — the variable list is finished but the data
is not: `READ` records the new `DATPTR`; `INPUT` and `GET` print `?EXTRA IGNORED`.

A blank line typed to `INPUT` is a silent `STOP` ([m6502.asm:2939-2940](../m6502.asm#L2939-L2940)) —
it goes to `STPEND` with carry clear, so no `BREAK` message is printed but the program stops and can
be `CONT`inued.

### 3.4 `DATA` scanning

`DATLOP` ([m6502.asm:3044-3072](../m6502.asm#L3044-L3072)) uses `DATAN` — the same scanner `DATA`
itself uses — to skip statements. At end of line it follows the link; a zero link raises `?OD`.
Each line's number is copied into `DATLIN` as it is crossed, and the first token of each statement
is compared against `DATATK`.

`DATAN` / `REMN` ([m6502.asm:2433-2462](../m6502.asm#L2433-L2462)) both scan forward for a terminator
using the `CHARAC`/`ENDCHR` pair, and **swap the two at every `"`** (`EXCHQT`). `DATAN` stops on `:`
or NUL; `REMN` only on NUL. The swap is what makes a quoted colon invisible to `DATA` scanning.
Both return `Y` = the offset to the terminator; `ADDON` adds it to `TXTPTR`.

### 3.5 `RESTORE`

[m6502.asm:2196-2202](../m6502.asm#L2196-L2202). Sets `DATPTR = TXTTAB-1` so the next `READ` rescans
from the start. Called by `CLEARC`, hence by `RUN`, `CLEAR`, `NEW` and every program edit.

### 3.6 `GET`

[m6502.asm:2879-2900](../m6502.asm#L2879-L2900), `GETCMD=1`. Illegal in direct mode (`ERRDIR`). Fetches
exactly one character into `BUF` via `CZGETL`, points `[X,Y]` at `BUF-1`, and sets `INPFLG = 64`. In
the value loop it skips the quote logic entirely and uses zero terminators, so exactly one character
is consumed.

---

## 4. `FOR` and `NEXT`

### 4.1 `FOR`

[m6502.asm:2081-2128](../m6502.asm#L2081-L2128). Frame layout in
[02-data-structures.md](02-data-structures.md#41-for-frame--18-bytes-forsiz--162addprc).

1. `SUBFLG = 128`, then `JSR LET` — the initial assignment is done by the ordinary assignment code,
   which also leaves the variable pointer in `FORPNT`. Setting `SUBFLG` is what forbids
   `FOR A(1)=…` and `FOR A%=…`.
2. `JSR FNDFOR`. **If a frame for the same variable already exists, it and everything above it are
   discarded** (`TXA / ADCI FORSIZ-3 / TAX / TXS`, plus the two `PLA`s at `NOTOL`). This is what
   stops a program that jumps out of a loop and re-enters it from consuming 18 bytes of stack each
   time — the rationale is spelled out at [m6502.asm:411-425](../m6502.asm#L411-L425).
3. `PLA/PLA` — discard `NEWSTT`'s return address.
4. `GETSTK` with `A = 8+ADDPRC`, reserving 18 bytes plus the 62-byte slack.
5. `DATAN` gives the distance to the end of the `FOR` statement; push `TXTPTR + Y`, **low byte
   first**.
6. `PSHWD CURLIN`.
7. `SYNCHK TOTK`, `CHKNUM`, `FRMNUM` for the limit; pack its sign into `FACHO` bit 7; push it.
8. FAC = 1.0; if `STEPTK` follows, evaluate the step instead.
9. `SIGN` then `PUSHF` — pushes the sign byte followed by the *unpacked* FAC.
10. `PSHWD FORPNT`; push `FORTK`; fall into `NEWSTT`.

### 4.2 `FNDFOR`

[m6502.asm:1371-1392](../m6502.asm#L1371-L1392).

```
        TSX / INX x4
FFLOOP: LDA 257,X / CMPI FORTK / BNE FFRTS    ; not a FOR frame: stop
        LDA FORPNT+1 / BNE CMPFOR
        LDA 258,X / STA FORPNT                ; FORPNT high == 0 => adopt this frame
        LDA 259,X / STA FORPNT+1
CMPFOR: CMP 259,X / BNE ADDFRS
        LDA FORPNT / CMP 258,X / BEQ FFRTS    ; match
ADDFRS: TXA / CLC / ADCI FORSIZ / TAX / BNE FFLOOP
FFRTS:  RTS
```

| Exit | `Z` | `X` | `A` |
|---|---|---|---|
| match | 1 | frame index (base = `0x100+X+1`) | `FORPNT` low |
| no match | 0 | the offending entry | the non-`FORTK` byte found |

`FORPNT+1 == 0` is the wildcard used by a bare `NEXT`.

### 4.3 The 1978-07-27 byte-`FF` bug

From the revision log at [m6502.asm:212-214](../m6502.asm#L212-L214):

> FIXED BUG WHERE FOR VARIABLE AT BYTE FF MATCHED RETURN SEARCHING FOR GOSUB ENTRY ON STACK IN
> FNDFOR CALL BY CHANGING `STA FORPNT` TO `STA FORPNT+1`. THIS IS A SERIOUS BUG IN ALL VERSIONS.

The site is `RETURN` ([m6502.asm:2417-2421](../m6502.asm#L2417-L2421)), which calls `FNDFOR` with a
deliberately unmatchable pointer in order to skip past all `FOR` frames. The old code stored 255
into `FORPNT` (the low byte), leaving `FORPNT+1` stale. Two failure modes followed: if the stale
high byte happened to be zero, `FNDFOR` took the wildcard branch and matched the first `FOR` frame;
and a loop variable whose pointer low byte was `$FF` compared equal. Either way `FNDFOR` stopped on
a `FOR` frame, `RETURN`'s `CMPI GOSUTK` failed, and a spurious `?RG ERROR` resulted.

Storing into the **high** byte fixes both: it is never zero, so no wildcard, and no variable lives
in page `$FF`, so no match.

### 4.4 `NEXT`

[m6502.asm:3110-3163](../m6502.asm#L3110-L3163).

1. No argument ⇒ `FORPNT = 0`; otherwise `PTRGET`.
2. `FNDFOR`; no match ⇒ `?NF ERROR`.
3. `HAVFOR: TXS` — **truncate the stack to this frame immediately**, discarding anything above it.
4. Load the step from frame+3 with `MOVFM`, then overwrite `FACSGN` from the explicit sign byte at
   frame+8.
5. `FADD` the loop variable, `MOVVF` it back.
6. `FCOMPN` against the packed limit at frame+9, then `SEC / SBC` the stored step sign.

The termination test is therefore

```
sign(variable - limit) - sign(step) == 0
```

`FCOMPN` returns +1 when the limit is below the FAC, -1 when above, 0 when equal. So an ascending
loop ends when `variable > limit`, a descending one when `variable < limit`, and **equality never
ends the loop**. A step of zero loops forever, since `sign(step)` is 0 and the comparison only
reaches 0 when the variable exactly equals the limit.

If the loop continues, `CURLIN` and `TXTPTR` are restored from frame+14..+17 and control jumps to
`NEWSTT`. If it ends, `LOOPDN` ([m6502.asm:3152](../m6502.asm#L3152)) does `TXA / ADCI 2*ADDPRC+15`
with carry set — that is +18 — and `TXS`, dropping the frame. A following comma re-enters at
`GETFOR`, so `NEXT I,J` works; the re-entry is a `JSR` so that a fresh return address exists.

`NEXT` never returns to `NEWSTT` by `RTS`, because `FOR` discarded that return address.

---

## 5. Control flow

### 5.1 `GOTO`

[m6502.asm:2391-2412](../m6502.asm#L2391-L2412).

```
        JSR LINGET                 ; target into LINNUM
        JSR REMN                   ; Y = distance to end of line
        LDA CURLIN+1 / CMP LINNUM+1
        BCS LUK4IT                 ; target <= current: search from TXTTAB
        TYA / SEC / ADC TXTPTR     ; else start from the NEXT line
        LDX TXTPTR+1 (+INX)
LUKALL: JSR FNDLNC
QFOUND: BCC USERR                  ; ?US ERROR
        TXTPTR = LOWTR - 1
        RTS
```

The forward-search optimisation compares only the **high bytes** of the current and target line
numbers, so it is conservative: a forward jump within the same 256-line block still rescans from the
beginning.

Setting `TXTPTR = LOWTR - 1` makes `NEWSTT` read a NUL there and then take its normal
"follow the link, load `CURLIN`, step over the header" path — the jump reuses the line-crossing code
rather than duplicating it.

### 5.2 `GOSUB` and `RETURN`

`GOSUB` ([m6502.asm:2381-2389](../m6502.asm#L2381-L2389)): `GETSTK` with `A=3`, push `TXTPTR`, push
`CURLIN`, push `GOSUTK`, then share `RUNC2` with `RUN` — `CHRGOT`, `JSR GOTO`, `JMP NEWSTT`.

`RETURN` ([m6502.asm:2417-2436](../m6502.asm#L2417-L2436)): requires a terminator; sets
`FORPNT+1 = 255`; `FNDFOR` to skip past every `FOR` frame; `TXS` to discard them; then the frame
must be a `GOSUTK` or `?RG ERROR`. Pops the token, `CURLIN` and `TXTPTR`, then **falls into
`DATA`/`DATAN`** to skip the remainder of the `GOSUB` statement.

That fall-through is what makes `ON X GOSUB 10,20,30` return to the right place: the saved `TXTPTR`
points at the comma list, and `DATAN` skips all of it.

Note that `RETURN` discarding `FOR` frames is the language rule "a `GOSUB` that leaves loops open
still returns correctly", stated at [m6502.asm:425-441](../m6502.asm#L425-L441).

### 5.3 `IF … THEN`

[m6502.asm:2465-2477](../m6502.asm#L2465-L2477).

```
IF:     JSR FRMEVL
        JSR CHRGOT
        CMPI GOTOTK / BEQ OKGOTO   ; "IF x GOTO n" - token NOT consumed
        SYNCHK THENTK              ; otherwise "THEN" is required, and consumed
OKGOTO: LDA FACEXP / BNE DOCOND
REM:    JSR REMN / BEQA ADDON      ; false: skip to END OF LINE
DOCOND: JSR CHRGOT
        BCS DOCO / JMP GOTO        ; digit => GOTO
DOCO:   JMP GONE3                  ; otherwise dispatch as a statement
```

Two consequences worth stating plainly:

- **Truth is `FACEXP != 0`.** Only the exponent byte is examined, so any non-zero value is true —
  and since a zero float always has exponent 0, this is exact.
- **A false `IF` skips the entire rest of the line**, not just to the next `:`. It shares the `REM`
  path, which uses `REMN` (terminates on NUL only). So in `IF X THEN A=1 : B=2`, `B=2` is part of
  the consequent, and in `IF X THEN 100 : PRINT "HI"` the `PRINT` can never execute.
- The `GOTOTK` is deliberately *not* consumed, so `IF X THEN GOTO 100` reaches `GONE3`, which
  dispatches `GOTO` normally.

### 5.4 `ON … GOTO` / `ON … GOSUB`

[m6502.asm:2480-2495](../m6502.asm#L2480-L2495).

```
ONGOTO: JSR GETBYT              ; index into FACLO; A = the following token
        PHA
        CMPI GOSUTK / BEQ ONGLOP
SNERR3: CMPI GOTOTK / BNE SNERR2
ONGLOP: DEC FACLO
        BNE ONGLP1
        PLA / JMP GONE2         ; dispatch GOTO or GOSUB
ONGLP1: JSR CHRGET / JSR LINGET ; parse and discard one line number
        CMPI 44 / BEQ ONGLOP
        PLA
ONGRTS: RTS
```

When the countdown reaches zero, the saved token is re-dispatched through `GONE2` with `TXTPTR`
still sitting on the comma *before* the wanted line number; the dispatcher's `JMP CHRGET` steps onto
the first digit.

An index of 0, or one larger than the list, simply `RTS`es — execution falls through to the next
statement with no error. Since `GETBYT` yields 0..255, `ON` cannot take a negative index; that would
already have raised `?FC` in `POSINT`.

---

## 6. Program state statements

### 6.1 `STOP` / `END`

[m6502.asm:2227-2248](../m6502.asm#L2227-L2248).

```
STOP:   BCS STOPC       ; carry doubles as the "print BREAK" flag
END:    CLC
STOPC:  BNE CONTRT      ; not a terminator => return, let NEWSTT raise SN (and no BREAK)
        if not direct mode: OLDTXT = TXTPTR ; OLDLIN = CURLIN
        PLA / PLA
ENDCON: print BRKTXT via ERRFIN if C=1, else JMP READY
```

`ENDCON` is also where `NEWSTT` lands when the program runs off the end
([m6502.asm:2152-2153](../m6502.asm#L2152-L2153)) — with `CLC`, so no `BREAK` is printed. `CNTWFL` is
zeroed so output is re-enabled.

Both statements save the pointer to their own terminating character, so `CONT` resumes *after* the
`STOP`.

### 6.2 `CONT`

See [04-interpreter-loop.md](04-interpreter-loop.md#45-cont).

### 6.3 `RUN`

[m6502.asm:2366-2368](../m6502.asm#L2366-L2368). Without an argument, `JEQ RUNC` — `STXTPT` +
`CLEARC` + `STKINI`, after which execution resumes through the stack `STKINI` rebuilt. With a line
number, `CLEARC` then `RUNC2` (shared with `GOSUB`).

### 6.4 `CLEAR` / `CLEARC` / `STKINI` / `STXTPT` / `RUNC`

| Routine | Line | Action |
|---|---|---|
| `RUNC` | [1922](../m6502.asm#L1922) | `STXTPT`, then fall into `CLEAR` with `A=0` so the syntax check passes |
| `CLEAR` | [1927](../m6502.asm#L1927) | `BNE STKRTS` — bare `RTS` if no terminator, so `NEWSTT` raises `SN` |
| `CLEARC` | [1934-1941](../m6502.asm#L1934-L1941) | `FRETOP = MEMSIZ`; `ARYTAB = STREND = VARTAB`; `RESTOR`; fall into `STKINI` |
| `STKINI` | [1949-1962](../m6502.asm#L1949-L1962) | `TEMPPT = TEMPST`; reset `S` to `STKEND-257`, preserving the return address; `OLDTXT+1 = 0`; `SUBFLG = 0` |
| `STXTPT` | [1964-1971](../m6502.asm#L1964-L1971) | `TXTPTR = TXTTAB - 1` |

`CLEAR` discards all variables and arrays simply by pointing `ARYTAB` and `STREND` back at
`VARTAB` — nothing is erased, it is just no longer reachable.

### 6.5 `NEW`

See [03-tokenizer-editor.md](03-tokenizer-editor.md#56-scrath--scrtch--new).

### 6.6 `NULL` (`NULCMD` only)

[m6502.asm:2268-2277](../m6502.asm#L2268-L2277). `GETBYT`, require a terminator, then
`INX / CPXI 240 / BCS FCERR1` — so the argument must be 0..238. Sets `NULCNT`, the number of NUL
characters `CRFIN` emits after each newline.

---

## 7. Memory statements

### 7.1 `POKE`, `PEEK`, `WAIT`

[m6502.asm:4791-4844](../m6502.asm#L4791-L4844). All three get their address through `GETADR`:

```
GETADR: LDA FACSGN / BMI GOFUC          ; negative => ?FC
        LDA FACEXP / CMPI 145 / BCS GOFUC   ; >= 65536 => ?FC
        JSR QINT
        LDWD FACMO / STY POKER / STA POKER+1
```

- **`POKE`** — `GETNUM`, `COMBYT`, `STADY POKER`.
- **`PEEK`** — saves and restores `POKER` around the fetch (`PSHWD`/`PULWD`). That is the 1977-12-01
  fix ([m6502.asm:233](../m6502.asm#L233)): `POKER` aliases `LINNUM`, and without the save
  `POKE X,PEEK(Y)` clobbered its own destination address. On the Commodore, `PEEK` returns 0 for
  addresses inside the BASIC ROM ([m6502.asm:4811-4815](../m6502.asm#L4811-L4815)).
- **`WAIT`** — address plus an AND mask, and an optional EOR mask defaulting to 0, then spins until
  `((*POKER) EOR EORMSK) AND ANDMSK != 0`. The two masks alias `FORPNT`. There is no timeout and no
  ^C check: `WAIT` on a condition that never occurs hangs the machine.

### 7.2 `FRE`, `POS`, `USR`

- **`FRE`** ([m6502.asm:4113-4122](../m6502.asm#L4113-L4122)) — frees its argument if it is a string,
  then **forces a full garbage collection** via `GARBA2`, and returns `FRETOP - STREND` floated by
  `GIVAYF` as a **signed** 16-bit value. More than 32767 free bytes therefore report as negative.
  Calling `FRE(0)` purely to compact string space is the idiomatic use.
- **`POS`** ([m6502.asm:4130-4132](../m6502.asm#L4130-L4132)) — returns `TRMPOS` as an unsigned byte.
  The argument is parsed and discarded.
- **`USR`** — `FUNDSP` entry 4 points at `USRLOC`, which in a ROM build points at `USRPOK`, a
  page-zero `JMP FCERR` whose target the user overwrites with `POKE`. The argument arrives in the
  FAC and the result is type-checked by `FINGO`'s `JMP CHKNUM`.

### 7.3 `DEF FN`

[m6502.asm:4134-4234](../m6502.asm#L4134-L4234). Covered in
[07-variables-arrays.md](07-variables-arrays.md#5-def-fn).

### 7.4 `LOAD` / `SAVE`

Only assembled when `DISKO=1` — not in this build, though the Apple cassette routines are present as
dead code at [m6502.asm:2323-2363](../m6502.asm#L2323-L2363) with no token and no dispatch entry. Per
target: KIM cassette ([m6502.asm:2281-2322](../m6502.asm#L2281-L2322)), Commodore via external
vectors, everything else via `LOAD`/`SAVE` stubs. The KIM version stashes the stack pointer in
`INPFLG`, patches location 1 as a return vector, and finishes at `FINI` so the program is relinked.
