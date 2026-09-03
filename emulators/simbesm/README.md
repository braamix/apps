# BESM-6 simulator

A simulator of the **BESM-6** (БЭСМ-6), the most widely used Soviet mainframe of
the 1960s–80s: a 48-bit machine, octal throughout, with sign-magnitude floating
point. It boots **Unix**, and everything needed to do that is in [data/](data/).

A fork of [Open SIMH](https://github.com/open-simh/simh) reduced to this one
simulator and ported to [Braam](https://braamix.github.io).

## Build and run

Two builds, one machine. Under the Braam toolchain it is a wasm32 program;
configured without one it is a native binary, which is what
[tests/unix.exp](tests/unix.exp) drives and how every step of the port was
checked. The difference between them is one file — [braam.cpp](braam.cpp) or
[host.cpp](host.cpp) — because everything that blocks is there and nowhere else.
Only `braam.cpp` encodes keys: termios hands the native build the escape
sequences a real terminal already sent, and doing the same on Braam is what
makes the two agree (Keys, below).

```sh
make                      # the native build, in build/besm6
cd data && ../build/besm6 # boot Unix and get a shell; ^E stops it

make -C ../..             # the Braam program, in build/emulators/simbesm/
```

The Braam build needs an SDK with `PROC_ABI` 20 — `Sys::TermOpen`, which the
second screen is — and one carrying `proc/keyenc.h`, which is the key encoder
below. Neither release is out yet. Until they are:

```sh
make -C ../../../braam-core release
make -C ../.. SDK=../braam-core/build/sdk
```

On Braam the program is `besm6`, from the `simbesm` package:

```text
besm6 [-r] [-S <screen>]
    -r           copy the packs from the store again, discarding this Unix
    -S <screen>  the second Consul line's terminal, or `none' to turn it off
```

The packs are written in place and the store is read-only, so the first run
copies them to `$HOME/.besm6` and creates the drums beside them. `$BESM6_PREFIX`
overrides where they are copied from.

`make test` here runs [tests/unix.exp](tests/unix.exp), which needs
`/usr/bin/expect`. [test/boot.mjs](test/boot.mjs) does the same under
braam-core's system harness, on two screens, and is what `make test` at the top
of the tree runs.

## What the port changed

The BESM-6 sources keep upstream's structure; the SIMH framework does not exist
any more. Four files replace it — 2.6k lines became 1.2k:

| | |
|---|---|
| [machine.h](machine.h) / [.cpp](machine.cpp) | types, devices, the event queue, the clock, the driver |
| [image.h](image.h) | disks and drums, as a file of words |
| [console.h](console.h) | the terminal lines |
| [debug.h](debug.h) | the one output path |

**The rule they exist for is that nothing below `cpu_burst()` may block.** On
Braam a read, a write and a sleep are coroutines; a coroutine cannot be entered
from a plain function, and making the instruction loop one is worse — a
`co_await` is a call and not a tail call, so awaiting without suspending grows
the native stack until the process traps. So `cpu_burst()` runs plain C++ until
it has something for its caller to do — a transfer, the burst being up, or a
stop — and says which. Three consequences:

- **A disk transfer is deferred** out of the instruction that starts it, and is
  *data* the driver performs rather than a function to call. Upstream did the
  host `fread()` inside the `увв` instruction and deferred only the completion
  interrupt. The guest waits for its `ГРП` interrupt either way, so this is the
  more faithful model too, and no instruction runs in between: the packs a boot
  writes are byte-for-byte what they were.
- **The console is a buffer the driver drains and a ring a task fills**, so
  `con_put()` and `con_get()` can still be reached from inside an instruction,
  as upstream's non-blocking `read(0)` and its `write(1)` per character were.
- **The stop key is Alt+Q**, on either Consul line, because there is no `VINTR`
  and no `SIGINT` here: the key task recognises the chord before it encodes the
  key, which is the order that matters — Alt+Q would otherwise go out as
  `ESC q` — and sets `stop_cpu`. `^E` is what stops the native build,
  where termios takes it before a read can see it; on Braam it is an ordinary
  byte and reaches the guest.
- **Traps return a stop code.** There is no `setjmp` on wasm32, and upstream's
  43 `longjmp`s to `cpu_halt` came from arbitrary depth — an operand protection
  fault out of `mmu_load()`, an overflow out of `besm6_add()`. Every routine
  that can raise one returns `status_t` and every caller checks (`CPU_TRY`);
  `cpu_trap()` is what the landing pad became.

Also gone: the telnet transport, the in-band `sim>` interpreter, the SCP command
language, and every peripheral Unix does not boot from — the line printer, punch
tape, cards and magnetic tape. Their I/O addresses are still decoded by
`cmd_033()`, but do nothing.

**Comments are English.** What stays Cyrillic is what *names* something: the
hardware's mnemonics (`ГРП`, `ПоП`, `БРЗ`, `КМД`, …), the two mnemonic sets the
disassembler prints, the `.b6` type letters, and the stop messages the machine
reports. Translating those would make the code disagree with the hardware
documentation and with what the simulator puts on the screen.

## Idling

**There is no wait-for-interrupt on this machine.** The only halt is `033 стоп`,
which every simulator treats as the end of the run, so a guest with nothing to do
spins — v7besm's kernel spins in `idle()` at `spl0` until the 250 Hz timer fires.
Run faithfully, that costs a whole host core to accomplish nothing, and in a
browser tab it is a core the page never gives back.

So the guest says so, and `cpu_burst()` returns `REASON_IDLE` for the driver to
sleep on. The way it says so is **`увв 0147`**, the power-supply control
register, which neither the machine nor any simulator does anything with —
upstream had already marked it "repurposed". v7besm writes it from inside the
spin. An *unassigned* `увв` would not do: it reaches `besm6_debug()`, which the
SIMH that tree's kernel tests boot under prints unconditionally.

**A simulator cannot work it out by watching, and it is worth knowing why.** The
obvious heuristic — a short backward branch taken over and over, in supervisor
mode, with interrupts open — was tried and measured: booting to a prompt it is
right, but during a `cat /bin/*` it fires 1478 times against one real idle, on
the kernel's own short read-only loops. A directory scan and a hash-chain walk
look exactly like a spin from outside. Sleeping through them cost 40 % of the
machine's throughput. Ruling out loops that *write* memory removed only half of
them. Hence the hint, and hence nothing else.

The sleep is **as long as the skipped instructions would have taken**, and is
charged to them (`sim_idle_ms()`, `sim_idle_skip()`). That is what keeps the
guest's clock right: `sim_rtcn_calb()` still sees a second's worth of ticks in a
real second, so the machine sees exactly `CLK_TPS` of them whether it idles for
four milliseconds or an hour. Nothing is skipped *past*, either — the sleep runs
to the head of the event queue, and every way the spin can end is an entry on it.
An event under a millisecond off is not slept for at all: a disk answers in 20
instructions, and paying a millisecond of real time for that would make every
transfer cost one.

An idle machine falls from 100 % of a core to about 2 %, and a busy one is not
slowed at all. `BESM6_NOIDLE=1` turns the whole thing off. A guest that does not
send the hint — DISPAK, or a v7besm kernel built before it — simply spins as it
always did.

## Machine model

- **Word.** 48 data bits plus a 2-bit tag marking it an *instruction* or a
  *number*. Fetching a data-tagged word raises a machine check.
- **Memory.** 512 K words, 15-bit addresses. Octal addresses `1`–`7` are the
  front-panel switches, not RAM.
- **Floating point.** Sign-magnitude, base-2 exponent.

Registers, indexed by the constants in [besm6_defs.h](besm6_defs.h). The
Cyrillic names are what traces and disassembly print.

| Cyrillic | Latin | Bits | Meaning |
|----------|-------|------|---------|
| `СчАС` | `PC` | 15 | Program counter. |
| `РК` | `RK` | 24 | Current instruction. |
| `Аисп` | `Aex` | 15 | Effective address. |
| `СМ` | `ACC` | 48 | Accumulator. |
| `РМР` | `RMR` | 48 | Low-order-bits register. |
| `РАУ` | `RAU` | 6 | ALU mode bits. |
| `М1`…`М17` | `M1`…`M17` | 15 | Index/modifier registers (М17 is also SP). |
| `М20` | `M20` | 15 | Address modifier (MOD). |
| `М21`, `М27` | `M21`, `M27` | 15 | Program status, saved status. |
| `М32`–`М33` | `M32`–`M33` | 15 | Extracode / interrupt return addresses. |
| `М34`, `М35` | `IBP`, `DWP` | 16 | Instruction breakpoint, data watchpoint. |
| `РУУ` | `RUU` | 9 | Execution-mode bits. |
| `ГРП` / `МГРП` | `GRP` / `MGRP` | 48 | Main interrupt register and mask. |
| `ПРП` / `МПРП` | `PRP` / `MPRP` | 24 | Peripheral interrupt register and mask. |

There is no command interpreter: to change what the simulator does, edit
[besm6_main.cpp](besm6_main.cpp), which performs the steps the old
`demo/unix.ini` script did. Options are plain assignments made before the run:

| C | Effect |
|---|--------|
| `besm6_latin = 1` | Disassemble as MADLEN (Latin) instead of БЕМШ. |
| `autotime = 1` | The front-panel date/time setup DISPAK expects at boot. |
| `GRP \|= GRP_PANEL_REQ` | Press the operator "request" button. |
| `pult_packet_switch = n` | Boot source: `0` = switch registers, `1`–`10` = a hardwired bootstrap. |
| `mmu_unit.flags \|= CACHE_ENB` | Model the БРЗ write cache. Accurate, ~20 % slower. |
| `mmu_unit.flags \|= CHECK_ENB` | Parity checking. |

## Terminals

The `TTY` device carries **24 serial lines** (`tty1`…`tty24`) plus **two
parallel "Consul" lines** (`tty25`, `tty26`). `tty25` is the console; `tty26` is
a **second screen**.

```c
tty_attach(&tty_unit[25], "console");   /* the program's own terminal */
tty_attach(&tty_unit[26], "screen2");   /* a terminal it opened */
tty_attach(&tty_unit[3],  "none");      /* mark the line unusable */
```

Those three words are all `tty_attach()` takes. Upstream also accepted
`Line=<n>,<port>` and listened for telnet; there is no socket in a browser tab,
and a second screen is where that line went instead.

**The second line wants a second canvas.** A page that mounts two —
`mount({ screens: [{canvas}, {canvas, shell: false}] })`, the shape of
`web/dual.html` — gets a Consul on each. The line is taken only when **both**
halves come: `Sys::TermOpen` is free, but a screen whose own shell sits at its
prompt holds the raw keys, and a terminal has to be typed at as well as printed
on. That is what `shell: false` is for. Without them `tty26` is turned off and
the machine runs with one console.

The open is **retried for a second** before it settles for one. A terminal
exists only once the page has measured its canvas, and that measurement arrives
from a `ResizeObserver` — which a hidden or throttled tab runs late. A program
`/etc/init` starts, with no prompt in front of it, can reach `Sys::TermOpen`
first, and losing the second Consul to that would be silent.

A line's mode is a character set, a terminal type and a backspace style, set
together in the unit's flag word **before** `tty_attach()`, which reads them:

```c
tty_unit[25].flags = (tty_unit[25].flags & ~TTY_CHARSET_MASK) | TTY_RAW8_CHARSET;
```

| Character set | |
|------|---------|
| `unicode` | UTF-8 in and out. |
| `jcuken` | Russian via the ЙЦУКЕН layout on Latin keys. |
| `qwerty` | Russian as transliterated Latin: `Q`=я, `W`=в, `Y`=ы, `J`=й, `X`=ь, `C`=ц, `V`=ж, `` ` ``=ю, `~`=ч, `{`=ш, `}`=щ, `\|`=э. |
| `raw` | No conversion, but the hardware's 7-bits-plus-parity contract holds. |
| `raw8` | The same, eight bits wide and with no parity. The guest owns the character set — what `v7besm` uses to carry UTF-8. |

Terminal type: `vt` (Videoton-340, the default), `tt` (MTK-2 Baudot teletype,
serial lines only), `consul` (Consul-254, lines 25/26 only), `off`. Backspace:
`destrbs` (erasing, the default) or `authbs`. Device-wide: `tty_rate`
(300…19200 Hz) and `tty_turbo` (interrupt timing follows model time when 1).

### Keys

There are no control characters on Braam: a key arrives as `Key{code, mods}`
and `^D` is `'d'` with the control modifier. So the program encodes, and the
table it encodes with is braam-core's
[ANSI_Escape_Codes.md](../../../braam-core/doc/ANSI_Escape_Codes.md) §5 —
`key_encode()` from the SDK's `proc/keyenc.h`, so nothing here spells the table
out. `con_feed_all()` puts a whole sequence in the ring or none of it:
half an escape would leave the guest waiting for a final byte that the next
keystroke would supply.

What goes out: a printable key in **UTF-8**, which is what makes `raw8`'s "the
guest owns the character set" true for input as well as output, and is why
Cyrillic can be typed at all; CR for Enter, HT for Tab, DEL for Backspace and
ESC for Escape; `ESC [ A` for the arrows and `ESC [ H`/`ESC [ F` for Home and
End, always CSI and never SS3; `ESC [ 5 ~` and its family; `ESC O P` for F1–F4,
which lose SS3 when modified; and `ESC [ 1 ; <m> A` when a named key carries a
modifier.

Four things never arrive. **Alt+Q** is taken as the stop chord above. **`^C`**
is Braam's own console pump's, before any raw claim, so the guest's `stty intr`
cannot be reached from here — natively `^E` does it, through `VINTR`. Shift with
PageUp, PageDown, Up or Down is Braam's scrollback gesture. And Ctrl+@ or
Ctrl+Space send NUL, which `vt_getc()` turns into BS as upstream had it.

## Peripherals

Disks and drums share a geometry: *zones* of `8 + 1024` words (8 service words,
then 1 Kword of data), each word an 8-byte little-endian record. Attach routines
are in [besm6_defs.h](besm6_defs.h); the switches the old `ATTACH` took ride in
`sim_switches` — `-n` creates a new image, `-e` requires an existing one.

**Magnetic disks `MD0`…`MD7`** — eight controllers of 8 units, `md_unit[0..63]`.
With `-n` the image is formatted and the volume number is taken from the digits
in the filename; it **must be 2048–4095**. Drive type is a unit flag:
`DISK_TYPE_7_25M` (1000 blocks) or `DISK_TYPE_29M` (4000).

**Magnetic drums `DRUM`** — two units, the paging and swap store.

```c
disk_attach(&md_unit[0], "root3072.disk");

sim_switches = SWMASK('N');                /* create empty */
drum_attach(&drum_unit[0], "unix0.drum");
sim_switches = 0;
```

## Booting Unix

`besm6_boot_unix()` in [besm6_main.cpp](besm6_main.cpp), in this order:

1. `besm6_latin = 1`, and `CACHE_ENB` so the БРЗ write-back cache is modelled —
   the kernel writes user memory through the map, so a build that only worked
   with the cache off would not have worked on the real machine.
2. `tty25` to `raw8` on the console; `tty26` to a second screen, or off.
3. Root pack on `md00`, `/usr` on `md01`, both writable; two drums created
   empty. The drums are **swapdev**, and `exece()` stages the argument list in
   swap before touching the new image — with no drum, every `exec` fails with
   `error 5`.
4. `sim_load()` on `unix`, a binary `a.out`, which sets the PC from its entry
   point; then the driver loop.

```text
phys mem  = 3072 kbytes
swap size = 3072 kbytes
root size = 6000 kbytes

Single-user mode -- type ^D to run /etc/rc and go multi-user
# _
```

Line editing at that prompt is the *kernel's*: `^?` erases a character, `^U`
kills the line. The arrow keys send escapes its line discipline does not
decode, so they insert bytes rather than recall anything — which is what a real
VT100 on a real BESM-6 did. `^D` ends the shell, `init` runs `/etc/rc` and the
machine comes up multi-user with a getty on each Consul line. **Alt+Q** stops
the run; natively it is `^E`, which termios takes as `VINTR`. Nothing calls
`sync(2)` for you.

There is no clock-calendar, so the date starts at whatever the filesystem was
stamped with; type `date` to set it.

The native build writes `data/`'s packs in place, which is why the regression
test copies them first; the Braam build copies them to `$HOME/.besm6` on its
first run.

## Loading programs

`sim_load()` in [besm6_sys.cpp](besm6_sys.cpp) reads a **text** file in DISPAK
format (`.b6`) and auto-detects binary `a.out` images. Each line starts with a
one-letter type code, Cyrillic or Latin, case-insensitive, followed by octal
operands; `;` starts a comment.

| Code | Meaning |
|------|---------|
| `в` / `b` | Set the load **address**. Loading starts at 1. |
| `п` / `p` | Set the **start address** (the PC). |
| `ч` / `f` | A **floating-point** number. |
| `с` / `c` | An octal **data word**, up to 16 digits. |
| `к` / `k` | One or two **instructions**, comma-separated. |

```text
в 1
к сл  7,  зп   11
к пе  6,  стоп
в 7
ч 1.0
п 1
```

Each word advances the load address by one; words below address `10` go to the
switch registers. `besm6_dump()` writes memory back out in the same format.

`fprint_sym()` prints an instruction in stop messages and in the CPU trace; the
switch bits it is given pick the format. One word, four ways:
`0000 2000 0000 0210` raw, `сч 4412(1)` as БЕМШ, `xta 4412(1)` as MADLEN,
`2.7e-20` as a real.

| Switch | Format |
|--------|--------|
| *(none)* | Four 12-bit octal groups. |
| `-m` / `-ml` | **БЕМШ** (Cyrillic) / **MADLEN** (Latin) mnemonics. |
| `-i` | Octal instruction fields: register, opcode, address. |
| `-f` | The word as a floating-point number. |
| `-b` / `-x` | Six octal bytes / 13 hexadecimal digits. |

## Debugging

`BESM6_DEBUG` names the trace file (`-` is stderr) and `BESM6_TRACE` the devices
to trace, comma separated; with `BESM6_DEBUG` unset nothing is traced. Setting a
device's `dctrl` field does the same without the environment.

```sh
BESM6_DEBUG=- BESM6_TRACE=cpu,mmu ./besm6     # trace to stderr
BESM6_DEBUG=run.log BESM6_TRACE=none ./besm6  # operator messages only
```

`sim_deb` is the trace file and `sim_con` the operator's console; both are
`Sink`s, a callback and its context, because there is no `FILE *` on Braam.
`besm6_debug()`, `besm6_log()` and `besm6_log_cont()` write to both — a format
starting with `_` goes to the file only, which is how operator dumps stay off a
terminal Unix is using. The disk device has named categories to OR into `dctrl`
instead of `~0`: `DEB_OPS`, `DEB_RRD`, `DEB_RWR`, `DEB_INT`, `DEB_TRC`,
`DEB_DAT`, `DEB_STA`.

`cpu_dev.dctrl` logs every executed instruction with the state it touches — the
address, `L`/`R` for the half-word, the octal fields, the mnemonic, then only
the registers that *changed* and any operand read or write:

```text
32012 R: 00 100 7766 зп -12
      Memory Write [77766] = 0000 0000 0000 0000
32013 L: 00 037 0000 ржа
      RAU = 00
```

The first line dumps every register; extracodes append their executive address
as `= addr`; faults get a line of their own. The trace runs to thousands of
lines per millisecond of model time, so switch `cpu_dev.dctrl` on and off around
the region you care about, or cap it with `trace_counter`.

Breakpoints are the machine's own: `М34` stops on an instruction fetch, `М35` on
a load or store. SIMH's software breakpoints are gone — nothing set one, and
their `E`/`R`/`W` types duplicated these.

```c
M[IBP] = 032013;   /* stop when PC reaches 032013 */
```

### Stop codes

| Message | Meaning |
|---------|---------|
| Останов | `STOP` executed. |
| Выход за пределы памяти | Ran past the end of memory. |
| Запрещенная команда | Illegal instruction. |
| Контроль команды | A data-tagged word fetched as an instruction. |
| Команда / Число в чужом листе | Paging fault on a fetch / on a data access. |
| Контроль числа МОЗУ / БРЗ | RAM / write-cache parity error. |
| Переполнение АУ | Arithmetic overflow. |
| Деление на нуль | Division by zero or a denormal. |
| Двойное внутреннее прерывание | Double internal interrupt. |
| Чтение неформатированного барабана / диска | Read from an unformatted drum / disk. |
| Останов по КРА / считыванию / записи | Breakpoint / load / store watchpoint hit. |
| Не реализовано | Unimplemented I/O or special-register feature. |
