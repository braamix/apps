# 07 — Variables, arrays, and user-defined functions

---

## 1. `PTRGET` — resolving a variable name

[m6502.asm:3626-3778](../m6502.asm#L3626-L3778). Returns a pointer to the variable's *value* in
`VARPNT` and in `[A,Y]`, leaving `TXTPTR` on the terminating character. It creates the variable if
it does not exist — usually.

### 1.1 Entry points

| Entry | Preset | Used by |
|---|---|---|
| `PTRGET` | `X=0`, i.e. `DIMFLG=0` | everything normal |
| `PTRGT1` | caller supplies `X` | `DIM`, which passes the first name character (non-zero, and since `'A'`..`'Z'` is `$41`..`$5A`, **bit 6 is always set** — `LOPPTA` later tests exactly that bit) |
| `PTRGT2` | caller supplies `A` | `GETFNM`, which passes the first character OR `$80` |

### 1.2 Name encoding

Two bytes in `VARNAM`:

| Kind | `VARNAM` bit 7 | `VARNAM+1` bit 7 | `VALTYP` | `INTFLG` |
|---|---|---|---|---|
| float `A`, `AB` | 0 | 0 | 0 | 0 |
| string `A$` | 0 | **1** | 255 | 0 |
| integer `A%` | **1** | **1** | 0 | 128 |
| `DEF FN A` | **1** | 0 | 0 | 0 |

Only the first two characters are significant. `EATEM`
([m6502.asm:3644-3647](../m6502.asm#L3644-L3647)) consumes and discards every following letter or
digit, so `COUNT` and `COUNTER` are the same variable — and `COUNT` also collides with the reserved
word check only at tokenizing time, not here.

A one-character name leaves `VARNAM+1` = 0, because `X` was cleared at `PTRGT3`.

`%` is rejected with a syntax error when `SUBFLG` is non-zero
([m6502.asm:3656-3657](../m6502.asm#L3656-L3657)) — integers cannot be `FOR` variables, `DEF FN` names,
or `DEF FN` parameters.

### 1.3 Array detection

```
STRNAM: STX VARNAM+1
        SEC
        ORA SUBFLG
        SBCI 40                 ; is the current character "(" ?
        JEQ ISARY
        CLR SUBFLG
```

The `ORA SUBFLG` is the whole mechanism for forbidding subscripts: when `SUBFLG` is 128 (or a name
byte with bit 7 set), the OR can never leave 40, so `(` is not recognised as opening a subscript
list. Control falls through to the scalar search, and the stray `(` becomes a syntax error one level
up.

`SUBFLG` is cleared on every successful non-array `PTRGET` and by `STKINI`
([m6502.asm:1961](../m6502.asm#L1961)).

### 1.4 The search

[m6502.asm:3674-3696](../m6502.asm#L3674-L3696). Walk `LOWTR` from `VARTAB` to `ARYTAB` in steps of
`6+ADDPRC` = 7, comparing both name bytes. The stride is type-independent, commented
`;MAKES NO DIF AMONG TYPES` — the table can be traversed without interpreting anything.

This is a linear scan of an unsorted list, so variable access is O(number of variables) and
creation order determines speed.

### 1.5 Not found: the caller check

`NOTFNS` ([m6502.asm:3709-3717](../m6502.asm#L3709-L3717)) does something unusual — it inspects **its
own caller's return address**:

```
NOTFNS: PLA / PHA
        CMPI ISVRET-1-<ISVRET-1>/256*256    ; low byte of ISVRET-1
        BNE NOTEVL
        TSX / LDA 258,X / CMPI <<ISVRET-1>/256>   ; high byte (IFN REALIO-3)
        BNE NOTEVL
LDZR:   LDWDI ZERO
        RTS
```

If the caller is `ISVAR` — that is, the variable is being *read* in an expression — the variable is
**not created**. Instead `[Y,A]` points at `ZERO`, a block of zero bytes that reads correctly as
0.0, as integer 0, and as a null string descriptor.

So `PRINT X` for an unknown `X` prints 0 without consuming any memory, while `X=0` creates the
entry. This matters for `FRE` and for programs that probe many variable names.

On the Commodore only the low byte is compared, because the code fits in one page there.

### 1.6 Creation

`VAROK` ([m6502.asm:3739](../m6502.asm#L3739)): set `LOWTR = ARYTAB`, `HIGHTR = STREND`,
`HIGHDS = STREND + 7`, call `BLTU` — which calls `REASON`, possibly forcing a garbage collection,
and updates `STREND` — then `ARYTAB += 7` and write the two name bytes followed by **five zero
bytes**.

Creating a simple variable therefore **moves the entire array table upward by 7 bytes**. That is the
reason no pointer into array space survives, and hence:

- arrays may not be `FOR` loop variables (a `FOR` frame holds a raw variable pointer);
- `DEF FN` values hold pointers into simple-variable space, so they cannot survive simple-variable
  movement either.

Both restrictions are enforced by `SUBFLG`, and the reasoning is spelled out at
[m6502.asm:545-562](../m6502.asm#L545-L562).

`FINPTR` returns `VARPNT = LOWTR + 2`.

### 1.7 `DIMFLG` and `SUBFLG`

`DIMFLG` is consulted at three points, as the source itself lists at
[m6502.asm:3601-3610](../m6502.asm#L3601-L3610):

1. `GOTARY` — the array already exists and `DIMFLG≠0` ⇒ `?DD REDIM'D ARRAY`.
2. `LOPPTA` — `BIT DIMFLG / BVC NOTDIM`: bit 6 set means take the extents from the stacked
   subscripts; otherwise use the default of 11.
3. After the entry is built — `DIMFLG≠0` means this was a `DIM`, so return without indexing.

`SUBFLG` (128) suppresses both array syntax and the `%` suffix. Set by `FOR`
([m6502.asm:2081-2082](../m6502.asm#L2081-L2082)), by `DEF` ([m6502.asm:4165-4166](../m6502.asm#L4165-L4166)),
and to `character|$80` by `GETFNM`.

---

## 2. Arrays

Layout in [02-data-structures.md](02-data-structures.md#24-array--variable-size).

### 2.1 `ISARY` — collecting subscripts

[m6502.asm:3823-3858](../m6502.asm#L3823-L3858).

```
ISARY: LDA DIMFLG / ORA INTFLG / PHA      ; one byte carries both flags
       LDA VALTYP / PHA
       LDYI 0
INDLOP: TYA / PHA
       PSHWD VARNAM
       JSR INTIDX                          ; CHRGET, FRMEVL, POSINT -> INDICE
       PULWD VARNAM / PLA / TAY
       TSX / LDA 258,X / PHA / LDA 257,X / PHA   ; bubble the two saved flags up
       LDA INDICE   / STA 258,X            ; index HIGH beneath them
       LDA INDICE+1 / STA 257,X            ; index LOW on top
       INY / JSR CHRGOT / CMPI 44 / BEQ INDLOP
       STY COUNT / JSR CHKCLS
       PLA / STA VALTYP
       PLA / STA INTFLG / ANDI 127 / STA DIMFLG
```

Each subscript occupies two stack bytes. The two saved flag bytes are kept floating on top of the
growing subscript list by copying them up each iteration — necessary because `INTIDX` calls
`FRMEVL`, which can recurse all the way back into `PTRGET` for a subscript like `A(B(I))`.

`INTIDX` forces a non-negative integer via `POSINT`, so `A(-1)` gives `?FC`, not `?BS`.

`DIMFLG` and `INTFLG` are packed into one stacked byte and separated again with `ANDI 127` — bit 7
was `INTFLG`, bit 6 was `DIMFLG`.

### 2.2 Searching and the two errors

`LOPFDA` ([m6502.asm:3859-3896](../m6502.asm#L3859-L3896)) walks `ARYTAB` to `STREND`, skipping by the
2-byte length at offsets 2 and 3 — the header alone is enough to traverse.

On a match: `?DD` if `DIMFLG≠0`; then `FMAPTR` computes the element base; then `COUNT` must equal
the stored dimension count or `?BS BAD SUBSCRIPT`. So using the wrong *number* of subscripts and
using an out-of-range subscript both give `?BS`.

### 2.3 Creation

`NOTFDD` ([m6502.asm:3923-4001](../m6502.asm#L3923-L4001)). Element size comes from the two name MSBs:

| Type | `ADDPRC=1` | Derivation |
|---|---|---|
| float | 5 | base |
| string | 3 | `VARNAM+1` bit 7 ⇒ `DEX` twice |
| integer `%` | 2 | `VARNAM` bit 7 ⇒ `DEX` once more |

> The source states this arithmetic is only correct when `ADDPRC=1`
> ([m6502.asm:3932-3934](../m6502.asm#L3932-L3934), [m6502.asm:4055](../m6502.asm#L4055)). `INTPRC`
> effectively requires `ADDPRC`.

Per-dimension loop `LOPPTA` ([m6502.asm:3948-3968](../m6502.asm#L3948-L3968)): the default extent is
**11**, so an undimensioned array has subscripts 0..10. When `DIMFLG` bit 6 is set the extent is
taken from the stacked subscript plus 1. Extents are stored **high byte first**. `UMULT` multiplies
the running total by each extent.

Then the total size is added to `ARYPNT`, `REASON` checks it, `STREND` is updated, and the whole
element area is **zeroed** ([m6502.asm:3980-3990](../m6502.asm#L3980-L3990)) — which is why numeric
arrays start at 0 and string arrays start as null strings. Finally the byte length `STREND-LOWTR` is
written into offsets 2 and 3.

If this was a `DIM`, return. Otherwise fall into `GETDEF` to index the element just created.

### 2.4 Subscript to offset

`GETDEF` ([m6502.asm:4016-4075](../m6502.asm#L4016-L4075)):

```
COUNT <- (LOWTR),4 ; CURTOL <- 0
INLPNM: pop the index (low into X and INDICE, high into INDICE+1)
        compare against the stored extent, high byte then low  -> BSERR if >=
        if CURTOL != 0: CURTOL = CURTOL * extent
        CURTOL += index
        DEC COUNT / BNE INLPNM
```

Subscripts are popped in the reverse of the order they were written, and the extents were stored in
that same reverse order, so the two line up. The resulting linear index for `A(i,j)` is

```
j * extent_i + i
```

— the **first subscript varies fastest**, i.e. column-major. A program that walks a 2-D array in
row order is therefore striding, which matters for the string garbage collector's scan more than for
speed.

The final address is `ARYPNT + CURTOL * element_size`, with the size recomputed from the name bits
into `ADDEND` and multiplied by `UMULTD`.

Line [m6502.asm:4051](../m6502.asm#L4051) is `STA CURTOL+1 ;FIX ARRAY BUG ****` — the historical
high-byte fix, left annotated in the source.

### 2.5 `UMULT`

[m6502.asm:4076-4110](../m6502.asm#L4076-L4110). Unsigned 16x16 to 16 bits, sixteen shift-and-add
iterations. **Any carry out of the top raises `?OM ERROR`**, not `?BS` — so `DIM A(300,300)` reports
out of memory even before the size is compared against available space.

`UMULT` loads its multiplicand from `(LOWTR),Y` and `(LOWTR),Y-1`; `UMULTD` is the entry used when
the high byte is already in `A`.

---

## 3. Why integers exist but barely help

With `INTPRC=1` an integer array element is 2 bytes instead of 5, so `DIM A%(1000)` saves 3000
bytes. But an integer *scalar* still occupies a full 7-byte entry, and every read goes through
`GIVAYF` to become a float and every write through `AYINT` to come back — so integer variables are
**slower** than floats, not faster. The feature is a memory optimisation for large arrays only.

---

## 4. The `ZERO` constant

`LDZR` returns a pointer to `ZERO` ([m6502.asm:6011](../m6502.asm#L6011)), which is positioned so that
the bytes there read as:

- a packed float with exponent 0, i.e. 0.0;
- a 16-bit integer 0;
- a string descriptor with length 0, i.e. the null string.

One block of zeros serves all three types, which is what lets `NOTFNS` return it without knowing
what the caller wanted.

---

## 5. `DEF FN`

[m6502.asm:4134-4234](../m6502.asm#L4134-L4234). The source records the restrictions at
[m6502.asm:4136-4139](../m6502.asm#L4136-L4139): single numeric argument, single-line definition.

### 5.1 `GETFNM`

```
GETFNM: SYNCHK FNTK
        ORAI 128 / STA SUBFLG    ; the name byte gets bit 7; arrays and % forbidden
        JSR PTRGT2 / STWD DEFPNT
        JMP CHKNUM               ; function names may not be strings
```

Setting `SUBFLG` to the character with bit 7 set does double duty: it forbids subscripts *and*
carries the first name character into `PTRGT2`.

### 5.2 Defining

`DEF` ([m6502.asm:4162-4175](../m6502.asm#L4162-L4175)): `GETFNM`; `ERRDIR` (definitions are illegal in
direct mode); `CHKOPN`; `SUBFLG=128`; `PTRGET` for the parameter; `CHKNUM`; `CHKCLS`;
`SYNCHK '='`. Then it pushes a junk byte, `VARPNT` and `TXTPTR`, calls `DATA` to skip to the end of
the statement, and `DEFFIN` pops the five bytes into `(DEFPNT)` — producing exactly the layout in
[02-data-structures.md](02-data-structures.md#23-a-def-fn-entry).

Using the stack to marshal the five bytes, then reusing `DEFFIN` (which is also the *restore* path
of a function call) is a deliberate economy.

`ERRDIR` ([m6502.asm:4154-4157](../m6502.asm#L4154-L4157)) detects direct mode as `CURLIN+1 == 255`.

### 5.3 Calling

`FNDOER` ([m6502.asm:4186-4217](../m6502.asm#L4186-L4217)):

1. `GETFNM` → `DEFPNT`; push it.
2. `PARCHK` evaluates the parenthesised actual argument; `CHKNUM`; pop `DEFPNT`.
3. Read the argument-variable pointer from offsets 2 and 3. **`BEQ ERRGUF` if the high byte is
   zero** ⇒ `?UF UNDEF'D FUNCTION`. This works because a never-defined `FN` name was auto-created by
   `PTRGT2` with all-zero value bytes.
4. `DEFSTF` pushes the argument variable's five *current* bytes.
5. `MOVMF` stores the FAC into the argument variable.
6. Push `TXTPTR`; set `TXTPTR` from offsets 0 and 1; push `VARPNT`.
7. `FRMNUM` evaluates the body; `CHRGOT` must find a terminator; restore `TXTPTR`.
8. `DEFFIN` pops the five saved bytes back into the argument variable.

Step 4 is what makes recursion and nesting safe: the parameter is a perfectly ordinary variable, so
`DEF FNA(X)` and a program variable `X` are the same cell, and saving/restoring it around the call
is the entire mechanism. A user can see this — `FNA(3)` leaves `X` unchanged, but a `DEF` whose body
references `FNA` itself will work to whatever depth the stack allows.

The 1978-01-24 note ([m6502.asm:232](../m6502.asm#L232)) records the fix that stopped the undefined
check from clobbering the error number in `X`.
