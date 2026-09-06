# 02 — Data structures

Byte-exact layouts for the checked-in Apple configuration (`ADDPRC=1`, `INTPRC=1`, so floats are
5 bytes and a simple variable entry is 7). Sizes that depend on a switch are given as an expression
as well as a number.

---

## 1. Program text

### 1.1 A stored line

```
offset 0,1   link: the absolute address of the NEXT line's link field, little-endian
offset 2,3   line number, little-endian, 0..63999
offset 4..   crunched text: tokens are single bytes >= 128, everything else is ASCII
offset n     0x00 terminator
```

The link is an **absolute address**, not an offset. It points at the *next line's link field*, so
walking the program is `p = *(uint16_t*)p`. End of program is a link whose **high byte is zero**
([m6502.asm:1882-1883](../../m6502.asm#L1882-L1883),
[m6502.asm:2151-2153](../../m6502.asm#L2151-L2153)) — the check is on the high byte alone, never on the
full 16 bits.

An empty program is two zero bytes at `TXTTAB`, written by `SCRTCH`
([m6502.asm:1910-1921](../../m6502.asm#L1910-L1921)), with `VARTAB = TXTTAB+2`.

Conceptually a zero byte *precedes* `TXTTAB`: `RESTORE` points `DATPTR` at `TXTTAB-1`
([m6502.asm:2196-2202](../../m6502.asm#L2196-L2202)) and `STXTPT` points `TXTPTR` at `TXTTAB-1`
([m6502.asm:1964-1971](../../m6502.asm#L1964-L1971)), both relying on reading a zero there so that the
normal "end of line, follow the link" path runs. `INIT` writes that byte
([m6502.asm:6877-6880](../../m6502.asm#L6877-L6880)).

Because the links are absolute they are invalidated by every insertion or deletion, so they are not
patched — they are recomputed wholesale by `LNKPRG` after every edit. See
[03-tokenizer-editor.md](03-tokenizer-editor.md#5-the-program-editor).

### 1.2 A direct line

A line typed without a number is executed in place, in `BUF`, with the same shape:

```
BUF-4, BUF-3   fake link (must be non-zero)
BUF-2, BUF-1   LINNUM, then a literal comma
BUF ..         crunched text
               0x00
               0x00     <- planted by CRDONE at terminator+2
```

The second zero at [m6502.asm:1859-1860](../../m6502.asm#L1859-L1860) makes the direct line's "next link
high byte" read as zero, so `NEWSTT` sees end-of-program and stops rather than running off into
whatever follows the buffer.

---

## 2. Variables

### 2.1 Simple variable entry — 7 bytes (`6+ADDPRC`)

```
offset 0     name character 1   (bit 7 set => integer)
offset 1     name character 2   (bit 7 set => string or integer; 0 if the name is one character)
offset 2..6  the value, 5 bytes
```

The stride is the same for every type ([m6502.asm:3693](../../m6502.asm#L3693), commented
`;MAKES NO DIF AMONG TYPES`), so the table can be walked without knowing what is in it. The value
bytes are interpreted according to the name bits:

| Type | Name bit 7 of byte 0 | of byte 1 | Value bytes |
|---|---|---|---|
| float `A`, `AB` | 0 | 0 | 5-byte packed float |
| string `A$` | 0 | **1** | 3-byte descriptor, then 2 unused |
| integer `A%` | **1** | **1** | 2 bytes, **big-endian**, then 3 unused |
| `DEF FN A` | **1** | 0 | see §2.3 |

Only the first two characters of a name are significant; `EATEM`
([m6502.asm:3644-3647](../../m6502.asm#L3644-L3647)) discards the rest. A one-character name leaves byte 1
as zero.

Setting bit 7 on the name rather than storing a type tag is what keeps `A` and `A$` distinct in a
single flat search: they simply have different two-byte names and never match each other.

> Note the integer value is stored **big-endian** (`FACMO` then `FACLO`,
> [m6502.asm:2558-2571](../../m6502.asm#L2558-L2571)) while every other 16-bit quantity in the program is
> little-endian. This is not an accident of the store; `ISVAR` reads it back high byte first
> ([m6502.asm:3425-3435](../../m6502.asm#L3425-L3435)).

### 2.2 String descriptor — 3 bytes (`STRSIZ`)

```
offset 0     length, 0..255
offset 1     data address, low     } meaningless when length == 0
offset 2     data address, high    }
```

A null string is length 0 with undefined address bytes; a freshly created string variable is three
zero bytes, which reads correctly as the null string.

The descriptor and the string *data* are separate. The descriptor lives in the variable table, an
array, or a temporary; the data lives in string space above `FRETOP` — unless it does not need to,
in which case the address points directly into the program text. See
[08-strings-gc.md](08-strings-gc.md).

### 2.3 A `DEF FN` entry

The five value bytes of a variable whose first name byte has bit 7 set:

```
offset 0,1   text pointer to the formula
offset 2,3   pointer to the argument variable's value
offset 4     unused ("the crazy byte", ADDPRC only)
```

A function name that has been referenced but never defined was auto-created by `PTRGT2` with all
five bytes zero, so `offset 3 == 0` is the "undefined function" test
([m6502.asm:4192-4196](../../m6502.asm#L4192-L4196)).

### 2.4 Array — variable size

Comment at [m6502.asm:3812-3821](../../m6502.asm#L3812-L3821).

```
offset 0       name character 1  (bit 7 => integer)
offset 1       name character 2  (bit 7 => string or integer)
offset 2,3     total length in bytes, little-endian, INCLUDING this header
offset 4       number of dimensions N
offset 5       for each dimension: max_index+1, 2 bytes, HIGH BYTE FIRST
   ..5+2N-1    ...and in REVERSE of the order the subscripts were written
offset 5+2N    the elements
```

Two irregularities to note. The extents are stored **big-endian**, unlike everything else, and they
are stored in **reverse subscript order**, because `ISARY` collects subscripts onto the 6502 stack
and the creation loop pops them. The combined effect is that for `A(i,j)` the linear element index
is `j * extent_i + i` — the *first* subscript varies fastest, i.e. column-major order.

Element size depends on the type and is recomputed from the name bits each time it is needed
([m6502.asm:3928-3944](../../m6502.asm#L3928-L3944), [m6502.asm:4052-4064](../../m6502.asm#L4052-L4064)):

| Type | Size (`ADDPRC=1`) |
|---|---|
| float | 5 |
| string | 3 (descriptor only) |
| integer `%` | 2 |

The `length` field at offset 2,3 lets the array table be walked without understanding any of this —
`LOPFDA` just adds it to move to the next array.

Undimensioned arrays get an extent of **11** per dimension, i.e. subscripts 0 through 10
([m6502.asm:3948](../../m6502.asm#L3948)). Indexing is 0-based throughout; there is no `OPTION BASE`.

---

## 3. String temporaries

`TEMPST` (page zero 88-96) holds `NUMTMP` = 3 descriptors. Two page-zero bytes track it:

- `TEMPPT` — the address of the next *free* slot.
- `LASTPT` — the address of the last *allocated* slot. Only the low byte is written; the high byte
  is zeroed once by `INIT` so a 16-bit compare works.

They are allocated and freed strictly in stack order. Three is enough because a temporary only has
to survive from the moment a string expression produces a value to the moment it is consumed;
exhausting them raises `?ST ERROR` ("string formula too complex"), which is a real limit on
expression nesting, not an internal error.

```
PUTNEW   copy DSCTMP into *TEMPPT; point the FAC at it; LASTPT = TEMPPT; TEMPPT += 3
FRETMS   if the pointer given IS LASTPT: TEMPPT = LASTPT; LASTPT -= 3
FRETMP   FRETMS, and if the freed body sat exactly at FRETOP, reclaim it: FRETOP += length
```

`FRETMP`'s second half is the only way string space is ever reclaimed without a full garbage
collection, and it only works for the most recently allocated block — a one-entry free list.

---

## 4. Stack frames

The 6502 hardware stack carries four different kinds of record. Three of them are *semi-permanent*:
they outlive the routine that created them and are found again by scanning. Each begins with a tag
byte at its lowest address, and `FNDFOR` identifies frames by that tag.

### 4.1 `FOR` frame — 18 bytes (`FORSIZ = 16+2*ADDPRC`)

Documented at [m6502.asm:2066-2078](../../m6502.asm#L2066-L2078) and again at
[m6502.asm:3096-3108](../../m6502.asm#L3096-L3108). Offsets are from the frame base, which is the lowest
address, i.e. the top of the stack.

| Offset | Bytes | Content | Pushed by |
|---|---|---|---|
| +0 | 1 | `FORTK` (129) | [m6502.asm:2127-2128](../../m6502.asm#L2127-L2128) — last |
| +1 | 1 | loop variable pointer, **low** | `PSHWD FORPNT` |
| +2 | 1 | loop variable pointer, **high** | |
| +3 | 1 | STEP exponent | `FORPSH` |
| +4 | 1 | STEP `FACHO` — **mantissa MSB, sign NOT packed in** | |
| +5 | 1 | STEP `FACMOH` | |
| +6 | 1 | STEP `FACMO` | |
| +7 | 1 | STEP `FACLO` | |
| +8 | 1 | **sign of STEP**: 1, 0 or 255, from `SIGN` | `PUSHF` |
| +9 | 1 | LIMIT exponent | `FORPSH` |
| +10 | 1 | LIMIT `FACHO` — **sign packed into bit 7** | |
| +11 | 1 | LIMIT `FACMOH` | |
| +12 | 1 | LIMIT `FACMO` | |
| +13 | 1 | LIMIT `FACLO` | |
| +14 | 1 | `CURLIN` **low** | `PSHWD CURLIN` |
| +15 | 1 | `CURLIN` **high** | |
| +16 | 1 | text pointer **high** | pushed second, by hand |
| +17 | 1 | text pointer **low** | pushed first, by hand |

Three things a reimplementation must not smooth over:

1. **The step and the limit are stored in different formats.** The step is *unpacked* — bit 7 of
   `FACHO` is the mantissa's hidden bit — with its sign in a separate byte at +8. The limit is
   *packed*, sign folded into bit 7 of `FACHO`, because `NEXT` compares against it with `FCOMPN`,
   which expects a packed operand. `FOR` explicitly packs the limit before pushing
   ([m6502.asm:2110-2113](../../m6502.asm#L2110-L2113)).
2. **The byte orders differ within one frame.** `CURLIN` ends up little-endian because `PSHWD`
   pushes high then low; the text pointer ends up big-endian because `FOR` pushes low then high by
   hand ([m6502.asm:2100-2105](../../m6502.asm#L2100-L2105)). `NEXT` reads back accordingly
   ([m6502.asm:3143-3150](../../m6502.asm#L3143-L3150)).
3. **The saved text pointer is not the loop body.** It is `TXTPTR + DATAN`, i.e. the address of the
   `:` or NUL that ends the `FOR` statement itself.

### 4.2 `GOSUB` frame — 5 bytes

Documented at [m6502.asm:2370-2380](../../m6502.asm#L2370-L2380), pushed at
[m6502.asm:2381-2386](../../m6502.asm#L2381-L2386).

| Offset | Content |
|---|---|
| +0 | `GOSUTK` (140) |
| +1 | `CURLIN` low |
| +2 | `CURLIN` high |
| +3 | `TXTPTR` low |
| +4 | `TXTPTR` high |

Both fields are little-endian here, because both are pushed with `PSHWD` — the inconsistency with
the `FOR` frame is real.

The saved `TXTPTR` points *before* the target line number, since `CHRGOT` has not run yet at the
point of the push. That is why `RETURN` falls straight into `DATA`/`DATAN` afterwards
([m6502.asm:2433-2443](../../m6502.asm#L2433-L2443)): it must skip the rest of the `GOSUB` statement,
which for `ON X GOSUB a,b,c` is the whole remaining line-number list.

### 4.3 Expression frame — 10 bytes (`9+ADDPRC`)

Built by `DOPRE1` + `PUSHF1`/`PUSHF`/`FORPSH` ([m6502.asm:3262-3293](../../m6502.asm#L3262-L3293)) each
time evaluation defers an operator to descend into a higher-precedence subexpression.

| Offset | Content |
|---|---|
| +0 | precedence of the new operator |
| +1 | `OPMASK` (relational bits; garbage for other operators) |
| +2 | `FACEXP` |
| +3 | `FACHO` |
| +4 | `FACMOH` |
| +5 | `FACMO` |
| +6 | `FACLO` |
| +7 | `FACSGN` |
| +8 | dispatch address, low |
| +9 | dispatch address, high |

Beneath it sit the 2-byte return address of the `JSR DOPRE1` and the caller's re-pushed precedence
byte.

The FAC is pushed **rounded but unpacked**, least significant byte first, so that bytes +2 through
+6 form exactly the sequence `MOVFM` expects. That is why the value can be reloaded straight off the
stack without repacking.

This frame has **no tag byte** — it is not scanned for, only unwound in order. `FNDFOR` never sees
one because a `FOR` or `GOSUB` can never be created in the middle of expression evaluation.

### 4.4 Frame discovery geometry

`FNDFOR` ([m6502.asm:1371-1373](../../m6502.asm#L1371-L1373)) starts with `TSX` then four `INX`, reading
from `257,X`. Inside `FNDFOR` the stack pointer is the caller's minus 2, so the first byte examined
is at `0x100 + S_caller + 3`. The two bytes below it are `NEWSTT`'s `JSR GONE3` return address, and
the two below those are `FNDFOR`'s own return address.

The invariant this depends on: **`NEWSTT`'s return address always sits immediately below the topmost
semi-permanent frame.** Statement handlers that create a frame must discard that return address
first, which is exactly why `FOR` does `PLA/PLA` at `NOTOL`
([m6502.asm:2092-2093](../../m6502.asm#L2092-L2093)).

Scanning terminates because `STKINI` leaves five bytes above the stack pointer that are guaranteed
not to be a tag, and `INIT` pushes a zero there. See
[01-memory-map.md](01-memory-map.md#113-page-1--the-stack).

---

## 5. Floating-point representation

Full treatment in [09-float-format-arith.md](09-float-format-arith.md); the layouts are repeated
here because they are data structures.

### 5.1 Packed — 5 bytes (`4+ADDPRC`), as stored in variables, arrays and constant tables

```
byte 0   exponent, excess-128; 0 means the whole value is zero
byte 1   bit 7 = sign; bits 6..0 = mantissa bits 2..8
byte 2   mantissa bits 9..16
byte 3   mantissa bits 17..24
byte 4   mantissa bits 25..32
```

The mantissa is big-endian, normalised to [0.5, 1), with an **implied leading 1 occupying the same
bit position as the sign**. Value:

```
(-1)^sign * (0x80 | byte1) : byte2 : byte3 : byte4  *  2^(exponent-128) / 2^32
```

When the exponent byte is 0 the value is zero and **the other four bytes are undefined** — the
source states this explicitly at [m6502.asm:4861](../../m6502.asm#L4861). There is no negative zero, no
infinity, no NaN and no denormal.

### 5.2 Unpacked — the FAC, 6 bytes plus a detached guard byte

```
FACEXP  160   exponent
FACHO   161   mantissa MSB, with bit 7 forced to 1 (the hidden bit made explicit)
FACMOH  162
FACMO   163
FACLO   164
FACSGN  165   sign: only bit 7 is meaningful
...
FACOV   175   guard byte, one below the mantissa LSB - NOT adjacent to the FAC
```

`ARG` (168-173) mirrors `FACEXP`…`FACSGN` exactly but has no guard byte.

Unpacking sets `FACHO` bit 7 and copies the packed sign into `FACSGN`; packing folds `FACSGN` bit 7
back into `FACHO` bit 7 and rounds using `FACOV`.

> `FACSGN` is not always 0 or 255. `FOUTC` stores a space or a `-` there
> ([m6502.asm:5858](../../m6502.asm#L5858)) and `FPWRT` stores 3 or 4
> ([m6502.asm:6134](../../m6502.asm#L6134)). Everything that reads it uses `BIT`, `BPL`, `BMI` or `ROL`,
> so only bit 7 is ever consulted. Treat it as "a byte whose bit 7 is the sign", not as a boolean.

### 5.3 Where the two forms are used

| Form | Used for |
|---|---|
| Packed | variable and array storage, constant tables, the `FOR` frame's limit, `TEMPF1`/`TEMPF2`/`TEMPF3` |
| Unpacked | the FAC and ARG registers, the expression stack frame, the `FOR` frame's step |
