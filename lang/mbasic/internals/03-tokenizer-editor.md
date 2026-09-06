# 03 — Text fetching, tokenizing, listing, editing

---

## 1. `CHRGET` / `CHRGOT`

[m6502.asm:943-976](../m6502.asm#L943-L976). This is the single most-executed routine in the
interpreter and it lives in RAM because it modifies itself.

```
CHRGET: INC CHRGET+7      ; low byte of TXTPTR
        BNE CHRGOT
        INC CHRGET+8      ; high byte
CHRGOT: LDA 60000         ; <-- the operand of THIS instruction is TXTPTR
        CMP #' '
        BEQ CHRGET        ; skip blanks
QNUM:   CMP #':'
        BCS CHRRTS        ; >= ':' : return, carry set
        SEC
        SBC #'0'
        SEC
        SBC #(256-'0')
CHRRTS: RTS
```

`TXTPTR` is defined as `CHRGOT+1` ([m6502.asm:965](../m6502.asm#L965)) — it *is* the address field of
that `LDA`. Advancing the text pointer means incrementing the instruction, which is what
`INC CHRGET+7` / `INC CHRGET+8` do.

The payoff is that fetching the next character of the program costs no index register and no
zero-page indirect setup: `X` and `Y` are untouched, so callers can keep loop counters and table
indices in them across arbitrarily many character fetches.

### 1.1 The contract

This is a calling convention, not just a return value. Callers several levels away depend on all of
it.

| Output | Meaning |
|---|---|
| `A` | the character |
| `C = 0` | the character is `'0'`..`'9'` |
| `C = 1` | anything else, including every token, every letter, and `:` |
| `Z = 1` | the character is `':'` **or** `0x00` — i.e. "statement terminator" |
| `X`, `Y` | preserved |

The `Z` result falls out of the arithmetic rather than being computed: for `':'` the `CMP #':'` sets
Z directly; for anything below `':'`, subtracting 48 and then 208 is subtracting 256, which leaves
`A` unchanged modulo 256 and sets Z only when `A` was 0. The double subtract is therefore a
classification that does not disturb the accumulator.

`CHRGET` pre-increments then fetches; `CHRGOT` re-fetches at the current position without moving.
Spaces are skipped transparently and without limit, which is why runs of spaces inside a statement
cost time but nothing else — and why the *tokenizer* has to handle spaces itself (§2).

Some callers rely on residual carry rather than calling again. `GONE2`
([m6502.asm:2172](../m6502.asm#L2172)) performs `SBCI ENDTK` with no preceding `SEC`, commented "carry
will be on if non-numeric".

### 1.2 There are two different copies of `CHRGET`

The version listed above, at page-zero 178, is what the *source listing* shows. It is not what runs.

`INIT` copies a template from ROM over it ([m6502.asm:6733-6737](../m6502.asm#L6733-L6737)):

```
        LDXI RNDX+4-CHRGET      ; = 28
MOVCHG: LDA INITAT-1,X
        STA CHRGET-1,X
        DEX
        BNE MOVCHG
```

**This copy is unconditional** — it is not guarded by `IFN ROMSW` — so it happens in RAM builds too.
The template at `INITAT` ([m6502.asm:6678-6698](../m6502.asm#L6678-L6698)) orders its two tests the
other way round:

| Offset from `CHRGET` | RAM listing (overwritten) | `INITAT` template (what runs) |
|---|---|---|
| +9 | `CMP #' '` | `CMP #':'` |
| +11 | `BEQ CHRGET` | `BCS CHZRTS` |
| **+13** | **`QNUM: CMP #':'`** | **`CMP #' '`** |
| +15 | `BCS CHRRTS` | `BEQ INITAT` |

Both orderings are the same length and, entered at `CHRGET` or `CHRGOT`, behave identically: a space
is below `':'` so it never takes the `BCS` exit, and reaches the space test either way.

The difference matters only for the label `QNUM`, which is a documented *third* entry point at
`CHRGET+13`, used by `TIMNUM` ([m6502.asm:2609-2610](../m6502.asm#L2609-L2610)) to classify a character
that did not come from `TXTPTR`. In the copy that actually runs, `CHRGET+13` is `CMP #' '`, not
`CMP #':'`. See [13-porting-notes.md](13-porting-notes.md#3-defects-in-the-1978-code) — that path is
compiled out in this build (`TIME=0`) but not on the Commodore.

The copy is also 28 bytes, covering `CHRGET` (24 bytes) plus **four** of the five `RNDX` seed bytes.

---

## 2. `CRUNCH` — the tokenizer

[m6502.asm:1785-1865](../m6502.asm#L1785-L1865). Runs once per input line, in place, rewriting `BUF`.

### 2.1 Token numbering

```
Q=128-1
DEFINE DCI(A),<Q=Q+1
        DC(A)>
```
([m6502.asm:1109-1111](../m6502.asm#L1109-L1111))

`DC` emits the word's characters with **bit 7 set on the last one** — that is the end-of-entry
marker the matcher keys on. A token's value is `128 + its 0-based ordinal position in RESLST`.

The matcher produces it arithmetically. Comparing input against table:

```
RESCON: LDA BUFOFS,X
        SEC
        SBC RESLST,Y
        BEQ RESER        ; characters equal, keep going
        CMPI 128         ; difference exactly 128?
        BNE NTHIS        ; no => genuinely different
        ORA COUNT        ; yes => that was the flagged last character: token = 128 | COUNT
```

A difference of exactly 128 means the table byte was the input byte with bit 7 set, i.e. the last
character of the entry, matched. `COUNT` holds how many complete entries have been skipped, so
`128 | COUNT` is the token. (`ORA` works rather than `ADC` because `COUNT < 128`.)

### 2.2 The scanning loop

```
CRUNCH: LDX TXTPTR       ; source index - the LOW BYTE of TXTPTR only
        LDYI 4           ; destination offset (stores go to BUF-5,Y after INY)
        STY DORES        ; 4 => bit 6 clear => crunching enabled
```

`BUFOFS` ([m6502.asm:1780-1784](../m6502.asm#L1780-L1784)) is 0 when `BUFPAG=0` and `(BUF/256)*256`
otherwise, so `LDA BUFOFS,X` is an absolute-indexed read into `BUF`'s page. Two consequences:
`BUF` must be page-aligned when `BUFPAG≠0`, and **an input line can never exceed 255 characters**
whatever `BUFLEN` says.

Per character, at `KLOOP`:

1. *(Commodore only)* a byte with bit 7 set is either `PI` (255, stored verbatim) or a
   non-printing character, which is dropped entirely.
2. **A space is stored verbatim** and never reaches the matcher.
3. The character is stored in `ENDCHR` unconditionally — so that if it turns out to be a quote, the
   literal copier already has its terminator.
4. `"` → `STRNG`: copy verbatim until `ENDCHR` (the quote) or NUL.
5. `BIT DORES / BVS STUFFH` — inside a `DATA` statement, store verbatim, no crunching.
6. `?` → emit the `PRINT` token.
7. Below `'0'` → try the matcher. `'0'`..`';'` (48-59: the digits, `:` and `;`) → store verbatim
   without attempting a match. `'<'` (60) and above → try the matcher.

After each store, `STUFFH` inspects what was stored
([m6502.asm:1827-1841](../m6502.asm#L1827-L1841)):

- `0` → `CRDONE`, line finished.
- `:` → `DORES = 0`, crunching re-enabled.
- `DATATK` → `DORES = DATATK-':'` = 73, which has bit 6 set, so crunching is disabled until the next
  `:`.
- `REMTK` → `ENDCHR = 0` and fall into the verbatim copier, so `REM` swallows the rest of the line
  including any `:` or `"`.

### 2.3 Why spaces cannot appear inside reserved words

Because step 2 emits a space immediately and the matcher never sees one, and the matcher itself does
no space skipping. This was a deliberate change on 1978-02-11
([m6502.asm:229](../m6502.asm#L229)): "DISALLOWED SPACES IN RESERVED WORDS. PUT IN SPECIAL CHECK FOR
`GO TO`".

The consequence is that `GO TO` cannot be tokenized as `GOTO`. Instead `GO` exists as its own token
(194) with no dispatch entry, and the statement dispatcher special-cases it at run time — see
[04-interpreter-loop.md](04-interpreter-loop.md#23-the-go-to-special-case).

### 2.4 The prefix hazard

Matching is first-fit in table order, and the source warns about it at
[m6502.asm:1208-1216](../m6502.asm#L1208-L1216):

> NOTE DANGER OF ONE RESERVED WORD BEING A PART OF ANOTHER:
> IE . . IF 2 GREATER THAN F OR T=5 THEN... WILL NOT WORK!!! SINCE "FOR" WILL BE CRUNCHED!!

`IF 2 > F OR T=5 THEN` tokenizes the `F OR T` as `FOR`, because with spaces already stripped from
consideration the matcher sees `FORT`. Likewise `IF T OR Q THEN` finds `TO`. The rule the table must
obey is that when one word is a prefix of another, **the longer one must come first** — hence
`INPUT#` before `INPUT`.

### 2.5 `CRDONE`

[m6502.asm:1859-1865](../m6502.asm#L1859-L1865). Stores a second zero at NUL+2 so that a direct line's
"next link high byte" reads zero; resets `TXTPTR` to `BUF-1`; returns with **`Y` = the total line
length including the 4-byte link + line-number prefix**, which `MAIN1` stores in `COUNT`.

---

## 3. The reserved-word table

`RESLST` at [m6502.asm:1112-1245](../m6502.asm#L1112-L1245). Token values below are re-derived for the
checked-in configuration (`EXTIO=0`, `NULCMD=0`, `DISKO=0`, `GETCMD=1`, `REALIO=4`).

### 3.1 Statements — tokens 128 to 153

These are the words with `STMDSP` entries; the dispatcher's range check is exactly
`ENDTK`..`SCRATK`.

| Tok | Word | | Tok | Word |
|---|---|---|---|---|
| 128 | `END` (`ENDTK`) | | 141 | `RETURN` |
| 129 | `FOR` (`FORTK`) | | 142 | `REM` (`REMTK`) |
| 130 | `NEXT` | | 143 | `STOP` |
| 131 | `DATA` (`DATATK`) | | 144 | `ON` |
| 132 | `INPUT` | | 145 | `WAIT` |
| 133 | `DIM` | | 146 | `DEF` |
| 134 | `READ` | | 147 | `POKE` |
| 135 | `LET` | | 148 | `PRINT` (`PRINTK`) |
| 136 | `GOTO` (`GOTOTK`) | | 149 | `CONT` |
| 137 | `RUN` | | 150 | `LIST` |
| 138 | `IF` | | 151 | `CLEAR` |
| 139 | `RESTORE` | | 152 | `GET` |
| 140 | `GOSUB` (`GOSUTK`) | | 153 | `NEW` (`SCRATK`) |

### 3.2 Non-statement keywords — 154 to 170

| Tok | Word | Note |
|---|---|---|
| 154 | `TAB(` (`TABTK`) | spelled out byte by byte, because the `DCI` macro cannot take a `(` as an argument ([m6502.asm:1169-1173](../m6502.asm#L1169-L1173)) |
| 155 | `TO` (`TOTK`) | |
| 156 | `FN` (`FNTK`) | |
| 157 | `SPC(` (`SPCTK`) | same byte-by-byte treatment |
| 158 | `THEN` (`THENTK`) | |
| 159 | `NOT` (`NOTTK`) | |
| 160 | `STEP` (`STEPTK`) | |
| 161 | `+` (`PLUSTK`) | |
| 162 | `-` (`MINUTK`) | |
| 163 | `*` | |
| 164 | `/` | |
| 165 | `^` | |
| 166 | `AND` | |
| 167 | `OR` | |
| 168 | `>` (`GREATK`) | emitted as the raw byte 190 = `'>'`+128 |
| 169 | `=` (`EQULTK`) | |
| 170 | `<` (`LESSTK`) | emitted as the raw byte 188 = `'<'`+128 |

The relational operators must be adjacent and in the order `>`, `=`, `<`, because `FRMEVL` maps
them to bits by subtracting `GREATK` and checking the result is below `LESSTK-GREATK+1` = 3.

### 3.3 Functions — 171 to 193

| Tok | Word | | Tok | Word |
|---|---|---|---|---|
| 171 | `SGN` (`ONEFUN`) | | 183 | `TAN` |
| 172 | `INT` | | 184 | `ATN` |
| 173 | `ABS` | | 185 | `PEEK` |
| 174 | `USR` | | 186 | `LEN` |
| 175 | `FRE` | | 187 | `STR$` |
| 176 | `POS` | | 188 | `VAL` |
| 177 | `SQR` | | 189 | `ASC` |
| 178 | `RND` | | 190 | `CHR$` (`LASNUM`) |
| 179 | `LOG` | | 191 | `LEFT$` |
| 180 | `EXP` | | 192 | `RIGHT$` |
| 181 | `COS` | | 193 | `MID$` |
| 182 | `SIN` | | | |

`ONEFUN` marks the first function and `LASNUM` the last one taking a single argument; the three
after `LASNUM` take more and are dispatched differently. See
[06-expressions.md](06-expressions.md#4-function-dispatch).

### 3.4 The tail

| Tok | Word |
|---|---|
| 194 | `GO` (`GOTK`) — no dispatch entry; handled by the syntax-error path |
| — | a `0` byte marks the end of the table |

### 3.5 Conditional insertions

Each of these shifts every token after it, in `RESLST`, `STMDSP` and `FUNDSP` simultaneously:

| Switch | Words inserted | Position |
|---|---|---|
| `EXTIO` | `INPUT#` | before `INPUT` |
| `EXTIO` | `PRINT#` | before `PRINT` |
| `EXTIO` | `CMD`, `SYS`, `OPEN`, `CLOSE` | after `CLEAR`/`CLR` |
| `NULCMD` | `NULL` | after `ON` |
| `DISKO` | `LOAD`, `SAVE` (+ `VERIFY` on Commodore) | after `WAIT` |
| `REALIO=0` | `DDT` | after `CONT` |
| `GETCMD` | `GET` | before `NEW` |
| `REALIO=3` | `CLR` replaces `CLEAR` | in place |

For the Commodore (`EXTIO=1`, `DISKO=1`, `GETCMD=1`, `NULCMD=0`) the statement block is nine words
longer, so `SCRATK` = 162 and `ONEFUN` = 180.

---

## 4. `LIST` — the exact inverse

[m6502.asm:1973-2062](../m6502.asm#L1973-L2062).

### 4.1 Argument parsing

`LINGET` reads the start line into `LINNUM`; `FNDLIN` positions `LOWTR` at the first line at or
after it. If a `-` follows, `LINGET` runs again and **overwrites `LINNUM`** with the end bound. At
`LSTEND`, if `LINNUM` is zero it becomes 65535. Hence:

| Form | Effect |
|---|---|
| `LIST` | start 0, end 65535 — everything |
| `LIST 100` | start 100, end 100 — one line |
| `LIST 100-` | start 100, end 65535 |
| `LIST -200` | start 0, end 200 |

`LIST` discards `NEWSTT`'s return address (`PLA/PLA`) and exits via `JMP READY`, so it always
returns to the prompt rather than continuing a program.

### 4.2 The de-tokenizing loop

```
LIST4:  check link high byte; zero => done
        ISCNTC, CRDO
        read the line number, compare against the end bound
        LINPRT the number, then a space
PLOOP:  print the character
PLOOP1: INY; BEQ GRODY          ; >256 characters on one line => bail out
        fetch next byte; zero => follow the link to the next line
QPLOP:  BPL PLOOP               ; plain character, print as-is
        SEC / SBCI 127 / TAX    ; X = token - 127 = 1-based ordinal
        LDYI 255
RESRCH: DEX / BEQ PRIT3         ; skip (ordinal-1) whole entries
RESCR1: INY / LDA RESLST,Y / BPL RESCR1 / BMI RESRCH
PRIT3:  INY / LDA RESLST,Y / BMI PRIT4   ; last character => strip bit 7, print, resume
        JSR OUTDO / BNE PRIT3
```

Expansion is a linear rescan of `RESLST` from the beginning for every token, skipping
`ordinal-1` entries by looking for bytes with bit 7 set. The final character of a keyword goes
through `PRIT4`, which does `AND #127` to strip the marker bit — the same instruction that handles
printing the space after a line number.

The `BEQ GRODY` after `INY` is a corruption guard: a line whose text runs past 255 characters is
abandoned rather than looped on forever.

`LIST` calls `ISCNTC` once per line, so a long listing can be interrupted.

> The comment at [m6502.asm:2030](../m6502.asm#L2030) reads "YES. END OF LINE" on a `BNE`, which is
> backwards — the branch is taken when the byte is *not* zero, i.e. when it is *not* end of line.

---

## 5. The program editor

`MAIN` at [m6502.asm:1556-1571](../m6502.asm#L1556-L1571).

```
MAIN:   JSR INLIN               ; returns a pointer to BUF-1 in [X,Y]
        STXY TXTPTR
        JSR CHRGET
        TAX                     ; set Z from A alone, distinguishing ':' from NUL
        BEQ MAIN                ; empty line, ask again
        LDXI 255
        STX CURLIN+1            ; mark direct mode NOW
        BCC MAIN1               ; C=0 => started with a digit => it is a program line
        JSR CRUNCH
        JMP GONE                ; direct statement: execute in place
MAIN1:  JSR LINGET / JSR CRUNCH / STY COUNT
        JSR FNDLIN
        BCC NODEL               ; no such line => nothing to delete
        ... delete it ...
NODEL:  JSR RUNC                ; CLEAR + reset stack + TXTPTR = TXTTAB-1
        JSR LNKPRG              ; relink
        LDA BUF / BEQ MAIN      ; nothing to insert (a bare line number)
        ... open a gap and store ...
FINI:   JSR RUNC / JSR LNKPRG / JMP MAIN
```

The `TAX` after `CHRGET` is there because `CHRGET`'s Z flag conflates `:` with NUL; re-deriving Z
from `A` alone separates them.

Note that **typing any program line clears all variables**: `RUNC` calls `CLEARC`. That is a
language-visible consequence of the storage layout — inserting a line moves `VARTAB` and everything
above it, so no variable could survive anyway.

### 5.1 `FNDLIN` / `FNDLNC`

[m6502.asm:1879-1904](../m6502.asm#L1879-L1904). Input in `LINNUM`. `FNDLIN` starts at `TXTTAB`;
`FNDLNC` starts at an address given in `[X,A]`. Walks the link list comparing line numbers (high
byte then low).

| Exit | Meaning |
|---|---|
| `C = 1` | found; `LOWTR` = the matching line's link field |
| `C = 0` | not found; `LOWTR` = the first line *greater* than `LINNUM`, or the terminating double zero |

The single routine therefore serves both "find this line" and "find where this line should be
inserted".

### 5.2 Deletion

[m6502.asm:1572-1610](../m6502.asm#L1572-L1610). A downward block copy of everything from the deleted
line's successor to `LOWTR`, done page at a time with `(zp),Y`:

- The line's length is computed as a *negative* value by `LOWTR_low SBC (LOWTR),0`, and added to
  `VARTAB` to get the new `VARTAB`.
- `Y` is the shared low-byte offset, `X` the number of 256-byte blocks.
- `QDECT1` performs `CLC / ADC INDEX1 / BCC MLOOP / DEC INDEX1+1` purely to pre-compensate the
  source page for the low-byte carry, without storing the sum.

The loop may copy a few bytes past the end of the region; this is harmless because the destination
is below the source.

### 5.3 Insertion

[m6502.asm:1611-1637](../m6502.asm#L1611-L1637). `HIGHTR = VARTAB` (top of what must move),
`HIGHDS = VARTAB + COUNT` (where it must end up), `LOWTR` = the insertion point from `FNDLIN`'s
`C=0` exit, then `BLTU` opens the gap and sets `STREND`. `STOLOP` then copies `COUNT` bytes from
`BUF-4` — the link placeholder, the line number, the text, and the NUL — into the gap.

`RUNC` is called **before** the insertion as well as after. That is the 1978-07-01 fix
([m6502.asm:224-225](../m6502.asm#L224-L225)): `BLTU` calls `REASON`, which may trigger a garbage
collection, and the collector must not run while `VARTAB` and `FRETOP` disagree about where things
are.

### 5.4 `BLTU` / `BLTUC` — the block mover

[m6502.asm:1413-1450](../m6502.asm#L1413-L1450), contract at
[m6502.asm:1398-1411](../m6502.asm#L1398-L1411).

On entry: `[Y,A]` = the new `HIGHDS`, `LOWTR` = the lowest source byte, `HIGHTR` = one past the
highest source byte, `HIGHDS` = where the high end must land. `BLTU` first calls `REASON` to verify
the destination against `FRETOP` and sets `STREND`; `BLTUC` skips both and is what the garbage
collector uses.

The copy runs **descending** so that overlapping regions move upward safely. Both base pointers are
biased down by the low byte of the length so the inner loop can run `(zp),Y` from `Y` up to 255.

On exit `LOWTR` is unchanged and `HIGHTR` and `HIGHDS` are each left **256 below** the region start.
The header comment says "minus 200 octal", which is wrong — the code decrements the high byte, so it
is 256. `GRBPAS` compensates with an explicit `INC HIGHDS+1`
([m6502.asm:2511](../m6502.asm#L2511)).

### 5.5 `LNKPRG` / `CHEAD` — relinking

[m6502.asm:1642-1671](../m6502.asm#L1642-L1671). Rebuilds every link from scratch rather than patching:

```
CHEAD:  LDYI 1 / LDADY INDEX / BEQ LNKRTS   ; link high byte zero => end of program
        LDYI 4
CZLOOP: INY / LDADY INDEX / BNE CZLOOP      ; scan for the terminating NUL
        INY                                 ; one past it => Y = line length
        write INDEX+Y back into offsets 0,1
        advance INDEX and repeat
```

`Y` starts at 4 and is incremented *before* the first load, on the reasoning that a line always has
at least one text byte. `BCCA CHEAD` is a branch the author knows is always taken.

Rebuilding wholesale is O(program size) on every keystroke-completed line, which is slow but
completely removes the class of bugs where a link is left stale — and the 1978-07-01 note at
[m6502.asm:218-220](../m6502.asm#L218-L220) records exactly such a bug being fixed by adding a
`LNKPRG` call.

### 5.6 `SCRATH` / `SCRTCH` — `NEW`

[m6502.asm:1909-1922](../m6502.asm#L1909-L1922). `SCRATH` begins `BNE FLNRTS`: if a terminator does not
follow, it simply returns, and `NEWSTT` then raises a syntax error because `TXTPTR` is not on a
terminator. This "return and let `NEWSTT` complain" idiom is used by several statements that must
not execute unless perfectly formed.

`SCRTCH` zeroes the two bytes at `TXTTAB`, sets `VARTAB = TXTTAB+2`, and falls into `RUNC`.

### 5.7 `LINGET` — parsing a line number

[m6502.asm:2508-2536](../m6502.asm#L2508-L2536).

```
LINGET: LDXI 0 / STX LINNUM / STX LINNUM+1
MORLIN: BCS ONGRTS              ; not a digit => done
        SBCI "0"-1              ; C=0 here, so this is A-48
        STA CHARAC
        LDA LINNUM+1 / STA INDEX
        CMPI 25 / BCS SNERR3    ; overflow guard
        ... LINNUM = LINNUM*10 + CHARAC ...
NXTLGC: JSR CHRGET / JMP MORLIN
```

The multiply by ten is done as `(x*4 + x) * 2` using `ASL`/`ROL` pairs through `INDEX`.

Three behaviours worth recording:

- With no digits at all it returns immediately with `LINNUM = 0`. `LIST` and `GOTO` both rely on
  this.
- The guard `LINNUM+1 < 25` is applied *before* each multiply, so the largest accepted line number
  is **63999**.
- Exceeding it is a **syntax error** (`SNERR3`), not an overflow error.

`LINGET` clobbers `CHARAC` and `INDEX1`.
