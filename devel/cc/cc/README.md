# cc — the compiler driver

The C compiler driver of VCC, built as `build/cc/cc` and installed as `bin/vcc`. It is a
**driver**, not a compiler itself: it runs the toolchain one program per stage, as Unix
`cc(1)` always has, so that

```sh
vcc hello.c && ./a.out
```

does what otherwise takes six commands, and builds a program for the machine it runs on. It is ported from the
[v7besm](https://github.com/besm6/v7besm) project's `b6cc` (`cmd/cc/`), a C11 rewrite of the
v7 driver, and made target-aware. The native BESM-6 build of that driver and its
documentation stay in v7besm.

## Pipeline

```text
vcpp         preprocess    .c   -> .i
vparse       parse         .i   -> .ast
vlower       lower + opt   .ast -> .tac
vgen<T>      code gen      .tac -> .s
assembler    assemble      .s   -> .o
linker       link          .o   -> a.out
```

| Target | `-t` | Code generator | Assembler | Linker |
| --- | --- | --- | --- | --- |
| Linux on x86-64, against glibc | `x86_64-linux` | `vgenx86 --linux` | `cc -c` | `cc -no-pie … -lvcc` |
| Linux on AArch64, against glibc | `aarch64-linux` | `vgenaarch64 --linux` | `cc -c` | `cc -no-pie … -lvcc` |
| Linux on RISC-V RV64, against glibc | `riscv64-linux` | `vgenriscv64 --linux` | `cc -c` | `cc -no-pie … -lvcc` |
| macOS on Apple silicon, against libSystem | `aarch64-darwin` | `vgenaarch64 --darwin` | `cc -c` | `cc …` |
| RISC-V RV64IMFD/LP64D | `riscv64` | `vgenriscv64` | `riscv64-unknown-elf-as -march=rv64imfd -mabi=lp64d` | `riscv64-unknown-elf-ld -T link.ld` |
| RISC-V RV32IMFD/ILP32D | `riscv32` | `vgenriscv32` | `riscv64-unknown-elf-as -march=rv32imfd -mabi=ilp32d` | `riscv64-unknown-elf-ld -m elf32lriscv -T link.ld` |
| AArch64 (ARMv8-A, AAPCS64) | `aarch64` | `vgenaarch64` | `aarch64-none-elf-as` | `aarch64-none-elf-ld -T link.ld` |
| ARM32 (ARMv7-A, AAPCS-VFP) | `arm32` | `vgenarm32` | `arm-none-eabi-as -mcpu=cortex-a15 -mfpu=vfpv3-d16 -mfloat-abi=hard` | `arm-none-eabi-ld -T link.ld` |
| x86-64 (SysV psABI) | `x86_64` | `vgenx86` | `x86_64-elf-as --64` | `x86_64-elf-ld -T link.ld` |
| AVR (ATmega1280, avr-gcc ABI) | `avr` | `vgenavr` | `avr-as -mmcu=atmega1280` | `avr-ld -m avr51 -T link.ld` |
| MSP430 (classic, MSPABI) | `msp430` | `vgenmsp430` | `msp430-elf-as -mcpu=msp430` | `msp430-elf-ld --gc-sections -T link.ld` |
| WebAssembly (clang's wasm32 C ABI) | `wasm32` | `vgenwasm` | `clang --target=wasm32 --no-default-config <features> -c` | `wasm-ld --stack-first -z stack-size=1048576` |
| A process of Braam (wasm32, [docs/Braam.md](../docs/Braam.md)) | `wasm32-braam` | `vgenwasm` | the same | `wasm-ld --no-entry --import-memory --stack-first -z stack-size=131072 --gc-sections --initial-memory=<pages * 64 KiB>`, then the `braam` section |
| MMIX (MMIXware ABI) | `mmix` | `vgenmmix` | `mmix-knuth-mmixware-as -x -no-predefined-syms` | `mmix-knuth-mmixware-ld --defsym=__.MMIX.start..text=0x100` |
| BESM-6 | `besm6` | `vgenbesm6` | `b6as -X` | `b6ld -X -e _start` |

The first four are **hosted**: the program runs under Linux, linked against glibc, or
under macOS, linked against libSystem, either of which supplies the startup files and the
C library. The rest are bare metal, run on qemu or a simulator, or for WebAssembly under
node. **The default target is
the host**: `x86_64-linux` on x86-64 Linux, `aarch64-linux` on AArch64 Linux,
`riscv64-linux` on RV64 Linux, `aarch64-darwin` on a Mac with Apple silicon, and `riscv64` on any other machine. It is
decided when `vcc` is compiled.

A hosted target assembles and links with a C compiler: the one that built `vcc`, when
the host is that target; else `x86_64-linux-gnu-gcc`, `aarch64-linux-gnu-gcc` or
`riscv64-linux-gnu-gcc` on `PATH`; else the host's `cc`; else `clang
--target=x86_64-linux-gnu` (or `aarch64-linux-gnu`, `riscv64-linux-gnu`,
`arm64-apple-macos`).

For the bare-metal targets, the binutils prefix is the first one found of several: `riscv64-unknown-elf`, `riscv64-elf`
or `riscv64-linux-gnu`; `aarch64-none-elf`, `aarch64-elf` or `aarch64-linux-gnu`;
`x86_64-elf`, `x86_64-linux-gnu` or the host's own `as`/`ld` on x86-64 Linux; `msp430-elf`
or `msp430-unknown-elf`. WebAssembly has no binutils: clang assembles it and `wasm-ld`
links it, always, with the features of the table below. Where there are no binutils,
the other targets but MSP430 and MMIX are assembled by clang and linked by `ld.lld`,
with no linker flags:

| `-t` | clang |
| --- | --- |
| `riscv64` | `clang --target=riscv64 -march=rv64imfd -mabi=lp64d -c` |
| `riscv32` | `clang --target=riscv32 -march=rv32imfd -mabi=ilp32d -c` |
| `aarch64` | `clang --target=aarch64-none-elf -c` |
| `arm32` | `clang --target=armv7a-none-eabihf -mcpu=cortex-a15 -mfpu=vfpv3-d16 -c` |
| `x86_64` | `clang --target=x86_64-none-elf -c` |
| `avr` | `clang --target=avr -mmcu=atmega1280 -c` |
| `wasm32`, `wasm32-braam` | `clang --target=wasm32 --no-default-config -mreference-types -mbulk-memory -msign-ext -mmutable-globals -mnontrapping-fptoint -c` |

The intermediate files are temporaries in `$TMPDIR` (or `/tmp`), named `vccXXXXXX.<suffix>`
and removed on exit.

## Input files

| Suffix | Handling |
| --- | --- |
| `.c` | full pipeline |
| `.S` | assembly preprocessed first (`vcpp` with `__ASSEMBLER__` defined, then the assembler) |
| `.s` | assembly (assembler only) |
| `.o`, `.a` | passed straight to the linker |

## Options

| Option | Meaning |
| --- | --- |
| `-t NAME`, `-tNAME`, `--target NAME`, `--target=NAME` | Target: `x86_64-linux`, `aarch64-linux`, `riscv64-linux`, `aarch64-darwin`, `riscv64`, `riscv32`, `aarch64`, `arm32`, `x86_64`, `avr`, `msp430`, `mmix`, `wasm32`, `wasm32-braam` or `besm6`; by default the host (see above) |
| `-c` | Compile and assemble, but do not link |
| `-S` | Compile only; emit assembly (`.s`) |
| `-Smadlen`, `-Sbemsh` | Like `-S`, but emit the BESM-6 Madlen (`.mad`) or Bemsh (`.bemsh`) dialect (`besm6` only) |
| `-E` | Preprocess only; write to `-o` or standard output |
| `-P` | With `-E`, no line markers |
| `-x LANG` | The language of every input, whatever its suffix: `c`, `assembler-with-cpp` (as `.S`), `assembler` (as `.s`), or `none` (by suffix) |
| `-o file` | Set the output file name (default `a.out` for a link) |
| `-v` | Echo each sub-command before running it |
| `-Dname[=v]`, `-Uname`, `-Ipath` | Passed to the preprocessor (`-D name` is folded into `-Dname`) |
| `-Lpath`, `-lname` | Passed to the linker, after the objects |
| `-T file` | Linker script instead of the standard `link.ld` (not `besm6`; for `mmix` and `wasm32`, which have none, a script of one's own); passed on for a hosted target |
| `-nostdinc` | Do not add the target's standard include directory |
| `--initial-pages=N` | `wasm32-braam`: the process's initial memory, 1 to 1600 pages of 64 KiB (default 4, braam-core's `BRAAM_BIN_INITIAL_PAGES`); the link's `--initial-memory` and the `braam` section both say it |
| `-nostdlib` | No `crt0.o`, no standard library directory, no implicit libraries; a hosted target passes it to its C compiler and drops `libvcc.a` |
| `-fmax-errors=N` | Stop after N errors (default 20, 0 for no limit); `vparse` and `vlower` take it as `--max-errors=N` |
| `-fdiagnostics-color[=WHEN]`, `-fno-diagnostics-color` | Color diagnostics: `always` (what the bare flag means), `never` (the `-fno-` form), or `auto` (the default: color only when stderr is a terminal, `NO_COLOR` is unset or empty, and `TERM` is not `dumb`). Every pass takes it as `--color=WHEN` |
| `-fno-diagnostics-show-caret` | Don't show the source line and caret under a diagnostic; `vparse` and `vlower` take it as `--no-show-source` |
| `-O`, `-g` | Accepted and ignored: `vlower` always optimizes, and there is no debug info yet |
| `-W…`, other `-f…`, `-w`, `-std=…`, `-pedantic`, `-pipe`, `-arch A`, `-isysroot D` | Accepted and ignored, so that a build system written for GCC or clang (CMake among them) can drive `vcc` |

The last stage is chosen by `-E`, `-S` or `-c`. With none of them, the objects are linked.
Output names derive from the input's base name in the current directory (`src/foo.c` →
`foo.o`). When linking, the objects of the sources are temporaries, removed afterwards,
as with gcc: `vcc -o t t.c` leaves no `t.o`. `-o` with `-c`, `-S` or `-E` takes a single
input.

The exit status is 0 on success and 1 on any failure. One failing file does not stop
the others from compiling, but it skips the link.

## Where things are found

**The installation is relocatable.** `vcc` finds its own directory (`_NSGetExecutablePath`
on macOS, `/proc/self/exe` on Linux, otherwise `argv[0]`), with symlinks resolved, and
takes everything relative to it:

| What | Where |
| --- | --- |
| `vcpp`, `vparse`, `vlower`, `vgen<T>` | the directory `vcc` is in |
| standard headers | `../share/vcc/<target>/include` |
| `crt0.o`, libraries, `link.ld` (hosted: `libvcc.a` alone; wasm32: no `link.ld`, and the host `run.mjs`) | `../share/vcc/<target>/lib` |

`vcc` passes `-nostdinc -I<share>/include` to `vcpp`, so `vcpp`'s own compiled-in include
directory plays no part. `<alloca.h>` is there on every target, BESM-6 included: `alloca`
needs no library, the code generator expands it. The assembler and linker belong to other projects. They are the
ones found when the build was configured (`scripts/CrossTools.cmake`: GNU binutils, else
clang and `ld.lld`; clang and `wasm-ld` for WebAssembly; nothing for the BESM-6), or else the first binutils on `PATH` by the
prefixes above, then `clang`/`ld.lld` (`b6as`/`b6ld` for the BESM-6). Which of the two a
tool is, by its name, decides its flags.

Each tool can be overridden with an environment variable. This is how the tests run the
driver against the build tree:

| Variable | Tool |
| --- | --- |
| `VCC_CPP`, `VCC_PARSE`, `VCC_LOWER` | the preprocessor, parser and lowerer |
| `VCC_GEN` | the code generator of the selected target |
| `VCC_AS`, `VCC_LD` | the assembler and linker; split into words at blanks, so they may carry arguments |

## Linking

The hosted Linux targets, `x86_64-linux`, `aarch64-linux` and `riscv64-linux`:

```text
cc -no-pie -o a.out objects... -L/-l flags... -L<lib> -lvcc
```

The C compiler adds glibc's startup files, `-lc` and libgcc; `libvcc.a` holds the few
helpers our code generator calls and glibc lacks (`__va_arg`, but on RISC-V, whose
`va_list` is a pointer walked in `<stdarg.h>`), and the coroutine runtime. The math library is not
implicit, as with GCC: add `-lm`. The executable is not position independent, because our
code takes the address of a function PC-relative, which a PIE cannot do for one in a
shared library. The headers are ours, not glibc's (which `vparse` cannot read), and agree
with glibc's on every layout and value a program hands to the C library: `errno`,
`jmp_buf`, `struct tm`, `struct lconv`, `fenv_t`, the `LC_*`, `E*`, `FE_*` and `<stdio.h>`
constants, `MB_LEN_MAX` (see `libc/linux/`). The run is plain `./a.out`; a `riscv64-linux`
program built on another machine runs under `qemu-riscv64 -L /usr/riscv64-linux-gnu`.

The hosted macOS target, `aarch64-darwin`:

```text
cc -o a.out objects... -L/-l flags... -L<lib> -lvcc
```

The C compiler adds the startup and libSystem, whose math library comes with it (`-lm`
is accepted and links nothing more). `libvcc.a` holds only the coroutine runtime: Apple's
`va_list` is a pointer walked in `<stdarg.h>`, and `long double` is `double`. The executable is position
independent, as every arm64 macOS executable is: our code reaches what the unit does not
define through the GOT. The headers are ours (`libc/darwin/`), and agree with libSystem's
on every layout and value a program hands to it:
- `errno` is `(*__error())`;
- `stdin` and its siblings are `__stdinp` and so on;
- `jmp_buf`, `mbstate_t`, `struct lconv` and `fenv_t`;
- the `LC_*`, `E*`, `FE_*` and `FP_*` numbers.

RISC-V, AArch64, ARM32, x86-64 and AVR:

```text
<prefix>-ld [flags] -T <lib>/link.ld -o a.out -L<lib> <lib>/crt0.o objects... -L/-l flags... -lc
```

with the flags of the table above (or `ld.lld` with none).

The result is an ELF for the qemu `virt` machine (`microvm` for x86-64, `arduino-mega` for AVR). It runs with
`qemu-system-riscv64 -M virt -bios none -display none -serial stdio -monitor none -kernel a.out`
(`qemu-system-riscv32` for `riscv32`), or for `aarch64` with
`qemu-system-aarch64 -M virt -cpu cortex-a57 -display none -serial stdio -monitor none -semihosting -kernel a.out`,
and for `arm32` with
`qemu-system-arm -M virt -cpu cortex-a15 -display none -serial stdio -monitor none -semihosting -kernel a.out`;
these two exit with `main`'s result. For `x86_64`:
`qemu-system-x86_64 -M microvm -display none -serial stdio -monitor none -device isa-debug-exit,iobase=0xf4,iosize=0x04 -kernel a.out`;
`exit` writes the status byte to the debug console (add `-debugcon file:status` to keep
it), and the exit device then stops qemu with status `(main's result << 1) | 1`.

For `avr` the ELF is for qemu's `arduino-mega` (an ATmega1280):
`qemu-system-avr -M arduino-mega -display none -monitor none -serial stdio -serial file:status -bios a.out`.
Nothing on the machine can stop qemu: `exit` writes `main`'s result as one byte to
USART1 (the `status` file) and waits, so qemu is stopped by hand, or by a script once the
byte has arrived. For a real board, `llvm-objcopy -O ihex a.out a.hex` makes the Intel HEX
image a flasher takes.

MSP430:

```text
msp430-elf-ld --gc-sections -T <lib>/link.ld -o a.out -L<lib> <lib>/crt0.o objects... -L/-l flags... -lc [libgcc.a]
```

`vgenmsp430` gives every function and variable a section of its own, so `--gc-sections`
keeps only what is reached: `printf("%d")` would be 53 KB without it. GCC's `libgcc.a`
comes last when it was found at configure time (`msp430-elf-gcc -mcpu=msp430
-print-libgcc-file-name`) and still exists: our `libc.a` has every helper our own code
calls, but objects compiled by GCC may call more (`__clzhi2` for `__builtin_clz`).

The ELF is for the classic MSP430 with 48 KB of ROM at 0x4000 and 15.5 KB of RAM at 0x0200, and runs with
[mspsim](https://github.com/sergev/mspsim): `mspsim a.out` prints the UART output and
exits with `main`'s result (`-q` drops the banner, `-t` traces, `-g` starts in the
debugger). `msp430-elf-objcopy -O ihex a.out a.hex` makes the Intel HEX image a flasher
takes, and mspsim runs that too.

clang's assembler and `ld.lld` can stand in for the GNU binutils:

```sh
VCC_AS="clang --target=msp430 -c" VCC_LD="ld.lld -n" vcc -t msp430 hello.c
```

`-n` keeps `ld.lld` from placing the ELF headers in a loaded segment, where they would
land on the peripheral area. A clang given this way takes no flags of vcc's.

MMIX:

```text
mmix-knuth-mmixware-ld --defsym=__.MMIX.start..text=0x100 -o a.out -L<lib> <lib>/crt0.o objects... -L/-l flags... -lc [libgcc.a]
```

These are the flags GCC passes: the assembler's `-x` lets the assembler and the linker
expand a branch, `geta`, `pushj` or `jmp` that is out of range and allocate the base
registers; the linker uses its own script and puts the text at 0x100, where the loader
expects it. GCC's `libgcc.a` comes last when it was found at configure time
(`mmix-knuth-mmixware-gcc -print-libgcc-file-name`), for objects compiled by GCC
(`__clzdi2` for `__builtin_clzl`); our own code calls no helper.

The output is Knuth's `.mmo` object format, which his simulator runs directly:
`mmix a.out` prints the program's output and exits with `main`'s result, which our
`exit` also reports as `[exit N]` on the standard error (`-q` drops the simulator's own
messages, `-s` prints the instruction, oop and mem counts, `-t` traces, `-i` starts in
the interactive mode). `objdump` cannot read a `.mmo`; to
disassemble, link the same objects once more with `--oformat elf64-mmix`:

```sh
vcc -t mmix -v -o a.out hello.c        # shows the link line
mmix-knuth-mmixware-ld --oformat elf64-mmix --defsym=__.MMIX.start..text=0x100 -o a.elf ...
mmix-knuth-mmixware-objdump -d a.elf
```

WebAssembly:

```text
wasm-ld --stack-first -z stack-size=1048576 -o a.out -L<lib> <lib>/crt0.o objects... -L/-l flags... -lc
```

There is no linker script: `wasm-ld` lays out the linear memory, the 1 MiB shadow stack
first (`--stack-first`, so that an overflow traps below address 0 rather than running
into the data), then the data and the heap. `vgenwasm` puts every function and variable
in a section of its own, and `wasm-ld` drops those nothing reaches, by default. The result is a WebAssembly module whose
entry is `_start` and which imports two functions, `env.putch` and `env.exit`. The host
installed beside the runtime runs it:
`node <lib>/run.mjs a.out` prints the program's output, reports `[exit N]` on the
standard error and exits with `main`'s result; a trap exits with 255 and no report.
`wasm-validate a.out` and `wasm-objdump -d a.out`, from WABT, check and disassemble it.
See [docs/Wasm_Backend.md](../docs/Wasm_Backend.md).

BESM-6:

```text
b6ld -X -e _start -o a.out -L<lib> <lib>/crt0.o objects... -L/-l flags... -lc -lruntime
```

`libruntime.a` (the `b$*` helpers) is ours. `crt0.o` and `libc.a` are **v7besm's**, and this
project does not install them, so a BESM-6 link needs them copied or installed into
`share/vcc/besm6/lib`; `vcc` says so when `crt0.o` is missing. The same goes for the hosted
headers (`<stdio.h>`, …), which belong in `share/vcc/besm6/include` or come in with `-I`. The
order of the two archives is a contract: `b6ld` scans an archive once, where it stands, and
libc calls the helpers, never the reverse.

`-nostdlib` drops `-L<lib>`, `crt0.o` and the implicit `-l`s, but keeps the linker
script, since it is the machine's memory map and not a library. Use `-T` to replace it
(or, for MMIX and WebAssembly, to give one where the linker's own is used).

## Testing

`cc-tests` ([test/cc_test.cpp](test/cc_test.cpp)) runs the built driver on small sources
in a temporary directory, with the in-tree passes chosen through the `VCC_*` variables:

- preprocessing and target selection, the default target included
- `-S` for every target and both BESM-6 dialects
- the usage errors
- for the host's hosted target (Linux or macOS), a staged installation that builds a
  program and runs it natively, and a probe of the header layouts and values built both
  by `vcc` and by the system's compiler, whose outputs must agree; the hosted link lines,
  with a stand-in linker
- `-c`, a link and a run under qemu for RISC-V, AArch64, ARM32, x86-64 and AVR, and under
  mspsim for the MSP430 (its Intel HEX as well, and with clang and `ld.lld` through
  `VCC_AS`/`VCC_LD`), under Knuth's `mmix` for MMIX, and under node for WebAssembly
  (a separately compiled `.S` among the sources)
- the BESM-6 link line, checked with a stand-in linker

The `StagedPrefix` cases build a miniature installation (`bin/vcc` plus links to the
passes, `share/vcc/<target>/`) and run it with no overrides. That is what tests the
relocatable lookup. They expect in the `-v` echo the assembler command CMake found, so
vcc's flags must agree with `scripts/CrossTools.cmake`'s. Cases that need the binutils
(or clang and `ld.lld`), qemu, mspsim, `mmix`, clang with `wasm-ld` and node, or `b6as`
skip when they are missing.

```sh
./build/cc/test/cc-tests
```

## A process of Braam

`-t wasm32-braam` builds a process for [Braam](../docs/Braam.md):
the same code generator as `wasm32`, `lower` with Braam's process model (`main` is
`coro(braam_call *) int main(int, char **)`), the headers and runtime of
`share/vcc/wasm32-braam`, and a link with the memory imported and no entry, the five
exports coming from `crt0.o`. After the link the driver appends the custom section
`braam` that `exec` reads (magic, `PROC_ABI`, flags, the initial pages, 1600), as
braam-core's `tools/stamp.py` does for its own programs, replacing an earlier one.
`node share/vcc/wasm32-braam/lib/run.mjs prog [args...]` runs the result on a fake
kernel, and `fimport` in a Braam tab brings it to Braam itself;
[docs/Braam.md](../docs/Braam.md) is the target's reference, and
[docs/Braam_Example.md](../docs/Braam_Example.md) works a program through.
