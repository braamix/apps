# 08 — Strings, temporaries, and garbage collection

The source's own explanation of this subsystem, at
[m6502.asm:588-698](../../m6502.asm#L588-L698), is the most detailed part of its design document and is
worth reading directly.

---

## 1. The central invariant

> IT IS THE NATURE OF GARBAGE COLLECTION THAT DISALLOWS HAVING TWO STRING DESCRIPTORS POINT TO THE
> SAME AREA IN STRING SPACE.
> — [m6502.asm:648-650](../../m6502.asm#L648-L650)

The collector works by repeatedly finding the highest not-yet-moved string and sliding it to the top
of free space, then rewriting *the one descriptor it came from*. If two descriptors shared a body,
the second would be left pointing at moved-away memory. Everything else in this document follows
from that one constraint.

---

## 2. Temporaries

`TEMPST` holds three 3-byte descriptors on page zero, tracked by `TEMPPT` (next free) and `LASTPT`
(last used). See
[02-data-structures.md](02-data-structures.md#3-string-temporaries).

### 2.1 `PUTNEW`

[m6502.asm:4312-4333](../../m6502.asm#L4312-L4333). Copies `DSCTMP` into `*TEMPPT`, points the FAC at that
slot, sets `VALTYP=255` and `FACOV=0`, records `LASTPT`, and advances `TEMPPT` by 3. If `TEMPPT` has
reached `TEMPST+9` it raises `?ST` — "string formula too complex".

Three temporaries is a real, user-visible limit on how deeply string expressions may nest. It is
enough because a temporary only has to live from the moment an operator produces a value to the
moment the enclosing operator consumes it.

### 2.2 Freeing

```
FRESTR: JSR CHKSTR
FREFAC: LDWD FACMO
FRETMP: STWD INDEX / JSR FRETMS / PHP
        read length -> A, address -> X,Y from (INDEX)
        PLP / BNE FRETRT              ; not the most recent temp: leave FRETOP alone
        if [Y,X] == FRETOP: FRETOP += length
FRETRT: STXY INDEX / RTS              ; A = length, INDEX = body address

FRETMS: CPY LASTPT+1 / CMP LASTPT / BNE FRERTS
        STA TEMPPT / SBCI STRSIZ / STA LASTPT / LDYI 0
```

[m6502.asm:4588-4626](../../m6502.asm#L4588-L4626).

`FRETMS` releases only the descriptor slot, and only if the pointer given really is the most recent
one. `FRETMP` additionally reclaims the *body* — but only when that body sits exactly at `FRETOP`,
i.e. it was the last thing allocated. This is a one-entry free list, and it is the only way string
space is recovered without a full collection.

`LASTPT+1` is zeroed once by `INIT` ([m6502.asm:6744](../../m6502.asm#L6744)) so that the 16-bit compare
works against page-zero addresses.

---

## 3. Allocation

### 3.1 `GETSPA`

[m6502.asm:4344-4361](../../m6502.asm#L4344-L4361). `A` = the number of bytes wanted.

```
GETSPA: LSR GARBFL                       ; clear the "already collected" bit
        A' = FRETOP - A                  ; via EOR #255 / SEC / ADC
        compare against STREND
        if it would collide -> GARBAG
        FRETOP = FRESPC = A'
        return the pointer in [Y,X], with A preserved
```

Allocation is a pointer decrement. There is no header, no size field, and no free list — a string
body is just bytes, and the only thing that knows how long it is, is the descriptor.

### 3.2 `STRINI` / `STRSPA`

`STRINI` ([m6502.asm:4253](../../m6502.asm#L4253)) saves `FACMO` into `DSCPNT` and falls into `STRSPA`,
which is `GETSPA` plus building the descriptor in `DSCTMP`.

### 3.3 The copy-if-volatile rule

`STRLIT` / `STRLT2` ([m6502.asm:4271-4305](../../m6502.asm#L4271-L4305)) turn a run of characters into a
descriptor. `STRLIT` presets both delimiters to `"`; `STRLT2` takes the first character's address in
`[Y,A]`, scans until 0, `CHARAC` or `ENDCHR`, and records the length.

Then:

```
if the string's page is page 0, or the BUF page:
        copy it into string space (STRINI + MOVSTR)
```

([m6502.asm:4295-4305](../../m6502.asm#L4295-L4305))

Everything else is left pointing where it is. So:

| Source | Copied? | Why |
|---|---|---|
| a literal in program text | no | program text does not move while it is running |
| a literal typed in a direct statement (`BUF`) | **yes** | `BUF` is reused by the next input |
| the result of `STR$` (at `LOFBUF`, page 0) | **yes** | `FBUFFR` is reused by the next `PRINT` |
| the result of `FOUT` for `PRINT` (at `FBUFFR`, page 1) | no | it is printed immediately |
| a value already in string space | no | it is already stable |

The `LOFBUF`/`FBUFFR` split across the page 0/1 boundary exists precisely so that one buffer can be
on either side of this test depending on which entry point produced it — see
[01-memory-map.md](01-memory-map.md#112-the-page-01-boundary-255-271).

`STRLT2` also has a carry subtlety: if the terminator was a quote, carry stays set and `STRNG2` is
advanced past it; otherwise `STRNG2` is left pointing *at* the terminator.

### 3.4 Moving bytes

`MOVINS` ([m6502.asm:4550-4559](../../m6502.asm#L4550-L4559)) loads a descriptor through `STRNG1` and
falls into `MOVSTR`/`MOVDO` ([m6502.asm:4560-4575](../../m6502.asm#L4560-L4575)), which copy `A` bytes
from `(INDEX)` to `(FRESPC)` **backwards** with a `DEY` loop, then advance `FRESPC`.

---

## 4. The six-step protocol

Stated by the source at [m6502.asm:653-665](../../m6502.asm#L653-L665). Every routine that produces a
string must do exactly this, in this order:

1. **Work out the length of the result.**
2. **Call `GETSPA`.** This may trigger a garbage collection, so *nothing* survives the call except
   pointers to descriptors — not string bodies, not addresses.
3. **Build the result descriptor in `DSCTMP`.**
4. **Copy the bytes** into the space `GETSPA` returned.
5. **Free the arguments** with `FRETMP`.
6. **`PUTNEW`** to move `DSCTMP` into a fresh temporary.

Steps 5 and 6 cannot be swapped: temporaries are allocated and freed in stack order, so the new one
cannot be created until the old ones are gone — and if the arguments were freed *before* the result
was built, the result could overwrite an argument still being read
([m6502.asm:666-678](../../m6502.asm#L666-L678)).

Step 2's "only descriptor pointers survive" rule is why every string function juggles descriptor
addresses on the 6502 stack rather than caching the data addresses it just computed.

---

## 5. Garbage collection

[m6502.asm:4362-4516](../../m6502.asm#L4362-L4516). The algorithm is stated at
[m6502.asm:679-698](../../m6502.asm#L679-L698) and the code matches it.

### 5.1 Structure

```
GARBAG:  if GARBFL bit 7 is already set -> ?OM
         collect; set GARBFL = 128; retry GETSPA once
GARBA2:  FRETOP = MEMSIZ
FNDVAR:  GRBPNT = 0 (both bytes); GRBTOP = STREND; INDEX1 = TEMPST
         scan temporaries          (FOUR6 = 3,          up to TEMPPT)
         scan simple variables     (FOUR6 = 6+ADDPRC=7, VARTAB -> ARYTAB)
         scan string arrays        (FOUR6 = STRSIZ = 3, element by element)
GRBPAS:  if GRBPNT == 0 -> done
         move the winning string up to FRETOP with BLTUC
         patch its descriptor
         loop back to FNDVAR with the new FRETOP
```

Each pass finds **one** string — the one with the highest data address that is still below `FRETOP`
and above `GRBTOP` — and slides it to the top of free space. Then the whole scan runs again.

This is **O(n²) in the number of live strings**, and the source knows it: `GARBA2` on the PDP-10
simulator rings the terminal bell so the developer can hear a collection happen
([m6502.asm:4371-4373](../../m6502.asm#L4371-L4373)).

### 5.2 The `FOUR6` stride trick

`DVAR` is one routine that walks three differently shaped tables. It is told the stride by writing
`FOUR6` before each phase: 3 for temporaries and array elements, 7 for simple variables. `DVARS`
adds the entry point that skips non-string entries by testing the name bits — and with `INTPRC`, an
extra test that skips integer variables, whose bit-7 pattern would otherwise look like a string
([m6502.asm:4444-4447](../../m6502.asm#L4444-L4447)).

`FOUR6` is also recorded into `SIZE` for the winning descriptor, and `GRBPAS` recovers the
descriptor's offset within its entry from it:

```
GRBPAS: LDA SIZE / ANDI 4 / LSR A / TAY
```

giving `Y=2` for a simple variable (whose descriptor starts two bytes into the entry, after the
name) and `Y=0` for a temporary or array element. One bit of the stride encodes the layout.

### 5.3 Walking string arrays

`ARYVA2` ([m6502.asm:4402-4441](../../m6502.asm#L4402-L4441)) reads each array header, uses the length
field to find the next array, tests the name bits to decide whether to descend, and computes the
element base as `2*ndims + 5` past the header. Non-string arrays are skipped entirely without
touching their contents.

### 5.4 The two 1978-07-01 fixes

Both are recorded in the revision log and both are visible in the code:

- [m6502.asm:221-223](../../m6502.asm#L221-L223): "FIXED BUG WHERE GARBAGE COLLECTION NEVER(!) COLLECTS
  TEMPS". The fix was `STY GRBPNT` at `FNDVAR` and `LDA GRBPNT ORA GRBPNT+1` at `GRBPAS` — i.e.
  clearing and testing **both** bytes of `GRBPNT`. Testing only the low byte meant a descriptor at
  a page boundary looked like "nothing found".
- [m6502.asm:224-225](../../m6502.asm#L224-L225): deleting or inserting a program line could trigger a
  collection with a stale `VARTAB`. Fixed by calling `RUNC` before the move as well as after.

### 5.5 What is *not* collected

Only string **data** is compacted. Variables, arrays and program text never move during collection —
`BLTUC` is used rather than `BLTU` precisely so that `STREND` is left alone. And because
allocation is a bare pointer decrement, there is no notion of a partially free block: after a
collection, all free space is one contiguous run between `STREND` and `FRETOP`.

---

## 6. String operations

### 6.1 Concatenation

`CAT` [m6502.asm:4522-4548](../../m6502.asm#L4522-L4548), reached from `FRMEVL` only when the operator is
`+` and `VALTYP` is 255.

It pushes the current descriptor pointer and calls **`EVAL`, not `FRMEVL`** — so string `+` behaves
like a maximally tight left-to-right operator with no precedence interaction at all, and re-enters
the evaluator at `TSTOP` when finished. A result longer than 255 characters raises `?LS`.

### 6.2 The functions

| Function | Line | Notes |
|---|---|---|
| `STR$` | [4242-4248](../../m6502.asm#L4242-L4248) | `FOUTC` with `Y=0`, so the text starts at `LOFBUF` on page 0 and is therefore copied |
| `CHR$` | [4632-4642](../../m6502.asm#L4632-L4642) | `CONINT` (0..255), allocate 1 byte, store, `PUTNEW` |
| `LEFT$` | [4648-4671](../../m6502.asm#L4648-L4671) | if `n >= len`, use `len` and offset 0 |
| `RIGHT$` | [4672-4676](../../m6502.asm#L4672-L4676) | computes offset = `len - n`, then shares `LEFT$`'s tail |
| `MID$` | [4684-4704](../../m6502.asm#L4684-L4704) | length defaults to 255; position 0 gives `?FC`; a position past the end yields the null string |
| `LEN` | [4730-4736](../../m6502.asm#L4730-L4736) | `LEN1` frees the argument, forces `VALTYP=0`, floats the length |
| `ASC` | [4741-4746](../../m6502.asm#L4741-L4746) | a null string gives `?FC` |
| `VAL` | [4763-4789](../../m6502.asm#L4763-L4789) | see below |

`PREAM` ([m6502.asm:4709-4725](../../m6502.asm#L4709-L4725)) is the shared prologue for the three
multi-argument functions: it checks the `)`, pops its own return address into `JMPER`, discards
`FINGO`'s return address so the function returns straight to `FRMEVL`, and recovers the byte
argument and descriptor pointer from the stack.

### 6.3 `VAL` writes outside the string

`VAL` needs a NUL-terminated buffer because it reuses `FIN`, the ordinary numeric parser, which stops
on whatever `CHRGET` says is a terminator. Strings are not NUL-terminated, so:

```
        INDEX2 = body + length
        save the byte at (INDEX2)
        store 0 at (INDEX2)
        CHRGOT / FIN
        restore the saved byte
```

The byte one past the end of the string is **written and then restored**. It is safe on a 6502 where
that byte is always addressable, but it is a genuine out-of-bounds write that any reimplementation
using a bounded string type has to handle deliberately — either by copying, or by keeping a
sentinel. It is also not re-entrant: an interrupt observing that byte would see a zero.

### 6.4 `GETBYT` / `CONINT`

[m6502.asm:4749-4755](../../m6502.asm#L4749-L4755). `FRMNUM`, then `POSINT`, then require `FACMO == 0`, so
the value must be 0..255. Returns the byte in **`X`** (and in `FACLO`), with `CHRGOT` flags set on
the terminator. `GTBYTC` does a `CHRGET` first.
