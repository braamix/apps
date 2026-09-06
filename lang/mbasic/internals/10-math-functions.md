# 10 — Numeric conversion and the transcendental functions

Still inside the `RADIX 8` region ([m6502.asm:4847](../m6502.asm#L4847) to
[m6502.asm:6671](../m6502.asm#L6671)) except where noted. Decimal first, octal source form in
parentheses.

Every constant in §8 was decoded from its source bytes using the format described in
[09-float-format-arith.md](09-float-format-arith.md#1-the-number-format) and checked against the
mathematical value it approximates.

---

## 1. `FIN` — decimal text to float

[m6502.asm:5694-5807](../m6502.asm#L5694-L5807). Entry contract
([m6502.asm:5695-5703](../m6502.asm#L5695-L5703)): `TXTPTR` points at the first character, that
character is in `A`, and the carry from `CHRGET` says whether it is a digit.

### 1.1 State

| Variable | Meaning |
|---|---|
| `DPTFLG` | bit 7 = a `.` has been seen; bit 6 = a *second* `.` has been seen |
| `DECCNT` | count of digits after the decimal point |
| `TENEXP` | the base-ten exponent being accumulated |
| `EXPSGN` | bit 7 = the exponent is negative |
| `SGNFLG` | 255 = the mantissa is negative |

All five, plus the six FAC bytes, are zeroed by one indexed loop
([m6502.asm:5704-5708](../m6502.asm#L5704-L5708)) — which is why those eleven page-zero cells must be
contiguous.

### 1.2 The parse

```
FIN:    zero DECCNT..SGNFLG (11 bytes); X ends at 0xFF
        BCC FINDGQ                   ; flags still set from CHRGET
        '-' -> SGNFLG := 0xFF ; '+' -> ignore
FINDIG: (a digit) PHA
        if DPTFLG bit 7: INC DECCNT
        JSR MUL10
        PLA / SEC / SBCI "0"
        JSR FINLOG                   ; FAC := FAC*10 + digit
FINDP:  ('.') ROR DPTFLG             ; C=1 from the CMP, so bit 7 := 1
        BIT DPTFLG / BVC FINC        ; a second '.' ends the number
        ('E') parse an optional sign - accepting BOTH the ASCII characters
              and the crunched PLUSTK/MINUTK tokens - then digits
FINE:   TENEXP := TENEXP - DECCNT
FINDIV: while TENEXP < 0: DIV10, INC TENEXP
FINMUL: while TENEXP > 0: MUL10, DEC TENEXP
FINQNG: if SGNFLG < 0: NEGOP
```

Accepting `PLUSTK` and `MINUTK` as well as `'+'` and `'-'` after the `E`
([m6502.asm:5726-5733](../m6502.asm#L5726-L5733)) is necessary because by the time `FIN` runs on
program text, the tokenizer has already replaced them.

### 1.3 Scaling by repeated multiplication

`FIN` applies the decimal exponent by calling `MUL10` or `DIV10` in a loop — there is **no table of
powers of ten**. So `1E38` costs 38 multiplications and accumulates the rounding error of each. This
is the main reason a reimplementation using `strtod` will not reproduce this BASIC's results
bit-for-bit.

### 1.4 Exponent overflow

[m6502.asm:5786-5807](../m6502.asm#L5786-L5807), commented "NOTE: EXP OVERFLOW IS NOT CHECKED FOR":

```
FINEDG: LDA TENEXP
        CMPI 10 (12)              ; would the exponent reach 100?
        BCC MLEX10
        LDAI 100 (144)
        BIT EXPSGN / BMI MLEXMI   ; negative exponent: clamp to 100, no error
        JMP OVERR                 ; positive: ?OV ERROR
MLEX10: A = A*10 + the current digit
```

A large *negative* exponent is silently clamped at 100 (and the value then underflows to zero); a
large *positive* one raises `?OV`. The digit is re-fetched through `TXTPTR` rather than kept in a
register.

---

## 2. `FOUT` — float to decimal text

[m6502.asm:5809-6008](../m6502.asm#L5809-L6008).

### 2.1 Entry points and the buffer

```
LINPRT: STWX FACHO / LDXI 144 (220) / SEC / JSR FLOATC / JSR FOUT
FOUT:   LDYI 1        ; text begins at FBUFFR (page 1) - not copied
FOUTC:  ...           ; STR$ enters with Y=0, so text begins at LOFBUF (page 0) - copied
```

`FOUT` returns a pointer to the NUL-terminated result in `(A=low, Y=high)`. The two entry points and
the page 0/1 split are what make the same buffer behave correctly for `PRINT` and for `STR$` — see
[08-strings-gc.md](08-strings-gc.md#33-the-copy-if-volatile-rule).

`LINPRT` presets carry so `FLOATC` treats the 16-bit value as **unsigned**, which is how line
numbers and the free-byte count print as 0..65535.

### 2.2 Scaling to an integer

```
FOUTC:  first character := ' ' if positive else '-'
        STA FACSGN                 ; both characters have bit 7 clear, so the FAC is now positive
        if FACEXP == 0: print "0" and stop
        if the value < 1.0: multiply by 10^9 (NZMIL) and set DECCNT := -9
FOUT4:  while FAC > 999999999.25   (NZ9999): DIV10, INC DECCNT
FOUT3:  while FAC <  99999999.90625(NZ0999): MUL10, DEC DECCNT
FOUT5:  JSR FADDH                  ; add 0.5 to round
BIGGES: JSR QINT                   ; now an exact 9-digit integer
```

Afterwards the FAC holds an integer *M* and the original value is `M x 10^DECCNT`. The
`BEQ BIGGES` shortcut skips the `+0.5` when the value exactly equals the upper bound.

With `ADDPRC=0` the same code produces 6 digits instead of 9, using different bounds.

### 2.3 Choosing fixed or exponential notation

[m6502.asm:5893-5907](../m6502.asm#L5893-L5907):

```
        LDXI 1
        LDA DECCNT / CLC / ADCI 10 (3*ADDPRC+7)
        BMI FOUTPI              ; DECCNT+10 < 0  => value < 0.01  => E notation
        CMPI 11 (3*ADDPRC+10)
        BCS FOUT6               ; DECCNT >= 1    => value >= 10^9 => E notation
        ADCI 255 / TAX          ; X = digits before the decimal point
        LDAI 2
FOUTPI: SEC
FOUT6:  SBCI 2
        STA TENEXP              ; 0 for fixed; DECCNT+8 for E notation
        STX DECCNT
```

So **fixed notation is used for 0.01 <= |x| < 10^9**, and exponential otherwise. In exponential form
the point goes after one digit and the printed exponent is `DECCNT+8`.

For values below 1 a leading `.` is emitted, preceded by at most one `0`
([m6502.asm:5908-5920](../m6502.asm#L5908-L5920)) — so `0.5` prints as `.5`, never as `0.5`.

### 2.4 Digit generation

[m6502.asm:5921-5970](../m6502.asm#L5921-L5970). Digits come from repeatedly adding the entries of
`FOUTBL`, which are **big-endian two's-complement integers with alternating signs**, not floats.

```
FOUT8:  LDYI 0
FOUTIM: LDXI 128 (200)          ; X is both the phase flag and the digit counter
FOUT2:  FAC += FOUTBL[Y..Y+3]   ; 32-bit big-endian add
        INX                     ; N now comes from INX, C from the ADC
        BCS FOUT41 / BPL FOUT2 / BMI FOUT40
FOUT41: BMI FOUT2
FOUT40: TXA
        BCC FOUTYP
        EORI 255 / ADCI 10 (12)  ; digit = 11 - count
FOUTYP: ADCI "0"-1
        ...store, and drop a '.' when DECCNT runs out...
FOUTCM: TXA / EORI 255 / ANDI 128 / TAX   ; flip the phase for the next entry
```

The loop condition is **continue while C equals N**, and crucially **N comes from the `INX`, not
from the addition**. For a table entry holding a *negative* power of ten, `X` starts at 128 (N=1):
keep subtracting while the result is still non-negative. For the next, *positive* entry `X` starts
at 0 (N=0): keep adding while the result is still negative. Alternating signs plus alternating phase
means one loop body does both, with no comparison and no division.

`FOUTCM` re-derives the phase from bit 7 of the character just produced, which is why the store does
`ANDI 127` first.

### 2.5 Trailing zeros and the exponent

[m6502.asm:5971-6008](../m6502.asm#L5971-L6008). Trailing `0`s are scanned off from the right, and if
the scan reaches the `.` that is dropped too. Then, if `TENEXP` is non-zero, `E`, a sign, and
**exactly two digits** are appended.

> The comments `;STORE HIGH DIGIT` and `;STORE LOW DIGIT` at
> [m6502.asm:5998](../m6502.asm#L5998) and [m6502.asm:6000](../m6502.asm#L6000) are the wrong way round;
> the bytes are emitted tens-then-units.

### 2.6 Output formats

| Value | Printed |
|---|---|
| zero | `" 0"` |
| positive | leading space, e.g. `" 123"` |
| negative | leading `-`, e.g. `"-123"` |
| fraction | `" .5"`, `" .05"` — at most one leading zero |
| large or small | `" 1E+10"`, `" 1.23456789E-05"` — sign always present, exponent always two digits |

Maximum width is 1 sign + 9 digits + 1 point + `E` + sign + 2 digits + NUL = **16 bytes**, which is
exactly the size of `FBUFFR`.

`PRINT` adds a trailing space after every number, so numbers appear separated even with `;`.

---

## 3. `LOG`

[m6502.asm:5165-5260](../m6502.asm#L5165-L5260). Uses `ln(F * 2^N) = (N + log2(F)) * ln 2`.

```
LOG:    JSR SIGN / BEQ LOGERR / BPL LOG1
LOGERR: JMP FCERR                     ; zero or negative => ?FC
LOG1:   A := FACEXP - 128 = N, saved on the stack
        FACEXP := 128                 ; FAC := F, in [0.5, 1)
        FADD SQRHLF                   ; F + sqrt(0.5)
        FDIV SQRTWO                   ; sqrt(2) / (F + sqrt(0.5))
        FSUB FONE                     ; 1 - that  ==  (F-sqrt.5)/(F+sqrt.5)
        POLYX LOGCN2                  ; odd polynomial ~ log2((1+z)/(1-z))
        FADD NEGHLF                   ; -0.5   => log2(F)
        PLA / JSR FINLOG              ; + N    => log2(x)
MULLN2: LDWDI LOG2                    ; x ln 2 => ln(x)   (falls into FMULT)
```

The argument is folded into [0.5, 1) by simply overwriting the exponent, which is exact. The
substitution `z = (F - sqrt(.5)) / (F + sqrt(.5))` maps that interval to a small symmetric range
where an odd polynomial converges quickly.

---

## 4. `EXP`, `SQR`, and `^`

### 4.1 `EXP`

[m6502.asm:6153-6283](../m6502.asm#L6153-L6283). Uses
`e^x = 2^(x log2 e)` and `2^y = 2^INT(y) * 2^frac(y)`.

```
EXP:    FMULT LOGEB2                  ; y := x * log2(e)
        LDA FACOV / ADCI 80 (120)     ; a rounding bias
        STA OLDOV
        JSR MOVEF                     ; ARG := FAC, WITHOUT rounding
        if |y| >= 128: MLDVEX         ; positive => ?OV ; negative => 0
EXP1:   JSR INT
        scale factor := INTEGR + 128
        swap FAC and ARG
        JSR FSUBT / JSR NEGOP         ; FAC := y - INT(y), in [0,1)
        POLY EXPCON                   ; P(f) ~ 2^f
        PLA / JSR MLDEXP              ; multiply by 2^INT(y)
        RTS                           ; HAS TO DO JSR DUE TO PULAS IN MULDIV
```

The final `JSR`-then-`RTS` rather than a `JMP` is required because `MULDIV`'s `ZEREMV` path pops a
return address — see
[09-float-format-arith.md](09-float-format-arith.md#51-muldiv--exponent-arithmetic).

> The header comment at [m6502.asm:6160-6162](../m6502.asm#L6160-L6162) describes an older formulation,
> `P(LN(2)*(INT+1)-X)`. The code as written evaluates `P(y - INT(y))` with base-2 coefficients —
> `EXPCON`'s linear term is 0.6931471861898899, i.e. ln 2.

### 4.2 `SQR` and `^`

[m6502.asm:6102-6150](../m6502.asm#L6102-L6150). `SQR` is literally `x ^ 0.5`:

```
SQR:    JSR MOVAF / LDWDI FHALF / JSR MOVFM
        (falls into FPWRT)
```

The rules, stated at [m6502.asm:6111-6119](../m6502.asm#L6111-L6119):

| Case | Result |
|---|---|
| `y == 0` | 1 — so **`0^0` is 1** |
| `x == 0` | 0 |
| `x < 0` | `y` must be an integer, else `?FC`; negate `x`, and negate the result if `y` is odd |
| otherwise | `exp(y * log(x))` |

```
FPWRT:  BEQ EXP                    ; exponent is 0 => EXP(0) = 1, covering 0^0
        LDA ARGEXP / BNE FPWRT1
        JMP ZEROF1                 ; base is 0 => 0
FPWRT1: TEMPF3 := y
        LDA ARGSGN / BPL FPWR1     ; base positive: nothing special
        JSR INT / FCOMP TEMPF3
        BNE FPWR1                  ; y is not an integer => LOG will raise ?FC
        TYA / LDY INTEGR           ; evenness = low byte of INT(y)
FPWR1:  FAC := ARG with the sign from A
        push evenness / JSR LOG / FMULT TEMPF3 / JSR EXP
        PLA / LSR A / BCC NEGRTS   ; even => done
NEGOP:  LDA FACEXP / BEQ NEGRTS / COM FACSGN
```

Because `INT` floors, `INT(y) <= y` always, so the `BNE` path is only reached with `A = -1` — which
is exactly what the comment at [m6502.asm:6133](../m6502.asm#L6133) claims, and it leaves `FACSGN`
negative so `LOG` raises the domain error.

`NEGOP` is a no-op on zero, so `-0` cannot be produced.

---

## 5. `POLY` and `POLYX`

[m6502.asm:6287-6327](../m6502.asm#L6287-L6327). Both take a pointer in `(A, Y)` to a **degree byte**
followed by packed coefficients, **highest order first**.

```
POLYX:  TEMPF1 := X ; FAC := X^2 ; POLY1 ; then multiply by X
POLY:   TEMPF2 := X
POLY1:  DEGREE := *POLYPT ; POLYPT += 1
POLY2:  FAC *= *POLYPT
        POLYPT += 4+ADDPRC
        FAC += *POLYPT
        next multiplicand is TEMPF2
        DEC DEGREE / BNE POLY2
```

With a degree byte of `N` there are `N+1` coefficients and `N` Horner iterations:

```
POLY(x)  = C0*x^N + C1*x^(N-1) + ... + CN
POLYX(x) = x * POLY(x^2) = C0*x^(2N+1) + C1*x^(2N-1) + ... + CN*x     (odd only)
```

`DEGREE` shares storage with `SGNFLG`, `POLYPT` with `FBUFPT`, and `TEMPF1`/`TEMPF2` are clobbered —
which `TAN` exploits deliberately (§7).

---

## 6. `RND`

[m6502.asm:6329-6397](../m6502.asm#L6329-L6397). The scheme is described at
[m6502.asm:6329-6342](../m6502.asm#L6329-L6342): multiply the previous value by a constant, add another
constant, **swap the high and low mantissa bytes**, put the old exponent where `NORMAL` will shift
it in, force the exponent so the result is below 1, normalise, and store back.

```
RND:    JSR SIGN / TAX
        BMI RND1                    ; negative argument: reseed from it, skipping the arithmetic
QSETNR: MOVFM RNDX                  ; the last random number
        TXA / BEQ RANDRT            ; argument 0: return the previous value unchanged
        FMULT RMULZC / FADD RADDZC
RND1:   swap FACHO and FACLO
STRNEX: FACSGN := 0
        FACOV := FACEXP             ; the old exponent becomes the guard byte
        FACEXP := 128 (200)
        JSR NORMAL
        MOVMF RNDX                  ; store the new seed
```

| Argument | Behaviour |
|---|---|
| `RND(0)` | returns the previous value again |
| `RND(x)`, x < 0 | reseeds the sequence from the argument itself |
| `RND(x)`, x > 0 | advances the sequence |

The byte swap is what makes the low-order bits, which the multiply disturbs most, become the
high-order bits of the result.

On the Commodore, `RND(positive)` instead reads the free-running VIA timers
([m6502.asm:6357-6371](../m6502.asm#L6357-L6371)) — a hardware entropy source rather than a
deterministic sequence.

> `RMULZC` and `RADDZC` are declared as **four** bytes each
> ([m6502.asm:6344](../m6502.asm#L6344), [m6502.asm:6348](../m6502.asm#L6348)) but read as five under
> `ADDPRC=1`. See [13-porting-notes.md](13-porting-notes.md#3-defects-in-the-1978-code).

---

## 7. `SIN`, `COS`, `TAN`

[m6502.asm:6400-6539](../m6502.asm#L6400-L6539). Assembled only when `KIMROM=0`.

```
COS:    FADD PI2                    ; cos(x) = sin(x + pi/2)
SIN:    ARG := x
        FDIVF TWOPI                 ; FAC := x / 2pi ; ARISGN := sign(x)
        ARG := x/2pi ; JSR INT
        JSR FSUBT                   ; frac := x/2pi - INT(x/2pi), in [0,1)
        FSUB FR4                    ; u := 1/4 - frac
        push the sign
        BPL SIN1                    ; quadrant I
        JSR FADDH                   ; v := u + 1/2
        BMI SIN2                    ; quadrant IV
        COM TANSGN                  ; quadrants II and III
SIN1:   JSR NEGOP
SIN2:   FADD FR4
        PLA / BPL SIN3 / JSR NEGOP
SIN3:   POLYX SINCON
```

Range reduction is done in **turns**, not radians: the argument is divided by 2*pi, its fractional
part taken, and the quadrant folded so that the result `w` lies in [-1/4, 1/4]. The polynomial then
approximates **sin(2*pi*w)** directly, which is why `SINCON`'s leading coefficient is 2*pi rather
than 1.

Because reduction happens by a single division and `INT`, accuracy degrades for large arguments —
`SIN(1E9)` retains only the precision left after subtracting a 9-digit integer.

```
TAN:    JSR MOV1F                   ; dead - POLYX inside SIN overwrites TEMPF1
        CLR TANSGN
        JSR SIN                     ; FAC := sin(x); TANSGN flipped in quadrants II/III
        TEMPF3 := sin(x)
        MOVFM TEMPF1                ; FAC := w, the reduced argument SIN's POLYX left behind
        FACSGN := 0                 ; |w|
        LDA TANSGN / JSR COSC       ; -> PHA ; JMP SIN1 => poly(1/4 - |w|) = cos(2 pi w)
        LDWDI TEMPF3 / JMP FDIV     ; sin / cos
```

`TAN` **depends on `POLYX` having left `SIN`'s reduced argument in `TEMPF1`** and re-enters the tail
of `SIN` with a quadrant flag pushed by hand. Since cosine is even,
`cos(2*pi*w) = sin(2*pi*(1/4 - |w|))`, with the sign supplied by `TANSGN`.

This is the most tightly coupled routine in the math package: it is correct only because of what two
*other* routines leave in scratch storage.

---

## 8. `ATN`

[m6502.asm:6541-6566](../m6502.asm#L6541-L6566).

```
ATN:    push FACSGN
        BPL ATN1 / JSR NEGOP        ; atan(-x) = -atan(x)
ATN1:   push FACEXP
        CMPI 129 (201) / BCC ATN2   ; |x| < 1 ?
        FDIV FONE                   ; atan(x) = pi/2 - atan(1/x)
ATN2:   POLYX ATNCON                ; odd polynomial on [0,1]
        PLA / CMPI 201 / BCC ATN3
        FSUB PI2
ATN3:   PLA / BPL ATN4 / JMP NEGOP
```

Reduced to [0, 1] by sign folding and reciprocal. Note the post-test re-examines the **saved
exponent byte**, not a separate flag recording that the reciprocal was taken.

---

## 9. Constant tables

All values below are computed from the literal octal bytes in the source using the packed format,
and checked against the ideal they approximate.

### 9.1 General

| Label | Line | Octal bytes (`ADDPRC=1`) | Value | Ideal |
|---|---|---|---|---|
| `FONE` | [5171](../m6502.asm#L5171) | 201 000 000 000 000 | 1.0 | 1 |
| `FHALF` | [5834](../m6502.asm#L5834) | 200 000 000 000 000 | 0.5 | 0.5 |
| `ZERO` | [6011](../m6502.asm#L6011) | 000 … | 0.0 | 0 |
| `TENZC` | [5381](../m6502.asm#L5381) | 204 040 000 000 000 | 10.0 | 10 |
| `SQRHLF` | [5213](../m6502.asm#L5213) | 200 065 004 363 064 | 0.7071067811921239 | 0.7071067811865476 |
| `SQRTWO` | [5218](../m6502.asm#L5218) | 201 065 004 363 064 | 1.4142135623842478 | 1.4142135623730951 |
| `NEGHLF` | [5223](../m6502.asm#L5223) | 200 200 000 000 000 | -0.5 | -0.5 |
| `LOG2` | [5228](../m6502.asm#L5228) | 200 061 162 027 370 | 0.6931471806019545 | 0.6931471805599453 |
| `LOGEB2` | [6165](../m6502.asm#L6165) | 201 070 252 073 051 | 1.4426950407214463 | 1.4426950408889634 |
| `PI2` | [6461](../m6502.asm#L6461) | 201 111 017 332 242 | 1.5707963267341256 | 1.5707963267948966 |
| `TWOPI` | [6466](../m6502.asm#L6466) | 203 111 017 332 242 | 6.2831853069365025 | 6.283185307179586 |
| `FR4` | [6471](../m6502.asm#L6471) | 177 000 000 000 000 | 0.25 | 0.25 |
| `N32768` | [3791](../m6502.asm#L3791) | 144 128 0 0 *(decimal, 4 bytes only)* | -32768 as declared | see the defect note |

`PI2` and `TWOPI` share their mantissa bytes; the third is written `333-ADDPRC`, so it is 0o332 here
and 0o333 with `ADDPRC=0`.

### 9.2 `LOGCN2` — degree byte 3, four coefficients

[m6502.asm:5192](../m6502.asm#L5192).

| Octal bytes | Value |
|---|---|
| 177 136 126 313 171 | 0.43425594188738614 |
| 200 023 233 013 144 | 0.5765845412388444 |
| 200 166 070 223 026 | 0.9618007591925561 |
| 202 070 252 073 040 | 2.8853900730609894 |

The last is 2/ln 2 = 2.8853900817779268 — a minimax rather than Taylor fit, hence the small
deviation.

With `ADDPRC=0` ([m6502.asm:5177](../m6502.asm#L5177)): degree 2, coefficients 0.598974347,
0.961470783, 2.885391235.

### 9.3 `EXPCON` — degree byte 7, eight coefficients

[m6502.asm:6204](../m6502.asm#L6204).

| Octal bytes | Value |
|---|---|
| 161 064 130 076 126 | 2.149876370083348e-05 |
| 164 026 176 263 033 | 0.0001435231403661419 |
| 167 057 356 343 205 | 0.0013422634824564739 |
| 172 035 204 034 052 | 0.009614017013518605 |
| 174 143 131 130 012 | 0.05550512686022557 |
| 176 165 375 347 306 | 0.24022638460155576 |
| 200 061 162 030 020 | 0.6931471861898899 (= ln 2) |
| 201 000 000 000 000 | 1.0 |

With `ADDPRC=0` ([m6502.asm:6172](../m6502.asm#L6172)): degree 6, seven coefficients ending
0.6931470036506653, 1.0.

### 9.4 `SINCON` — degree byte 5, six coefficients

[m6502.asm:6499](../m6502.asm#L6499).

| Octal bytes | Value | Ideal `(2pi)^n/n!` |
|---|---|---|
| 204 346 032 055 033 | -14.381390672177076 | -15.0946 |
| 206 050 007 373 370 | 42.00779712200165 | 42.0587 |
| 207 231 150 211 001 | -76.7041702568531 | -76.7059 |
| 207 043 065 337 341 | 81.605223685503 | 81.6052 |
| 206 245 135 347 050 | -41.34170210361481 | -41.3417 |
| 203 111 017 332 242 | 6.2831853069365025 | 6.2831853 (= 2pi) |

The low-order coefficients match the Taylor series almost exactly; the highest deviates by about 5%,
which is the signature of a minimax fit trading accuracy at the origin for a smaller maximum error
across the interval.

**Two further 5-byte groups follow `SINCON`** (`241 124 106 217 023` and `217 122 103 211 315`).
They are *not* coefficients — they are the Commodore easter-egg data, §10.

With `ADDPRC=0` ([m6502.asm:6476](../m6502.asm#L6476)): degree 4, five coefficients ending
6.2831854820251465.

### 9.5 `ATNCON` — degree byte 11 (`13`), twelve coefficients

[m6502.asm:6608](../m6502.asm#L6608).

| Octal bytes | Value |
|---|---|
| 166 263 203 275 323 | -0.0006847939118870272 |
| 171 036 364 246 365 | 0.004850942155826488 |
| 173 203 374 260 020 | -0.016111701843328774 |
| 174 014 037 147 312 | 0.03420963804819621 |
| 174 336 123 313 301 | -0.054279132760711946 |
| 175 024 144 160 114 | 0.07245719654019922 |
| 175 267 352 121 172 | -0.08980239537777379 |
| 175 143 060 210 176 | 0.11093241343041882 |
| 176 222 104 231 072 | -0.1428398076677695 |
| 176 114 314 221 307 | 0.19999912049388513 |
| 177 252 252 252 023 | -0.3333333156770095 |
| 201 000 000 000 000 | 1.0 |

The last three converge on the arctangent series -1/3, 1/5, 1.

With `ADDPRC=0` ([m6502.asm:6569](../m6502.asm#L6569)): degree 8, nine coefficients ending
-0.333330720663, 1.0.

### 9.6 `FOUT` bounds

| Label | Octal bytes | Value |
|---|---|---|
| `NZ0999` | 233 076 274 037 375 | 99999999.90625 |
| `NZ9999` | 236 156 153 047 375 | 999999999.25 |
| `NZMIL` | 236 156 153 050 000 | 1000000000.0 |

The source comments call these 99999999.9499 and 999999999.499; those are approximations, and the
stored values are as computed above.

### 9.7 `FOUTBL`

[m6502.asm:6037-6072](../m6502.asm#L6037-L6072).

Nine entries of four bytes, big-endian two's-complement integers, terminated by `FDCEND`:

| Octal bytes | Value |
|---|---|
| 372 012 037 000 | -100000000 |
| 000 230 226 200 | 10000000 |
| 377 360 275 300 | -1000000 |
| 000 001 206 240 | 100000 |
| 377 377 330 360 | -10000 |
| 000 000 003 350 | 1000 |
| 377 377 377 234 | -100 |
| 000 000 000 012 | 10 |
| 377 377 377 377 | -1 |

With `ADDPRC=0` ([m6502.asm:6017-6034](../m6502.asm#L6017-L6034)): six entries of three bytes,
-100000 through -1.

When `TIME=1` a second table of -2160000, 216000, -36000, 3600, -600, 60 is appended
([m6502.asm:6074-6099](../m6502.asm#L6074-L6099)) for the Commodore's time-of-day converter — the same
digit generator, driven by a base-60 table.

### 9.8 `RND` constants

| Label | Declared bytes | As 4 bytes | As actually read (5 bytes, `ADDPRC=1`) |
|---|---|---|---|
| `RMULZC` | 230 065 104 172 | 11879546.0 | 11879546.40625 |
| `RADDZC` | 150 050 261 106 | 3.927677738602142e-08 | 3.927677783011063e-08 |
| `RNDX` seed | 128 79 199 82 89 (decimal) | — | 0.8116351573262364 |

The fifth byte read for `RMULZC` is `RADDZC`'s first byte; the fifth for `RADDZC` is the `JSR`
opcode (32) that begins `RND`.

The ROM template of the seed at [m6502.asm:6698](../m6502.asm#L6698) ends in **88**, while the RAM
declaration at [m6502.asm:982](../m6502.asm#L982) ends in **89** — and `INIT` copies only the first
four bytes anyway.

---

## 10. The Commodore easter egg

Under `REALIO=3`, `POKE`ing address 0x9166 triggers `ZSTORD`/`MRCHKR`
([m6502.asm:4913-4939](../m6502.asm#L4913-L4939)), which reads ten bytes from `SINCON+0o36` — the two
extra 5-byte groups appended to the coefficient table — masks each with 0o77, and writes them
repeatedly to $8000, spelling `MICROSOFT!` in screen codes.

This is why `SINCON` appears to have two coefficients too many, and it is a caution against
assuming that data adjacent to a table belongs to it.
