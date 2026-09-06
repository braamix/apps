# 12 — Configuration matrix

The switches are declared at [m6502.asm:10-107](../../m6502.asm#L10-L107). Roughly 330 `IFE`/`IFN` blocks
depend on them.

Setting `REALIO` is not a single choice: the per-target block below it **overrides other switches**,
so the effective configuration is the cascade, not the literal values at the top of the file.

---

## 1. `REALIO` — the target machine

| Value | Machine | Forced settings |
|---|---|---|
| 0 | PDP-10 simulating a 6502 | `ROMLOC=^O20000`, `RAMLOC=^O1400` |
| 1 | MOS Technology KIM-1 | `GETCMD=1`, `DISKO=1`, `ROMLOC=^O20000`, `RORSW=0`, external `OUTCH`, `CZGETL` |
| 2 | Ohio Scientific | `RORSW=0`, `RAMLOC=^O1000` (or `^O100000` with ROM), external `OUTCH` |
| 3 | Commodore PET | `DISKO=1`, `EXTIO=1`, `TIME=1`, `GETCMD=1`, `NULCMD=0`, `ROMSW=1`, `RORSW=1`, `LINLEN=40`, `BUFLEN=81`, `CLMWID=10`, `BUFPAG=2`, `STKEND=507`, `PI=255`, and a long list of external vectors |
| 4 | **Apple II — the checked-in value** | `RORSW=1`, `NULCMD=0`, `GETCMD=1`, `LINLEN=40`, `BUFLEN=240`, `BUFPAG=2`, `STKEND=507`, `ROMLOC=^O4000`, `RAMLOC=^O25000`, external `CQINLN`/`CQINCH`/`OUTCH` |
| 5 | STM | banner only; behaves like a generic build |

### 1.1 Per-target behavioural differences

| Area | Difference |
|---|---|
| Page-zero head | `REALIO=3` has **no** `START`, `RDYJSR`, `ADRAYI`, `ADRGAY` ([m6502.asm:731](../../m6502.asm#L731)); `READY` calls `STROUT` directly and there is no restart vector at location 0 |
| Apple page zero | `ORG 80` at [m6502.asm:790](../../m6502.asm#L790) leaves 24-79 for the Apple monitor |
| `TRMPOS` | external (`^O306`) on the Commodore, page zero elsewhere |
| `ISCNTC` | absent for `REALIO=0`; separate implementations for 1, 2 and 4; external vector on 3 |
| `INLIN` | Apple delegates to the monitor and masks bit 7 off the buffer; OSI echoes characters itself; Commodore uses an external vector |
| `PI` | only `REALIO=3` recognises character 255 as pi, in `CRUNCH`, `EVAL` and `LIST` |
| `LIST` quote mode | only `REALIO=3` tracks quotes (in `DORES`) so that tokens inside strings list literally |
| `CLEAR` keyword | spelled `CLR` on the Commodore |
| `READY` message | `"READY."` on the Commodore, `"OK"` elsewhere |
| `PRINT` width check | omitted on the Commodore |
| `OUTSPC` | emits character 29 (cursor right) on the Commodore |
| `PEEK` | returns 0 inside the BASIC ROM on the Commodore ([m6502.asm:4811-4815](../../m6502.asm#L4811-L4815)) |
| `RND(positive)` | reads the VIA timers on the Commodore instead of advancing the sequence |
| `NOTFNS` caller check | omits the high-byte test on the Commodore |
| `DDT` statement | only `REALIO=0` |
| Easter egg | only `REALIO=3` ([10-math-functions.md](10-math-functions.md#10-the-commodore-easter-egg)) |
| `GARBA2` bell | only when `REALIO=0` and `DISKO=0` |

---

## 2. `ADDPRC` — extra precision

`1` here. The most pervasive switch in the source: it adds a fifth byte to every float.

| Affected | `ADDPRC=0` | `ADDPRC=1` |
|---|---|---|
| Packed float | 4 bytes | 5 bytes |
| Mantissa precision | 24 bits | 32 bits |
| Simple variable entry | 6 bytes | 7 bytes |
| Float array element | 4 | 5 |
| Integer array element | (unsupported) | 2 |
| `FOR` frame (`FORSIZ`) | 16 | 18 |
| Expression frame | 9 | 10 |
| `FBUFFR` | 13 | 16 |
| `GETSTK` reserve | +13 | +16 |
| `RNDX` seed | 4 bytes | 5 |
| `REASON`'s saved block | 8 bytes | 9 |
| `FOUT` digits | 6 | 9 |
| Extra page-zero cells | — | `RESMOH`, `FACMOH`, `ARGMOH`, plus a filler in each of `TEMPF1`/`TEMPF2`/`TEMPF3` |
| Coefficient tables | separate shorter tables with lower degrees | the tables listed in [10-math-functions.md](10-math-functions.md#9-constant-tables) |

`INTPRC` is only correct when `ADDPRC=1`; the source says so at
[m6502.asm:3932-3934](../../m6502.asm#L3932-L3934) and [m6502.asm:4055](../../m6502.asm#L4055).

---

## 3. `INTPRC` — integer variables

`1` here. Adds the `%` suffix and the `INTFLG` page-zero byte.

| Site | Change |
|---|---|
| `PTRGET` | recognises `%`, sets `INTFLG`, sets bit 7 of *both* name bytes, rejects `%` when `SUBFLG` is set |
| `LET` | stacks `INTFLG` around `FRMEVL`; `QINTGR` stores 2 big-endian bytes |
| `ISVAR` / `GOOO` | reads an integer scalar as two big-endian bytes and floats it |
| `ISARY` | packs `DIMFLG` and `INTFLG` into one stacked byte |
| array creation | element size 2 |
| garbage collection | `DVARS` skips entries whose first name byte is negative |
| `READ` / `INPUT` | numeric store goes through `QINTGR` |

See [07-variables-arrays.md](07-variables-arrays.md#3-why-integers-exist-but-barely-help) — integer
*scalars* are slower than floats; only arrays benefit.

---

## 4. `LNGERR` — long error messages

`0` here. Selects between the two-character `ERRTAB` and the spelled-out one.

**It changes the numeric value of every error code**, because the codes are byte offsets into two
differently shaped tables. It also changes the print loop in `ERROR` (a fixed two characters versus
a bit-7-terminated scan). With `LNGERR=1` the total message text must stay under 256 bytes.

---

## 5. `EXTIO` — external I/O channels

`0` here. Adds:

- the `CHANNL` page-zero byte;
- six reserved words — `INPUT#`, `PRINT#`, `CMD`, `SYS`, `OPEN`, `CLOSE` — with matching `STMDSP`
  entries, renumbering every later token;
- the `FD` error code, shifting `ST`, `CN` and `UF` by 2;
- channel-aware behaviour throughout `PRINT` (`TRMPOS` and automatic newlines suppressed on
  non-terminal channels), `INPUT`, `ERROR` (close the channel first) and `CLEARC` (close all);
- `IODONE` / `IORELE`.

---

## 6. `DISKO` — `LOAD` and `SAVE`

`0` here. Adds `LOAD` and `SAVE` (plus `VERIFY` on the Commodore) to `RESLST` and `STMDSP`,
renumbering later tokens by 2 or 3. For targets with no implementation it emits two `ADR(511)`
placeholders.

The Apple cassette code exists regardless but is unreachable without this switch.

---

## 7. `ROMSW` — is the interpreter in ROM?

`1` here.

| `ROMSW=0` | `ROMSW=1` |
|---|---|
| everything in RAM at `LASTWR` | code at `ROMLOC`, data at `RAMLOC` |
| `INIT` may delete `SIN`/`COS`/`TAN`/`ATN` by patching `FUNDSP` | that code is not assembled |
| no `USRPOK` | `USRPOK` in page zero, so `USR` can still be redirected |
| `TXTTAB` is **never stored by `INIT`** — see the defects list | `TXTTAB = RAMLOC` |
| a `BLOCK 1` pad precedes `INITAT` | no pad |

---

## 8. `BUFPAG` — where the input buffer lives

`2` here (so `BUF` = 512).

| `BUFPAG=0` | `BUFPAG != 0` |
|---|---|
| `BUF` is in page zero, right after `LINNUM` | `BUF` is at `BUFPAG*256` and must be page-aligned |
| `LINWID`,`NCMWID`,`LINNUM`,comma form the link + line-number prefix that `STOLOP` copies | `INIT` writes 1 into `BUF-3`/`BUF-4`, and `NODEL` copies `LINNUM` into `BUF-2` by hand |
| direct mode is `TXTPTR+1 == 0` | direct mode is `TXTPTR+1 == BUFPAG` |
| `CRUNCH`'s `BUFOFS` is 0 | `BUFOFS` is `BUF`'s page base |
| `DIRCON:` label after `LDYI 0` | before it |
| `CRDONE` does not adjust `TXTPTR+1` | it does |

The 1978-02-25 note ([m6502.asm:227](../../m6502.asm#L227)) records a bug where `INPFLG` was set wrongly
when `BUFPAG != 0`.

---

## 9. The remaining switches

| Switch | Value here | Effect |
|---|---|---|
| `NULCMD` | 0 | adds the `NULL` token and statement, the `NULCNT` page-zero byte, and the null-padding form of `CRFIN` |
| `GETCMD` | 1 | adds the `GET` token and statement, and the `BVS`/`BVC` branches through the input machinery |
| `TIME` | 0 | adds `TI` and `TI$` handling in `INPCOM`, `ISVAR` and `NOTEVL`, and the base-60 extension to `FOUTBL` |
| `RORSW` | 1 | when 0, defines a `ROR` macro emulating the instruction for early 6502s whose `ROR` was broken |
| `KIMROM` | 0 | when 1, `FUNDSP`'s four trigonometric slots are assembled as `ADR(FCERR)`; the tokens still exist. Forced to 0 unless `ROMSW=1` and `REALIO=1` ([m6502.asm:34-35](../../m6502.asm#L34-L35)) |
| `LONGI` | 1 | the "long initialization" switch; with `REALIO=0` and `LONGI=0`, `INIT` skips the questions and hard-codes `MEMSIZ=16190` |
| `NUMLEV` | 23 | guaranteed expression nesting levels, enforced by `GETSTK` |
| `STRSIZ` | 3 | string descriptor size |
| `NUMTMP` | 3 | number of string temporaries |
| `CLMWID` | 14 | `PRINT` comma field width (10 on the Commodore) |
| `LINLEN` / `BUFLEN` | 40 / 240 | terminal width and input buffer size |
| `STKEND` | 507 | top of the usable 6502 stack |

---

## 10. The coupled tables

This is the constraint that makes the switches dangerous.

`RESLST` ([m6502.asm:1112](../../m6502.asm#L1112)), `STMDSP` ([m6502.asm:997](../../m6502.asm#L997)) and
`FUNDSP` ([m6502.asm:1055](../../m6502.asm#L1055)) are three parallel tables, each guarded by the same
conditionals, and the connection between them is purely positional:

- A token's value is `128 + its ordinal in RESLST`.
- The statement dispatcher indexes `STMDSP` by `token - ENDTK` and range-checks against
  `SCRATK - ENDTK + 1`.
- The function dispatcher indexes `FUNDSP` by `token - ONEFUN`, with `LASNUM` separating the
  one-argument functions from the rest.

So `ENDTK`, `SCRATK`, `ONEFUN` and `LASNUM` are all derived from positions, and adding one word
under one conditional shifts every later token in all three tables at once. Adding a statement means
adding it to `RESLST` *and* `STMDSP` *at the same position, under the same conditional*, or the
dispatch silently goes to the wrong handler.

`OPTAB` ([m6502.asm:1084](../../m6502.asm#L1084)) is a fourth table with a positional link: `FRMEVL`
computes its index as `3 * (token - PLUSTK)`, so the seven binary operators must stay contiguous and
in the same order as in `RESLST`. The relational operators are similarly required to be adjacent and
ordered `>`, `=`, `<`.
