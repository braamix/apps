# 11 — Terminal I/O and system initialization

---

## 1. The terminal layer

Everything BASIC does to a terminal goes through four routines. Each `REALIO` target supplies its
own implementations of the two lowest.

| Routine | Role |
|---|---|
| `INCHR` | fetch one character |
| `OUTDO` | emit one character, maintaining `TRMPOS` and wrapping |
| `INLIN` | read a whole line into `BUF`, with editing |
| `CRDO` / `CRFIN` | newline, with optional null padding |

### 1.1 `INCHR`

[m6502.asm:1742-1771](../m6502.asm#L1742-L1771).

| Target | Source |
|---|---|
| `REALIO=0` | `TJSR INSIM##` — the PDP-10 simulator |
| `REALIO=1` | `JSR ^O17132` — the KIM monitor |
| `REALIO=2` | poll `^O176000`, then read `^O176001`, mask to 7 bits |
| `REALIO=3` | `JSR CQINCH` — a Commodore ROM vector |
| `REALIO=4` | `JSR CQINCH` (the Apple monitor at `$FD0C`), then `ANDI 127` |

After the target-specific part, every non-simulator build checks for `CONTW` (^O) and, if seen,
complements `CNTWFL` — the output-suppression toggle. With `EXTIO`, this is skipped for
non-terminal channels.

### 1.2 `INLIN`

[m6502.asm:1673-1741](../m6502.asm#L1673-L1741). The contract, from
[m6502.asm:1673-1680](../m6502.asm#L1673-L1680): read into `BUF` using backarrow/underscore as the
character delete and `@` as the line delete; beyond `BUFLEN` characters, stop echoing and emit a
^G for each extra one. Returns a pointer to `BUF-1` in `[X,Y]`.

The Apple version ([m6502.asm:1681-1699](../m6502.asm#L1681-L1699)) is quite different: it delegates
the whole line edit to the monitor's `GETLN`, then truncates to `BUFLEN-1`, plants a terminating
zero, and **masks bit 7 off every byte of the buffer** — the Apple monitor returns characters with
the high bit set, which would otherwise look like tokens to `CRUNCH`.

The generic version filters input: characters below 32 and at or above 125 are discarded silently,
except carriage return. So control characters cannot be typed into a program line on those machines.

### 1.3 `OUTDO`

[m6502.asm:2818-2848](../m6502.asm#L2818-L2848). Covered in
[05-statements.md](05-statements.md#25-crdo-and-outdo). The four responsibilities are output
suppression, `TRMPOS` maintenance, automatic wrap at `LINWID`, and the per-target call.

### 1.4 Prompting

`QINLIN` ([m6502.asm:2941-2947](../m6502.asm#L2941-L2947)) is `OUTQST` + `OUTSPC` + `INLIN` — it emits
`"? "` before reading. This is why `INPUT` shows `?` and why the startup questions read
`MEMORY SIZE? ` and `TERMINAL WIDTH? `.

---

## 2. `PRINT`'s column machinery

See [05-statements.md](05-statements.md#2-print). The relevant point for initialization is that
`LINWID` and `NCMWID` are **RAM bytes with preloaded values**, not constants:

```
LINWID: LINLEN          ; 40
NCMWID: NCMPOS          ; ((LINLEN/CLMWID)-1)*CLMWID = 14
```

([m6502.asm:798-800](../m6502.asm#L798-L800))

They are preloaded in the source image and rewritten by `INIT`, because in a ROM build the source
image's page-zero values never reach RAM.

---

## 3. `INIT`, step by step

[m6502.asm:6670-6907](../m6502.asm#L6670-L6907). `RADIX 10` resumes at
[m6502.asm:6671](../m6502.asm#L6671). The source notes that this block "SHOULD BE LOCATED WHERE IT WILL
BE WIPED OUT IN RAM IF CODE IS ALL IN RAM"
([m6502.asm:6672-6673](../m6502.asm#L6672-L6673)) — in a RAM build, the initialization code becomes
program storage once it has run.

### 3.1 Vectors and flags

1. `CURLIN+1 = 255` — make it look like direct mode, so an error during startup prints sanely.
2. `TXS` with `STKEND-256`, i.e. `S = 251` for this build.
3. `START+1` and `RDYJSR+1` both point at `INIT`, so **any error before startup finishes restarts
   startup**.
4. Publish `ADRAYI = AYINT` and `ADRGAY = GIVAYF` — the two conversion routines, exported for
   machine-language callers.
5. Write the `JMP` opcode (76) into `START`, `RDYJSR` and `JMPER`; with `ROMSW`, also into `USRPOK`,
   whose target is set to `FCERR` so `USR` errors until the user `POKE`s it.
6. `LINWID = LINLEN`, `NCMWID = NCMPOS`. The comment at
   [m6502.asm:6727-6730](../m6502.asm#L6727-L6730) explains why these must be non-zero: with
   `BUFPAG=0` they sit immediately before `BUF` and serve as the fake link that `CHEAD` reads after
   a new line is moved into the program.

### 3.2 Copying `CHRGET` into RAM

```
        LDXI RNDX+4-CHRGET      ; = 28
MOVCHG: LDA INITAT-1,X
        STA CHRGET-1,X
        DEX
        BNE MOVCHG
```

28 bytes: the 24-byte `CHRGET` routine plus **four** of the five `RNDX` seed bytes. This copy is
unconditional, so it happens in RAM builds too — and the template is not byte-identical to the RAM
listing. See
[03-tokenizer-editor.md](03-tokenizer-editor.md#12-there-are-two-different-copies-of-chrget).

### 3.3 More RAM constants

7. `FOUR6 = STRSIZ` (3), `BITS = 0` (the `SHIFTR` fill), `LASTPT+1 = 0`, `CNTWFL = 0`, and a zero
   pushed on the stack so `FNDFOR`'s scan terminates.
8. With `BUFPAG≠0`, write 1 into `BUF-3` and `BUF-4` — the explicit substitute for the page-zero
   adjacency trick.
9. `TEMPPT = TEMPST`.

### 3.4 Sizing memory

[m6502.asm:6759-6819](../m6502.asm#L6759-L6819).

```
        print "MEMORY SIZE"
        JSR QINLIN                      ; prints "? " and reads a line
        (KIMROM=0) if the answer is "A", print the authors' names and restart INIT
        if the answer is empty: probe RAM
                start at RAMLOC (ROMSW=1) or LASTWR (ROMSW=0)
LOOPMM:         INC LINNUM
LOOPM1:         write 85, read back; if different -> that is the top
                write 170 (85 shifted), read back; if different -> that is the top
        else: LINGET the number, require a terminator
USEDEC: MEMSIZ = FRETOP = the result
```

The probe writes `0x55` then `0xAA` — complementary bit patterns, so a bus line stuck either way is
detected — and stops at the first address that does not read back. `REALIO=2` additionally checks
for wrapping into page zero and plants a `JMP` opcode there
([m6502.asm:6801-6807](../m6502.asm#L6801-L6807)).

For `REALIO=0` with `LONGI=0`, `MEMSIZ` is simply 16190, which the source calls "A STRANGE NUMBER"
([m6502.asm:6817](../m6502.asm#L6817)).

Typing `A` at the prompt prints `WRITTEN BY WEILAND & GATES`
([m6502.asm:6700-6702](../m6502.asm#L6700-L6702), [m6502.asm:6913-6919](../m6502.asm#L6913-L6919)) and
then re-enters `INIT`.

### 3.5 Terminal width

[m6502.asm:6820-6843](../m6502.asm#L6820-L6843).

```
TTYW:   print "TERMINAL WIDTH"    ; just "WIDTH" when KIMROM
        JSR QINLIN
        empty answer -> keep the default
        LINGET; reject a value >= 256; reject a value < 16
        LINWID = the value
MORCPS: SBCI CLMWID / BCS MORCPS  ; A mod CLMWID
        EORI 255 / SBCI CLMWID-2 / CLC / ADC LINWID
        STA NCMWID
```

> The runtime formula and the compile-time default disagree. `MORCPS` computes
> `NCMWID = LINWID - (LINWID mod CLMWID)`, which for `LINWID=40`, `CLMWID=14` gives **28**. The
> compile-time `NCMPOS = ((LINLEN/CLMWID)-1)*CLMWID` gives **14**. So answering `40` to the width
> question produces different comma-zone behaviour than accepting the default. See
> [13-porting-notes.md](13-porting-notes.md#3-defects-in-the-1978-code).

### 3.6 Deleting the trigonometric functions

[m6502.asm:6845-6870](../m6502.asm#L6845-L6870). Assembled **only when `ROMSW=0`** — so not in this
build.

```
ASKAGN: print "WANT SIN-COS-TAN-ATN"
        "Y" -> keep everything
        "A" -> keep all but ATN:  STXY ATNFIX with FCERR
        "N" -> STXY ATNFIX, COSFIX, TANFIX, SINFIX with FCERR
```

`SINFIX`, `COSFIX`, `TANFIX` and `ATNFIX` are labels on the `FUNDSP` dispatch slots
([m6502.asm:1071-1074](../m6502.asm#L1071-L1074)). Overwriting a slot with `FCERR` turns the function
into an `?FC ERROR` and frees the code from `COS` (or `ATN`) upward for program text — the tokens
still exist, but nothing dispatches to the routines.

This is why `FUNDSP` entries for the four trigonometric functions have individual labels while no
other entry does.

`KIMROM=1` achieves the same at assembly time by emitting four `ADR(FCERR)` entries instead
([m6502.asm:1066-1070](../m6502.asm#L1066-L1070)).

### 3.7 Finishing

[m6502.asm:6873-6907](../m6502.asm#L6873-L6907).

```
        (ROMSW=1) TXTTAB = RAMLOC
        write a zero byte at TXTTAB, then INC TXTTAB    ; the zero in front of the program
QROOM:  REASON on TXTTAB                                 ; ?OM if it collides with FRETOP
        print the free byte count: MEMSIZ - TXTTAB, via LINPRT (unsigned)
        print WORDS ... and the banner
        JSR SCRTCH                                       ; NEW: set VARTAB, ARYTAB, STREND, DATPTR
        RDYJSR+1 = STROUT      ; RDYJSR becomes "JMP STROUT"
        START+1  = READY       ; location 0 becomes "JMP READY"
        JMPD START+1
```

The last three lines are the point of no return: location 0 stops meaning "initialize" and starts
meaning "return to the prompt", and `RDYJSR` stops routing errors back into `INIT`.

### 3.8 The startup messages

[m6502.asm:6909-6948](../m6502.asm#L6909-L6948).

```
MEMORY: "MEMORY SIZE", 0
TTYWID: "TERMINAL WIDTH", 0          ; "WIDTH" only, when KIMROM
WORDS:  " BYTES FREE"
        (REALIO != 3) CR LF CR LF
        (REALIO = 3)  15, 0, FREMES:
        (REALIO = 0)  "SIMULATED BASIC FOR THE 6502 V1.1"
        (REALIO = 1)  "KIM BASIC V1.1"
        (REALIO = 2)  "OSI 6502 BASIC VERSION 1.1"
        (REALIO = 3)  "### COMMODORE BASIC ###" 15 15
        (REALIO = 4)  "APPLE BASIC V1.1"
        (REALIO = 5)  "STM BASIC V1.1"
        (REALIO != 3) CR LF "COPYRIGHT 1978 MICROSOFT" CR LF
        0
```

For every target but the Commodore, `WORDS` and the banner are **one contiguous NUL-terminated
string**, so the Apple startup order is:

```
<CR>
<free byte count> BYTES FREE

APPLE BASIC V1.1
COPYRIGHT 1978 MICROSOFT
```

— the sign-on banner comes *after* the free-memory line. The Commodore prints `FREMES` before the
count instead, and terminates `WORDS` with a single CR.

`LASTWR` follows, reserving 100 bytes of temporary stack; for `REALIO=0` a further 13600-byte
`TSTACK` is reserved.

---

## 4. `LOAD` and `SAVE`

Only assembled when `DISKO=1`, which this build does not set.

| Target | Mechanism |
|---|---|
| KIM (`REALIO=1`) | cassette, [m6502.asm:2281-2322](../m6502.asm#L2281-L2322). Stashes the stack pointer in `INPFLG`, patches location 1 as a return vector, and finishes at `FINI` so the program is relinked |
| Commodore (`REALIO=3`) | external ROM vectors `CQLOAD`, `CQSAVE`, `CQVERF` |
| Apple (`REALIO=4`) | cassette code exists at [m6502.asm:2323-2363](../m6502.asm#L2323-L2363) but, because `DISKO=0`, has **no token and no dispatch entry** — it is dead code in this build |
| others | `LOAD`/`SAVE` stubs, or two `ADR(511)` placeholders in `STMDSP` |

The KIM version's relink-on-load is necessary because the links are absolute addresses, so a program
loaded at a different address than it was saved from would otherwise be corrupt.
