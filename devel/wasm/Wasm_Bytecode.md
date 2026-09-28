# WebAssembly instructions, as `disasm` prints them

A reader's guide to the code in a `disasm` listing. No prior knowledge of
WebAssembly is assumed. Every instruction `disasm` decodes is here, in
groups, with the spelling it prints. The listings come from clang 23 and
are what `disasm` and `llvm-objdump -d` both print.

## 1. The machine

WebAssembly is a **stack machine**. An instruction takes its inputs from
the top of a value stack and pushes its result back. There are no
registers. `a = b + 1` becomes:

```
local.get 1     push b
i32.const 1     push 1
i32.add         pop two, push their sum
local.set 0     pop into a
```

What the machine has:

- **Values** of five types: `i32`, `i64` (integers of 32 and 64 bits),
  `f32`, `f64` (floats) and `v128` (a 128-bit vector). Integers have no
  sign of their own; an instruction that cares says so with `_s` (signed)
  or `_u` (unsigned).
- **Locals**, numbered from 0: the function's parameters first, then the
  locals it declares. A C variable that lives in a register is a local.
- **Globals**, numbered across the module. clang keeps the C stack
  pointer in global 0, `__stack_pointer`.
- **Linear memory**: one flat array of bytes, the C heap, stack and
  static data. An address is an `i32` (an `i64` in a wasm64 module).
- **Tables** of references. Table 0 holds the functions a C function
  pointer can name; a function pointer is an index into it.
- **Functions**, numbered across the module, imports first.

The code cannot jump to an address. Control flow is structured: blocks and
loops nest, and a branch leaves a block or restarts a loop (§4).

## 2. Reading a listing

```
0000005f <pick>:
        .local i32
      63: 41 7f        	i32.const	-1
      65: 21 01        	local.set	1
      67: 02 40        	block   	
      69: 20 00        	local.get	0
      6b: 41 03        	i32.const	3
      6d: 4b           	i32.gt_u
      6e: 0d 00        	br_if   	0                       # 0: down to label3
      70: 20 00        	local.get	0
      72: 2d 00 90 80 80 80 00 	i32.load8_u	16
			00000074:  R_WASM_MEMORY_ADDR_LEB	.Lswitch.table.pick+0
      79: 21 01        	local.set	1
      7b: 0b           	end
      7c: 20 01        	local.get	1
      7e: 0b           	end
```

- `0000005f <pick>:` starts a function: its offset and its name. A
  program without names shows `<>`. `<CODE>` at 0 is the section itself,
  followed by the count of functions.
- `.local i32` lists the declared locals. They follow the parameters, so
  here parameter `k` is local 0 and the declared one is local 1. A
  function with none has an empty line.
- Each line is an offset (in the file for a program, in the section for
  an object), the instruction's bytes, its name and its operands.
- `# ...` is a comment `disasm` adds: where a branch goes (§4).
- A line under an instruction, indented, is a **relocation** (with `-r`,
  objects only): the linker will patch the bytes at that offset with the
  named symbol's final value. Until then the operand is a placeholder,
  often padded to five bytes (`90 80 80 80 00` is 16).
- `...` stands for a run of zero bytes.
- `<unknown>` is a byte sequence that is not an instruction `disasm`
  knows. It is shown and decoding goes on after it.

Names are `type.operation`: `i32.add` adds two `i32`s, `f64.load` loads
an `f64`. A conversion is named by its result and its source:
`f64.convert_i32_s` makes an `f64` from a signed `i32`.

Operands are printed in decimal. An index is a number, not a name: `call
3` calls function 3, whatever it is called. In an object, `-r` shows the
name in the relocation beneath.

## 3. Basic instructions

| Instruction | Does |
| --- | --- |
| `nop` | nothing |
| `unreachable` | traps: stops the program with an error. clang puts it where C says control never goes, as after `abort()` |
| `drop` | pops a value and forgets it |
| `f32.select` | pops a condition and two values; pushes the first if the condition is not 0, else the second. Works for any type; llvm names it `f32.select` whatever the type |
| `select T` | the same, with the type written out |

## 4. Control flow

| Instruction | Does |
| --- | --- |
| `block T` | starts a block. A branch to it jumps **forward**, past its `end` |
| `loop T` | starts a loop. A branch to it jumps **back**, to its start |
| `if T` | pops a condition; runs what follows if it is not 0 |
| `else` | the other half of an `if` |
| `end` | ends a block, loop, `if`, `try` or the function |
| `br N` | branches to the block N levels out |
| `br_if N` | pops a condition; branches if it is not 0 |
| `br_table {A, B, ..., D}` | pops an index `i`; branches to the `i`th depth in the list, or to the last one, `D`, if `i` is past the end. A C `switch` |
| `return` | returns from the function |

`T` is the block's result type: empty for none, or `i32`, `f64` and so on
for a block that leaves one value on the stack. `unknown_type` is a
signature from the type section, which `disasm` does not look up.

A branch names a **depth**, not an address: `br 0` is the innermost
enclosing block, `br 1` the one around it. A loop does not repeat by
itself; it runs once unless a branch goes back to it. This counts the
spaces in a string, `while (*s) { if (*s == ' ') n++; s++; }`:

```
       9: 03 7f        	loop    	i32                     # label0:
       b: 02 40        	block   	
       d: 02 40        	block   	
       f: 20 00        	local.get	0
      11: 2d 00 00     	i32.load8_u	0
      14: 22 02        	local.tee	2
      16: 41 20        	i32.const	32
      18: 46           	i32.eq  
      19: 0d 00        	br_if   	0                       # 0: down to label2
      1b: 20 02        	local.get	2
      1d: 0d 01        	br_if   	1                       # 1: down to label1
      1f: 20 01        	local.get	1
      21: 0f           	return
      22: 0b           	end
      23: 20 01        	local.get	1
      25: 41 01        	i32.const	1
      27: 6a           	i32.add 
      28: 21 01        	local.set	1
      2a: 0b           	end
      2b: 20 00        	local.get	0
      2d: 41 01        	i32.const	1
      2f: 6a           	i32.add 
      30: 21 00        	local.set	0
      32: 0c 00        	br      	0                       # 0: down to label2
      34: 0b           	end
```

A space jumps to `23` and counts it; another non-zero byte jumps to `2b`;
a zero byte returns `n`. The `br 0` at `32` goes back to the loop.

The comments help to follow this. `disasm` numbers every `block`, `loop`,
`try` and `try_table` in the order they appear, from the start of the
section, and prints `labelN:` at the start of each loop. A branch gets
`# D: down to labelN` (forward, out of a block) or `# D: up to labelN`
(back, to a loop).

**The comments are llvm's and have its flaw**: an `end` does not close a
label, and an `if` does not open one. So after an inner `end`, or inside
an `if`, the depth is counted wrongly. Above, the `br 0` at `32` is said
to go down to label2, a block that has ended; it goes up to label0. Trust
a comment near the start of a block; elsewhere count the depth yourself,
from the `block`, `loop`, `if` and `end` lines. `Invalid depth argument!`
means the depth reaches past every label.

## 5. Calls

| Instruction | Does |
| --- | --- |
| `call F` | calls function F. Arguments are popped, results pushed |
| `call_indirect T` | pops a table index and calls that function, which must have type T. A C call through a pointer. The table number is not printed |
| `return_call F` | a tail call: returns what F returns, without a frame of its own |
| `return_call_indirect T` | the same, through the table |
| `call_ref T` | calls a function reference popped from the stack |
| `return_call_ref T` | the same, as a tail call |

## 6. Variables

| Instruction | Does |
| --- | --- |
| `local.get N` | pushes local N |
| `local.set N` | pops into local N |
| `local.tee N` | stores into local N and keeps the value on the stack |
| `global.get N` | pushes global N |
| `global.set N` | pops into global N |

## 7. Constants

| Instruction | Pushes |
| --- | --- |
| `i32.const V` | a 32-bit integer |
| `i64.const V` | a 64-bit integer |
| `f32.const V` | a 32-bit float |
| `f64.const V` | a 64-bit float |

Integers are printed signed: `-1`, not `4294967295`. In an object an
`i32.const` is often an address the linker fills in (see `-r`).

Floats are printed **exactly**, in hexadecimal: `0x1.8p3` is 1.5 × 2³ =
12, `0x1p-1` is 0.5, `0x0p0` is 0. The digits after `0x` are a hex
fraction, the number after `p` a power of two in decimal. `infinity` and
`nan` are spelled out; `nan:0x...` is a NaN with a payload.

## 8. Memory

A load pops an address and pushes the value read there; a store pops a
value and an address and writes it.

| Instruction | Moves |
| --- | --- |
| `i32.load`, `i64.load`, `f32.load`, `f64.load` | a whole value |
| `i32.load8_s`, `i32.load8_u` | one byte into an `i32`, sign- or zero-extended |
| `i32.load16_s`, `i32.load16_u` | two bytes into an `i32` |
| `i64.load8_s`, `i64.load8_u`, `i64.load16_s`, `i64.load16_u`, `i64.load32_s`, `i64.load32_u` | 1, 2 or 4 bytes into an `i64` |
| `i32.store`, `i64.store`, `f32.store`, `f64.store` | a whole value |
| `i32.store8`, `i32.store16` | the low 1 or 2 bytes of an `i32` |
| `i64.store8`, `i64.store16`, `i64.store32` | the low 1, 2 or 4 bytes of an `i64` |
| `f32.load_f16`, `f32.store_f16` | a 16-bit float, widened to `f32` or narrowed from it |

The operand is an **offset**, added to the popped address: `i32.load 8`
reads at address + 8, the way C reads `p->field`. `:p2align=N` after it
means the access is aligned to 2ᴺ bytes, less than its size; it is shown
only then. It is a hint, not a rule: memory can be read at any address.

| Instruction | Does |
| --- | --- |
| `memory.size M` | pushes the size of memory M, in 64 KiB pages |
| `memory.grow M` | pops a page count, grows memory M by it, pushes the old size or -1 |
| `memory.fill M` | pops address, byte and length: C's `memset` |
| `memory.copy D, S` | pops destination, source and length: C's `memmove`, from memory S to D |
| `memory.init N, M` | copies from passive data segment N into memory M |
| `data.drop N` | discards passive segment N, which is then empty |

`M` is 0 in a module with one memory, which is every C program.

## 9. Integer arithmetic

The same operations exist for `i32` and `i64`; only `i32` is shown.

| Instruction | Does |
| --- | --- |
| `i32.add`, `i32.sub`, `i32.mul` | `+`, `-`, `*`; the result wraps around |
| `i32.div_s`, `i32.div_u` | `/`, signed or unsigned. Division by 0 traps |
| `i32.rem_s`, `i32.rem_u` | `%` |
| `i32.and`, `i32.or`, `i32.xor` | `&`, `\|`, `^` |
| `i32.shl` | `<<` |
| `i32.shr_s`, `i32.shr_u` | `>>`: `_s` copies the sign bit in, `_u` shifts in zeros |
| `i32.rotl`, `i32.rotr` | rotates left or right |
| `i32.clz`, `i32.ctz` | counts zero bits at the top, at the bottom |
| `i32.popcnt` | counts one bits |
| `i32.eqz` | 1 if the value is 0, else 0: C's `!x` |

There is no `not` and no `neg`: clang writes `x ^ -1` and `0 - x`.

`i64.add128`, `i64.sub128` add and subtract 128-bit numbers held as two
`i64` halves. `i64.mul_wide_s`, `i64.mul_wide_u` multiply two `i64`s into
a 128-bit product, as two halves.

## 10. Comparisons

A comparison pops two values and pushes an `i32`: 1 if true, 0 if false.

| Integer | Float | Is |
| --- | --- | --- |
| `i32.eq` | `f32.eq` | `==` |
| `i32.ne` | `f32.ne` | `!=` |
| `i32.lt_s`, `i32.lt_u` | `f32.lt` | `<` |
| `i32.gt_s`, `i32.gt_u` | `f32.gt` | `>` |
| `i32.le_s`, `i32.le_u` | `f32.le` | `<=` |
| `i32.ge_s`, `i32.ge_u` | `f32.ge` | `>=` |

`i64`, `f64` alike. Floats are always signed.

## 11. Float arithmetic

For `f32` and `f64`; only `f64` is shown.

| Instruction | Does |
| --- | --- |
| `f64.add`, `f64.sub`, `f64.mul`, `f64.div` | `+`, `-`, `*`, `/` |
| `f64.min`, `f64.max` | the smaller, the larger |
| `f64.abs`, `f64.neg` | `fabs(x)`, `-x` |
| `f64.sqrt` | `sqrt(x)` |
| `f64.ceil`, `f64.floor`, `f64.trunc`, `f64.nearest` | rounds up, down, towards 0, to the nearest even |
| `f64.copysign` | the first value with the sign of the second |

## 12. Conversions

| Instruction | Does |
| --- | --- |
| `i32.wrap_i64` | keeps the low 32 bits of an `i64` |
| `i64.extend_i32_s`, `i64.extend_i32_u` | widens an `i32` to `i64` |
| `i32.extend8_s`, `i32.extend16_s` | sign-extends the low 8 or 16 bits: `(int)(char)x` |
| `i64.extend8_s`, `i64.extend16_s`, `i64.extend32_s` | the same, in an `i64` |
| `i32.trunc_f32_s` ... `i64.trunc_f64_u` | float to integer, towards 0. Traps if it does not fit |
| `i32.trunc_sat_f32_s` ... `i64.trunc_sat_f64_u` | the same, but a value out of range gives the nearest limit, and NaN gives 0 |
| `f32.convert_i32_s` ... `f64.convert_i64_u` | integer to float |
| `f32.demote_f64` | `f64` to `f32` |
| `f64.promote_f32` | `f32` to `f64` |
| `i32.reinterpret_f32`, `i64.reinterpret_f64` | the same bits, read as an integer |
| `f32.reinterpret_i32`, `f64.reinterpret_i64` | the same bits, read as a float |

Each `trunc`, `trunc_sat` and `convert` family has all eight: `i32` or
`i64`, `f32` or `f64`, `_s` or `_u`.

## 13. References and tables

A **reference** is an opaque handle: a function (`funcref`), a host
object (`externref`) or an exception (`exnref`). It lives on the stack or
in a table, never in memory.

| Instruction | Does |
| --- | --- |
| `ref.null_func`, `ref.null_extern`, `ref.null_exn` | pushes a null reference of that kind |
| `ref.is_null` | pops a reference; 1 if it is null |
| `ref.func F` | pushes a reference to function F |
| `table.get T` | pops an index, pushes the entry of table T |
| `table.set T` | pops a reference and an index, stores it in table T |
| `table.size T` | pushes the length of table T |
| `table.grow T` | pops a count and an initial value, grows table T, pushes the old length or -1 |
| `table.fill T` | pops start, value and count; fills table T |
| `table.copy D, S` | pops destination, source and count; copies from table S to D |
| `ref.test T` | pops a reference; 1 if it has type T |
| `ref.cast T` | the same reference, or a trap if it has not type T |

The other instructions of garbage-collected types (prefix `fb`) are shown
as `<unknown>`.

## 14. Exceptions

C++ `throw` and `catch`, with `-fwasm-exceptions`. Exceptions carry a
**tag**, which says what kind of exception it is and what values it holds.
C++ uses one tag, `__cpp_exception`, holding a pointer.

| Instruction | Does |
| --- | --- |
| `throw X` | pops the tag X's values and throws |
| `throw_ref` | pops an `exnref` and throws it again |
| `try_table T (catch X L) ...` | a block; an exception thrown inside it goes to label L of the first clause that matches |

A `try_table`'s clauses are in parentheses, one per catch:

| Clause | Catches |
| --- | --- |
| `(catch X L)` | tag X; pushes its values and branches to L |
| `(catch_ref X L)` | the same, and also pushes the `exnref` |
| `(catch_all L)` | anything, and branches to L |
| `(catch_all_ref L)` | anything, and pushes the `exnref` |

The older form, which clang still writes with `-wasm-use-legacy-eh`:

| Instruction | Does |
| --- | --- |
| `try T` | starts a block whose exceptions go to its `catch` |
| `catch X` | starts the handler for tag X; comment `# catchN:` |
| `catch_all` | starts the handler for anything else |
| `rethrow N` | throws again what the handler N levels out caught; comment `# down to catchN`, or `to caller` |
| `delegate N` | ends a `try` and hands its exceptions to the handler N levels out; comment `# label/catchN: down to catchM`, or `to caller` |

`try-catch mismatch!` and similar comments mean a `catch` has no `try`.

## 15. Atomics

For threads and shared memory. Every access is **atomic**: no other thread
sees half of it. The operand is an offset, as in §8. A space comes before
it, or `acqrel` for an acquire-release rather than a sequentially
consistent access.

| Instruction | Does |
| --- | --- |
| `i32.atomic.load`, `i64.atomic.load` and their `8_u`, `16_u`, `32_u` forms | load |
| `i32.atomic.store`, `i64.atomic.store` and their `8`, `16`, `32` forms | store |
| `i32.atomic.rmw.add`, `.sub`, `.and`, `.or`, `.xor`, `.xchg` | read, modify and write back; pushes the old value |
| `i32.atomic.rmw.cmpxchg` | pops an expected and a new value; stores the new one if memory holds the expected one; pushes the old value |
| `memory.atomic.wait32`, `memory.atomic.wait64` | sleeps while memory holds a given value, until notified or timed out |
| `memory.atomic.notify` | wakes up to N threads waiting at an address |
| `atomic.fence` | orders the memory accesses before it before those after it |

The `rmw` forms exist for `i64` too, and in narrow forms that touch 8, 16
or 32 bits: `i32.atomic.rmw8.add_u`, `i64.atomic.rmw32.cmpxchg_u`.

`compiler_fence` is not WebAssembly. It is llvm's own marker, for the
compiler alone.

## 16. Vectors (SIMD)

A `v128` holds several **lanes** of the same type, and one instruction
works on all of them at once. The prefix names the lanes:

| Prefix | Lanes |
| --- | --- |
| `i8x16` | 16 × 8-bit integers |
| `i16x8` | 8 × 16-bit integers |
| `i32x4` | 4 × 32-bit integers |
| `i64x2` | 2 × 64-bit integers |
| `f32x4` | 4 × `f32` |
| `f64x2` | 2 × `f64` |
| `v128` | 128 bits, lanes ignored |

**Whole vectors**:

| Instruction | Does |
| --- | --- |
| `v128.const A, B, C, D` | pushes a vector, printed as four 32-bit unsigned numbers, low first |
| `v128.load`, `v128.store` | loads, stores 16 bytes |
| `v128.not`, `v128.and`, `v128.andnot`, `v128.or`, `v128.xor` | bitwise; `andnot` is `a & ~b` |
| `v128.bitselect` | bits from the first where the third has ones, from the second elsewhere |
| `v128.any_true` | 1 if any bit is set |

**Loads that spread or widen**:

| Instruction | Does |
| --- | --- |
| `v128.load8_splat` ... `v128.load64_splat` | loads one value into every lane |
| `v128.load32_zero`, `v128.load64_zero` | loads into lane 0; the rest are 0 |
| `v128.load8_lane O, L` ... `v128.load64_lane` | loads into lane L only |
| `v128.store8_lane O, L` ... `v128.store64_lane` | stores lane L only |
| `i16x8.load8x8_s`, `i16x8.load8x8_u` | loads 8 bytes, each widened to 16 bits |
| `i32x4.load16x4_s`, `_u`; `i64x2.load32x2_s`, `_u` | the same, 16 to 32, 32 to 64 |

**Lanes**:

| Instruction | Does |
| --- | --- |
| `i32x4.splat` (any prefix) | makes a vector of one value in every lane |
| `i32x4.extract_lane L` | pushes lane L. `i8x16` and `i16x8` have `_s` and `_u` |
| `i32x4.replace_lane L` | replaces lane L |
| `i8x16.shuffle A, ..., P` | makes a vector of 16 bytes chosen from two: 0–15 from the first, 16–31 from the second |
| `i8x16.swizzle` | the same, with the choices from a vector, and 0 for one out of range |

**Arithmetic and comparisons**, lane by lane, with the meanings of §9 to
§11. The integer forms:

| Operation | Prefixes |
| --- | --- |
| `add`, `sub`, `neg`, `abs`, `shl`, `shr_s`, `shr_u` | all integer ones |
| `mul` | `i16x8`, `i32x4`, `i64x2` |
| `eq`, `ne` | all integer ones |
| `lt_s`, `gt_s`, `le_s`, `ge_s` | all integer ones |
| `lt_u`, `gt_u`, `le_u`, `ge_u` | `i8x16`, `i16x8`, `i32x4` |
| `min_s`, `min_u`, `max_s`, `max_u` | `i8x16`, `i16x8`, `i32x4` |
| `add_sat_s`, `add_sat_u`, `sub_sat_s`, `sub_sat_u` | `i8x16`, `i16x8`; clamp at the limit instead of wrapping |
| `avgr_u` | `i8x16`, `i16x8`; the average, rounded up |
| `popcnt` | `i8x16` |
| `all_true` | all integer ones; 1 if no lane is 0 |
| `bitmask` | all integer ones; an `i32` of each lane's top bit |
| `q15mulr_sat_s` | `i16x8`; fixed-point multiply |
| `dot_i16x8_s` | `i32x4`; multiplies pairs of 16-bit lanes and adds each pair |

A comparison gives each lane all ones for true, all zeros for false.

The float forms, for `f32x4` and `f64x2`: `add`, `sub`, `mul`, `div`,
`min`, `max`, `abs`, `neg`, `sqrt`, `ceil`, `floor`, `trunc`, `nearest`,
`eq`, `ne`, `lt`, `gt`, `le`, `ge`. And `pmin`, `pmax`: C's `b < a ? b :
a`, which differs from `min` for NaN and -0.

**Changing lane size**:

| Instruction | Does |
| --- | --- |
| `i16x8.extend_low_i8x16_s`, `extend_high_...`, `_u` | widens the low or high half of the lanes. Also `i32x4` from `i16x8`, `i64x2` from `i32x4` |
| `i8x16.narrow_i16x8_s`, `_u` | narrows two vectors into one, clamping. Also `i16x8` from `i32x4` |
| `i16x8.extmul_low_i8x16_s`, `extmul_high_...`, `_u` | multiplies half the lanes into wider ones. Also `i32x4`, `i64x2` |
| `i16x8.extadd_pairwise_i8x16_s`, `_u` | adds neighbouring lanes into wider ones. Also `i32x4` from `i16x8` |
| `i32x4.trunc_sat_f32x4_s`, `_u` | float lanes to integers, clamping |
| `i32x4.trunc_sat_f64x2_s_zero`, `_u_zero` | 2 doubles to 2 integers; the upper lanes are 0 |
| `f32x4.convert_i32x4_s`, `_u` | integer lanes to float |
| `f64x2.convert_low_i32x4_s`, `_u` | the low 2 integers to doubles |
| `f32x4.demote_f64x2_zero` | 2 doubles to 2 floats; the upper lanes are 0 |
| `f64x2.promote_low_f32x4` | the low 2 floats to doubles |

## 17. Encoding, in brief

An instruction is one opcode byte and its operands. The groups with many
members share a **prefix** byte, followed by the operation's number as an
unsigned LEB: `fc` for saturating conversions, bulk memory and tables,
`fd` for vectors, `fe` for atomics, `fb` for garbage-collected types.
Integers and indices are LEB128 ([Wasm_Object_Format.md](Wasm_Object_Format.md)
§2); floats are 4 or 8 bytes little-endian; a vector constant is 16 bytes.
A memory access carries two LEBs, the alignment's power of two and the
offset. So in §2, `2d 00 90 80 80 80 00` is `i32.load8_u` (`2d`),
alignment 2⁰ (`00`) and offset 16 padded to five bytes.
