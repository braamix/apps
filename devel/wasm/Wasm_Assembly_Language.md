# WebAssembly assembly language

The language `as` reads. It is LLVM's WebAssembly assembly: what
`clang -S` writes and `llvm-mc` reads, and what the C compiler `cc` writes
on Braam. It looks like GNU assembler — one instruction per line,
directives starting with a dot, labels ending with a colon — and it is a
different language from the one `wat` reads
([Wasm_Text_Format.md](Wasm_Text_Format.md)).

`as` turns each source into an **object**, and `ld` links objects into a
program. What each instruction does is in
[Wasm_Bytecode.md](Wasm_Bytecode.md). How a Braam program talks to the
kernel is in [Tutorial.md](Tutorial.md); this manual uses its library,
`libw.a`.

## 1. A first program

`squares.s` prints the squares of 1 to 5:

    # squares.s: prints the squares of 1 to 5.
    #
    #   as squares.s
    #   ld squares.o -lw -o squares

    	.functype	sys (i32, i32, i32, i32) -> (i32)
    	.import_module	sys, kernel
    	.import_name	sys, sys
    	.functype	_free (i32, i32) -> ()
    	.functype	out_str (i32, i32) -> ()
    	.functype	out_u64 (i64) -> ()
    	.functype	out_nl () -> ()
    	.functype	out_flush (i32) -> (i32)
    	.functype	out_wrote (i32) -> (i32)

    	.section	.rodata.title,"",@
    title:
    	.ascii	"squares:\n"
    	.size	title, 9

    # i32 square(i32 n)
    	.section	.text.square,"",@
    square:
    	.functype	square (i32) -> (i32)
    	local.get	0
    	local.get	0
    	i32.mul
    	end_function

    # i32 exit(i32 status): asks the kernel to end the process.
    	.section	.text.exit,"",@
    exit:
    	.functype	exit (i32) -> (i32)
    	i32.const	1		# Exit
    	local.get	0
    	i32.const	0
    	i32.const	0
    	call	sys
    	drop
    	i32.const	0
    	end_function

    	.section	.text._start,"",@
    	.globl	_start
    	.export_name	_start, _start
    _start:
    	.functype	_start (i32, i32) -> (i32)
    	.local	i32			# local 2: n
    	i32.const	title
    	i32.const	9
    	call	out_str
    	i32.const	1
    	local.set	2
    	block
    	loop
    	local.get	2
    	call	square
    	i64.extend_i32_u
    	call	out_u64
    	call	out_nl
    	local.get	2
    	i32.const	1
    	i32.add
    	local.tee	2
    	i32.const	5
    	i32.gt_u
    	br_if	1		# out of the block
    	br	0		# to the loop's top
    	end_loop
    	end_block
    	i32.const	1
    	call	out_flush
    	if
    	i32.const	1
    	return
    	end_if
    	i32.const	0
    	call	exit
    	end_function

    	.section	.text._resume,"",@
    	.globl	_resume
    	.export_name	_resume, _resume
    _resume:
    	.functype	_resume (i32, i32, i32) -> (i32)
    	.local	i32
    	local.get	1
    	call	out_wrote
    	local.set	3
    	local.get	1
    	local.get	2
    	call	_free
    	local.get	3
    	i32.const	0
    	i32.gt_s
    	if
    	i32.const	1
    	return
    	end_if
    	local.get	3
    	i32.const	0
    	i32.lt_s
    	call	exit
    	end_function

Build it and run it:

    $ as squares.s
    $ ld squares.o -lw -o squares
    $ ./squares
    squares:
    1
    4
    9
    16
    25

What to notice:

- The first block of lines **declares** functions defined elsewhere:
  `sys` in the kernel, the rest in `libw.a`. A function must be declared
  before it is called.
- `title` is **data**: bytes in memory, with a name and a size.
- `square`, `exit`, `_start` and `_resume` are **functions**. Each one
  is a label, a `.functype` giving its signature, instructions, and
  `end_function`.
- There are no registers. WebAssembly is a **stack machine**: `i32.mul`
  takes two values off the stack and pushes their product.
- Branches go by **depth**, not to labels: `br_if 1` leaves the block,
  `br 0` goes back to the top of the loop (§6.2).

`_start` and `_resume` are what the kernel calls, and the
[Tutorial](Tutorial.md) explains them; `hello2.wat` there is the same
shape.

## 2. Lines

A source is a sequence of **statements**: a label, a directive or an
instruction. One goes on a line, or several on one line separated by `;`:

    	i32.const 1 ; i32.const 2 ; i32.add

Spaces and tabs separate words. Indentation means nothing, but labels are
usually written at the start of a line and the rest indented by a tab.

**Comments** are of three kinds:

    # to the end of the line
    // to the end of the line
    /* to the closing */, even across lines

Note that `;` is *not* a comment, as it is in many assemblers: it starts
a new statement.

A **label** is a name followed by a colon. It may share a line with a
statement:

    second:	.functype	second () -> ()

A **directive** is a word starting with a dot: `.section`, `.int32`.

An **instruction** is any other word: `i32.add`, `local.get`.
Its operands follow it, separated by commas.

## 3. Words, numbers and strings

### 3.1 Names

A name is made of letters, digits and `_ $ . ?`, and does not start with
a digit: `main`, `_start`, `fopen$resume`, `.Ltmp3`. Case matters:
`Main` and `main` are two names.

A name starting with **`.L`** is **private**: it never leaves the object,
and `nm` does not list it unless something refers to it.

### 3.2 Integers

| Written | Base | Example |
| --- | --- | --- |
| `123` | decimal | `i32.const 123` |
| `0x7b`, `0X7B` | hexadecimal | `i32.const 0x7fffffff` |
| `0173` | octal: a leading `0` | `i32.const 017` is 15 |
| `0b1111011` | binary | `i32.const 0b101` is 5 |

A minus sign makes it negative: `-1`. A C suffix — `u`, `l`, `UL` — is
allowed and ignored. Watch the leading zero: `010` is eight.

An `i32` operand may be written signed or unsigned: `-1` and `4294967295`
are the same bits. The same goes for `i64`.

### 3.3 Floats

    1.5    .5    1e10    1.5e3    2.5e-3
    0x1.8p+1     -0x1p-149
    infinity    -infinity    nan    -nan

A hexadecimal float is `0x` *mantissa* `p` *exponent*, the exponent a
power of two in decimal: `0x1.8p+1` is 1.5 × 2¹ = 3. `infinity` and
`nan` may be written in any case.

**A float must be exact.** `as` refuses any value a `double` cannot hold
exactly, so `f64.const 0.1` is an error:

    a.s:3:12: error: Cannot parse real: 0.1

Halves, quarters and integers are exact; for any other fraction write the
hexadecimal form, which is always exact: `0x1.999999999999ap-4` is the
`double` nearest to 0.1. `f32.const` takes the same values and rounds them
to `f32`.

### 3.4 Strings

Strings are in double quotes and take C's escapes:

| Escape | Byte |
| --- | --- |
| `\n` `\t` `\r` `\b` `\f` | newline, tab, return, backspace, form feed |
| `\"` `\\` | a quote, a backslash |
| `\101` | octal: one to three digits, up to `\377` |
| `\x41` | hexadecimal: every hex digit that follows |

There is no `\0` special case: it is just the octal escape for byte 0.
And `\x` takes every hex digit after it, so to write `\x41` followed by
the letter `B`, close the string and start another (§7.2).

Bytes above 127 pass through unchanged, so a UTF-8 string can be written
as it is.

### 3.5 Expressions

Where a value is expected, an **expression** is allowed: numbers and at
most one symbol, joined by `+` and `-`, with parentheses:

    	.int32	table+8
    	.int32	2+3-1
    	.int32	buffer-4
    	.int32	(4+4)+label

A symbol may be added, never subtracted: `-label` and `a-b` are errors.
There is no `*`, `/` or shift.

In **instruction operands** the form is simpler: a number, or a symbol
with an optional `+n` or `-n`: `i32.const title+4`, `i32.load buf-4`.
`i32.const 2+3` is an error there.

Where the value must be known now — `.skip`, `.p2align` — no symbol is
allowed at all.

## 4. Sections

Everything `as` writes goes into the **current section**. A section is
one of three kinds, decided by its name:

| Name starts with | Kind | Holds |
| --- | --- | --- |
| `.text` | code | one function |
| `.custom_section.` | custom | bytes stored as a custom section of the module |
| anything else, usually `.data`, `.rodata`, `.bss` | data | bytes for memory |

Switch with

    	.section	name,"flags",@

The `"flags"` string is usually empty; the `,@` at the end is required.
The flags are letters:

| Flag | Meaning |
| --- | --- |
| `S` | holds zero-terminated strings; `ld` may merge equal ones |
| `R` | retain: `ld` keeps it even if nothing uses it |
| `p` | passive: not placed in memory at start; of no use here (§11) |
| `T` | thread-local; `ld` refuses it |

A section may be entered again later, with the same flags; its bytes
continue where they stopped. Entering it with other flags is an error.

The source starts in code. Two shortcuts:

- `.data` switches to the section `.data`.
- `.text` does **nothing** (as in llvm). It does not come back to code
  after data — use `.section .text.name,"",@` for that.

### 4.1 One section per function, one per variable

**A function always gets a section of its own.** A label in code, with a
name not starting with `.L`, switches to the section `.text.`*name*, so
these are the same:

    	.section	.text.square,"",@
    square:

    square:

Writing the `.section` line is customary, and needed after data, where a
label would otherwise make a data symbol.

Give each variable a section of its own too: `.data.count`,
`.rodata.title`, `.bss.buffer`. `ld` drops each section nothing uses, so
small sections make small programs. It merges `.rodata.*` into
`.rodata`, `.data.*` into `.data` and `.bss.*` into `.bss`, and places
them in that order.

### 4.2 `.rodata`, `.data` and `.bss`

To `as`, all three are just bytes. To the reader and to `ld` they say:

- `.rodata.*`: constants. Nothing stops a program writing them, though.
- `.data.*`: variables with initial values.
- `.bss.*`: variables that start at zero. `ld` stores none of their bytes
  in the program, since a Braam process starts with zeroed memory, so
  **put only zeros in a `.bss` section** — use `.skip`.

## 5. Symbols

A **symbol** is a name for something the linker can find: a function,
a piece of data, a global or a table. A label **defines** a symbol at the
current place; a name used but never defined is **undefined**, and `ld`
must find it in another object or library.

### 5.1 What kind of symbol

| Kind | How it is made |
| --- | --- |
| function | `.functype`, or `.type name,@function` |
| global | `.globaltype` |
| table | `.tabletype` |
| data | anything else: a label in a data section |

`.type name,@object` says "data" explicitly, and `.type name,@global`
"global". These are optional, but harmless, and `cc` writes them.

### 5.2 Who can see it

| Directive | Effect |
| --- | --- |
| *(none)* | **local**: visible in this object only, like C's `static` |
| `.globl name`, `.global name` | **global**: other objects can use it |
| `.weak name` | global, and **weak**: see below |
| `.hidden name` | hidden visibility; it matters to shared libraries, which Braam has none of |

Each takes a list: `.globl a, b, c`. They may come before or after the
label.

**Weak.** A weak definition gives way to an ordinary definition in
another object. A weak undefined symbol that nothing defines is not an
error: its address is 0.

### 5.3 Private labels

A label starting with `.L` makes no function and goes into no symbol
table. Inside a function, a label must be a `.L` one — any other name
would start a new function. Labels inside code mean nothing to the
machine anyway; branches use depths (§6.2).

## 6. Functions

    	.section	.text.add,"",@
    	.globl	add
    	.type	add,@function
    add:
    	.functype	add (i32, i32) -> (i32)
    	.local	i64, i64
    	...instructions...
    	end_function

1. **The label** names the function.
2. **`.functype`** right after it gives the signature and starts the
   body: the parameter types in parentheses, `->`, then the result types.
   Both lists may be empty: `() -> ()`. Several results are allowed:
   `() -> (i32, i32)`.
3. **`.local`**, optional, lists the extra local variables, in one line.
4. **Instructions.**
5. **`end_function`** ends it.

The types are `i32`, `i64`, `f32` and `f64`. Pointers are `i32`.

### 6.1 Locals and parameters

Locals are numbered from 0, **parameters first**. In `add` above, 0 and 1
are the parameters, 2 and 3 the `i64` locals. Locals start at zero.

    	local.get	1	# push local 1
    	local.set	2	# pop into local 2
    	local.tee	2	# copy the top into local 2, and keep it

The value a function leaves on the stack at `end_function` is its
result. `return` returns at once, with the results on top of the stack.

### 6.2 Blocks and branches

Control flow is structured: there is no jump to a label. There are three
kinds of block, and each must be closed by its own end:

| Begin | End | A branch to it goes |
| --- | --- | --- |
| `block` | `end_block` | forward, past `end_block` |
| `loop` | `end_loop` | back, to the top of the loop |
| `if` | `end_if` | forward, past `end_if` |

A branch names its block by **depth**: 0 is the innermost block around
it, 1 the next one out, and so on.

    	block			# depth 1 from inside the loop
    	loop			# depth 0
    	...
    	br_if	1		# if true: leave the block
    	...
    	br	0		# go round again
    	end_loop
    	end_block

| Instruction | Does |
| --- | --- |
| `br n` | branch |
| `br_if n` | pop an `i32`; branch if it is not zero |
| `br_table {a, b, ..., d}` | pop an `i32` *i*; branch to the *i*-th depth in the list, or to the last, *d*, if *i* is too large |

`if` pops an `i32` and runs its body if it is not zero. `else` divides an
`if` into two halves:

    	local.get	0
    	if	i32
    	i32.const	1
    	else
    	i32.const	2
    	end_if			# 1 or 2 is on the stack

A block can leave values, as the `if` above leaves an `i32`. Write the
type after the keyword:

| Written | The block leaves |
| --- | --- |
| `block`, `block void` | nothing |
| `block i32` | one value (`i64`, `f32`, `f64` alike) |
| `block (i32) -> (i32, i32)` | takes values from the stack and leaves others |

A branch out of a block carries its values along.

### 6.3 Calls

    	call	square

pops the arguments, the last one on top, and pushes the results.
**The callee must have a `.functype` earlier in the source**, either its
own definition or a declaration:

    	.functype	square (i32) -> (i32)

Without one, `as` stops:

    a.s:3:7: error: symbol square: missing .functype

### 6.4 Function addresses

The address of a function is not a memory address but its **slot in the
function table**. Write it like a data address:

    	i32.const	square		# square's slot
    	.int32	square			# the same, in data

Slot 0 is never a function, so 0 serves as a null pointer. A call through
an address names the signature it expects:

    	local.get	0		# the argument
    	i32.const	square		# the address, last
    	call_indirect	(i32) -> (i32)

If the function in that slot has another signature, the program traps.
The table may be named first, `call_indirect __indirect_function_table,
(i32) -> (i32)`; it is the only table `ld` links, and `as` declares it
by itself. `cc` declares it once more, which does no harm:

    	.tabletype	__indirect_function_table, funcref

### 6.5 Memory

A load pops an address and pushes the value there; a store pops a value,
then an address. The operand is an **offset** added to the address, and
may be a symbol:

    	local.get	0
    	i32.load	8		# the i32 at (local 0) + 8
    	i32.const	0
    	i32.load	count		# the variable count
    	i32.const	0
    	local.get	1
    	i32.store	buffer+12	# local 1 into buffer[3]

The offset is required; write `0` for none.

After the offset, `:p2align=k` says the address is a multiple of 2^k. It
is a hint for speed, never checked; without it the access is assumed
aligned to its own size. Write a smaller one for data that may not be
aligned:

    	i64.load	8:p2align=2	# aligned to 4 only

| Load | Store | Size |
| --- | --- | --- |
| `i32.load`, `i64.load`, `f32.load`, `f64.load` | `i32.store`, `i64.store`, `f32.store`, `f64.store` | the type's |
| `i32.load8_s`, `i32.load8_u`, `i64.load8_s`, `i64.load8_u` | `i32.store8`, `i64.store8` | byte |
| `i32.load16_s`, `i32.load16_u`, `i64.load16_s`, `i64.load16_u` | `i32.store16`, `i64.store16` | 16 bits |
| `i64.load32_s`, `i64.load32_u` | `i64.store32` | 32 bits |

`_s` extends the sign of a narrow value, `_u` fills with zeros. A store
keeps the low bits.

There is one memory, number 0, and its four instructions name it:

    	memory.size	0		# pages of 64 KB
    	memory.grow	0		# pop pages to add; push the old size, or -1
    	memory.copy	0, 0		# dest, src, n: copy like memmove
    	memory.fill	0		# dest, byte, n: fill like memset

The operands of the last two are pushed in the order shown, `n` last.

### 6.6 The instructions

`as` knows the WebAssembly 1.0 instructions and the few extensions C
needs:

| Group | Instructions |
| --- | --- |
| control | `unreachable` `nop` `block` `loop` `if` `else` `end_block` `end_loop` `end_if` `end_function` `br` `br_if` `br_table` `return` `call` `call_indirect` |
| stack | `drop` `select` |
| variables | `local.get` `local.set` `local.tee` `global.get` `global.set` |
| memory | the loads and stores of §6.5, `memory.size` `memory.grow` `memory.copy` `memory.fill` |
| constants | `i32.const` `i64.const` `f32.const` `f64.const` |
| numbers | every comparison, arithmetic and conversion of `i32`, `i64`, `f32` and `f64`, such as `i32.add`, `i64.lt_s`, `f64.sqrt`, `i32.wrap_i64`, `f64.convert_i32_s` |
| sign extension | `i32.extend8_s` `i32.extend16_s` `i64.extend8_s` `i64.extend16_s` `i64.extend32_s` |
| saturating | `i32.trunc_sat_f32_s` and the seven like it: float to integer without a trap |

[Wasm_Bytecode.md](Wasm_Bytecode.md) explains each one. Not supported:
SIMD, references, tail calls, exceptions, threads, `memory.init`, and
`table.*`. A bare `end` is not an instruction here — say which end.

`select` pops a condition and two values, and pushes the first if the
condition is not zero, else the second. `select i32` is the same,
with the type written out.

## 7. Data

Data is written in a data section, after a label:

    	.section	.data.limits,"",@
    	.globl	limits
    	.p2align	2
    limits:
    	.int32	100
    	.int32	-1
    	.size	limits, 8

**Every data symbol needs `.size`**, its length in bytes, or the object
is refused:

    as: error: data symbols must have a size set with .size: limits

`.size` may come anywhere in the source. On a function it is ignored with
a warning.

### 7.1 Numbers

| Directive | Writes |
| --- | --- |
| `.int8 v` | 1 byte |
| `.int16 v` | 2 bytes |
| `.int32 v` | 4 bytes |
| `.int64 v` | 8 bytes |

Little-endian, as everything in WebAssembly. The value is an expression
(§3.5), signed or unsigned: `.int8 -128` and `.int8 255` both fit, `.int8
256` is an error. One value per directive.

A symbol in the value is filled in by `ld`: a data symbol becomes its
address, a function its table slot (§6.4). That is how a pointer or a
table of functions is written:

    	.int32	title		# char *p = title;
    	.int32	title+5		# char *q = title + 5;
    	.int32	square		# int (*f)(int) = square;

Only `.int32` and `.int64` can hold a symbol. There are no floats: write
their bits, `.int64 0x3ff0000000000000` for 1.0.

### 7.2 Strings

| Directive | Writes |
| --- | --- |
| `.ascii "s", ...` | the bytes |
| `.asciz "s", ...`, `.string "s", ...` | the bytes and a zero after each |

Each takes a list of strings.

### 7.3 Space and alignment

| Directive | Does |
| --- | --- |
| `.skip n`, `.space n` | write *n* zero bytes |
| `.skip n, b` | write *n* bytes of value *b* |
| `.p2align k` | pad with zeros to a multiple of 2^k |
| `.align k` | the same as `.p2align` |
| `.balign n` | pad to a multiple of *n*, a power of two |
| `.p2align k, b` | pad with bytes *b* |
| `.p2align k, b, m` | the same, but only if it takes at most *m* bytes |

An alignment also aligns **the section itself**: `ld` places it at a
multiple of the largest alignment asked for inside it. So write it before
the label:

    	.section	.bss.buffer,"",@
    	.p2align	4		# 16-byte aligned
    buffer:
    	.skip	64
    	.size	buffer, 64

## 8. Globals

A **global** is a variable the machine keeps outside memory, read with
`global.get` and written with `global.set`. Declare it first:

    	.globaltype	counter, i32			# mutable
    	.globaltype	limit, i64, immutable		# constant

A global that has a label is defined here, and **starts at zero**; the
label goes in a data section, and takes no bytes:

    	.section	.data.counter,"",@
    counter:

A global with no label is undefined, and comes from another object.
One comes from `ld` itself: `__stack_pointer`, the top of the stack C
uses, 128 KB by default.

    	.globaltype	__stack_pointer, i32
    	...
    	global.get	__stack_pointer
    	i32.const	16
    	i32.sub
    	global.set	__stack_pointer		# 16 bytes for this call

## 9. Imports and exports

**Imports.** An undefined function or global is resolved by `ld` from
another object or a library. To take it from **outside the program**, from
the kernel, name both halves of the import:

    	.functype	sys (i32, i32, i32, i32) -> (i32)
    	.import_module	sys, kernel
    	.import_name	sys, sys

The symbol `sys` is then the kernel's `kernel.sys`. The symbol's own name
need not be the same: `.import_name braam_sys, sys` is fine. Without
`.import_name`, `ld` looks for the symbol in the objects, and says
`undefined symbol` when it is not there.

**Exports.** The kernel calls a program through its exports:

    	.export_name	_start, _start

exports the function `_start` under the name `_start`. A Braam program
exports `_start`, `_resume`, `_alloc`, `_free` and `_sig`; `libw.a`
supplies the last three. See [README.md](README.md) for the whole
process ABI.

## 10. Custom sections

A section named `.custom_section.`*name* becomes a custom section *name*
of the object, holding its bytes as written:

    	.section	.custom_section.comment,"",@
    	.ascii	"built by hand"

Its values must be plain numbers; a symbol there is an error.
`producers` and `target_features`, which `cc` writes, are custom sections
too, and are placed last, as llvm places them.

## 11. What as does not check

**There is no type checker.** `as` checks the syntax, that blocks are
closed by the right ends, and that calls and globals are declared. It
does **not** check that the stack holds the right values, nor that a local
exists:

    f:
    	.functype	f () -> (i32)
    	local.get	5		# f has no local 5
    	end_function

assembles without a word, and links too; only running it fails:

    $ ./f
    ./f: will not instantiate

When that happens, look at the code with `disasm` and count the stack by
hand.

`as` also accepts some things `ld` refuses or the kernel cannot use:
thread-local sections, passive data (no instruction here can load it), and
undefined symbols that nothing defines.

## 12. Messages

An error names the file, line and column, and shows the line:

    a.s:3:7: error: symbol g: missing .functype
            call g
                 ^

An error found while writing the object has no place:

    as: error: data symbols must have a size set with .size: x

A warning looks the same with `warning:`, and the object is still
written. After an error nothing is written for that source, and the next
source on the command line is still assembled. The exit status is 1 if
any source failed.

To see what came out, `nm file.o` lists the symbols — `T` a global
function, `t` a local one, `D` and `d` data, `U` undefined, `w` weak
undefined —
and `disasm file.o` the code.

## 13. If you know GNU as

The same: labels, `.globl`, `.section`, `.int32`-style data, `.ascii`,
`.p2align`, `.size`, `.type`, comments with `#` and `/* */`.

Different:

- No registers; a stack machine, and locals by number.
- Branches go by block depth; there are no jumps to labels, and no
  numeric labels such as `1f` and `1b`.
- Each function is a section of its own, declared by `.functype`, and
  ended by `end_function`.
- `;` separates statements; it is not a comment.
- No macros, `.set`, `.equ`, `.if`, `.include`, `.rept` or `.org`.
- `.text` does not switch back to code.

## 14. Directives at a glance

| Directive | Section |
| --- | --- |
| `.section name,"flags",@` | §4 |
| `.text`, `.data` | §4 |
| `.globl`, `.global`, `.weak`, `.hidden` *names* | §5.2 |
| `.type name,@function` / `@object` / `@global` | §5.1 |
| `.functype name (params) -> (results)` | §6, §6.3 |
| `.local types` | §6.1 |
| `.size name, n` | §7 |
| `.int8`, `.int16`, `.int32`, `.int64` *value* | §7.1 |
| `.ascii`, `.asciz`, `.string` *strings* | §7.2 |
| `.skip`, `.space` *n*[, *b*] | §7.3 |
| `.p2align`, `.align` *k*[, *b*[, *m*]]; `.balign` *n*[, *b*[, *m*]] | §7.3 |
| `.globaltype name, type[, immutable]` | §8 |
| `.tabletype name, funcref[, min[, max]]` | §6.4 |
| `.import_module name, module`, `.import_name name, field` | §9 |
| `.export_name name, export` | §9 |
| `.file`, `.ident` | ignored |
