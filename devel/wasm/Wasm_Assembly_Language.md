# WebAssembly assembly language

The language `as` reads: the WebAssembly text format, `.wat`, as
WebAssembly 3.0 defines it, and four extensions (§10). A source file
describes one module. This is the whole grammar: tokens, values, types,
instructions and modules, and what each abbreviation stands for. What a
module means, and how it is checked, is the core specification's; the
bytes each instruction encodes to are the binary format's, and
[Wasm_Bytecode.md](Wasm_Bytecode.md) explains the instructions.

## 1. A first look

```
(module
  (import "env" "puts" (func $puts (param i32)))
  (memory (export "memory") 1)
  (data (i32.const 16) "hello\00")
  (func $main (export "main") (result i32)
    (local $i i32)
    (call $puts (i32.const 16))
    (local.set $i (i32.const 3))
    (block $done
      (loop $again
        (br_if $done (i32.eqz (local.get $i)))
        local.get $i
        i32.const 1
        i32.sub
        local.set $i
        br $again))
    i32.const 0))
```

A module is an S-expression: a list in parentheses, its fields lists
too. Code is written flat, one instruction after another as the machine
runs them, or **folded**, where an instruction in parentheses takes its
operands from the lists inside it. Both forms mix freely. Every index
can be a number or a name: `$puts` is function 0, `$i` local 0.

## 2. Notation

```
name ::= …      a production
'text'          literal characters
a b             a, then b
a | b           a or b
( … )           grouping
x?  x*  x+      optional, zero or more, one or more
x^n             exactly n times
A ≡ B           abbreviation: A is written, B is meant
```

Literal parentheses are always quoted: `'(' 'module' … ')'`. A bare
parenthesis only groups.

§3 and §4 are **lexical**: nothing may stand between the parts of a
production. The rest is **syntactic**: white space may stand between
any two tokens, and must stand between two that would otherwise run
together (§3.2).

Abbreviations are expanded in the order they appear, before anything is
numbered. The assembler is free to never write the long form out.

## 3. Lexical structure

### 3.1 Characters

Source text is UTF-8. Any Unicode character may appear in a comment or
a string; everything else is ASCII. A line ends with LF, CR, or CR LF.

### 3.2 Tokens

```
token    ::= keyword | uN | sN | fN | string | id | '(' | ')' | reserved
keyword  ::= ('a' … 'z') idchar*
reserved ::= (idchar | string)+ | ',' | ';' | '[' | ']' | '{' | '}'
idchar   ::= '0' … '9' | 'A' … 'Z' | 'a' … 'z'
           | '!' | '#' | '$' | '%' | '&' | ''' | '*' | '+' | '-' | '.'
           | '/' | ':' | '<' | '=' | '>' | '?' | '@' | '\' | '^' | '_'
           | '`' | '|' | '~'
```

The text is cut into tokens from left to right, each the **longest**
run of characters that forms one. A run that is no other token is
reserved, and a reserved token is an error. So tokens other than
parentheses must be separated by white space: `i32.const0`, `0$x`,
`br 0drop`, `"a""b"` and `data"a"` are each one reserved token and an
error.

The keywords are exactly the words the grammar spells, such as `module`,
`i32.add` and `offset=16`; another word that looks like one is an error.
`offset=` and `align=` with their number are one token, with no space
around `=`. `inf`, `nan` and `nan:0x…` are float literals where the
grammar asks for a float.

### 3.3 White space and comments

```
space        ::= (' ' | U+09 | newline | comment | annotation)*
newline      ::= U+0A | U+0D | U+0D U+0A
comment      ::= linecomment | blockcomment
linecomment  ::= ';;' linechar* (newline | end of input)
linechar     ::= any character but U+0A and U+0D
blockcomment ::= '(;' (any text, blockcomments nested) ';)'
```

White space only separates tokens. Block comments nest:
`(; a (; b ;) c ;)` is one comment. An unclosed block comment is an
error, and so is a control character other than tab, LF and CR outside
comments and strings.

### 3.4 Annotations

```
annotation ::= '(@' annotid (space | token)* ')'
annotid    ::= idchar+ | name
```

An annotation may stand wherever white space may. There is no space
between `(@` and its id. Its tokens may be anything, as long as its
parentheses balance and its strings and comments close. It is white
space unless the assembler knows its id; §9 lists the ones it knows.

## 4. Values

### 4.1 Integers

```
sign     ::= '+' | '-'
digit    ::= '0' … '9'
hexdigit ::= digit | 'a' … 'f' | 'A' … 'F'
num      ::= digit ('_'? digit)*
hexnum   ::= hexdigit ('_'? hexdigit)*
uN       ::= num | '0x' hexnum
sN       ::= sign? uN
iN       ::= uN | sN
```

An underscore separates two digits and means nothing: `1_000_000`,
`0xffff_ffff`. It cannot lead, trail or double, nor follow `0x`.

| Form | Range |
| --- | --- |
| `uN`, unsigned | 0 … 2ᴺ−1 |
| `sN`, signed | −2ᴺ⁻¹ … 2ᴺ⁻¹−1 |
| `iN`, either | −2ᴺ⁻¹ … 2ᴺ−1; a negative value is stored as 2ᴺ plus it |

So `i32.const -1` and `i32.const 0xffffffff` are the same instruction.
`uN` takes no sign: `+1` is not an index.

### 4.2 Floats

```
float    ::= num ('.' num?)? (('e' | 'E') sign? num)?
hexfloat ::= '0x' hexnum ('.' hexnum?)? (('p' | 'P') sign? num)?
fNmag    ::= float | hexfloat | 'inf' | 'nan' | 'nan:0x' hexnum
fN       ::= sign? fNmag
```

`p.q e E` is p.q × 10ᴱ; `0xp.q p E` is p.q in hexadecimal × 2ᴱ, the
exponent in decimal. Digits after the point are optional, and so is the
point: `1`, `1.`, `1.5`, `1e10`, `0x1p-3`, `0x1.8p+1` are all floats.
There must be a digit before the point: `.5` is an error.

- The value is rounded to the nearest `f32` or `f64`, ties to even. A
  value that rounds to infinity is an error; write `inf`.
- `nan` is the canonical NaN: only the top bit of the significand set.
  `nan:0xH` sets the significand to H, with 1 ≤ H < 2²³ for `f32` and
  2⁵² for `f64`.
- The sign is the sign bit, for `inf` and `nan` too: `-nan`, `-0`.

### 4.3 Strings

```
string     ::= '"' stringelem* '"'
stringelem ::= stringchar | '\' hexdigit hexdigit
stringchar ::= any character from U+20 up, but U+7F, '"' and '\'
             | '\t' | '\n' | '\r' | '\"' | '\'' | '\\'
             | '\u{' hexnum '}'
```

A string is a sequence of bytes, shorter than 2³² of them. A character
stands for its UTF-8 encoding; `\hh` stands for the one byte hh, so a
string need not be UTF-8. `\u{…}` is a code point below U+D800, or from
U+E000 below U+110000. A string cannot span a line: a raw newline or tab
is an error inside it.

```
name ::= string        its bytes valid UTF-8
```

A **name** is a string that is text: import and export names, custom
section names.

### 4.4 Identifiers

```
id ::= '$' idchar+ | '$' name
```

The quoted form allows any characters, and must not be empty: `$"a b"`.
`$x` and `$"x"` are the same identifier.

## 5. Types

### 5.1 Value types

```
numtype     ::= 'i32' | 'i64' | 'f32' | 'f64'
vectype     ::= 'v128'
absheaptype ::= 'any' | 'eq' | 'i31' | 'struct' | 'array' | 'none'
              | 'func' | 'nofunc' | 'exn' | 'noexn' | 'extern' | 'noextern'
heaptype    ::= absheaptype | typeidx
reftype     ::= '(' 'ref' 'null'? heaptype ')'
valtype     ::= numtype | vectype | reftype
```

Each abstract heap type has a nullable shorthand:

| Shorthand | Means |
| --- | --- |
| `anyref`, `eqref`, `i31ref` | `(ref null any)`, `(ref null eq)`, `(ref null i31)` |
| `structref`, `arrayref`, `nullref` | `(ref null struct)`, `(ref null array)`, `(ref null none)` |
| `funcref`, `nullfuncref` | `(ref null func)`, `(ref null nofunc)` |
| `exnref`, `nullexnref` | `(ref null exn)`, `(ref null noexn)` |
| `externref`, `nullexternref` | `(ref null extern)`, `(ref null noextern)` |

### 5.2 Type definitions

```
packtype    ::= 'i8' | 'i16'
storagetype ::= valtype | packtype
fieldtype   ::= storagetype | '(' 'mut' storagetype ')'
field       ::= '(' 'field' id? fieldtype ')'
param       ::= '(' 'param' id? valtype ')'
result      ::= '(' 'result' valtype ')'
comptype    ::= '(' 'func' param* result* ')'
              | '(' 'struct' field* ')'
              | '(' 'array' fieldtype ')'
subtype     ::= '(' 'sub' 'final'? typeidx* comptype ')'
typedef     ::= '(' 'type' id? subtype ')'
rectype     ::= '(' 'rec' typedef* ')'
```

Abbreviations:

- `(field t₁ … tₙ)` ≡ `(field t₁) … (field tₙ)`, and alike for `param`,
  `result` and `local` (§8.3). The short form binds no id, and may be
  empty: `(param)` is nothing.
- `comptype` ≡ `(sub final comptype)`: a type written without `sub` is
  final and has no supertype.
- `typedef` ≡ `(rec typedef)`: a type outside `rec` is a group of one.

Params always precede results. A param's id in a type definition is
documentation and cannot be referred to. A field's id names the field
within its own type (§6).

### 5.3 Types of imports and definitions

```
addrtype   ::= 'i32' | 'i64'
limits     ::= u64 u64?
memtype    ::= addrtype? limits
tabletype  ::= addrtype? limits reftype
globaltype ::= valtype | '(' 'mut' valtype ')'
tagtype    ::= typeuse
```

The address type is `i32` when left out; `i64` makes a 64-bit memory or
table. The limits are the minimum and the optional maximum, in 64 KiB
pages for a memory and in entries for a table. §10 adds `shared` and a
page size to `memtype`.

### 5.4 Type uses

A **type use** names a function type: of a function, an imported
function, a tag, a block, or an indirect call.

```
typeuse ::= '(' 'type' typeidx ')' param* result*
          | param* result*
```

- `(type x)` alone uses type x, which must be a function type.
- `(type x)` with params and results uses type x too; they must repeat
  its params and results exactly, and serve to give the params ids.
- Params and results alone are an abbreviation for `(type x)` followed
  by them, where x is the first type in the module that is a final
  function type with those params and results, with no supertype, and
  alone in its `rec` group. If there is none, one is made: `(type (func
  param* result*))` is appended to the module, after every type it
  defines. The next use of the same signature finds it.

In a block type and an indirect call, the params must have no ids.

## 6. Indices and scope

```
idx ::= u32 | id
```

Every index space has its own names; `$x` can be a function and a global
at once. The spaces are types, functions, tables, memories, globals,
tags, element segments, data segments, locals, labels, and each struct
type's fields.

- **Binding.** A definition or import binds the id written after its
  keyword: `(func $f …)`, `(global $g …)`, `(elem $e …)`. An id bound
  twice in one space is an error.
- **Scope.** A module-level id is known in the whole module, before its
  definition as well as after.
- **Numbering.** Each space is numbered from 0 in the order the text
  defines it. Imports come first because they must (§8.1). A `rec`
  group numbers its types in order; the types §5.4 makes follow every
  type the text defines. A segment written inside a `memory` or `table`
  (§8.5) is numbered where it is written.
- **Locals** are the params, then the declared locals, in order. Both
  can bind ids, from the function's type use and from `(local $x t)`,
  and share one space.
- **Labels** count outwards: 0 is the innermost enclosing block, loop,
  `if`, `try_table` or `try`. A label id is the innermost enclosing
  block that binds it, so an inner block of the same name hides an outer
  one. One more than the outermost block is the function body itself; a
  branch to it returns. It has no name.
- **Fields.** In `struct.get $t $f`, `$f` is looked up among the field
  ids of type `$t`, and a number is the field's position.

## 7. Instructions

```
instr ::= plaininstr | blockinstr | foldedinstr
expr  ::= instr*
```

A plain instruction is its keyword and immediates. The keyword is the
instruction's name, and §7.5 lists every one with its immediates.

### 7.1 Blocks

```
label      ::= id?
blocktype  ::= result? | typeuse
blockinstr ::= 'block' label blocktype instr* 'end' id?
             | 'loop' label blocktype instr* 'end' id?
             | 'if' label blocktype instr* ('else' id? instr*)? 'end' id?
             | 'try_table' label blocktype catch* instr* 'end' id?
catch      ::= '(' 'catch' tagidx labelidx ')'
             | '(' 'catch_ref' tagidx labelidx ')'
             | '(' 'catch_all' labelidx ')'
             | '(' 'catch_all_ref' labelidx ')'
```

- A block type of nothing, or of one `(result t)`, is written in the
  binary as that; anything else, `(type x)`, params, or several results,
  is a type use (§5.4) and is written as a type index.
- An id after `else` or `end` repeats the block's label, as a check. It
  must equal it, and is an error on a block with no label.
- `if … end` with no `else` ≡ `if … else end`.
- A `catch`'s label counts from outside the `try_table`, not from inside
  it.

### 7.2 Folded instructions

```
foldedinstr ::= '(' plaininstr foldedinstr* ')'
              | '(' 'block' label blocktype instr* ')'
              | '(' 'loop' label blocktype instr* ')'
              | '(' 'if' label blocktype foldedinstr*
                    '(' 'then' instr* ')' ('(' 'else' instr* ')')? ')'
              | '(' 'try_table' label blocktype catch* instr* ')'
```

| Folded | Means |
| --- | --- |
| `(P F₁ … Fₙ)` | `F₁ … Fₙ P` |
| `(block L T I*)` | `block L T I* end`; `loop` and `try_table` alike |
| `(if L T F* (then I₁*) (else I₂*))` | `F* if L T I₁* else I₂* end` |

The operands of a folded plain instruction are folded too; the body of
a folded block, `then` or `else` is any instructions. Folding is only
notation: nothing checks that the operands fit the instruction.

`(i32.mul (i32.add (local.get $x) (i32.const 2)) (i32.const 3))` is
`local.get $x i32.const 2 i32.add i32.const 3 i32.mul`.

Two instructions have immediates in parentheses: `select` its results
and `call_indirect` its type use. In the folded form these come first,
and the operands after: `(select (result i32) F₁ F₂ F₃)`,
`(call_indirect $t (param i32) (result i32) F₁ F₂)`. An operand never
starts with `(type`, `(param` or `(result`.

### 7.3 Memory operands

```
memarg ::= ('offset=' u64)? ('align=' u64)?
```

A memory access is `memidx? memarg`, and a lane access `memidx? memarg
laneidx`. The memory index is 0 when left out; offset and alignment
come in that order. The offset is 0 when left out. The alignment is in
bytes, a power of two, and is the access's natural size, N in §7.5,
when left out; the binary holds its logarithm.

`i32.load 1` reads memory 1; `v128.load8_lane 1` is lane 1 of memory 0,
and `v128.load8_lane 1 2` lane 2 of memory 1. A lane access takes its
last number as the lane.

### 7.4 Immediates

| Written | Is |
| --- | --- |
| `x`, `y` | an index of the kind named in the table: `funcidx`, `typeidx`, … |
| `x?` | an index that is 0 when left out |
| `(x y)?` | two indices, or neither, both then 0 |
| `x? y` | one index is `y`, two are `x y` |
| `lane` | `u8` |
| `ma` | `memidx? memarg` |

### 7.5 Every instruction

**Control**

| Instruction | Immediates |
| --- | --- |
| `unreachable`, `nop`, `drop`, `return`, `throw_ref` | |
| `select` | `result*` |
| `br`, `br_if`, `br_on_null`, `br_on_non_null` | `labelidx` |
| `br_table` | `labelidx+`, the last the default |
| `br_on_cast`, `br_on_cast_fail` | `labelidx reftype reftype` |
| `call`, `return_call` | `funcidx` |
| `call_ref`, `return_call_ref` | `typeidx` |
| `call_indirect`, `return_call_indirect` | `tableidx? typeuse` |
| `throw` | `tagidx` |

`block`, `loop`, `if`, `else`, `end` and `try_table` are §7.1.

**Variables**

| Instruction | Immediates |
| --- | --- |
| `local.get`, `local.set`, `local.tee` | `localidx` |
| `global.get`, `global.set` | `globalidx` |

**Tables**

| Instruction | Immediates |
| --- | --- |
| `table.get`, `table.set`, `table.size`, `table.grow`, `table.fill` | `tableidx?` |
| `table.copy` | `(tableidx tableidx)?`, destination first |
| `table.init` | `tableidx? elemidx` |
| `elem.drop` | `elemidx` |

**Memory**

| Instruction | N | Immediates |
| --- | --- | --- |
| `i32.load8_s`, `i32.load8_u`, `i64.load8_s`, `i64.load8_u` | 1 | `ma` |
| `i32.load16_s`, `i32.load16_u`, `i64.load16_s`, `i64.load16_u` | 2 | `ma` |
| `i32.load`, `f32.load`, `i64.load32_s`, `i64.load32_u` | 4 | `ma` |
| `i64.load`, `f64.load` | 8 | `ma` |
| `i32.store8`, `i64.store8` | 1 | `ma` |
| `i32.store16`, `i64.store16` | 2 | `ma` |
| `i32.store`, `f32.store`, `i64.store32` | 4 | `ma` |
| `i64.store`, `f64.store` | 8 | `ma` |
| `v128.load`, `v128.store` | 16 | `ma` |
| `v128.load8x8_s`, `v128.load8x8_u`, `v128.load16x4_s`, `v128.load16x4_u`, `v128.load32x2_s`, `v128.load32x2_u` | 8 | `ma` |
| `v128.load8_splat`, `v128.load16_splat`, `v128.load32_splat`, `v128.load64_splat` | 1, 2, 4, 8 | `ma` |
| `v128.load32_zero`, `v128.load64_zero` | 4, 8 | `ma` |
| `v128.load8_lane`, `v128.load16_lane`, `v128.load32_lane`, `v128.load64_lane` | 1, 2, 4, 8 | `ma lane` |
| `v128.store8_lane`, `v128.store16_lane`, `v128.store32_lane`, `v128.store64_lane` | 1, 2, 4, 8 | `ma lane` |
| `memory.size`, `memory.grow`, `memory.fill` | | `memidx?` |
| `memory.copy` | | `(memidx memidx)?`, destination first |
| `memory.init` | | `memidx? dataidx` |
| `data.drop` | | `dataidx` |

**References and aggregates**

| Instruction | Immediates |
| --- | --- |
| `ref.null` | `heaptype`: `ref.null func`, `ref.null $t` |
| `ref.func` | `funcidx` |
| `ref.is_null`, `ref.as_non_null`, `ref.eq` | |
| `ref.test`, `ref.cast` | `reftype` |
| `ref.i31`, `i31.get_s`, `i31.get_u` | |
| `struct.new`, `struct.new_default` | `typeidx` |
| `struct.get`, `struct.get_s`, `struct.get_u`, `struct.set` | `typeidx fieldidx` |
| `array.new`, `array.new_default`, `array.get`, `array.get_s`, `array.get_u`, `array.set`, `array.fill` | `typeidx` |
| `array.new_fixed` | `typeidx u32` |
| `array.new_data`, `array.init_data` | `typeidx dataidx` |
| `array.new_elem`, `array.init_elem` | `typeidx elemidx` |
| `array.copy` | `typeidx typeidx`, destination first |
| `array.len`, `any.convert_extern`, `extern.convert_any` | |

**Numbers.** The constants:

| Instruction | Immediate |
| --- | --- |
| `i32.const` | `i32` |
| `i64.const` | `i64` |
| `f32.const` | `f32` |
| `f64.const` | `f64` |

Every other numeric instruction has no immediate. `t.` stands for each
type named in the heading.

```
i32. i64.   eqz eq ne lt_s lt_u gt_s gt_u le_s le_u ge_s ge_u
            clz ctz popcnt extend8_s extend16_s
            add sub mul div_s div_u rem_s rem_u and or xor
            shl shr_s shr_u rotl rotr
i64.        extend32_s

f32. f64.   eq ne lt gt le ge
            abs neg sqrt ceil floor trunc nearest
            add sub mul div min max copysign

            i32.wrap_i64  i64.extend_i32_s  i64.extend_i32_u
            f32.demote_f64  f64.promote_f32
            i32.reinterpret_f32  i64.reinterpret_f64
            f32.reinterpret_i32  f64.reinterpret_i64
```

and, for I each of `i32`, `i64`, F each of `f32`, `f64`, and x each of
`s`, `u`: `I.trunc_F_x`, `I.trunc_sat_F_x` and `F.convert_I_x`.

**Vectors.** With immediates:

| Instruction | Immediates |
| --- | --- |
| `v128.const` | a shape and its lanes: `i8x16` `i8`¹⁶, `i16x8` `i16`⁸, `i32x4` `i32`⁴, `i64x2` `i64`², `f32x4` `f32`⁴, `f64x2` `f64`² |
| `i8x16.shuffle` | `lane`¹⁶ |
| `i8x16.extract_lane_s`, `i8x16.extract_lane_u`, `i16x8.extract_lane_s`, `i16x8.extract_lane_u` | `lane` |
| `i32x4.extract_lane`, `i64x2.extract_lane`, `f32x4.extract_lane`, `f64x2.extract_lane` | `lane` |
| `i8x16.replace_lane`, `i16x8.replace_lane`, `i32x4.replace_lane`, `i64x2.replace_lane`, `f32x4.replace_lane`, `f64x2.replace_lane` | `lane` |

`v128.const i32x4 1 2 3 4`; `v128.const f64x2 0.5 -inf`.

Without, one row for each prefix; no other combination exists:

```
v128.   not and andnot or xor bitselect any_true

i8x16.  splat swizzle relaxed_swizzle all_true bitmask
        eq ne lt_s lt_u gt_s gt_u le_s le_u ge_s ge_u
        abs neg popcnt add add_sat_s add_sat_u sub sub_sat_s sub_sat_u
        min_s min_u max_s max_u avgr_u shl shr_s shr_u
        narrow_i16x8_s narrow_i16x8_u relaxed_laneselect

i16x8.  splat all_true bitmask
        eq ne lt_s lt_u gt_s gt_u le_s le_u ge_s ge_u
        abs neg add add_sat_s add_sat_u sub sub_sat_s sub_sat_u mul
        min_s min_u max_s max_u avgr_u q15mulr_sat_s relaxed_q15mulr_s
        shl shr_s shr_u narrow_i32x4_s narrow_i32x4_u
        extend_low_i8x16_s extend_low_i8x16_u
        extend_high_i8x16_s extend_high_i8x16_u
        extadd_pairwise_i8x16_s extadd_pairwise_i8x16_u
        extmul_low_i8x16_s extmul_low_i8x16_u
        extmul_high_i8x16_s extmul_high_i8x16_u
        relaxed_dot_i8x16_i7x16_s relaxed_laneselect

i32x4.  splat all_true bitmask
        eq ne lt_s lt_u gt_s gt_u le_s le_u ge_s ge_u
        abs neg add sub mul min_s min_u max_s max_u dot_i16x8_s
        shl shr_s shr_u
        extend_low_i16x8_s extend_low_i16x8_u
        extend_high_i16x8_s extend_high_i16x8_u
        extadd_pairwise_i16x8_s extadd_pairwise_i16x8_u
        extmul_low_i16x8_s extmul_low_i16x8_u
        extmul_high_i16x8_s extmul_high_i16x8_u
        trunc_sat_f32x4_s trunc_sat_f32x4_u
        trunc_sat_f64x2_s_zero trunc_sat_f64x2_u_zero
        relaxed_trunc_f32x4_s relaxed_trunc_f32x4_u
        relaxed_trunc_f64x2_s_zero relaxed_trunc_f64x2_u_zero
        relaxed_dot_i8x16_i7x16_add_s relaxed_laneselect

i64x2.  splat all_true bitmask
        eq ne lt_s gt_s le_s ge_s
        abs neg add sub mul shl shr_s shr_u
        extend_low_i32x4_s extend_low_i32x4_u
        extend_high_i32x4_s extend_high_i32x4_u
        extmul_low_i32x4_s extmul_low_i32x4_u
        extmul_high_i32x4_s extmul_high_i32x4_u
        relaxed_laneselect

f32x4.  splat eq ne lt gt le ge
        abs neg sqrt ceil floor trunc nearest
        add sub mul div min max pmin pmax
        relaxed_min relaxed_max relaxed_madd relaxed_nmadd
        demote_f64x2_zero convert_i32x4_s convert_i32x4_u

f64x2.  splat eq ne lt gt le ge
        abs neg sqrt ceil floor trunc nearest
        add sub mul div min max pmin pmax
        relaxed_min relaxed_max relaxed_madd relaxed_nmadd
        promote_low_f32x4 convert_low_i32x4_s convert_low_i32x4_u
```

## 8. Modules

```
module      ::= '(' 'module' id? modulefield* ')'
              | modulefield*
modulefield ::= type | import | func | table | memory | global | tag
              | export | start | elem | data
```

A file holds one module. The `(module …)` around it may be left out.
The module's id is documentation.

Fields come in any order, with two rules: no import after the
definition of a function, table, memory, global or tag, whatever the
kinds of the two; and at most one `start`.

### 8.1 Types, imports, exports, start

```
type       ::= rectype
import     ::= '(' 'import' name name externtype ')'
externtype ::= '(' 'func' id? typeuse ')'
             | '(' 'table' id? tabletype ')'
             | '(' 'memory' id? memtype ')'
             | '(' 'global' id? globaltype ')'
             | '(' 'tag' id? tagtype ')'
export     ::= '(' 'export' name externidx ')'
externidx  ::= '(' 'func' funcidx ')' | '(' 'table' tableidx ')'
             | '(' 'memory' memidx ')' | '(' 'global' globalidx ')'
             | '(' 'tag' tagidx ')'
start      ::= '(' 'start' funcidx ')'
```

An import names its module, then the item in it.

### 8.2 Functions

```
func  ::= '(' 'func' id? typeuse local* instr* ')'
local ::= '(' 'local' id? valtype ')'
```

The body has no `end`; the closing parenthesis ends it.

### 8.3 Tables, memories, globals, tags

```
table  ::= '(' 'table' id? tabletype expr ')'
memory ::= '(' 'memory' id? memtype ')'
global ::= '(' 'global' id? globaltype expr ')'
tag    ::= '(' 'tag' id? tagtype ')'
```

A table's `expr`, one instruction or more, is the value of every entry
at the start. It may be left out: `(table id? tabletype)` ≡
`(table id? tabletype (ref.null ht))`, where ht is the heap type of the
table's reference type.

### 8.4 Element and data segments

```
elem     ::= '(' 'elem' id? elemlist ')'
           | '(' 'elem' id? tableuse? offset elemlist ')'
           | '(' 'elem' id? 'declare' elemlist ')'
elemlist ::= reftype elemexpr* | 'func' funcidx*
elemexpr ::= '(' 'item' expr ')'
tableuse ::= '(' 'table' tableidx ')'
data     ::= '(' 'data' id? string* ')'
           | '(' 'data' id? memuse? offset string* ')'
memuse   ::= '(' 'memory' memidx ')'
offset   ::= '(' 'offset' expr ')'
```

A segment is **passive**, copied in by `table.init` or `memory.init`;
**active**, copied to its offset in a table or memory when the module
starts; or **declared**, which only says that `ref.func` may name its
functions. A data segment's strings are its bytes, one after another.

Abbreviations:

- A table or memory use left out is 0.
- An `offset` or `item` that is one folded instruction may be just
  that: `(elem (i32.const 1) …)`, `(elem funcref (ref.null func))`.
- `func x*` ≡ `(ref func) (item ref.func x)*`.
- With no table use, `func` may be left out too: `(elem offset x*)` ≡
  `(elem offset func x*)`.

### 8.5 Abbreviations of module fields

**Inline exports.** Any number of exports may follow the id of a
function, table, memory, global or tag:

```
(func $f (export "a") (export "b") …)
  ≡ (func $f …) (export "a" (func $f)) (export "b" (func $f))
```

A field with no id is given a fresh one.

**Inline imports.** After the exports, one import makes the field an
import, and what follows is its type: a type use, `tabletype`,
`memtype`, `globaltype` or `tagtype`.

```
(func $f (export "e") (import "m" "n") typeuse)
  ≡ (import "m" "n" (func $f typeuse)) (export "e" (func $f))
(table id? (import "m" "n") tt)   ≡ (import "m" "n" (table id? tt))
(memory id? (import "m" "n") mt)  ≡ (import "m" "n" (memory id? mt))
(global id? (import "m" "n") gt)  ≡ (import "m" "n" (global id? gt))
(tag id? (import "m" "n") tu)     ≡ (import "m" "n" (tag id? tu))
```

Such a field counts as an import for the rule on order in §8.

**Inline data.** A memory may hold its one active segment, at 0:

```
(memory $m at? (data s*))
  ≡ (memory $m at? n n) (data (memory $m) (at.const 0) s*)
```

where n is the length of the strings in pages, rounded up, and `at` is
the address type, `i32` if left out.

**Inline elements.** A table likewise:

```
(table $t at? rt (elem e*))
  ≡ (table $t at? n n rt) (elem (table $t) (at.const 0) rt e*)
```

where `e*` is `elemexpr*` or `funcidx*`, and n is how many there are.
The segment has the table's reference type either way.

### 8.6 Constant expressions

The `expr` of a global, a table, an offset and an item is evaluated
once, when the module starts. The grammar allows any instructions;
validation allows only these:

```
i32.const i64.const f32.const f64.const v128.const
i32.add i32.sub i32.mul i64.add i64.sub i64.mul
ref.null ref.func ref.i31 global.get
struct.new struct.new_default array.new array.new_default array.new_fixed
any.convert_extern extern.convert_any
```

`global.get` may read only an immutable global that the module imports
or defines before.

## 9. Annotations the assembler knows

**`(@name string)`** follows the id, or the keyword where there is no
id, of a module, function, local, param, global, table, memory, tag,
element or data segment. It is the name the `name` section gives the
thing, in place of its id:

```
(module (@name "Modül")
  (func $lambda (@name "λ") (param $x (@name "α") i32) …))
```

At most one per thing.

**`(@custom name place? string*)`**, among the module fields, is a
custom section: the name, then the bytes of the strings. `place` says
where it goes in the binary:

```
place   ::= '(' 'before' section ')' | '(' 'after' section ')'
          | '(' 'before' 'first' ')' | '(' 'after' 'last' ')'
section ::= 'type' | 'import' | 'func' | 'table' | 'memory' | 'tag'
          | 'global' | 'export' | 'start' | 'elem' | 'code' | 'data'
          | 'datacount'
```

It is `(after last)` when left out. A section named must be one the
module has. A `@custom` inside another field is ignored.

**`(@metadata.code.branch_hint string)`** stands right before an `if`
or a `br_if`, flat or folded. The string is `"\00"`, likely not taken,
or `"\01"`, likely taken. The hints go into the
`metadata.code.branch_hint` section.

Any other annotation is white space.

## 10. Beyond WebAssembly 3.0

Four extensions, each with its tests in the test suite's `proposals/`.
None changes anything above.

### 10.1 Threads

A memory can be shared between threads, and must then have a maximum:

```
memtype ::= addrtype? limits 'shared'?
```

The atomic instructions all take `ma`, as a load does, and N is their
natural alignment; an `align=` must equal it.

| Instruction | N |
| --- | --- |
| `i32.atomic.load8_u`, `i64.atomic.load8_u`, `i32.atomic.store8`, `i64.atomic.store8` | 1 |
| `i32.atomic.load16_u`, `i64.atomic.load16_u`, `i32.atomic.store16`, `i64.atomic.store16` | 2 |
| `i32.atomic.load`, `i64.atomic.load32_u`, `i32.atomic.store`, `i64.atomic.store32` | 4 |
| `i64.atomic.load`, `i64.atomic.store` | 8 |
| `i32.atomic.rmw.OP` | 4 |
| `i64.atomic.rmw.OP` | 8 |
| `i32.atomic.rmw8.OP_u`, `i64.atomic.rmw8.OP_u` | 1 |
| `i32.atomic.rmw16.OP_u`, `i64.atomic.rmw16.OP_u` | 2 |
| `i64.atomic.rmw32.OP_u` | 4 |
| `memory.atomic.notify`, `memory.atomic.wait32` | 4 |
| `memory.atomic.wait64` | 8 |

OP is each of `add`, `sub`, `and`, `or`, `xor`, `xchg` and `cmpxchg`:
`i32.atomic.rmw.add`, `i64.atomic.rmw32.cmpxchg_u`. And `atomic.fence`,
with no immediate.

### 10.2 Legacy exceptions

The exception instructions before `try_table`, which clang writes with
`-wasm-use-legacy-eh`:

```
blockinstr  ::= 'try' label blocktype instr*
                    ('catch' tagidx instr*)* ('catch_all' instr*)? 'end' id?
              | 'try' label blocktype instr* 'delegate' labelidx
plaininstr  ::= 'rethrow' labelidx
foldedinstr ::= '(' 'try' label blocktype '(' 'do' instr* ')'
                    ('(' 'catch' tagidx instr* ')')*
                    ('(' 'catch_all' instr* ')')? ')'
              | '(' 'try' label blocktype '(' 'do' instr* ')'
                    '(' 'delegate' labelidx ')' ')'
```

A `try` opens a label, as a block does. `rethrow`'s label is that of a
`try` whose `catch` or `catch_all` it is in. `delegate` ends its `try`
and counts its label from outside it.

### 10.3 Wide arithmetic

`i64.add128`, `i64.sub128`, `i64.mul_wide_s` and `i64.mul_wide_u`, with
no immediates.

### 10.4 Custom page sizes

```
memtype  ::= addrtype? limits 'shared'? pagesize?
pagesize ::= '(' 'pagesize' u32 ')'
```

The page size is a power of two, and only 1 and 65536 are valid. The
limits count pages of that size. It may also come before an inline
data segment, and then counts its pages: `(memory (pagesize 1) (data
"xyz"))` is 3 pages.

## 11. Not in the language

A `.wast` script, the test suite's format, holds many modules and
commands about them: `(module binary …)`, `(module quote …)`,
`(module definition …)`, `register`, `invoke`, `assert_return` and the
other `assert_`s, and `nan:canonical` and `nan:arithmetic`. None of it
is a module, and the assembler reads none of it.

The names WebAssembly used before 2018 are gone and are errors:
`get_local`, `set_local`, `tee_local`, `get_global`, `set_global`,
`current_memory`, `grow_memory`, `anyfunc`, and conversions spelled
with a slash, such as `i32.wrap/i64`, `i32.trunc_s:sat/f32` and
`f32x4.convert_s/i32x4`.
