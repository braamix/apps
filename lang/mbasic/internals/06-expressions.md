# 06 — Expression evaluation

---

## 1. Type checking

[m6502.asm:3169-3180](../m6502.asm#L3169-L3180).

```
FRMNUM: JSR FRMEVL
CHKNUM: CLC
        SKIP1              ; the BIT opcode swallows the SEC below
CHKSTR: SEC
CHKVAL: BIT VALTYP
        BMI DOCSTR
        BCS CHKERR
CHKOK:  RTS
DOCSTR: BCS CHKOK
CHKERR: LDXI ERRTM         ; ?TM TYPE MISMATCH
```

The convention is **carry in = "I require a string"**. `VALTYP` is 0 for numeric and 255 for string,
and is tested with `BIT` so the answer arrives in the N flag without disturbing `A`.

On return the Z flag reflects `A AND VALTYP`, which `LET` exploits to distinguish the two store
paths without a second test ([m6502.asm:2554](../m6502.asm#L2554)).

---

## 2. `FRMEVL` — the formula evaluator

[m6502.asm:3193-3321](../m6502.asm#L3193-L3321). A precedence-climbing evaluator that uses the 6502
hardware stack for its operand and operator stack.

### 2.1 `OPTAB`

[m6502.asm:1084-1103](../m6502.asm#L1084-L1103). Three bytes per entry: a precedence byte followed by
the handler address **minus one** (an `RTS` target).

| Index | `Y = 3i` | Token | Precedence | Handler |
|---|---|---|---|---|
| 0 | 0 | `+` (`PLUSTK`) | 121 | `FADDT` |
| 1 | 3 | `-` (`MINUTK`) | 121 | `FSUBT` |
| 2 | 6 | `*` | 123 | `FMULTT` |
| 3 | 9 | `/` | 123 | `FDIVT` |
| 4 | 12 | `^` | 127 | `FPWRT` |
| 5 | 15 | `AND` | 80 | `ANDOP` |
| 6 | 18 | `OR` | 70 | `OROP` |
| 7 | 21 | `NEGTAB` — unary `-` | 125 | `NEGOP` |
| 8 | 24 | `NOTTAB` — `NOT` | 90 | `NOTOP` |
| 9 | 27 | `PTDORL` — any relational | 100 | `DOREL` |

Entries 0-6 are reached arithmetically from the token; 7, 8 and 9 only by explicit
`LDYI NEGTAB-OPTAB` and friends. The precedences are arbitrary except in their ordering — the source
says so at [m6502.asm:335-339](../m6502.asm#L335-L339) — so `NOT` binding tighter than `AND` but looser
than a comparison, and unary minus binding tighter than `*`, are the design decisions encoded here.

### 2.2 Entry

```
FRMEVL: LDX TXTPTR / BNE FRMEV1 / DEC TXTPTR+1
FRMEV1: DEC TXTPTR          ; back up one, so EVAL's CHRGET re-reads this character
        LDXI 0              ; dummy precedence 0
        SKIP1               ; skips the PHA at LPOPER
LPOPER: PHA                 ; push OPMASK  (skipped on first entry)
        TXA / PHA           ; push precedence
        LDAI 1 / JSR GETSTK
        JSR EVAL
        CLR OPMASK
TSTOP:  JSR CHRGOT
```

On the very first entry only **one** byte is pushed (the dummy precedence 0); on every recursive
entry from `DOPRE1` two are (the mask, then the precedence). The `SKIP1` is what makes one entry
point serve both.

The dummy zero precedence is the sentinel that means "the whole expression is finished" — `QOPGO`
tests for it.

### 2.3 The deferred-operation frame

Built by `DOPRE1` + `PUSHF1`/`PUSHF`/`FORPSH`
([m6502.asm:3262-3293](../m6502.asm#L3262-L3293)) plus `LPOPER`. Layout in
[02-data-structures.md](02-data-structures.md#43-expression-frame--10-bytes-9addprc).

`PUSHF` ([m6502.asm:3272](../m6502.asm#L3272)) is worth reading closely because it does something
unusual: it **pops its own return address into `INDEX1`, increments it, pushes the operand, and
returns by `JMPD INDEX1`**. It has to, because the value it is pushing must end up contiguous with
the dispatch address already on the stack, and an ordinary `RTS` would leave its return address in
the middle.

`FORPSH` calls `ROUND` before pushing. The 1978-02-11 revision note
([m6502.asm:230-231](../m6502.asm#L230-L231)) records a bug here: rounding the FAC before pushing could
increment a *string pointer* held in the FAC, because for a string value the FAC holds a descriptor
address rather than a number.

### 2.4 Relational accumulation

`LOPREL` ([m6502.asm:3210-3223](../m6502.asm#L3210-L3223)) runs before anything else, gathering a run
of relational characters into a bit mask:

```
LOPREL: SEC / SBCI GREATK
        BCC ENDREL                  ; below '>' : not relational
        CMPI LESSTK-GREATK+1        ; = 3
        BCS ENDREL                  ; above '<' : not relational
        CMPI 1 / ROL A / EORI 1     ; 0->1, 1->2, 2->4
        EOR OPMASK
        CMP OPMASK / BCC SNERR5     ; reject a repeated bit, e.g. "<<"
        STA OPMASK
        JSR CHRGET / JMP LOPREL
```

| Bit | Meaning |
|---|---|
| 1 | `>` |
| 2 | `=` |
| 4 | `<` |

Compound operators are just OR-ed bits: `<=` is 6, `>=` is 3, `<>` is 5. Because any combination is
accepted, `=<` and `><` are legal spellings. The `CMP OPMASK / BCC` check rejects a repeat, so `<<`
is a syntax error.

This design means the relational operators must be adjacent and in the order `>`, `=`, `<` in
`RESLST`.

### 2.5 Recognising an arithmetic operator

`ENDREL` ([m6502.asm:3224-3235](../m6502.asm#L3224-L3235)):

```
ENDREL: LDX OPMASK / BNE FINREL     ; relationals were seen
        BCS QOP                     ; character above '<' : not an operator
        ADCI GREATK-PLUSTK          ; C=0, so A = char - PLUSTK
        BCC QOP                     ; character below '+' : not an operator
        ADC VALTYP                  ; C=1
        JEQ CAT                     ; A==0 and VALTYP==-1  =>  string concatenation
        ADCI 377                    ; restore A
        STA INDEX1 / ASL A / ADC INDEX1 / TAY   ; Y = 3*(token - PLUSTK)
```

The `ADC VALTYP` line is a neat piece of compression: it is simultaneously "is this a `+`?" and "is
the current value a string?", and only when both hold does the sum come out zero, sending control to
string concatenation.

### 2.6 The precedence decision

```
QPREC:  PLA                     ; the enclosing precedence
        CMP OPTAB,Y
        BCS QCHNUM              ; old >= new: apply the stacked operator now
        JSR CHKNUM
DOPREC: PHA                     ; old < new: re-save it and recurse
NEGPRC: JSR DOPRE1
```

and after an operator routine returns, control resumes at
[m6502.asm:3242](../m6502.asm#L3242):

```
        PLA                     ; next-older precedence
        LDY OPPTR
        BPL QPREC1              ; a real operator is still pending
        TAX / BEQ QOPGO         ; OPPTR=255 and precedence 0 => finished
        BNE PULSTK              ; OPPTR=255 => just apply the next stacked operator
QPREC1: CMP OPTAB,Y
        BCS PULSTK
        BCC DOPREC
```

**`OPPTR` is the "operator seen but not yet applied"** — a byte offset into `OPTAB`, with 255
meaning none. It aliases `VARTXT`, which is why `INPUT`/`READ` cannot be re-entered from inside an
expression.

### 2.7 Applying an operator

```
QOP:    LDYI 255
        PLA
QOPGO:  BEQ QOPRTS              ; precedence 0 => the whole formula is done
QCHNUM: CMPI 100                ; relational? then operands may legally be strings
        BEQ UNPSTK
        JSR CHKNUM
UNPSTK: STY OPPTR
PULSTK: PLA / LSR A / STA DOMASK
        PLA -> ARGEXP, ARGHO, [ARGMOH], ARGMO, ARGLO, ARGSGN
        EOR FACSGN / STA ARISGN
QOPRTS: LDA FACEXP
UNPRTS: RTS                     ; lands on OPTAB's address-1 => the operator routine
```

The final `RTS` at [m6502.asm:3321](../m6502.asm#L3321) pops the two-byte dispatch address and enters
the operator with:

- **ARG** = the left operand, **FAC** = the right operand
- `ARISGN` = the XOR of the two signs
- `DOMASK` = the relational bits
- carry = "this is a string comparison"

`FRMEVL` returns with `A = FACEXP`, deliberately *not* the terminating character — stated at
[m6502.asm:3185](../m6502.asm#L3185). Callers must `CHRGOT` to see where they are.

### 2.8 Recursion bound

Every nesting level calls `GETSTK` with `A=1` ([m6502.asm:3204-3205](../m6502.asm#L3204-L3205)),
demanding `S > 64`. `NUMLEV`=23 is the guaranteed depth. Exceeding it is `?OM ERROR`, not a crash —
see [04-interpreter-loop.md](04-interpreter-loop.md#51-getstk--is-there-enough-6502-stack).

---

## 3. `EVAL` — one term

[m6502.asm:3323-3372](../m6502.asm#L3323-L3372). Recognition order:

| # | Test | Action |
|---|---|---|
| 1 | carry clear from `CHRGET` — a digit | `JMP FIN` (floating-point input) |
| 2 | `ISLETC` — a letter | `ISVAR` |
| 3 | character 255 (`PI`) | *Commodore only*: load the `PIVAL` constant |
| 4 | `.` | `FIN` — a leading decimal point |
| 5 | `MINUTK` | `LDYI NEGTAB-OPTAB`, `GONPRC` |
| 6 | `PLUSTK` | loop back to `EVAL0` — unary plus is a no-op |
| 7 | `"` | `STRTXT` → `STRLIT`, then point `TXTPTR` past the literal |
| 8 | `NOTTK` | `LDYI NOTTAB-OPTAB`, `GONPRC` |
| 9 | `FNTK` | `FNDOER` — call a user function |
| 10 | `>= ONEFUN` | `ISFUN` |
| 11 | anything else | `PARCHK` — require a parenthesised subexpression |

`ISLETC` ([m6502.asm:3702-3707](../m6502.asm#L3702-L3707)) is the same double-subtract trick as
`CHRGET`'s digit test, returning carry set for `A`..`Z`.

`GONPRC` ([m6502.asm:3395](../m6502.asm#L3395)) does `PLA PLA` to discard `EVAL`'s own return address
and then jumps to `NEGPRC`. A unary operator is thus pushed as an ordinary `OPTAB` entry and its
eventual `RTS` lands back in the *caller's* `FRMEVL` loop — unary minus and `NOT` need no special
case beyond having their own precedence.

### 3.1 The syntax helpers

[m6502.asm:3372-3393](../m6502.asm#L3372-L3393):

```
PARCHK: JSR CHKOPN / JSR FRMEVL
CHKCLS: LDAI 41 / SKIP2
CHKOPN: LDAI 40 / SKIP2
CHKCOM: LDAI 44
SYNCHR: LDYI 0 / CMPDY TXTPTR / BNE SNERR
CHRGO5: JMP CHRGET
```

Three entry points sharing one body through `SKIP2`. `SYNCHR` compares `A` against the character at
`(TXTPTR),0` — reading it directly rather than through `CHRGOT` — and then `CHRGET`s past it,
returning the *following* character. The `SYNCHK` macro is `LDAI q / JSR SYNCHR`.

---

## 4. Function dispatch

`ISFUN` [m6502.asm:3468-3516](../m6502.asm#L3468-L3516).

```
ISFUN: ASL A / PHA / TAX / JSR CHRGET
       CPXI 2*LASNUM-256+1
       BCC OKNORM
```

`A` is the token; `ASL` gives `2*token - 256`. Tokens up to `LASNUM` (= `CHR$`, 190) are
**one-argument** functions and take `OKNORM`: `PARCHK` to evaluate the parenthesised argument, then
`FINGO`.

`FINGO` ([m6502.asm:3510](../m6502.asm#L3510)) self-modifies the `JMPER` trampoline in page zero with
the address from `FUNDSP` and calls it:

```
FINGO: LDA FUNDSP-2*ONEFUN+256,Y / STA JMPER+1
       LDA FUNDSP-2*ONEFUN+257,Y / STA JMPER+2
       JSR JMPER
       JMP CHKNUM
```

Effective address = `FUNDSP + 2*(token - ONEFUN)`.

### 4.1 Multi-argument functions

`LEFT$`, `RIGHT$` and `MID$` ([m6502.asm:3491-3506](../m6502.asm#L3491-L3506)) are past `LASNUM` and
take a different path: `CHKOPN`, `FRMEVL` for the string, `CHKCOM`, `CHKSTR`, then the descriptor
pointer and the function number are juggled on the stack while `GETBYT` reads the numeric argument.
At function entry the stack holds:

```
[length byte][descriptor lo][descriptor hi][JSR JMPER return address]...
```

`PREAM` ([m6502.asm:4709-4725](../m6502.asm#L4709-L4725)) is the shared prologue that unpicks this.

### 4.2 String-returning functions

Any function that returns a string must **discard `FINGO`'s `JSR JMPER` return address** with
`PLA PLA`, so that it returns directly to `FRMEVL` and skips the `JMP CHKNUM` that would otherwise
reject its own result. `STR$` ([m6502.asm:4245-4246](../m6502.asm#L4245-L4246)), `CHR$`
([m6502.asm:4640-4641](../m6502.asm#L4640-L4641)) and `PREAM`
([m6502.asm:4714-4715](../m6502.asm#L4714-L4715)) all do this.

---

## 5. Relational operators

`DOREL` [m6502.asm:3547-3596](../m6502.asm#L3547-L3596).

### 5.1 Getting the mask across

`FINREL` ([m6502.asm:3248](../m6502.asm#L3248)) packs the string flag into the mask before it is
pushed:

```
FINREL: LSR VALTYP        ; string bit -> carry, AND turns VALTYP from $FF into $7F
        TXA / ROL A       ; OPMASK = (relbits << 1) | is_string
        ...decrement TXTPTR (the relational scan overshot by one)...
        LDYI PTDORL-OPTAB
```

The `LSR VALTYP` does double duty: it captures the string flag *and* clears bit 7 of `VALTYP`, so
the `CHKNUM` at [m6502.asm:3239](../m6502.asm#L3239) will not reject the operand — noted in the comment
at [m6502.asm:3258](../m6502.asm#L3258).

`PULSTK` later does `PLA / LSR A / STA DOMASK`, putting the string flag back into carry and the
three plain bits into `DOMASK`.

### 5.2 Numeric comparison

```
DOREL:  JSR CHKVAL                  ; carry selects string vs numeric, and enforces a match
        BCS STRCMP
        LDA ARGSGN / ORAI 127 / AND ARGHO / STA ARGHO   ; re-pack ARG
        LDWDI ARGEXP / JSR FCOMP / TAX / JMP QCOMP
```

`FCOMP` returns 1 if `ARG < FAC`, 0 if equal, -1 if `ARG > FAC`.

### 5.3 String comparison

`STRCMP` ([m6502.asm:3557-3589](../m6502.asm#L3557-L3589)) frees both temporaries, takes the length
difference to establish a tie-break sign, then compares bytes up to the shorter length. The first
differing byte decides; if none differs, the length difference does. So comparison is
lexicographic with a shorter prefix ordering first, and there is no case folding.

### 5.4 Producing the result

`DOCMP` ([m6502.asm:3590-3596](../m6502.asm#L3590-L3596)):

```
DOCMP: INX / TXA / ROL A / AND DOMASK
       BEQ GOFLOT
       LDAI 377
GOFLOT: JMP FLOAT
```

`X` ∈ {-1, 0, 1} becomes {0, 1, 2} after `INX`, then `ROL` maps them to {1, 2, 4} — exactly the
`>`, `=`, `<` bits. AND with `DOMASK`: non-zero gives **-1 (true)**, zero gives **0 (false)**.

So BASIC's true is -1, and because it is produced by masking, `(A<B) OR (A=B)` and `A<=B` yield
identical values.

---

## 6. `AND`, `OR`, `NOT`

[m6502.asm:3518-3540](../m6502.asm#L3518-L3540).

```
OROP:  LDYI 255 / SKIP2
ANDOP: LDYI 0
       STY COUNT
       JSR AYINT
       FACMO EOR COUNT -> INTEGR ; FACLO EOR COUNT -> INTEGR+1
       JSR MOVFA                 ; ARG -> FAC
       JSR AYINT
       FACLO EOR COUNT AND INTEGR+1 EOR COUNT -> Y
       FACMO EOR COUNT AND INTEGR   EOR COUNT -> A
       JMP GIVAYF
```

One routine implements both by De Morgan: `COUNT` = 0 gives AND, `COUNT` = 255 complements the
inputs and the output, giving OR.

```
NOTOP: JSR AYINT
       FACLO EOR 255 -> Y ; FACMO EOR 255 -> A
       JMP GIVAYF
```

so `NOT x = ~x = -x-1` in 16-bit two's complement, and `NOT 0 = -1`, which is consistent with the
comparison result.

Both operands go through `AYINT`, so **the operands must be in -32768..32767** or `?FC ERROR`
results, and any fractional part is discarded by `QINT`'s floor. The result is always in that range
too, since `GIVAYF` floats a signed 16-bit value.

### 6.1 `AYINT`

[m6502.asm:3801-3810](../m6502.asm#L3801-L3810).

```
POSINT: JSR CHKNUM / LDA FACSGN / BMI NONONO
AYINT:  LDA FACEXP / CMPI 144 / BCC QINTGO
        LDWDI N32768 / JSR FCOMP
NONONO: BNE FCERR
QINTGO: JMP QINT
```

Exponent below 144 implies `|x| < 32768`; the only legal value at or above it is exactly -32768.
`POSINT` additionally rejects negatives and is the entry used by array subscripts and `CONINT`.

> `N32768` is declared as only four bytes ([m6502.asm:3791](../m6502.asm#L3791)) but is read as five
> under `ADDPRC=1`, so the comparison value is -32768.00048828125 and **exactly -32768 is rejected
> with `?FC ERROR`**. See [13-porting-notes.md](13-porting-notes.md#3-defects-in-the-1978-code).

---

## 7. `ISVAR` — a variable as a term

[m6502.asm:3399-3465](../m6502.asm#L3399-L3465). `PTRGET` resolves the name, then:

```
ISVRET: STWD FACMO         ; the variable's ADDRESS becomes the FAC's low two bytes
```

If `VALTYP` is non-zero the string result is already complete — the FAC now points at the variable's
own 3-byte descriptor. Otherwise, with `INTPRC`, `GOOO` tests `INTFLG`: an integer variable is read
as two **big-endian** bytes and floated by `GIVAYF`; a float is unpacked by `MOVFM`.

This is the point at which "a string in the FAC is a pointer to a descriptor" is established. See
[01-memory-map.md](01-memory-map.md#2-alias-table).
