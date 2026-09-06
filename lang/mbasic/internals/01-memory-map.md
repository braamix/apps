# 01 — Memory map

Addresses are decimal absolute page-zero addresses for the checked-in Apple configuration
(`REALIO=4`, `INTPRC=1`, `ADDPRC=1`, `ROMSW=1`, `EXTIO=0`, `NULCMD=0`, `BUFPAG=2`). The declaration
block is [m6502.asm:730-941](../m6502.asm#L730-L941), which begins at `ORG 0`
([m6502.asm:7](../m6502.asm#L7)).

Page zero is not merely "fast storage" here. It is where every mutable interpreter variable lives,
and the source states the constraint plainly at [m6502.asm:747-751](../m6502.asm#L747-L751): this area
is *volatile*, none of it can live in ROM, and any constant in it has to be written by `INIT` at
startup. In a ROM build the ROM holds only code and constant tables; all state is these 256 bytes
plus the four dynamic regions above.

---

## 1. Page zero, cell by cell

### 1.1 Vectors (0-12)

[m6502.asm:731-745](../m6502.asm#L731-L745).

Present only when `REALIO≠3`; the Commodore build has none of these because its equivalents live in
the machine's own ROM.

| Addr | Label | Size | Purpose |
|---|---|---|---|
| 0-2 | `START` | 3 | `JMP INIT`. Rewritten by `INIT` to `JMP READY` at the very end of startup ([m6502.asm:6902-6905](../m6502.asm#L6902-L6905)). Restarting the machine at location 0 during a program therefore lands in `READY`, not in `INIT` — restarting `INIT` a second time is impossible by design |
| 3-5 | `RDYJSR` | 3 | `JMP INIT`, patched by `INIT` to `JMP STROUT`. `READY` reaches `STROUT` through it ([m6502.asm:1553](../m6502.asm#L1553)) so that an error raised *during* `INIT` re-enters `INIT` instead of printing |
| 6-7 | `ADRAYI` | 2 | Address of `AYINT` (FAC → signed 16-bit in Y,A), published for external callers |
| 8-9 | `ADRGAY` | 2 | Address of `GIVAYF` (Y,A → FAC) |
| 10-12 | `USRPOK` | 3 | `IFN ROMSW`. `JMP FCERR`, patched by the user via `POKE` to point at a machine-language routine. `FUNDSP`'s `USRLOC` entry points here ([m6502.asm:1059-1060](../m6502.asm#L1059-L1060)), which is how a ROM build can still offer `USR` |

### 1.2 General scratch and flags (13-23)

[m6502.asm:752-788](../m6502.asm#L752-L788).

| Addr | Label | Size | Purpose |
|---|---|---|---|
| 13 | `CHARAC` | 1 | A delimiting character. Used as the primary terminator by `STRLIT`, by `DATAN`/`REMN` ([m6502.asm:2445-2454](../m6502.asm#L2445-L2454)), and as a digit scratch by `LINGET` ([m6502.asm:2513](../m6502.asm#L2513)) |
| 13 | `INTEGR` | — | *Alias of `CHARAC`.* The one-byte integer left by `QINT`, read by `INT` and by `^` |
| 14 | `ENDCHR` | 1 | The second delimiting character. `CRUNCH` stores every candidate character here so that a quote terminates its own literal copy ([m6502.asm:1797](../m6502.asm#L1797)); `DATAN`/`REMN` swap it with `CHARAC` at each `"` |
| 15 | `COUNT` | 1 | General counter. Holds the crunched line length in `MAIN1` ([m6502.asm:1569](../m6502.asm#L1569)), the reserved-word index in `CRUNCH`, the subscript count in `ISARY` |
| 16 | `DIMFLG` | 1 | Non-zero ⇒ `PTRGET` was called from `DIM`. **The source requires `DIMFLG` and `VALTYP` to be consecutive** ([m6502.asm:762-763](../m6502.asm#L762-L763)) |
| 16 | `KIMY` | — | *Alias of `DIMFLG`.* Y is preserved here during `OUTDO` on `REALIO=1` |
| 17 | `VALTYP` | 1 | Type indicator: 0 = numeric, 255 = string. Tested with `BIT VALTYP` so the N flag carries the answer ([m6502.asm:3173](../m6502.asm#L3173)). The comment says "1=STRING" but the code uses 255 |
| 18 | `INTFLG` | 1 | `IFN INTPRC`. 128 ⇒ the variable is an integer (`%`) |
| 19 | `DORES` | 1 | `CRUNCH`'s "may I crunch?" flag. Tested with `BIT DORES / BVS`, so **only bit 6 matters** ([m6502.asm:1801-1802](../m6502.asm#L1801-L1802)). On `REALIO=3` it doubles as `LIST`'s quote-mode flag |
| 19 | `GARBFL` | — | *Alias of `DORES`.* "Garbage collection already attempted" — `LSR GARBFL` at `GETSPA`, `BMI` at `GARBAG`, so **only bit 7 matters**. The two uses are disjoint in time |
| 20 | `SUBFLG` | 1 | 128 ⇒ subscripted variables and `%` integers are not allowed here. Set by `FOR` and `DEF FN`, cleared by `STKINI` and by every successful non-array `PTRGET` |
| 21 | `INPFLG` | 1 | Which of `INPUT` / `READ` / `GET` is running. Also borrowed by the KIM `SAVE` code to stash the stack pointer |
| 22 | `TANSGN` | 1 | Sign of the tangent |
| 22 | `DOMASK` | — | *Alias of `TANSGN`.* The relational-operator bit mask currently being applied ([m6502.asm:3303](../m6502.asm#L3303)) |
| 23 | `CNTWFL` | 1 | `IFN REALIO`. Output suppression. **Bit 7 only**: `OUTDO` does `BIT CNTWFL / BMI OUTRTS`. Toggled by typing ^O, cleared by `ERROR` and `READY` |

### 1.3 The Apple hole (24-79)

[m6502.asm:790](../m6502.asm#L790) is `IFE REALIO-4,<ORG 80>`, leaving 24-79 untouched for the Apple II
monitor. Other targets continue packing from 24.

### 1.4 Terminal handling and line-number staging (80-84)

[m6502.asm:791-815](../m6502.asm#L791-L815).

| Addr | Label | Size | Purpose |
|---|---|---|---|
| — | `CHANNL` | 1 | `IFN EXTIO` only. Absent here |
| — | `NULCNT` | 1 | `IFN NULCMD` only. Absent here |
| 80 | `TRMPOS` | 1 | Terminal carriage position. `IFN REALIO-3`; the Commodore uses an external location instead ([m6502.asm:75](../m6502.asm#L75)) |
| 81 | `LINWID` | 1 | Line width. Preloaded with `LINLEN`; rewritten by `INIT` and by the `TERMINAL WIDTH?` answer |
| 82 | `NCMWID` | 1 | Column beyond which there are no more comma fields. Preloaded with `NCMPOS` |
| 83-84 | `LINNUM` | 2 | Binary line number staging area. Declared as `0` followed by a literal `44` — a comma — at `LINNUM+1` ([m6502.asm:801-806](../m6502.asm#L801-L806)) |
| 83 | `POKER` | — | *Alias of `LINNUM`.* The `POKE`/`PEEK`/`WAIT` address, and an `INPUT`/`READ` temporary |
| — | `BUF` | `BUFLEN` | **`IFE BUFPAG` only.** Here `BUFPAG=2`, so the input buffer is at 512, not in page zero |

The literal comma at `LINNUM+1` is not decoration. `INPUT`'s data pointer always begins on a comma
or a terminator, so a direct `INPUT` can point at this byte and find one.

### 1.5 Temporaries (85-105)

[m6502.asm:817-831](../m6502.asm#L817-L831).

| Addr | Label | Size | Purpose |
|---|---|---|---|
| 85 | `TEMPPT` | 1 | Page-zero address of the first *free* string temporary. Initialised to `TEMPST` |
| 86-87 | `LASTPT` | 2 | Pointer to the last *used* string temporary. Only the low byte is ever written (`STX LASTPT`); `LASTPT+1` is zeroed once by `INIT` ([m6502.asm:6744](../m6502.asm#L6744)) so the 16-bit compare in `FRETMS` works |
| 88-96 | `TEMPST` | 9 | `NUMTMP`=3 descriptors of `STRSIZ`=3 bytes |
| 97-98 | `INDEX1` | 2 | General indirect pointer. `INDEX` is an alias for it |
| 99-100 | `INDEX2` | 2 | Second indirect pointer |
| 101 | `RESHO` | 1 | High byte of the multiply/divide result |
| 102 | `RESMOH` | 1 | `IFN ADDPRC` |
| 103 | `RESMO` | 1 | |
| 103 | `ADDEND` | — | *Alias of `RESMO`.* `UMULT`'s multiplicand |
| 104 | `RESLO` | 1 | |
| 105 | — | 1 | Unnamed overflow byte for `RES`. `DIVIDE` writes a discarded byte here; the source notes it costs one byte of RAM more than strictly necessary ([m6502.asm:5424](../m6502.asm#L5424)) |

### 1.6 The dynamic-storage pointers (106-119)

[m6502.asm:833-853](../m6502.asm#L833-L853).

These seven pointers *are* BASIC's memory manager.

| Addr | Label | Meaning | Who changes it |
|---|---|---|---|
| 106-107 | `TXTTAB` | Start of program text | `INIT` only, then never |
| 108-109 | `VARTAB` | Start of simple variables = end of program | every line insert/delete; `SCRTCH` sets it to `TXTTAB+2` |
| 110-111 | `ARYTAB` | Start of the array table | `+7` per new simple variable; `CLEARC` sets it to `VARTAB` |
| 112-113 | `STREND` | End of storage in use, start of free space | `BLTU`; array creation; `CLEARC` |
| 114-115 | `FRETOP` | Bottom of the in-use string space | `GETSPA` (downward), `FRETMP` (upward), `CLEARC`/GC reset it to `MEMSIZ` |
| 116-117 | `FRESPC` | Pointer to the string just allocated | `GETSPA` |
| 118-119 | `MEMSIZ` | Highest usable location + 1 | `INIT` only |

The ordering invariant is
`TXTTAB ≤ VARTAB ≤ ARYTAB ≤ STREND … FRETOP ≤ MEMSIZ`, and `STREND` colliding with `FRETOP` is the
out-of-memory condition.

### 1.7 Interpreter position (120-131)

[m6502.asm:855-869](../m6502.asm#L855-L869).

| Addr | Label | Purpose |
|---|---|---|
| 120-121 | `CURLIN` | Current line number. **`CURLIN+1 = 255` means direct mode** — the test is `LDY CURLIN+1 / INY / BEQ` ([m6502.asm:1544-1546](../m6502.asm#L1544-L1546)) |
| 122-123 | `OLDLIN` | Line number to resume at, saved by ^C, `STOP` or `END` |
| 124-125 | `OLDTXT` | Text pointer to resume at. **`OLDTXT+1 = 0` means "can't continue"**; `STKINI` zeroes it ([m6502.asm:1960](../m6502.asm#L1960)) |
| 126-127 | `DATLIN` | Line number of the `DATA` statement currently being read, kept so errors are reported against it |
| 128-129 | `DATPTR` | The `READ` pointer. `RESTORE` sets it to `TXTTAB-1` |
| 130-131 | `INPPTR` | Where `INPUT`/`READ` text is currently coming from |

### 1.8 Evaluation scratch (132-146)

[m6502.asm:871-888](../m6502.asm#L871-L888).

| Addr | Label | Purpose |
|---|---|---|
| 132-133 | `VARNAM` | The two-byte variable name being looked up. Bit 7 of byte 1 ⇒ string; bit 7 of *both* ⇒ integer |
| 134-135 | `VARPNT` | Pointer to the variable's value |
| 134-135 | `FDECPT` | *Alias.* `FOUT`'s pointer into the powers-of-ten table |
| 136-137 | `FORPNT` | The loop/assignment variable pointer for `FOR` and `LET` |
| 136-137 | `LSTPNT` | *Alias.* `LIST`'s pointer into the line being printed |
| 136 / 137 | `ANDMSK` / `EORMSK` | *Aliases.* `WAIT`'s two masks |
| 138-139 | `OPPTR` | Offset into `OPTAB` of the operator seen but not yet applied; 255 = none |
| 138-139 | `VARTXT` | *Alias.* `INPUT`/`READ`'s saved text pointer |
| 140 | `OPMASK` | The relational bit mask being built by the current operator |
| 141-142 | `DEFPNT` | Pointer used while defining or calling a user function |
| 141-142 | `GRBPNT` | *Alias.* The garbage collector's "descriptor of the lowest uncollected string" |
| 143-144 | `DSCPNT` | Pointer to a string descriptor |
| 145 | — | `IFN ADDPRC` filler, so that `TEMPF3` is five bytes |
| 141-145 | `TEMPF3` | *Alias of `DEFPNT`.* A 5-byte packed-float temporary |
| 146 | `FOUR6` | Declared `EXP STRSIZ`, i.e. preloaded 3. The garbage collector's stride: 3 for temporaries and array elements, 7 for simple variables ([m6502.asm:4389-4390](../m6502.asm#L4389-L4390)). `INIT` re-writes it because it must be RAM |

### 1.9 Trampoline and block-move pointers (147-159)

[m6502.asm:890-908](../m6502.asm#L890-L908).

| Addr | Label | Purpose |
|---|---|---|
| 147-149 | `JMPER` | A `JMP 60000` instruction whose operand is patched at run time. This is how `FINGO` calls a function through `FUNDSP` |
| 148 | `SIZE` | *Alias of `JMPER+1`.* The garbage collector remembers the winning descriptor's `FOUR6` here |
| 149 | `OLDOV` | *Alias of `JMPER+2`.* The saved overflow byte |
| 150 | `TEMPF1` | 5-byte packed-float temporary — it **overlaps** `HIGHDS` and `HIGHTR` |
| 151-152 | `HIGHDS` | Block-move destination (high end) |
| 151-152 | `ARYPNT` | *Alias.* The pointer used while building an array |
| 153-154 | `HIGHTR` | Block-move source (high end) |
| 155 | `TEMPF2` | 5-byte packed-float temporary — overlaps `LOWDS` and `LOWTR` |
| 156-157 | `LOWDS` | Last byte transferred into |
| 156 / 157 | `DECCNT` / `TENEXP` | *Aliases.* `FIN`/`FOUT` decimal state |
| 158-159 | `LOWTR` | Lowest byte to move |
| 158-159 | `GRBTOP` | *Alias.* The garbage collector's `MINPTR` |
| 158 / 159 | `DPTFLG` / `EXPSGN` | *Aliases.* `FIN`'s decimal-point and exponent-sign flags |

> The comments at [m6502.asm:906-907](../m6502.asm#L906-L907) are **swapped**: `TENEXP` is annotated
> "HAS A DPT BEEN INPUT?" and `DPTFLG` "BASE TEN EXPONENT", which is backwards. The code uses
> `DPTFLG` for the decimal-point flag and `TENEXP` for the exponent. Trust the names.

### 1.10 The floating accumulators (160-177)

[m6502.asm:910-941](../m6502.asm#L910-L941).

| Addr | Label | Purpose |
|---|---|---|
| 160 | `FAC` = `FACEXP` = `DSCTMP` | Exponent — **and** the first byte of the scratch string descriptor |
| 161 | `FACHO` | Mantissa, most significant byte |
| 162 | `FACMOH` | `IFN ADDPRC`, the extra precision byte |
| 163 | `FACMO` = `INDICE` | Mantissa middle; `QINT` leaves the integer's high byte here |
| 164 | `FACLO` | Mantissa least significant |
| 165 | `FACSGN` | Sign, 0 or 255 when unpacked |
| 166 | `SGNFLG` = `DEGREE` | `FIN` preserves the mantissa sign here; `POLY` counts down the degree here |
| 167 | `BITS` | The fill byte `SHIFTR` shifts in. `INIT` sets it to 0; `QINT` sets it to 255 for sign extension |
| 168-173 | `ARGEXP` `ARGHO` `ARGMOH` `ARGMO` `ARGLO` `ARGSGN` | The second operand register. No overflow byte |
| 174 | `ARISGN` = `STRNG1` | Sign of the result, i.e. the XOR of the two operand signs; also a string pointer |
| 175 | `FACOV` | The FAC's guard/rounding byte. Note it is **not** contiguous with the FAC — `ARISGN` sits between |
| 176-177 | `FBUFPT` = `BUFPTR` = `STRNG2` = `POLYPT` = `CURTOL` | A five-way-aliased 16-bit scratch pointer |

`DSCTMP = FAC` at [m6502.asm:921](../m6502.asm#L921) is the most consequential alias in the program:
the three bytes at 160, 161, 162 are the scratch string descriptor that every string-producing
routine fills in before calling `PUTNEW`.

### 1.11 The RAM-resident code (178-206)

[m6502.asm:943-982](../m6502.asm#L943-L982).

| Addr | Content |
|---|---|
| 178-179 | `CHRGET: INC CHRGET+7` |
| 180-181 | `BNE CHRGOT` |
| 182-183 | `INC CHRGET+8` |
| 184-186 | `CHRGOT: LDA <absolute>` — **`TXTPTR` = 185-186**, the operand field |
| 187-190 | `CMP #' '` / `BEQ CHRGET` |
| 191-194 | `QNUM: CMP #':'` / `BCS CHRRTS` |
| 195-200 | `SEC` / `SBC #'0'` / `SEC` / `SBC #(256-'0')` |
| 201 | `CHRRTS: RTS` |
| 202-206 | `RNDX` — the random seed, 128, 79, 199, 82, 89 |

`TXTPTR` = `CHRGOT+1` = `CHRGET+7`, which is exactly what the `INC CHRGET+7` / `INC CHRGET+8` at
the top of the routine increment. See
[03-tokenizer-editor.md](03-tokenizer-editor.md#1-chrget--chrgot) for why, and for the fact that
`INIT` overwrites all of this with a slightly different copy.

### 1.12 The page 0/1 boundary (255-271)

[m6502.asm:984-989](../m6502.asm#L984-L989).

```
ORG     255
LOFBUF: BLOCK 1                 ;THE LOW FAC BUFFER. COPYABLE.
;---  PAGE ZERO/ONE BOUNDARY ---.
                                ;MUST HAVE 13 CONTIGUOUS BYTES.
FBUFFR: BLOCK 3*ADDPRC+13       ;BUFFER FOR "FOUT".
                                ;ON PAGE 1 SO THAT STRING IS NOT COPIED.
```

This straddling is deliberate and load-bearing in both directions:

- `FBUFFR` (256-271, 16 bytes) is on **page 1**, so a number formatted by `FOUT` and then printed by
  `PRINT` is *not* copied into string space — `STRLT2`'s copy test only fires for page 0 and the
  `BUF` page.
- `LOFBUF` (255) is the byte immediately below it, on **page 0**. `STR$` calls `FOUTC` with `Y=0` so
  the sign character lands at 255 and the result therefore *does* look like a page-0 string and
  *is* copied into string space — which is required, because `FBUFFR` is about to be reused.

One buffer, two opposite behaviours, selected by which end of it the caller starts at.

### 1.13 Page 1 — the stack

The 6502 stack occupies page 1 from just above `FBUFFR` up to `STKEND`. `STKINI`
([m6502.asm:1949-1962](../m6502.asm#L1949-L1962)) resets the stack pointer to `STKEND-257` = 250 for
this build, leaving the five bytes at `$01FB`-`$01FF` above it permanently unused.

That gap is not waste. `FNDFOR` walks *upward* through the stack looking for frame tag bytes and
stops at the first byte that is not one; the reserved region guarantees it always finds a non-tag
byte and terminates. `INIT` additionally pushes a zero there
([m6502.asm:6749-6750](../m6502.asm#L6749-L6750)).

---

## 2. Alias table

Page zero is 256 bytes and BASIC needs more names than that, so many cells carry several. Some
aliases are pure economy — the two uses never overlap in time — and some are semantic, where the
sharing *is* the mechanism. A reimplementation can discard the first kind and must preserve the
second.

| Address | Names | Kind | Note |
|---|---|---|---|
| 13 | `CHARAC` / `INTEGR` | economy | |
| 16 | `DIMFLG` / `KIMY` | economy | `KIMY` only on `REALIO=1` |
| 19 | `DORES` / `GARBFL` | economy | Disjoint in time; and they use *different bits* (6 vs 7) |
| 22 | `TANSGN` / `DOMASK` | economy | |
| 83 | `LINNUM` / `POKER` | economy | But see [the `PEEK` defect](13-porting-notes.md#3-defects-in-the-1978-code) — they once collided |
| 103 | `RESMO` / `ADDEND` | economy | |
| 134 | `VARPNT` / `FDECPT` | economy | |
| 136 | `FORPNT` / `LSTPNT` / `ANDMSK` / `EORMSK` | economy | |
| 138 | `OPPTR` / `VARTXT` | economy | Means `INPUT`/`READ` cannot be re-entered mid-expression |
| 141 | `DEFPNT` / `GRBPNT` / `TEMPF3` | economy | |
| 148 | `JMPER+1` / `SIZE` | economy | |
| 149 | `JMPER+2` / `OLDOV` | economy | |
| 150 | `TEMPF1` over `HIGHDS`,`HIGHTR` | economy | |
| 155 | `TEMPF2` over `LOWDS`,`LOWTR` | economy | |
| 156 | `LOWDS` / `DECCNT`,`TENEXP` | economy | |
| 158 | `LOWTR` / `GRBTOP` / `DPTFLG`,`EXPSGN` | economy | |
| 160 | **`FAC` / `DSCTMP`** | **semantic** | A string in the FAC is a pointer to a descriptor; the FAC is also where descriptors are built |
| 163 | `FACMO` / `INDICE` | semantic | `QINT`'s output location is defined as being here |
| 166 | `SGNFLG` / `DEGREE` | economy | |
| 174 | `ARISGN` / `STRNG1` | economy | |
| 176 | `FBUFPT` / `BUFPTR` / `STRNG2` / `POLYPT` / `CURTOL` | economy | Five uses, none concurrent |

---

## 3. Adjacency constraints

Several routines address a *run* of page-zero cells as a block. Reordering any of these breaks code
that is nowhere near the declaration.

| Constraint | Enforced by | Where |
|---|---|---|
| `DIMFLG` immediately followed by `VALTYP` | read and restored as a pair by the array code | stated at [m6502.asm:762-763](../m6502.asm#L762-L763) |
| `HIGHDS`, `HIGHTR`, pad, `LOWDS`, `LOWTR` — 9 bytes at 151-159 | `REASON` pushes and pops the whole run across a garbage collection | [m6502.asm:1491-1506](../m6502.asm#L1491-L1506) |
| `DECCNT`, `TENEXP`, `DPTFLG`, `EXPSGN`, `FACEXP`, `FACHO`, `FACMOH`, `FACMO`, `FACLO`, `FACSGN`, `SGNFLG` — 11 bytes at 156-166 | `FIN` zeroes them all in one indexed loop | [m6502.asm:5704-5708](../m6502.asm#L5704-L5708) |
| `FACHO`…`FACLO` contiguous | `NORMAL`'s byte-at-a-time shift reads `FACHO+1`, `FACMOH+1`, `FACMO+1` | [m6502.asm:5006-5016](../m6502.asm#L5006-L5016) |
| `FAC+1`…`FAC+4` and `ARGEXP+1`…`ARGEXP+4` addressable as `1,X`…`4,X` | `SHIFTR`, `QINT1`, `FADD3` take the base address in X and index off it | [m6502.asm:5104](../m6502.asm#L5104) |
| `RESHO`, `RESMOH`, `RESMO`, `RESLO` contiguous | `MULSHF` indexes them as `1,X`…`4,X`; `DIVIDE` indexes `RESLO,X` with X from -3 to +1 | [m6502.asm:5092](../m6502.asm#L5092), [m6502.asm:5419](../m6502.asm#L5419) |
| `LINWID`, `NCMWID`, `LINNUM`, comma, then `BUF` | when `BUFPAG=0`, these four bytes are exactly the link + line-number prefix that `STOLOP` copies into the program. `LINWID` and `NCMWID` must be non-zero so `CHEAD` sees a valid link | [m6502.asm:815](../m6502.asm#L815), [m6502.asm:6728-6730](../m6502.asm#L6728-L6730) |
| `LOFBUF` at 255 immediately below `FBUFFR` at 256 | see §1.12 | [m6502.asm:984-989](../m6502.asm#L984-L989) |

When `BUFPAG≠0` — as in this build — the `BUF` prefix trick cannot work, so `INIT` writes 1 into
`BUF-3` and `BUF-4` explicitly ([m6502.asm:6753-6755](../m6502.asm#L6753-L6755)) and `NODEL` copies
`LINNUM` into `BUF-2` by hand ([m6502.asm:1627-1629](../m6502.asm#L1627-L1629)).

---

## 4. Above page zero

### 4.1 The input buffer

`BUF` is at `BUFPAG*256` = 512, `BUFLEN` = 240 bytes. It must be page-aligned when `BUFPAG≠0`
because `CRUNCH` reads it with an absolute-indexed `LDA BUFOFS,X` using only the low byte of
`TXTPTR` as the index ([m6502.asm:1780-1784](../m6502.asm#L1780-L1784)) — which also caps an input line
at 255 characters regardless of `BUFLEN`.

Direct statements execute in place out of `BUF`. That is why the source insists `BUF` be somewhere
that `STRLT2`'s copy test recognises as volatile: a string constant typed in a direct statement must
be copied into string space before the buffer is reused. `INPUT` reuses `BUF`, which is why the
source warns that `INPUT` smashes it ([m6502.asm:812](../m6502.asm#L812)).

### 4.2 The ROM/RAM split

With `ROMSW=1` the layout is two disjoint regions:

- **`ROMLOC`** (2048, `^O4000` on Apple) — `STMDSP`, `FUNDSP`, `OPTAB`, `RESLST`, `ERRTAB`, all
  message text, all interpreter code, the math package, and `INIT`. Read-only.
- **`RAMLOC`** (`^O25000` on Apple) — the program text and all four dynamic regions, plus the
  `CHRGET` copy in page zero.

`ROMLOC` and `RAMLOC` may be in either order; the code makes no assumption about which is lower.

Two things must be copied from ROM into RAM at startup because they are code that modifies itself or
constants that live in the volatile area: the `CHRGET` routine (from the `INITAT` template) and the
`RNDX` seed. `INIT` does both in one loop — see
[11-io-init.md](11-io-init.md#3-init-step-by-step).

With `ROMSW=0` the whole image is loaded into RAM and `INIT` may additionally *delete* the
trigonometric functions, freeing everything from `COS` (or `ATN`) upward for program text.

### 4.3 The dynamic regions

Described by the source at [m6502.asm:271-310](../m6502.asm#L271-L310) and detailed in
[02-data-structures.md](02-data-structures.md). In order:

```
[TXTTAB]  a zero byte, then the linked list of program lines, ending in a zero link
[VARTAB]  simple variables, 7 bytes each, in creation order (unsorted)
[ARYTAB]  arrays, variable size, in creation order (unsorted)
[STREND]  free space
             ^ grows up as variables and arrays are created
             v grows down as strings are allocated
[FRETOP]  string data in use
[MEMSIZ]  one past the top of memory
```

Both variable tables are unsorted linear lists searched from the start, so variable access is O(n)
in the number of variables — which is why the source recommends declaring frequently used variables
first. Insertion into either table is a block move of everything above it, which is why no pointer
into array space survives the creation of a new simple variable, and hence why arrays may not be
`FOR` loop variables.
