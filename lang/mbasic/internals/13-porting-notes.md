# 13 — Porting notes

This file separates three things a reimplementation must treat differently: mechanisms that exist
only because the target was a 6502 and can simply be dropped; mechanisms that look like tricks but
actually carry language semantics; and defects in the original that need a deliberate decision
either way.

Nothing here prescribes a design. It marks which parts of the analysis constrain one.

---

## 1. Machine tricks — safe to discard

None of these are visible to a BASIC program.

| Mechanism | Where | Why it exists |
|---|---|---|
| Self-modifying `CHRGET`, with `TXTPTR` as an instruction operand | [m6502.asm:943-976](../../m6502.asm#L943-L976) | saves an index register and a zero-page indirect setup on the hottest path. A plain pointer is equivalent |
| `SKIP1` / `SKIP2` opcode swallowing | [m6502.asm:188-189](../../m6502.asm#L188-L189) | "enter this routine one instruction in" — a fall-through with a different preset |
| Dispatch by pushing `address-1` and letting `RTS` jump | `STMDSP`, `FUNDSP`, `OPTAB` | the 6502 has no indirect `JSR` |
| The `JMPER` trampoline in page zero | [m6502.asm:891](../../m6502.asm#L891) | likewise — a patched `JMP` standing in for an indirect call |
| `ROR` emulation under `RORSW=0` | [m6502.asm:108-115](../../m6502.asm#L108-L115) | early 6502s shipped with a broken `ROR` |
| `BCCA`, `BNEA` and friends | [m6502.asm:174-181](../../m6502.asm#L174-L181) | the 6502 has no unconditional branch; these document intent |
| `JEQ` / `JNE` | [m6502.asm:169-173](../../m6502.asm#L169-L173) | long conditional branches, since 6502 branches reach only ±127 |
| Most page-zero aliasing | [01-memory-map.md](01-memory-map.md#2-alias-table) | 256 bytes of fast memory, more names than bytes |
| Byte-at-a-time then bit-at-a-time normalisation | `NORM3` | a speed optimisation |
| The `MLTPLY` sentinel bit | [m6502.asm:5297](../../m6502.asm#L5297) | makes a loop run exactly 8 times with no counter |
| `ZEREMV` popping the caller's return address | [m6502.asm:5361](../../m6502.asm#L5361) | a two-level early return |
| `ROLSHF` / `MULSHF` as interior entry points of `SHIFTR`'s loop | [m6502.asm:5092-5125](../../m6502.asm#L5092-L5125) | code sharing; note these are *not* callable subroutines |
| Rebuilding all program links after every edit (`LNKPRG`) | [m6502.asm:1642](../../m6502.asm#L1642) | simpler than patching; a port using offsets or indices needs neither |

---

## 2. Mechanisms that carry real semantics

These are observable from a BASIC program, or are required for the rest of the system to be correct.

### 2.1 Processor flags are a calling convention

`CHRGET` returns a character in `A` **and** two flags that mean "is a digit" and "is a statement
terminator", and callers many levels away consume them. `GONE2` even relies on carry left over from
a *previous* routine ([m6502.asm:2172](../../m6502.asm#L2172)).

Other flag-based contracts: `FNDFOR`'s (Z, X, A); `CHKVAL`'s carry-in meaning "I want a string";
`FCOMP`'s -1/0/+1 in `A`; `ISLETC`'s carry; `FNDLIN`'s carry distinguishing "found" from "here is
where it goes"; `TABER`'s `PHP`/`PLP` to remember whether it was `TAB` or `SPC`.

A port should make each of these an explicit return value, and should expect to find more of them —
they are not documented in the source.

### 2.2 The 6502 stack is a typed, scanned data structure

It holds four record kinds ([02-data-structures.md](02-data-structures.md#4-stack-frames)), three of
which are tagged and found again by linear scanning:

- `FNDFOR` walks *upward* from the current stack pointer looking for `FORTK` bytes, and stops at the
  first byte that is not one.
- `FOR` truncates the stack with `TXS` to discard a matching frame and everything above it.
- `NEXT` and `RETURN` do the same.
- The sentinel region above `STKEND` exists **only** so the scan terminates
  ([01-memory-map.md](01-memory-map.md#113-page-1--the-stack)).
- `NEWSTT`'s return address must sit immediately below the topmost frame, which is why several
  statement handlers discard it explicitly.

A reimplementation needs an explicit tagged frame stack with the same search and truncate
operations. Using the host language's call stack will not work, because `FOR` and `RETURN` unwind
by *content*, not by depth.

### 2.3 A string in the FAC is a pointer, not a value

`DSCTMP = FAC` ([m6502.asm:921](../../m6502.asm#L921)). When `VALTYP` is 255, `FACMO`/`FACLO` hold the
*address of a 3-byte descriptor*, not a number. Numeric and string values are genuinely different
shapes sharing one register.

The 1978-02-11 bug ([m6502.asm:230-231](../../m6502.asm#L230-L231)) is exactly this biting: rounding the
FAC before pushing it could increment a string pointer.

### 2.4 One descriptor per string body

The garbage collector moves a string and rewrites *the one descriptor it came from*. Two descriptors
sharing a body would leave one dangling. Everything in
[08-strings-gc.md](08-strings-gc.md) follows: the six-step protocol, the copy-if-volatile rule,
`LET`'s copy decision, and the string temporaries.

A port with reference-counted or garbage-collected strings gets this for free but must then decide
what to do about `FRE(0)`, which programs call for its compaction side effect, and about the `?OM`
that programs rely on to detect exhaustion.

### 2.5 The copy-if-volatile rule is observable

Whether `A$ = "X"` copies depends on **which page the text is on**
([m6502.asm:4295-4305](../../m6502.asm#L4295-L4305)). A string constant in a program line is not copied
and points into the program text; the same constant typed as a direct statement is copied. A program
can observe the difference through `FRE`.

### 2.6 The numeric model

- 32-bit mantissa, excess-128 exponent, no NaN/Inf/denormals/negative zero.
- `INT` and `QINT` are **floor**, not truncation: `INT(-2.5)` is -3.
- `AYINT` restricts to -32768..32767 and raises `?FC` outside it, so `AND`, `OR`, `NOT` and all
  array subscripts inherit that range.
- Decimal conversion in **both** directions is by repeated `MUL10`/`DIV10`, not by a table, so
  rounding differs from any library `strtod`/`printf`.
- `FOUT` produces at most 9 significant digits, switches to exponential outside
  0.01 <= |x| < 10^9, and always emits a leading space for non-negative values and a trailing space
  from `PRINT`.
- Comparison includes the guard byte `FACOV`.

Reproducing BASIC-visible results bit-for-bit means reimplementing the arithmetic, not calling
`double`. Using `double` will be close but will differ in the last digits, in the `FOUT` formatting
boundaries, and in `FOR` loop termination for accumulating steps.

### 2.7 Aliasing and adjacency that are load-bearing

Listed in full in [01-memory-map.md](01-memory-map.md#3-adjacency-constraints). The ones that are
not merely economies:

- `REASON` saves the 9 bytes `HIGHDS`..`LOWTR+1` as a block across a garbage collection.
- `FIN` zeroes 11 consecutive bytes, `DECCNT` through `SGNFLG`, in one loop.
- `DIMFLG` and `VALTYP` are read as a pair.
- `FAC+1..+4`, `ARGEXP+1..+4` and `RESHO..RESLO` are indexed from a base address held in `X`.
- The garbage collector encodes a descriptor's offset within its entry in one bit of the stride
  (`SIZE AND 4`).

### 2.8 Small semantics that are easy to lose

| Behaviour | Where |
|---|---|
| A false `IF` skips the **whole rest of the line**, including statements after a `:` | [05-statements.md](05-statements.md#53-if--then) |
| `FOR`/`NEXT` never terminates on equality; a zero step loops forever | [05-statements.md](05-statements.md#44-next) |
| Arrays are column-major: the *first* subscript varies fastest | [07-variables-arrays.md](07-variables-arrays.md#24-subscript-to-offset) |
| Reading an undefined variable does not create it; assigning does | [07-variables-arrays.md](07-variables-arrays.md#15-not-found-the-caller-check) |
| Only the first two characters of a name are significant | [07-variables-arrays.md](07-variables-arrays.md#12-name-encoding) |
| Typing any program line clears all variables | [03-tokenizer-editor.md](03-tokenizer-editor.md#5-the-program-editor) |
| True is -1, produced by masking, so `A<=B` and `(A<B) OR (A=B)` are identical | [06-expressions.md](06-expressions.md#54-producing-the-result) |
| `=<` and `><` are legal spellings; `<<` is a syntax error | [06-expressions.md](06-expressions.md#24-relational-accumulation) |
| String `+` calls `EVAL`, not `FRMEVL`, so it binds tighter than everything | [08-strings-gc.md](08-strings-gc.md#61-concatenation) |
| Only three string temporaries; exceeding them is `?ST` | [08-strings-gc.md](08-strings-gc.md#21-putnew) |
| Line numbers are limited to 63999, and exceeding it is a **syntax** error | [03-tokenizer-editor.md](03-tokenizer-editor.md#57-linget--parsing-a-line-number) |
| `IF 2 > F OR T=5 THEN` mis-tokenizes because `F OR T` matches `FOR` | [03-tokenizer-editor.md](03-tokenizer-editor.md#24-the-prefix-hazard) |
| `?REDO FROM START` re-executes the **entire** `INPUT` statement | [05-statements.md](05-statements.md#33-the-error-paths) |
| A blank line typed to `INPUT` is a silent, continuable `STOP` | [05-statements.md](05-statements.md#33-the-error-paths) |
| `WAIT` has no timeout and no ^C check | [05-statements.md](05-statements.md#71-poke-peek-wait) |
| `0^0` is 1 | [10-math-functions.md](10-math-functions.md#42-sqr-and-) |
| Error codes are byte offsets and differ between `LNGERR` settings | [04-interpreter-loop.md](04-interpreter-loop.md#43-error-codes-lngerr--1) |

---

## 3. Defects in the 1978 code

Each was checked by reading the cited lines directly. Each needs a conscious decision: reproduce it
for fidelity, or fix it.

### 3.1 The Apple ^C check never breaks

[m6502.asm:2221-2226](../../m6502.asm#L2221-L2226) with
[m6502.asm:1756-1757](../../m6502.asm#L1756-L1757).

```
ISCNTC: LDA ^O140000     ; $C000 - keyboard, bit 7 set when a key is ready
        CMPI ^O203       ; $83 = ^C with the strobe bit  -- correct so far
        BEQ ISCCAP
        RTS
ISCCAP: JSR INCHR        ; Apple INCHR does ANDI 127, so A becomes 3
        CMPI ^O203       ; compares 3 against 131 -- always fails
STOP:   BCS STOPC        ; C=0, not taken
END:    CLC
STOPC:  BNE CONTRT       ; Z=0, taken -> return
```

The keyboard test is right; the confirmation after `INCHR` compares the stripped value against the
unstripped constant. The character is consumed and execution continues. **On the Apple build as
checked in, ^C cannot interrupt a running program.**

### 3.2 `AYINT` rejects exactly -32768

[m6502.asm:3791](../../m6502.asm#L3791): `N32768: EXP 144,128,0,0` — four bytes. `EXP` emits one byte per
expression, and with `ADDPRC=1` `FCOMP` reads five. The fifth byte is the first opcode of `INTIDX`,
`JSR` = 32, so the constant compares as **-32768.00048828125**.

`AYINT` allows exponents below 144 and otherwise permits only an exact match against this constant.
Exactly -32768 therefore fails and raises `?FC ERROR`, so
`A% = -32768`, `NOT 32767` and `-32768 AND -1` all fail.

### 3.3 The `RND` constants read a stray byte

[m6502.asm:6344](../../m6502.asm#L6344) and [m6502.asm:6348](../../m6502.asm#L6348) declare `RMULZC` and
`RADDZC` with four bytes and no `IFN ADDPRC` fifth, unlike every other constant in the file. Under
`ADDPRC=1` they are read as five:

| Constant | Intended | Actually |
|---|---|---|
| `RMULZC` | 11879546.0 | 11879546.40625 (fifth byte = `RADDZC`'s first) |
| `RADDZC` | 3.927677738602142e-08 | 3.927677783011063e-08 (fifth byte = the `JSR` opcode 32) |

The generator still works; the sequence is simply not the one intended.

### 3.4 `INIT` copies only four of the five seed bytes

[m6502.asm:6733](../../m6502.asm#L6733): `LDXI RNDX+4-CHRGET` = 28 bytes, covering `CHRGET` (24) plus
`RNDX[0..3]`. `RNDX+4` — the `ADDPRC` fifth byte — is never copied, so the seed's least significant
byte is whatever the RAM happened to contain.

Compounding it, the ROM template ends in **88** ([m6502.asm:6698](../../m6502.asm#L6698)) while the RAM
declaration ends in **89** ([m6502.asm:982](../../m6502.asm#L982)), so the two disagree about a byte that
is never transferred anyway.

### 3.5 The two copies of `CHRGET` are not identical

`INIT`'s `MOVCHG` is unconditional, so the `INITAT` template
([m6502.asm:6678-6692](../../m6502.asm#L6678-L6692)) overwrites the page-zero `CHRGET`
([m6502.asm:958-975](../../m6502.asm#L958-L975)) **in every build**, RAM or ROM. The two order their `:`
and `' '` tests oppositely.

Entered at `CHRGET` or `CHRGOT` the two are equivalent. But `QNUM` is a documented third entry point
at `CHRGET+13`, and in the copy that actually runs, that address holds `CMP #' '` rather than
`CMP #':'`. Tracing the running version:

| Input | Intended `QNUM` | Running `QNUM` |
|---|---|---|
| `'0'`..`'9'` | C=0 (numeric) | C=0 — same |
| `'A'` | C=1 (not numeric) | **C=0 — reports numeric** |
| `' '` | C=1 | **branches to `CHRGET`, advancing `TXTPTR` and refetching** |

`QNUM`'s only caller is `TIMNUM` ([m6502.asm:2609-2610](../../m6502.asm#L2609-L2610)), inside
`IFN TIME` — compiled out here, but active on the Commodore, where it validates the digits assigned
to `TI$`. There, a non-digit would be accepted rather than raising `?FC`.

This one was not in the source's revision log and appears to be unrecorded.

### 3.6 `ROMSW=0` builds never store `TXTTAB`

[m6502.asm:6845-6875](../../m6502.asm#L6845-L6875). In the RAM build, `HAVFNS` leaves the intended start
of program text in `[X,Y]` (and the `IFE REALIO!LONGI` path loads `INITAT-1`), but the only
`STXY TXTTAB` is inside `IFN ROMSW`. The value in `[X,Y]` is discarded by the following
`LDYI 0 / TYA`.

So the whole "delete `SIN`/`COS`/`TAN`/`ATN` to make room" feature computes a start address it never
uses. Not reachable in this build, since `ROMSW=1`.

### 3.7 Two different formulas for `NCMWID`

Compile-time ([m6502.asm:2749](../../m6502.asm#L2749)):
`NCMPOS = ((LINLEN/CLMWID)-1)*CLMWID` → **14** for `LINLEN=40, CLMWID=14`.

Run-time, `MORCPS` ([m6502.asm:6838-6843](../../m6502.asm#L6838-L6843)):
`NCMWID = LINWID - (LINWID mod CLMWID)` → **28** for the same inputs.

Answering `40` to `TERMINAL WIDTH?` therefore yields different comma-zone behaviour than pressing
return and keeping the default 40.

### 3.8 Equal-exponent `FADD` adds one to the guard byte

`FADD` reaches `FADD2` from `BEQ FADD4` ([m6502.asm:4954](../../m6502.asm#L4954)) with carry **set**,
because the branch follows an `SBC` that produced zero. `FADD2`'s first instruction is
`ADC OLDOV` ([m6502.asm:5025](../../m6502.asm#L5025)), so the guard byte becomes `OLDOV+1`, and an
`OLDOV` of 255 carries into `FACLO`.

On the shifted paths carry is clear, because both `SHFTRT` and the `ROLSHF` loop end with `CLC`. So
addition of two numbers with identical exponents rounds fractionally differently from every other
case.

### 3.9 `VAL` writes one byte past the string

[m6502.asm:4763-4789](../../m6502.asm#L4763-L4789). It stores a zero at `body + length` to give `FIN` a
terminator, then restores the original byte. Harmless on a 6502; a genuine out-of-bounds write for
any bounded string type, and not re-entrant.

### 3.10 Dead code

- `JSR MOV1F` at the head of `TAN` ([m6502.asm:6446](../../m6502.asm#L6446)) — `POLYX` inside `SIN`
  overwrites `TEMPF1` before `TAN` reads it back.
- `CLR ARISGN` before `FSUBT` in `SIN` ([m6502.asm:6424](../../m6502.asm#L6424)) — `FSUBT` recomputes it.
- The Apple `LOAD`/`SAVE` routines ([m6502.asm:2323-2363](../../m6502.asm#L2323-L2363)) — no token, no
  dispatch entry, because `DISKO=0`.
- `REASON`'s save loop performs one load too many
  ([m6502.asm:1495](../../m6502.asm#L1495)): the loop runs ten times but pushes only nine of the bytes it
  loads, so the final `LDA HIGHDS-1,X` with `X=0` fetches address 150 and discards it. The nine
  bytes actually saved are 151-159, which is exactly what the restore loop puts back.

---

## 4. Comment defects in the original

The source's comments are wrong in these places. Trust the code.

| Location | Says | Actually |
|---|---|---|
| [m6502.asm:906-907](../../m6502.asm#L906-L907) | `TENEXP` is "HAS A DPT BEEN INPUT?", `DPTFLG` is "BASE TEN EXPONENT" | swapped; the names are right |
| [m6502.asm:1410](../../m6502.asm#L1410) | `BLTU` leaves pointers "MINUS 200 OCTAL" | minus 256 — the high byte is decremented |
| [m6502.asm:2030](../../m6502.asm#L2030) | "YES. END OF LINE" on a `BNE` | the branch is taken when it is *not* end of line |
| [m6502.asm:3153](../../m6502.asm#L3153) | `LOOPDN` "adds 16 with carry" | 18, with `ADDPRC=1` |
| [m6502.asm:576-580](../../m6502.asm#L576-L580) | the string flag is on the first name character | it is on the **second**; bit 7 of the first means integer or `DEF FN` |
| [m6502.asm:766](../../m6502.asm#L766) | `VALTYP` "0=NUMERIC 1=STRING" | string is 255, not 1 |
| [m6502.asm:3895](../../m6502.asm#L3895) | comment on the dimension-count check | misleading about which error is raised |
| [m6502.asm:5998,6000](../../m6502.asm#L5998-L6000) | "STORE HIGH DIGIT" / "STORE LOW DIGIT" | swapped; tens are emitted first |
| [m6502.asm:6160-6162](../../m6502.asm#L6160-L6162) | `EXP` uses `P(LN(2)*(INT+1)-X)` | the code evaluates `P(y - INT(y))` with base-2 coefficients |
| [m6502.asm:5811-5825](../../m6502.asm#L5811-L5825) | `NZ0999` is 99999999.9499, `NZ9999` is 999999999.499 | 99999999.90625 and 999999999.25 |
| [m6502.asm:4859](../../m6502.asm#L4859) | "THE MANTISSA IS 24 BITS LONG" | 32 bits when `ADDPRC=1`, as in this build |

---

## 5. Verification performed on this document set

The claims here were checked rather than assumed:

1. **Line references** — every cited range was read in the source.
2. **Constants** — every octal float table (`LOGCN2`, `EXPCON`, `SINCON`, `ATNCON`, `FOUTBL`, `PI2`,
   `TWOPI`, `LOG2`, `LOGEB2`, `SQRHLF`, `SQRTWO`, `RNDX`, the `FOUT` bounds) was decoded
   independently using the documented packed format and compared against the mathematical value it
   approximates. Agreement is to the limit of a 32-bit mantissa, and `FOUTBL` decodes to exact
   powers of ten. This confirms the format description in
   [09-float-format-arith.md](09-float-format-arith.md#1-the-number-format).
3. **Token numbering** — `RESLST` was recounted from
   [m6502.asm:1112-1245](../../m6502.asm#L1112-L1245) under this build's switches, confirming the table in
   [03-tokenizer-editor.md](03-tokenizer-editor.md#3-the-reserved-word-table).
4. **Structure sizes** — the `FOR` frame layout was re-derived from `FOR`'s push order and checked
   against the offsets `NEXT` indexes; likewise the `GOSUB` frame, the variable stride, array element
   sizes and `GETSTK`'s reserve.
5. **Defects** — each entry in §3 was traced instruction by instruction at its source line. §3.5 was
   found during this pass and is not in the source's revision log.

What has **not** been done: the source has never been assembled or executed, here or elsewhere in
this repository. Every statement about runtime behaviour is derived from reading the code.
