# Internals of Microsoft BASIC 1.1 for the 6502

A recovered design document for [m6502.asm](m6502.asm), reconstructed by reading the source. It is
written for someone who intends to reimplement this interpreter: it describes what the code does and
which of its mechanisms carry real semantics, but it proposes no replacement design.

The source has its own explanatory comment block at [m6502.asm:245-729](m6502.asm#L245-L729). It is
worth reading, but it is incomplete and in several places wrong (see
[Porting notes](internals/13-porting-notes.md#4-comment-defects-in-the-original)). Where this
document and that comment block disagree, this document follows the code.

---

## 1. How to read the source

### 1.1 It is MACRO-10, not a 6502 assembler

The file is written for **MACRO-10**, the DEC PDP-10 assembler, used as a cross-assembler. It will
not assemble with any modern 6502 tool without translation.

Line 2 is `SEARCH M6502`, which pulls in a MACRO-10 *universal file* defining the 6502 instruction
set as macros. **That file is not in this repository.** Everything below comes from it and is
therefore undefined here:

| Form | Meaning |
|---|---|
| `LDAI`, `CMPI`, `SBCI`, `LDXI`, `LDYI`, `ORAI`, `EORI`, `ANDI`, `ADCI` | the `I` suffix is *immediate* addressing: `LDAI 5` is `LDA #5` |
| `LDADY x`, `STADY x`, `CMPDY x` | indirect indexed: `LDA (x),Y` |
| `JMPD x` | indirect jump: `JMP (x)` |
| `LSR A,`, `ROR A` | accumulator addressing |
| `ADR(x)` | emit a 2-byte little-endian address |
| `BLOCK n` | reserve `n` bytes |
| `XWD a,b` | emit a raw value (used for the opcode-swallowing tricks) |

Assembler directives used throughout: `DEFINE` (macro definition), `IRPC` (character iteration),
`REPEAT`, `IFE`/`IFN` (conditional assembly, "if expression equals / not equals zero"), `ORG`,
`RADIX`, `SUBTTL`, `PAGE`, `PRINTX` (assembly-time message), `COMMENT *…*`, `EXP` (emit one value
per expression), and `!` as inclusive-OR in expressions. `=` defines a symbol; `==` defines one that
may be redefined; `::` makes a symbol global.

### 1.2 The macros defined in this file

Defined at [m6502.asm:108-205](m6502.asm#L108-L205). These do carry over to a reader:

| Macro | Expansion | Register convention |
|---|---|---|
| `LDWD x` | `LDA x` / `LDY x+1` | A = low, Y = high |
| `LDWDI x` | `LDA #<x` / `LDY #>x` | A = low, Y = high |
| `STWD x` | `STA x` / `STY x+1` | |
| `LDWX x` / `LDWXI x` / `STWX x` | same with X instead of Y | A = low, X = high (but `STWX` is used with A = *high* in `LINPRT`) |
| `LDXY x` / `LDXYI x` / `STXY x` | `LDX`/`LDY` pair | X = low, Y = high |
| `CLR x` | `LDA #0` / `STA x` | |
| `COM x` | `LDA x` / `EOR #$FF` / `STA x` | one's complement in place |
| `PSHWD x` | push high, then low | so low ends up on top |
| `PULWD x` | pop low, then high | |
| `INCW x` | 16-bit increment | |
| `JEQ x` / `JNE x` | `BNE .+5` / `JMP x` — a long conditional branch | |
| `BCCA`, `BEQA`, `BNEA`, … | a branch the author knows always branches (the 6502 has no unconditional branch) | |
| `SYNCHK(q)` | `LDA #q` / `JSR SYNCHR` — require and consume a specific character | |
| `SKIP1` / `SKIP2` | a `BIT` opcode that swallows the next 1 or 2 bytes | |
| `DT(q)` | emit a character string | |
| `ROR x` | on `RORSW=0` targets, an emulation of `ROR` for early 6502s whose `ROR` was broken | |

`SKIP1`/`SKIP2` deserve a note because they look like data. `SKIP1` emits the opcode for
`BIT <zero page>`, so the following *one* byte becomes its operand and is not executed; `SKIP2`
does the same for a 2-byte operand. This is how the source implements "fall into the next routine
but skip its first instruction" — for example
[m6502.asm:3169-3177](m6502.asm#L3169-L3177), where `CHKNUM` sets carry clear and then skips over
the `SEC` belonging to `CHKSTR`.

### 1.3 The radix trap

This is the single most dangerous thing about reading the file.

- [m6502.asm:4](m6502.asm#L4) sets `RADIX 10`. Bare numbers are **decimal**.
- [m6502.asm:4847](m6502.asm#L4847) sets `RADIX 8` — the comment reads `;!!!! ALERT !!!!`. From
  there to [m6502.asm:6671](m6502.asm#L6671), every bare number in the floating-point package is
  **octal**.
- `^O` forces octal anywhere; `^D` forces decimal.

So `CMPI 200` means 128 inside the math package and 200 outside it. Every numeric literal quoted in
these documents from the math-package range is given in decimal with the octal source form in
parentheses.

---

## 2. Configuration

The source is one codebase targeting six machines, selected by `REALIO` at
[m6502.asm:10](m6502.asm#L10), plus about a dozen feature switches. There are roughly 330
`IFE`/`IFN` blocks.

| `REALIO` | Target |
|---|---|
| 0 | PDP-10 simulating a 6502 (adds a `DDT` statement) |
| 1 | MOS Technology KIM-1 |
| 2 | Ohio Scientific |
| 3 | Commodore PET |
| 4 | **Apple II — the value checked in** |
| 5 | STM |

**Everything in these documents uses the configuration actually checked in**, unless a difference is
called out explicitly:

```
REALIO=4  INTPRC=1  ADDPRC=1  LNGERR=0  TIME=0   EXTIO=0  DISKO=0
NULCMD=0  GETCMD=1  RORSW=1   ROMSW=1   LONGI=1  KIMROM=0
BUFPAG=2 (BUF=512)  BUFLEN=240  LINLEN=40  CLMWID=14  STKEND=507
NUMLEV=23  STRSIZ=3  NUMTMP=3  ROMLOC=2048 (^O4000)  RAMLOC=^O25000
```

That choice matters: `ADDPRC=1` makes floats 5 bytes rather than 4 and changes almost every
structure size, and `INTPRC=1` adds `%` integer variables. The full switch-by-switch effect table is
in [12-config-matrix.md](internals/12-config-matrix.md).

One hazard is worth stating up front, because it constrains any edit to the source and any
reimplementation that wants to stay switch-compatible: **the statement dispatch table `STMDSP`, the
function dispatch table `FUNDSP`, and the reserved-word list `RESLST` are three parallel tables
guarded by the same conditionals.** A token's numeric value is its ordinal position in `RESLST`, so
adding or removing a word under one conditional silently renumbers everything after it in all three.

---

## 3. The big picture

### 3.1 Memory map

```
        +--------------------------------------------------+
  $0000 | START / RDYJSR / ADRAYI / ADRGAY / USRPOK        |  vectors, patched by INIT
  $000D | flags: CHARAC ENDCHR COUNT DIMFLG VALTYP INTFLG  |
        | DORES SUBFLG INPFLG TANSGN CNTWFL                |
  $0018 | (hole - reserved for the Apple monitor)          |  ORG 80 on REALIO=4
  $0050 | TRMPOS LINWID NCMWID LINNUM                      |  terminal + line-number staging
  $0055 | TEMPPT LASTPT TEMPST INDEX1 INDEX2 RES*          |  string temps, scratch
  $006A | TXTTAB VARTAB ARYTAB STREND FRETOP FRESPC MEMSIZ |  the dynamic-storage pointers
  $0078 | CURLIN OLDLIN OLDTXT DATLIN DATPTR INPPTR        |  interpreter position
  $0084 | VARNAM VARPNT FORPNT OPPTR OPMASK DEFPNT DSCPNT  |  evaluation scratch
  $0093 | JMPER TEMPF1 HIGHDS HIGHTR TEMPF2 LOWDS LOWTR    |  trampoline + block-move pointers
  $00A0 | FAC (6) SGNFLG BITS  ARG (6)  ARISGN FACOV FBUFPT|  the floating accumulators
  $00B2 | CHRGET .. CHRRTS, RNDX                           |  self-modifying text fetcher
  $00FF | LOFBUF                                           |  <- page 0/1 boundary is deliberate
  $0100 | FBUFFR (16 bytes)                                |  FOUT's output buffer
  $0110 | ...the 6502 stack, growing down from STKEND...   |
  $0200 | BUF (BUFLEN bytes) - the input line              |  BUFPAG=2
        +--------------------------------------------------+
        | ROM: dispatch tables, reserved words, error text |  at ROMLOC
        |      all interpreter code, math package, INIT    |
        +--------------------------------------------------+
        | RAMLOC:                                          |
        | [TXTTAB] program text: linked list of lines      |
        | [VARTAB] simple variables, 7 bytes each          |
        | [ARYTAB] arrays                                  |
        | [STREND] free space                              |
        |          ...                                     |
        | [FRETOP] string data, allocated downward         |
        | [MEMSIZ] top of memory                           |
        +--------------------------------------------------+
```

The four regions between `TXTTAB` and `MEMSIZ` are the whole of BASIC's dynamic state. They are kept
adjacent and are shuffled by block moves whenever any of them grows — there is no allocator, no free
list, and no fragmentation, only compaction. Program text, simple variables and arrays grow upward
and collide with string space growing downward; the collision is what triggers garbage collection
and, failing that, `?OM ERROR`.

### 3.2 Execution pipeline

```
   terminal
      |
      v
   INLIN ------------------> BUF            raw characters, echoed, with rubout handling
      |
      v
   CRUNCH -----------------> BUF            reserved words replaced by tokens (128 + ordinal)
      |
      +-- line starts with a digit? --> LINGET, then insert/delete/replace in the program
      |                                 (FNDLIN, BLTU, LNKPRG) and go back to MAIN
      |
      +-- otherwise -----------------> execute directly out of BUF
                                        |
                                        v
                              +--> NEWSTT  <-------------------------+
                              |      |                               |
                              |      | ISCNTC (^C), save OLDTXT      |
                              |      | cross a line? load CURLIN     |
                              |      v                               |
                              |    GONE3/GONE2 -- token >= 128? ----+ |
                              |      |            no: implied LET   | |
                              |      v                              | |
                              |    STMDSP[token] via RTS-to-addr-1  | |
                              |      |                              | |
                              |      v                              | |
                              |    statement handler                | |
                              |      |                              | |
                              |      +-- needs a value? --> FRMEVL -+ |
                              |      |                        |       |
                              |      |                        v       |
                              |      |                      EVAL -> FIN / PTRGET / ISFUN
                              |      |                        |       |
                              |      |                        v       |
                              |      |                       FAC      |
                              |      v                                |
                              +----- RTS ------------------------------+
```

Four things about this loop shape a reimplementation more than anything else:

1. **Statements return by `RTS` to `NEWSTT`.** A handler that pushes a long-lived stack frame
   (`FOR`) or that abandons the current position (`STOP`, `LIST`) must first discard `NEWSTT`'s
   return address and jump back to `NEWSTT` itself.
2. **The current text position is `TXTPTR`, and it is the operand field of a live instruction.**
   See [03-tokenizer-editor.md](internals/03-tokenizer-editor.md).
3. **The 6502 hardware stack is a typed, linearly-scanned data structure**, not just a return-address
   stack. It holds `NEWSTT` return addresses, 18-byte `FOR` frames, 5-byte `GOSUB` frames and
   10-byte expression-evaluation frames, each identified by a leading tag byte, and `FNDFOR` walks it
   looking for tags.
4. **Processor flags are part of the calling convention.** `CHRGET` returns the character in A *and*
   a carry meaning "is a digit" and a zero flag meaning "is a statement terminator", and callers
   several levels away depend on both.

---

## 4. The detail documents

| File | Contents |
|---|---|
| [01-memory-map.md](internals/01-memory-map.md) | Every page-zero cell with its address, the alias table, the adjacency constraints code depends on, page 1, the ROM/RAM split |
| [02-data-structures.md](internals/02-data-structures.md) | Byte-exact layouts: program lines, variables, arrays, string descriptors, `FOR`/`GOSUB`/expression stack frames, float formats |
| [03-tokenizer-editor.md](internals/03-tokenizer-editor.md) | `CHRGET`/`CHRGOT`, `CRUNCH`, the full token table, `LIST`, and the program line editor |
| [04-interpreter-loop.md](internals/04-interpreter-loop.md) | `NEWSTT`, dispatch, direct vs. program mode, the error handler and the full error list, `CONT`, the two memory guards |
| [05-statements.md](internals/05-statements.md) | Exact semantics of every statement |
| [06-expressions.md](internals/06-expressions.md) | `FRMEVL`, `EVAL`, `OPTAB`, relational operators, `AND`/`OR`/`NOT`, function dispatch |
| [07-variables-arrays.md](internals/07-variables-arrays.md) | `PTRGET`, name encoding, array creation and subscripting, `DEF FN` |
| [08-strings-gc.md](internals/08-strings-gc.md) | Descriptors, temporaries, allocation, the garbage collector, every string function |
| [09-float-format-arith.md](internals/09-float-format-arith.md) | The number format, FAC/ARG, the four arithmetic operations, comparison, integer conversion |
| [10-math-functions.md](internals/10-math-functions.md) | `FIN`, `FOUT`, `LOG`/`EXP`/`SQR`/`^`, `RND`, trigonometry, and every constant table decoded |
| [11-io-init.md](internals/11-io-init.md) | The terminal layer, `PRINT`'s column machinery, and the `INIT` startup sequence |
| [12-config-matrix.md](internals/12-config-matrix.md) | What each switch and each `REALIO` target changes |
| [13-porting-notes.md](internals/13-porting-notes.md) | Machine trick vs. real semantics, and the catalogue of defects found |

---

## 5. Porting hazards at a glance

Detail and line references in [13-porting-notes.md](internals/13-porting-notes.md).

| Hazard | Why it matters |
|---|---|
| Flags are the ABI | `CHRGET`'s (C, Z) contract and `FNDFOR`'s (Z, X, A) contract are consumed by distant callers; some code relies on carry left over from a *previous* routine |
| The stack is a typed structure | `FOR`/`GOSUB`/expression frames are scanned linearly by tag byte and truncated wholesale by `TXS`; a sentinel region below `STKEND` exists only so the scan terminates |
| `DSCTMP` aliases `FAC` | A *string* value "in the FAC" is really a pointer to a 3-byte descriptor held in `FACMO`/`FACLO`. Numeric and string values are not the same shape |
| One owner per string body | The garbage collector cannot cope with two descriptors pointing at the same data, which is the entire reason for the six-step string protocol and the copy-if-volatile rule |
| Floats are 32-bit mantissa, excess-128 | Not IEEE. Different rounding, no NaN/Inf/denormals, `INT` is floor rather than truncate. Bit-identical results require reimplementing the arithmetic, not calling `double` |
| Aliasing and adjacency are load-bearing | e.g. `REASON` saves 9 consecutive page-zero bytes as a block; `FIN` zeroes 11 consecutive bytes in one loop; `DIMFLG` and `VALTYP` are read as a pair |
| Known defects in the 1978 code | Apple's ^C check never breaks; `AYINT` rejects exactly -32768; the `RND` constants read a stray adjacent byte; `INIT` copies only 4 of 5 seed bytes; the two copies of `CHRGET` are not identical |

---

## 6. Conventions used in these documents

- Every non-obvious claim cites its source as a link, e.g. [m6502.asm:3169](m6502.asm#L3169).
- Numbers are decimal. Where the source literal is octal (the `RADIX 8` region), the octal form
  follows in parentheses: "128 (`200`)".
- Code excerpts are quoted as they appear, except that macro forms are occasionally expanded inline
  when the macro hides the point being made.
- Original label names and spellings are preserved exactly, including the 1978 misspellings.
- These documents describe; they do not prescribe. Where the code is defective, that is recorded as
  a fact with a decision left open, not corrected.
