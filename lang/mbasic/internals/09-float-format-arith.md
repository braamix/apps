# 09 — Floating-point format and arithmetic

**Radix warning.** This whole subsystem lies between [m6502.asm:4847](../../m6502.asm#L4847)
(`RADIX 8 ;!!!! ALERT !!!!`) and [m6502.asm:6671](../../m6502.asm#L6671). Every bare numeral in the source
here is **octal**. This document gives decimal with the source's octal form in parentheses.

The authoritative comment block is [m6502.asm:4849-4896](../../m6502.asm#L4849-L4896). It describes the
24-bit (`ADDPRC=0`) form; this build has `ADDPRC=1` and 32 bits.

---

## 1. The number format

### 1.1 Packed — 5 bytes (`4+ADDPRC`)

```
byte 0   exponent, excess-128 (excess-200 octal); 0 means the value is zero
byte 1   bit 7 = sign; bits 6..0 = mantissa bits 2..8
byte 2   mantissa bits 9..16
byte 3   mantissa bits 17..24
byte 4   mantissa bits 25..32
```

The mantissa is big-endian, normalised to the range [0.5, 1), with **an implied leading 1 occupying
the same bit position as the sign**. That is the whole trick: the leading bit of a normalised
mantissa is always 1, so it need not be stored, and the freed bit holds the sign.

```
value = (-1)^sign * (0x80 | byte1 : byte2 : byte3 : byte4) * 2^(exponent-128) / 2^32
```

| Property | Value |
|---|---|
| Mantissa precision | 32 bits (24 with `ADDPRC=0`) — about 9 significant decimal digits |
| Exponent range | 1..255 ⇒ 2^-127 .. 2^127, roughly 2.9e-39 .. 1.7e38 |
| Zero | exponent byte 0; **the other bytes are undefined** ([m6502.asm:4861](../../m6502.asm#L4861)) |
| Negative zero | does not exist |
| Infinity, NaN, denormals | do not exist |
| Overflow | `?OV ERROR` |
| Underflow | silently becomes zero |

Scaling rule ([m6502.asm:4862-4864](../../m6502.asm#L4862-L4864)): shifting the mantissa right increments
the exponent, shifting left decrements it.

This format was independently verified by decoding every constant table in the source and comparing
against the mathematical values they approximate; agreement is to the limit of a 32-bit mantissa.
See [10-math-functions.md](10-math-functions.md#9-constant-tables).

### 1.2 Unpacked — the FAC

```
FACEXP  160   exponent, same encoding as packed
FACHO   161   mantissa MSB, bit 7 forced to 1 (the hidden bit made explicit)
FACMOH  162   ADDPRC only
FACMO   163
FACLO   164
FACSGN  165   sign; only bit 7 is ever tested
SGNFLG  166
BITS    167   the fill byte SHIFTR shifts in
ARGEXP  168 ] the second operand register: EXP, HO, MOH, MO, LO, SGN
   ...  173 ]
ARISGN  174   sign of the result = FACSGN XOR ARGSGN
FACOV   175   the FAC's guard byte
```

Two layout facts a port must not tidy up:

- **`FACOV` is not adjacent to the FAC.** `ARISGN` sits between `FACSGN` and `FACOV`. Code that
  shifts the mantissa and the guard byte together names them separately.
- **`ARG` has no guard byte.** When ARG is the operand being shifted, `FACOV` is borrowed as its
  shift-out byte and `OLDOV` holds the FAC's real one.

The source explains the reason for having two forms at
[m6502.asm:4891-4896](../../m6502.asm#L4891-L4896): "THIS IS DONE FOR SPEED OF OPERATION". Unpacking once
on load and packing once on store avoids masking the hidden bit on every arithmetic step.

> `FACSGN` is not reliably 0 or 255. `FOUTC` stores `' '` or `'-'` in it
> ([m6502.asm:5858](../../m6502.asm#L5858)); `FPWRT` stores 3 or 4 ([m6502.asm:6134](../../m6502.asm#L6134)).
> Every reader uses `BIT`, `BPL`, `BMI` or `ROL`. Model it as a byte whose bit 7 is the sign.

### 1.3 Relative-addressing constraints

Several routines take a *base address* in `X` and index off it, so these runs must stay contiguous
and in order:

| Run | Indexed by |
|---|---|
| `FAC+1`..`FAC+4` | `SHIFTR`, `QINT1`, `FADD3` as `1,X`..`4,X` with `X = FAC` |
| `ARGEXP+1`..`ARGEXP+4` | the same, with `X = ARGEXP` |
| `RESHO`, `RESMOH`, `RESMO`, `RESLO` | `MULSHF` as `1,X`..`4,X` ([m6502.asm:5092](../../m6502.asm#L5092)); `DIVIDE` as `RESLO,X` with `X` from -3 to +1 |
| `FACHO`..`FACLO` | `NORM3`'s byte-shift reads `FACHO+1`, `FACMOH+1`, `FACMO+1` |

---

## 2. Calling conventions

From [m6502.asm:4873-4890](../../m6502.asm#L4873-L4890):

- One-argument functions: argument in the FAC, result in the FAC.
- Two-argument operations: **first operand in ARG, second in the FAC**, result in the FAC.
- A `…T` entry assumes both registers are already loaded. The non-`T` entry takes a pointer to a
  packed value in `(A = low, Y = high)` and calls `CONUPK` to unpack it into ARG.
- Pushing a FAC onto the stack goes SGN, LO, MO, HO, EXP.

### 2.1 Operand order is not symmetric

| Entry | Line | Semantics |
|---|---|---|
| `FADD` / `FADDT` | [4944](../../m6502.asm#L4944) / [4945](../../m6502.asm#L4945) | FAC := ARG + FAC |
| `FADDH` | [4900](../../m6502.asm#L4900) | FAC := FAC + 0.5 |
| `FSUB` / `FSUBT` | [4902](../../m6502.asm#L4902) / [4903](../../m6502.asm#L4903) | **FAC := ARG − FAC**, i.e. *memory minus FAC* |
| `FMULT` / `FMULTT` | [5264](../../m6502.asm#L5264) / [5265](../../m6502.asm#L5265) | FAC := ARG × FAC |
| `FDIV` / `FDIVT` | [5392](../../m6502.asm#L5392) / [5393](../../m6502.asm#L5393) | **FAC := ARG ÷ FAC**, i.e. *memory divided by FAC* |
| `FCOMP` | [5602](../../m6502.asm#L5602) | compare memory against FAC, result in `A` |
| `FPWRT` | [6120](../../m6502.asm#L6120) | FAC := ARG ^ FAC |
| `MUL10` / `DIV10` | [5367](../../m6502.asm#L5367) / [5386](../../m6502.asm#L5386) | FAC := FAC × 10 / FAC ÷ 10 |

The reversed sense of `FSUB` and `FDIV` is easy to misread. It is confirmed by `DIV10`
([m6502.asm:5386-5391](../../m6502.asm#L5386-L5391)), which puts *x* into ARG and 10.0 into the FAC before
calling `FDIVT` to get *x*/10.

### 2.2 Movement routines

[m6502.asm:5463-5557](../../m6502.asm#L5463-L5557).

| Routine | Pointer in | Action |
|---|---|---|
| `CONUPK` | `(A,Y)` | packed memory → ARG; also sets `ARISGN = ARGSGN XOR FACSGN`; ends with `LDA FACEXP` so the caller's `JEQ` sees the FAC's zero-ness |
| `MOVFM` | `(A,Y)` | packed memory → FAC; `FACHO |= 0x80`; `FACSGN` = the packed byte; `FACOV = 0` |
| `MOVMF` | `(X,Y)` | `ROUND`, then FAC → packed memory, folding `FACSGN` bit 7 into `FACHO` |
| `MOVVF` | — | `LDXY FORPNT` then `MOVMF` |
| `MOV1F` / `MOV2F` | — | FAC → `TEMPF1` / `TEMPF2` |
| `MOVFA` | — | ARG → FAC |
| `MOVAF` | — | `ROUND`, then FAC → ARG; returns `A = FACEXP` |
| `MOVEF` | — | FAC → ARG **without rounding** (used by `EXP`) |
| `MOVFR` | — | RES → FAC, then `NORMAL` |
| `ROUND` | — | if `FACEXP != 0` and `FACOV` bit 7 is set, increment the mantissa |

Note `MOVFM` takes its pointer in `(A,Y)` but `MOVMF` takes it in `(X,Y)`.

---

## 3. Normalisation, shifting, rounding

### 3.1 `NORMAL`

[m6502.asm:5003-5065](../../m6502.asm#L5003-L5065).

```
NORMAL: A = 0 (shift count), C = 0
NORM3:  LDX FACHO / BNE NORM1        ; top byte non-zero => go bit by bit
        shift the mantissa left one WHOLE BYTE, pulling FACOV in, zeroing FACOV
        count += 8
        if all mantissa bytes were zero -> ZEROFC
NORM2:  count += 1
        ASL FACOV / ROL FACLO / ROL FACMO / ROL FACMOH / ROL FACHO
NORM1:  BPL NORM2                    ; until FACHO bit 7 = 1
        FACEXP -= count, underflow -> ZEROFC
SQUEEZ: BCC RNDRTS                   ; always taken from NORMAL
RNDSHF: INC FACEXP / BEQ OVERR
        rotate the whole mantissa and FACOV right one bit
RNDRTS: RTS
```

Byte-at-a-time first, then bit-at-a-time — a cheap normalisation for the common case of a large
cancellation.

`NORMAL` never rounds. `FACOV` is only folded in by `ROUND`, which is called from `MOVMF`, `MOVAF`
and `FDIVT`. So intermediate results carry a guard byte and only rounding-on-store loses it.

`ZEROFC` / `ZEROF1` / `ZEROML` ([m6502.asm:5021-5023](../../m6502.asm#L5021-L5023)) are three entry points
into "make the FAC zero", differing in what they preserve.

### 3.2 `SHIFTR` / `MULSHF` / `ROLSHF`

[m6502.asm:5089-5163](../../m6502.asm#L5089-L5163). Shifts the four mantissa bytes at `[X+1..X+4]` plus
the accumulator right by `-A` bits, using `BITS` as the fill during whole-byte steps.

```
SHFTR3: ASL 1,X / BCC SHFTR4 / INC 1,X
SHFTR4: ROR 1,X / ROR 1,X         ; YES, TWO OF THEM
ROLSHF: ROR 2,X / ROR 3,X / ROR 4,X
        ROR A
```

The `ASL / INC / ROR / ROR` sequence is a **sign-preserving** right shift of the high byte — it
captures bit 7, then restores it. That is normally harmless (the first bit shift in `FADD` is an
`LSR`, so bit 7 is already 0) and is exactly what `QINT` needs when it sets `BITS = 255` to get sign
extension for negative values.

**`ROLSHF` is not a subroutine.** It is an entry point in the middle of `SHIFTR`'s loop; `JSR ROLSHF`
runs the loop `-Y` times and exits through `SHFTRT`. `MULSHF` likewise enters at `SHFTR2`. A
reimplementation that turns these into functions has to reproduce the shared loop counter.

With `RORSW=0` (early 6502s whose `ROR` was broken) an emulation is assembled instead
([m6502.asm:5117-5124](../../m6502.asm#L5117-L5124), [m6502.asm:108-115](../../m6502.asm#L108-L115)).

---

## 4. Addition and subtraction

[m6502.asm:4899-5163](../../m6502.asm#L4899-L5163).

### 4.1 Alignment

```
FADD:   JSR CONUPK
FADDT:  JEQ MOVFA               ; FAC == 0 => the answer is ARG
        LDX FACOV / STX OLDOV   ; save the FAC's guard byte
        LDXI ARGEXP             ; X = the operand to be shifted (default: ARG)
        LDA ARGEXP
FADDC:  TAY / BEQ ZERRTS        ; ARG == 0 => the answer is the FAC
        SEC / SBC FACEXP
        BEQ FADD4               ; equal exponents, no shift needed
        BCC FADDA               ; ARG smaller: shift ARG right
        ; ARG bigger:
        result exponent := ARGEXP ; result sign := ARGSGN
        A := -(difference) ; OLDOV := 0 ; X := FAC   (shift the FAC instead)
FADD1:  CMPI 249 (^D256-7)      ; shift more than 7 bits?
        BMI FADD5               ; yes: JSR SHIFTR, byte-wise
        ...bit-wise shift...
FADD4:  BIT ARISGN / BPL FADD2  ; same signs => add
```

The smaller operand is shifted right by the exponent difference, with the bits shifted out
accumulating in `A` as the new guard byte. The constant 249 also "ALLOWS SHIFTING OF NEG NUMS BY
QINT" ([m6502.asm:4967-4970](../../m6502.asm#L4967-L4970)).

### 4.2 Adding

`FADD2` ([m6502.asm:5025-5040](../../m6502.asm#L5025-L5040)): `ADC OLDOV` to combine the guard bytes, then
a four-byte `ADC` chain, then `SQUEEZ` — which shifts right and increments the exponent if a carry
came out of the top.

> **On the equal-exponent path, carry is set when `ADC OLDOV` executes.** `BEQ FADD4` is reached
> from a `SBC` that produced zero, so `C=1`, and the guard byte becomes `OLDOV+1`. On the shifted
> paths carry is clear, because both `SHFTRT` and the `ROLSHF` loop end with `CLC`. A guard byte of
> 255 therefore carries into `FACLO` when the exponents happen to match exactly. See
> [13-porting-notes.md](13-porting-notes.md#3-defects-in-the-1978-code).

### 4.3 Subtracting

`FADD3` ([m6502.asm:4979-4999](../../m6502.asm#L4979-L4999)): `X` still holds the base address of whichever
operand was shifted (the smaller one), and `Y` is set to the other. The four bytes are subtracted
`[Y+n] - [X+n]`, the guard byte is `OLDOV` minus the shifted-out bits, and then:

```
FADFLT: BCS NORMAL           ; result is non-negative
        JSR NEGFAC           ; otherwise negate the mantissa AND flip FACSGN
        (fall into NORMAL)
```

So subtraction always produces a magnitude and fixes the sign afterwards, rather than deciding the
order in advance.

---

## 5. Multiplication

[m6502.asm:5262-5314](../../m6502.asm#L5262-L5314).

```
FMULT:  JSR CONUPK
FMULTT: JEQ MULTRT              ; FAC == 0 => the result is already 0
        JSR MULDIV              ; exponent and sign
        RESHO = RESMOH = RESMO = RESLO = 0
        LDA FACOV  / JSR MLTPLY ; multiply ARG by the guard byte first
        LDA FACLO  / JSR MLTPLY
        LDA FACMO  / JSR MLTPLY
        LDA FACMOH / JSR MLTPLY
        LDA FACHO  / JSR MLTPL1
        JMP MOVFR               ; RES -> FAC, then NORMAL
```

Multiplier bytes are consumed **least significant first, including `FACOV`**, and each `MLTPLY`
shifts the running product right eight bits — so the final `FACOV` holds the product's guard byte
for free.

```
MLTPLY: JEQ MULSHF              ; a zero multiplier byte: just shift RES right 8
MLTPL1: LSR A / ORAI 128 (200)  ; bit 0 into carry; a sentinel into bit 7
MLTPL2: TAY
        BCC MLTPL3
        RES += ARG              ; four-byte ADC chain
MLTPL3: ROR RESHO / ROR RESMOH / ROR RESMO / ROR RESLO / ROR FACOV
        TYA / LSR A
        BNE MLTPL2              ; SLOW AS A TURTLE !
```

The `ORAI 128` sentinel guarantees exactly eight iterations: `A` only becomes zero after the eighth
`LSR` has pushed the sentinel out. `MLTPL1` is the entry that skips the zero check, so a zero *high*
byte still shifts correctly.

The author's own comment on the inner loop is `;SLOW AS A TURTLE !`.

### 5.1 `MULDIV` — exponent arithmetic

[m6502.asm:5342-5364](../../m6502.asm#L5342-L5364).

```
MULDIV: LDA ARGEXP
MLDEXP: BEQ ZEREMV               ; ARG == 0 => result 0
        CLC / ADC FACEXP         ; sum of two excess-128 exponents
        BCC TRYOFF               ; the test is C XOR N
        BMI GOOVER               ; C=1, N=1 => sum >= 384 => overflow
        CLC / SKIP2
TRYOFF: BPL ZEREMV               ; C=0, N=0 => sum < 128 => underflow
        ADCI 128 (200)           ; re-bias
        STA FACEXP
        JEQ ZEROML
        LDA ARISGN / STA FACSGN
        RTS
ZEREMV: PLA / PLA / JMP ZEROFC    ; DISCARDS THE CALLER'S RETURN ADDRESS
```

The validity test is `C XOR N` on the raw 8-bit sum: legal only for C=1,N=0 (sum 256..383) or
C=0,N=1 (sum 128..255).

**`ZEREMV` pops the caller's return address**, so `MULDIV` can return two levels up. `EXP` therefore
has to call `MLDEXP` with a `JSR` and immediately `RTS` — the source notes "HAS TO DO JSR DUE TO
PULAS IN MULDIV" ([m6502.asm:6283](../../m6502.asm#L6283)).

### 5.2 `MUL10` and `DIV10`

[m6502.asm:5366-5391](../../m6502.asm#L5366-L5391). `MUL10` computes 5x then doubles it, by lying to
`FADDC` about ARG's exponent:

```
MUL10:  JSR MOVAF        ; ARG := FAC (rounded); A := FACEXP
        CLC / ADCI 2     ; pretend ARG's exponent is 2 greater => ARG counts as 4x
        JSR FADDC        ; 4x + x = 5x
        INC FACEXP       ; x2 => 10x
```

`DIV10` is a plain `FDIVT` against the constant 10.0.

These two are the scaling primitives for both `FIN` and `FOUT`, which is why decimal conversion in
this BASIC is iterative rather than table-driven.

---

## 6. Division

[m6502.asm:5392-5461](../../m6502.asm#L5392-L5461). Non-restoring, MSB first.

```
FDIV:   JSR CONUPK
FDIVT:  BEQ DV0ERR                ; FAC == 0 => ?/0 ERROR
        JSR ROUND                 ; fold the divisor's guard byte in
        FACEXP := -FACEXP
        JSR MULDIV                ; exponent and sign
        INC FACEXP / BEQ GOOVER
        LDXI -4 (^D256-3-ADDPRC)  ; result byte index
        LDAI 1                    ; quotient accumulator, with a sentinel
DIVIDE: compare ARG against FAC, byte by byte, MSB first
SAVQUO: PHP                       ; save the comparison's carry
        ROL A                     ; shift that carry into the quotient
        BCC QSHFT                 ; sentinel not out yet
        INX / STA RESLO,X         ; a byte is complete
        ...
QSHFT:  PLP
        BCS DIVSUB                ; ARG >= FAC: subtract
SHFARG: shift ARG left one bit
        ...
DIVSUB: ARG -= FAC ; JMP SHFARG
DIVNRM: REPEAT 6,<ASL A> / STA FACOV    ; the last two bits become the guard byte
```

The `PHP`/`PLP` pair preserves the comparison's carry across the `ROL A` that consumes it. The
source's comment at [m6502.asm:5405](../../m6502.asm#L5405) is
`;THIS IS THE BEST CODE IN THE WHOLE PILE`.

The quotient is four bytes plus two guard bits. The final `STA RESLO,X` with `X=1` writes into the
spare byte declared at [m6502.asm:831](../../m6502.asm#L831); its content is discarded, and the source
acknowledges the cost at [m6502.asm:5424](../../m6502.asm#L5424) — "NOTE THIS REQ 1 MO RAM THEN NECESS".

Division by zero is detected at entry and raises `?/0` immediately, because there would be nowhere
to put a result.

---

## 7. Comparison

`FCOMP` [m6502.asm:5598-5637](../../m6502.asm#L5598-L5637). Convention stated at
[m6502.asm:5599-5601](../../m6502.asm#L5599-L5601):

```
A =  1  if ARG < FAC       (memory operand less than the FAC)
A =  0  if equal
A = -1  if ARG > FAC
```

The pointer to the packed operand arrives in `(A, Y)`; `FCOMPN` allows the low byte to be preloaded.

```
FCOMP:  exponent byte == 0?  -> answer is SIGN(FAC)
        signs differ?        -> answer is the sign of the FAC
        compare EXP, HO, MOH, MO
        LDAI 127 / CMP FACOV      ; C=0 iff FACOV >= 0x80
        LDADY INDEX2 / SBC FACLO  ; so the guard byte counts as half an LSB
        BEQ QINTRT
```

It is a big-endian unsigned magnitude comparison, **including the FAC's guard byte** — so two values
that differ only in `FACOV` compare unequal. That is why `NEXT` can use `FCOMPN` for the loop test
and get a result consistent with what `FADD` just produced.

`SIGN` / `FCSIGN` / `FCOMPS` ([m6502.asm:5562-5569](../../m6502.asm#L5562-L5569)) return 0, +1 or -1;
`SGN` is `JSR SIGN` falling into `FLOAT`. `ABS` is a single instruction, `LSR FACSGN`.

---

## 8. Integer conversion

### 8.1 `QINT`

[m6502.asm:5641-5670](../../m6502.asm#L5641-L5670). "QUICK GREATEST INTEGER FUNCTION."

```
QINT:   LDA FACEXP / BEQ CLRFAC
        SEC / SBCI 160 (8*ADDPRC+230)   ; target exponent
        BIT FACSGN / BPL QISHFT
        TAX / LDAI 255 / STA BITS       ; sign-extension fill
        JSR NEGFCH                      ; two's-complement the mantissa first
        TXA
QISHFT: LDXI FAC
        ...shift right by the difference...
```

Target exponent 160 (`^O240`) means `160-128 = 32`, so the mantissa becomes a right-justified 32-bit
integer in `FACHO`..`FACLO`.

**`QINT` is a true floor, not a truncation.** Negatives are two's-complemented *before* the shift
and the shift is arithmetic, so `INT(-2.5)` is -3. `FACEXP` and `FACSGN` are left unchanged; `Y`
returns 0.

### 8.2 `INT`

[m6502.asm:5672-5692](../../m6502.asm#L5672-L5692). Values already integral (exponent ≥ 160) are returned
untouched. Otherwise `QINT`, then rebuild a float with exponent 160 and re-normalise. Side effect:
`INTEGR` = the result's low byte, which `^` and `EXP` both read.

### 8.3 `AYINT`

See [06-expressions.md](06-expressions.md#61-ayint), including the `N32768` defect.

### 8.4 Floating an integer

[m6502.asm:4124-4132](../../m6502.asm#L4124-L4132), [m6502.asm:5574-5591](../../m6502.asm#L5574-L5591).

| Routine | Input | Notes |
|---|---|---|
| `GIVAYF` | `A` = **high** byte, `Y` = **low** byte of a signed 16-bit | exponent 144 (`^O220`) |
| `SNGFLT` | `Y` = an unsigned byte | `A=0`, then `GIVAYF` |
| `FLOAT` | `A` = a **signed** byte | exponent 136 (`^O210`) |
| `FLOATS` | — | derives the sign from `FACHO` |
| `FLOATC` | — | entered with carry preset, so the caller controls signedness |

`FLOATC` with `SEC` is how `LINPRT` prints line numbers and the free-byte count as **unsigned**
0..65535 ([m6502.asm:5845-5848](../../m6502.asm#L5845-L5848)) even though the same code path normally
treats 16-bit values as signed.

---

## 9. Error exits

| Routine | Line | Error |
|---|---|---|
| `OVERR` | [5086](../../m6502.asm#L5086) | `?OV` — raised on exponent overflow in `MULDIV`, `RNDSHF`, `MUL10`, `FIN` |
| `DV0ERR` | [5460](../../m6502.asm#L5460) | `?/0` — division by zero |
| `FCERR` | — | `?FC` — domain errors in `LOG`, `SQR`, `AYINT`, `GETADR` |

There is no underflow error: a result too small silently becomes zero, in `NORMAL` and in `MULDIV`.
