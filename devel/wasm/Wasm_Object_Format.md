# The wasm object file and the executable module

What a linker reads and what it writes, byte by byte. No prior knowledge of
WebAssembly is assumed. Sources: the WebAssembly core spec (binary format)
and tool-conventions `Linking.md` (object files). Every hex dump here comes
from a real file compiled by clang 23.

In this package, [lib/](lib/) is what reads it: `module.cpp` the sections
of any module (§3), `object.cpp` an object (§7 to §9), `archive.cpp` an
archive (§10) and `stamp.cpp` the `braam` section (§11).

## 1. The big picture

A C or C++ source is compiled to an **object file** (`.o`). An object file is
already a valid wasm module, but it is incomplete:

- it calls functions it does not contain (`printf`, `sys`);
- its variables have **provisional addresses**, as if the file were alone in
  memory, starting at 0;
- its function numbers count only its own functions.

So the object carries extra notes that say where every such number sits and
what it stands for. The linker combines many objects, picks final numbers,
**patches** them in, removes the notes, and writes one **executable module**
(`.wasm`) that the browser can run.

```
  a.o  b.o  libx.a ──► linker ──► prog.wasm
  (modules + notes)               (module, no notes)
```

Two kinds of notes, both stored as custom sections:

- `linking` — a **symbol table**: names of functions and variables, which are
  defined here and which are wanted from elsewhere.
- `reloc.*` — **relocations**: "at byte N of section S, put the final value
  of symbol K".

## 2. Basic encodings

### 2.1 Bytes and integers

Everything is little-endian. Most integers are **LEB128**: 7 bits per byte,
low bits first, the top bit of a byte says "more bytes follow".

```
   5 → 05
 300 → ac 02        300 = 0b10_0101100 → (0x2c|0x80), 0x02
```

**Unsigned LEB** (`u32`) for sizes, counts and indices. **Signed LEB**
(`s32`) for `i32.const` values: the last byte's bit 6 is the sign, so
positive 104 needs two bytes (`e8 00`), and -1 is one byte (`7f`).

A u32 takes at most 5 bytes. The same number may be written **padded** —
with extra `0x80` bytes before a final `0x00`. Both decode to 12:

```
0c                 minimal
8c 80 80 80 00     padded to 5 bytes
```

Compilers write padded 5-byte LEBs wherever a relocation will patch a value.
The linker can then write any final value into the same 5 bytes without
moving the rest of the code. Signed padding works the same way:
-1 padded is `ff ff ff ff 7f`.

Fixed-width integers (`u32le`) appear only in data and in a few relocation
targets.

### 2.2 Names and vectors

- **vec(X)**: a `u32` count, then that many X.
- **name**: a `u32` byte length, then UTF-8 bytes, no terminator.
  `03 65 6e 76` is `"env"`.

### 2.3 Types

| Byte   | Type        | Meaning                        |
|--------|-------------|--------------------------------|
| `7f`   | `i32`       | 32-bit integer, also a pointer |
| `7e`   | `i64`       | 64-bit integer                 |
| `7d`   | `f32`       | float                          |
| `7c`   | `f64`       | double                         |
| `7b`   | `v128`      | SIMD vector                    |
| `70`   | `funcref`   | reference to a function        |
| `6f`   | `externref` | opaque host reference          |

A **function type** is `60 vec(param types) vec(result types)`.
`60 01 7f 01 7f` is `int(int)`.

### 2.4 Limits

Sizes of memories and tables:

| Flag | Then       | Meaning                    |
|------|------------|----------------------------|
| `00` | min        | at least min, no maximum   |
| `01` | min max    | between min and max        |

Memory is counted in **pages of 64 KiB**, tables in entries. (Flags `02`,
`03` are shared memory and `04`+ is 64-bit memory; not used here.)

### 2.5 Constant expressions

A few places need a value computed at load time: a global's initial value,
a data segment's address. They are a tiny instruction sequence ending in
`0b` (`end`). In practice it is always one of:

```
41 <s32> 0b     i32.const N
23 <u32> 0b     global.get G   (only for imported globals)
```

## 3. Module layout

```
00 61 73 6d     magic "\0asm"
01 00 00 00     version 1
section*
```

Each **section** is:

```
id:u8   size:u32   contents[size]
```

The size lets a reader skip a section it does not understand.

| Id | Section     | Holds                                          |
|----|-------------|------------------------------------------------|
|  0 | custom      | a name, then anything (symbols, names, ...)    |
|  1 | type        | function types                                 |
|  2 | import      | what the module needs from outside             |
|  3 | function    | the type of each function defined here         |
|  4 | table       | tables defined here                            |
|  5 | memory      | memories defined here                          |
| 13 | tag         | exception tags                                 |
|  6 | global      | globals defined here, with initial values      |
|  7 | export      | what the module offers to outside              |
|  8 | start       | a function run automatically at load           |
|  9 | element     | initial contents of tables                     |
| 12 | datacount   | number of data segments                        |
| 10 | code        | function bodies                                |
| 11 | data        | initial contents of memory                     |

Non-custom sections appear **at most once and in the order of this table**
(note: 13 comes after 5, and 12 before 10). Custom sections may appear
anywhere, any number of times.

### 3.1 Index spaces

Functions, tables, memories, globals and tags are referred to by number.
Each kind has its own numbering, and **imports come first**:

```
function index:  0 .. nimported-1   imported functions, in import order
                 nimported ..        functions from the function section
```

So adding one imported function renumbers every defined function. This is
the main reason code needs relocations.

Types and data segments are numbered by their position in their section.

## 4. The standard sections

Each shown as its contents, after `id` and `size`.

**Type (1)**: `vec(functype)`.

**Import (2)**: `vec(import)`, each

```
module:name  field:name  kind:u8  description
```

| Kind | What     | Description                   |
|------|----------|-------------------------------|
| `00` | function | type index `u32`              |
| `01` | table    | ref type, limits              |
| `02` | memory   | limits                        |
| `03` | global   | value type, mutable `u8` 0/1  |
| `04` | tag      | `00`, type index              |

**Function (3)**: `vec(u32)` — for each defined function, its type index.
The bodies are in the code section, in the same order.

**Table (4)**: `vec(reftype limits)`.

**Memory (5)**: `vec(limits)`.

**Global (6)**: `vec(valtype mut:u8 constexpr)`.
`7f 01 41 80 80 08 0b` is a mutable i32 starting at 131072.

**Export (7)**: `vec(name kind:u8 index:u32)`, kinds as for import.
`06 5f 73 74 61 72 74 00 03` exports function 3 as `_start`.

**Start (8)**: one function index, called after the module is loaded.

**Element (9)**: `vec(segment)`. A segment fills table slots. The first byte
is a flags value 0..7. A linker writes and reads only:

```
00  constexpr  vec(funcidx)     active: table 0, starting at slot = constexpr
```

`00 41 01 0b 02 05 07` puts functions 5 and 7 into table slots 1 and 2.
Flags 1..7 are passive/declarative forms and explicit table numbers.

**Datacount (12)**: one `u32`, the number of data segments. Required when
code uses `memory.init`/`data.drop` (bulk memory); harmless otherwise.

**Code (10)**: `vec(body)`, each

```
size:u32   vec(count:u32 valtype)   instructions...   0b
           \_ local variables _/
```

`07 00 20 00 41 01 74 0b` is a 7-byte body: no extra locals,
`local.get 0; i32.const 1; i32.shl; end` — `return x << 1`.

**Data (11)**: `vec(segment)`, each

| Flags | Then                              | Meaning                      |
|-------|-----------------------------------|------------------------------|
| `00`  | constexpr `vec(byte)`             | active: copy to memory 0 at address |
| `01`  | `vec(byte)`                       | passive: copied only on `memory.init` |
| `02`  | memidx constexpr `vec(byte)`      | active, explicit memory      |

`00 41 04 0b 04 01 00 00 00` writes the 4 bytes `01 00 00 00` at address 4.

## 5. Instructions a linker must know

The linker never decodes instructions: relocations tell it exactly which
bytes to change. It helps to recognise these when reading dumps:

| Bytes                 | Instruction                | Patched field       |
|-----------------------|----------------------------|---------------------|
| `10 f`                | `call f`                   | function index      |
| `11 t tbl`            | `call_indirect (type t)`   | type, table index   |
| `41 v`                | `i32.const v`              | an address or slot  |
| `23 g` / `24 g`       | `global.get` / `global.set`| global index        |
| `28 a off`            | `i32.load align offset`    | offset = address    |
| `36 a off`            | `i32.store align offset`   | offset = address    |
| `d2 f`                | `ref.func f`               | function index      |
| `25 t` / `26 t`       | `table.get` / `table.set`  | table index         |
| `0b`                  | `end`                      | —                   |

Loads and stores add their `offset` to an address on the stack. For a global
variable the compiler pushes `i32.const 0` and puts the variable's address
in `offset`: `41 00 28 02 8c 80 80 80 00` reads the int at address 12.

## 6. Standard custom sections

A custom section's contents start with its **name**; the rest is up to it.

### 6.1 `name` — debugging names

Makes stack traces readable. Contents: a series of subsections
`id:u8 size:u32 contents`.

| Id | Holds                                  |
|----|----------------------------------------|
| 0  | module name                            |
| 1  | function names: `vec(index:u32 name)`  |
| 2  | local names                            |
| 7  | global names: `vec(index name)`        |
| 9  | data segment names: `vec(index name)`  |

Optional; the module runs the same without it.

### 6.2 `producers`

`vec(field)`, each `name vec(tool:name version:name)`. Field names are
`language`, `processed-by`, `sdk`. For example `processed-by: clang 23.1.2`.
A linker merges the inputs' lists.

### 6.3 `target_features`

`vec(prefix:u8 feature:name)`. The prefix is `+` (0x2b): "this file uses the
feature", `-` (0x2d): "this file must not be linked with anything that uses
it", `=` (0x3d, old): "every file must have it". Features are names like
`bulk-memory`, `sign-ext`, `mutable-globals`, `reference-types`,
`nontrapping-fptoint`. A linker checks they agree and writes the union of
`+`.

## 7. The object file

An object is a normal module plus `linking` and `reloc.*`. Some parts look
different from an executable:

- **Memory is imported** as `env.__linear_memory`, with min = pages enough
  for this file's data.
- **The function table is imported** as `env.__indirect_function_table`, if
  the file calls through pointers.
- **The stack pointer** is an imported mutable global `env.__stack_pointer`,
  if a function needs stack memory.
- **Wanted functions** are function imports: `env.printf`, or a module named
  in source with `import_module`.
- **Each function and each variable is separate.** clang puts every variable
  in its own data segment (`.data.x`, `.rodata.y`, `.bss.z`) and every
  function is its own body anyway. This lets the linker drop unused ones.
- **Data addresses are provisional**: segments are laid out one after
  another from address 0, each at its alignment. `.bss` segments are present,
  full of zeros.
- The **export** and **element** sections are informative only. The linker
  rebuilds both from the symbol table and relocations.

### 7.1 A worked example

```c
extern int ext(int);
__attribute__((import_module("kernel"), import_name("sys"))) int sys(int);
int counter;
const char msg[] = "hi";
const char *p = msg + 1;
static int twice(int x) { return 2 * x; }
int (*fp)(int) = twice;
__attribute__((export_name("_start")))
int start(void) { return fp(counter) + ext(1) + sys(msg[0]); }
__attribute__((constructor(200))) static void init(void) { counter = 5; }
```

`clang --target=wasm32 -Os -mreference-types -mbulk-memory -c t.c`.
The optimiser folded the constructor into `counter`'s initial value.

**Import section**:

```
04                                         4 imports
03 "env" 0f "__linear_memory"   02 00 01   memory, min 1 page
03 "env" 19 "__indirect_function_table"
                                01 70 00 01  table of funcref, min 1
03 "env" 03 "ext"               00 00      function 0, type 0
06 "kernel" 03 "sys"            00 00      function 1, type 0
```

So `twice` is function 2 and `start` function 3.

**Code section** (offsets from the start of the contents, in hex):

```
00  02                              2 bodies
01  07 00 20 00 41 01 74 0b         twice
09  32 00                           start: 50 bytes, no locals
0b  41 00 28 02 8c 80 80 80 00      i32.load [0 + 12]      counter
14  41 00 28 02 88 80 80 80 00      i32.load [0 + 8]       fp
1d  11 80 80 80 80 00               call_indirect type 0
       80 80 80 80 00                            table 0
28  41 01 10 80 80 80 80 00         call 0                 ext(1)
30  6a                              i32.add
31  41 e8 00                        i32.const 104          'h', folded
34  10 81 80 80 80 00               call 1                 sys
3a  6a 0b
```

Every number the linker will change is padded to 5 bytes. `msg[0]` needed
no relocation: the compiler knew it was `'h'`.

**Data section**:

```
00  04                              4 segments
01  00 41 00 0b 03 68 69 00         at 0:  "hi\0"          .rodata.msg
09  00 41 04 0b 04 01 00 00 00      at 4:  p = 1           .data.p
12  00 41 08 0b 04 01 00 00 00      at 8:  fp = slot 1     .data.fp
1b  00 41 0c 0b 04 05 00 00 00      at 12: counter = 5     .data.counter
```

The values of `p` and `fp` are provisional; relocations fix them.

## 8. The `linking` section

```
name "linking"
version:u32              always 2
subsection*              id:u8  size:u32  contents
```

| Id | Subsection          | Holds                                    |
|----|---------------------|------------------------------------------|
| 5  | `WASM_SEGMENT_INFO` | name, alignment and flags of each data segment |
| 6  | `WASM_INIT_FUNCS`   | constructors to call at startup          |
| 7  | `WASM_COMDAT_INFO`  | groups of which only one copy is kept    |
| 8  | `WASM_SYMBOL_TABLE` | all symbols                              |

Subsection sizes are usually padded LEBs too.

### 8.1 Symbol table

`vec(symbol)`, each starting with `kind:u8 flags:u32`:

| Kind | Symbol   | Then                                           |
|------|----------|------------------------------------------------|
| 0    | function | index:u32, name if present (see below)         |
| 1    | data     | name; if defined: segment:u32 offset:u32 size:u32 |
| 2    | global   | index:u32, name if present                     |
| 3    | section  | section index:u32 (no name)                    |
| 4    | tag      | index:u32, name if present                     |
| 5    | table    | index:u32, name if present                     |

For function, global, tag and table symbols, **index** is in the object's
own index space (§3.1). A defined symbol points at a defined item; an
undefined one points at an import.

**Name present?** Always for defined symbols. For undefined ones only with
the `EXPLICIT_NAME` flag; otherwise the symbol's name is the import's field
name.

**Data symbols** name a segment by position, an offset inside it and a size.
Their address is the segment's final address plus the offset.

**Section symbols** stand for a whole section (used by debug info).

Flags:

| Bit     | Name                 | Meaning                                    |
|---------|----------------------|--------------------------------------------|
| `0x001` | `BINDING_WEAK`       | may be overridden by a normal definition   |
| `0x002` | `BINDING_LOCAL`      | visible in this file only (`static`)       |
| `0x004` | `VISIBILITY_HIDDEN`  | not exported from a shared library         |
| `0x010` | `UNDEFINED`          | wanted from elsewhere                      |
| `0x020` | `EXPORTED`           | export from the executable                 |
| `0x040` | `EXPLICIT_NAME`      | name written even though undefined         |
| `0x080` | `NO_STRIP`           | keep even if nothing refers to it          |
| `0x100` | `TLS`                | thread-local data                          |
| `0x200` | `ABSOLUTE`           | data symbol whose offset is an address     |

Neither weak nor local means **global binding**: an ordinary definition.
Symbols are numbered by their position; relocations use that number.

The example's symbol table (subsection 8, 9 symbols):

```
#0  00 02    02    05 "twice"         function, local, index 2
#1  00 a4 01 03    05 "start"         function, hidden|exported|no_strip
#2  01 04    07 "counter" 03 00 04    data, segment 3, offset 0, size 4
#3  01 04    02 "fp"      02 00 04    data, segment 2
#4  05 90 01 00                       table, undefined|no_strip, import 0
#5  00 10    00                       function, undefined, import 0 = ext
#6  00 50    01 03 "sys"              function, undefined|explicit name
#7  01 04    03 "msg"     00 00 03    data, segment 0, size 3
#8  01 04    01 "p"       01 00 04    data, segment 1
```

`__indirect_function_table` (#4) and `ext` (#5) take their names from their
imports.

### 8.2 Segment info

`vec(name alignment:u32 flags:u32)`, one per data segment in order.
Alignment is a power of two written as its exponent: `02` means 4 bytes.

| Flag | Name      | Meaning                                         |
|------|-----------|-------------------------------------------------|
| 1    | `STRINGS` | NUL-terminated strings; identical ones may merge|
| 2    | `TLS`     | thread-local                                    |
| 4    | `RETAIN`  | keep even if unused                             |

The **name prefix** tells the linker where the segment goes: `.rodata.*`,
`.data.*`, `.bss.*`, `.tdata.*`.

```
0b ".rodata.msg"   00 00       align 1
07 ".data.p"       02 00       align 4
```

### 8.3 Init functions

`vec(priority:u32 symbol:u32)`. Each is a `void()` function to call before
the program starts: C++ global constructors and
`__attribute__((constructor))`. Lower priority runs first; 65535 is the
default. From a C++ file with a global object:

```
06 85 80 80 80 00  01  ff ff 03  05      priority 65535, symbol #5
```

The linker makes one function, `__wasm_call_ctors`, that calls them all in
order.

### 8.4 Comdats

`vec(comdat)`, each `name flags:u32 vec(kind:u8 index:u32)`. Flags are 0.

| Kind | Member            |
|------|-------------------|
| 0    | data segment      |
| 1    | function          |
| 2    | global            |
| 3    | tag               |
| 4    | table             |
| 5    | custom section    |

C++ inline functions and templates are compiled into every file that uses
them. Each copy is placed in a comdat with the same name; the linker keeps
the **first** group of each name and drops every member of the others.

## 9. Relocation sections

One per section that needs patching, named `reloc.` plus the section name:
`reloc.CODE`, `reloc.DATA`, `reloc..debug_info`.

```
name "reloc.CODE"
section:u32              index of the patched section, counting all
                         sections of the file from 0
vec(entry)               sorted by offset
```

Each entry:

```
type:u8   offset:u32   index:u32   [addend:s32]
```

- **offset**: where to patch, counted from the start of the target section's
  contents. For a custom section, from after its name.
- **index**: a symbol number, except for `TYPE_INDEX_LEB`, where it is a type
  number.
- **addend**: only for the types marked below. Added to the symbol's value:
  `&arr[3]` is `arr` with addend 12.

Relocation types:

| #  | Name                           | Writes                            | Form     | Addend |
|----|--------------------------------|-----------------------------------|----------|--------|
| 0  | `FUNCTION_INDEX_LEB`           | function index                    | 5-byte u | —      |
| 1  | `TABLE_INDEX_SLEB`             | table slot of a function          | 5-byte s | —      |
| 2  | `TABLE_INDEX_I32`              | table slot of a function          | u32le    | —      |
| 3  | `MEMORY_ADDR_LEB`              | data address                      | 5-byte u | yes    |
| 4  | `MEMORY_ADDR_SLEB`             | data address                      | 5-byte s | yes    |
| 5  | `MEMORY_ADDR_I32`              | data address                      | u32le    | yes    |
| 6  | `TYPE_INDEX_LEB`               | type index                        | 5-byte u | —      |
| 7  | `GLOBAL_INDEX_LEB`             | global index                      | 5-byte u | —      |
| 8  | `FUNCTION_OFFSET_I32`          | offset of a body in code section  | u32le    | yes    |
| 9  | `SECTION_OFFSET_I32`           | offset inside a section           | u32le    | yes    |
| 10 | `TAG_INDEX_LEB`                | tag index                         | 5-byte u | —      |
| 11 | `MEMORY_ADDR_REL_SLEB`         | address − `__memory_base` (PIC)   | 5-byte s | yes    |
| 12 | `TABLE_INDEX_REL_SLEB`         | slot − `__table_base` (PIC)       | 5-byte s | —      |
| 13 | `GLOBAL_INDEX_I32`             | global index                      | u32le    | —      |
| 14 | `MEMORY_ADDR_LEB64`            | wasm64 forms of 3, 4, 5           | 10-byte  | yes    |
| 15 | `MEMORY_ADDR_SLEB64`           |                                   | 10-byte  | yes    |
| 16 | `MEMORY_ADDR_I64`              |                                   | u64le    | yes    |
| 17 | `MEMORY_ADDR_REL_SLEB64`       | wasm64 form of 11                 | 10-byte  | yes    |
| 18 | `TABLE_INDEX_SLEB64`           | wasm64 forms of 1, 2              | 10-byte  | —      |
| 19 | `TABLE_INDEX_I64`              |                                   | u64le    | —      |
| 20 | `TABLE_NUMBER_LEB`             | table index                       | 5-byte u | —      |
| 21 | `MEMORY_ADDR_TLS_SLEB`         | address − `__tls_base`            | 5-byte s | yes    |
| 22 | `FUNCTION_OFFSET_I64`          | wasm64 form of 8                  | u64le    | yes    |
| 23 | `MEMORY_ADDR_LOCREL_I32`       | address − address of this field   | u32le    | yes    |
| 24 | `TABLE_INDEX_REL_SLEB64`       | wasm64 form of 12                 | 10-byte  | —      |
| 25 | `MEMORY_ADDR_TLS_SLEB64`       | wasm64 form of 21                 | 10-byte  | yes    |
| 26 | `FUNCTION_INDEX_I32`           | function index                    | u32le    | —      |

"5-byte u/s" means a padded unsigned or signed LEB that fills exactly the
bytes already there.

Where each appears:

- **In code**: `call` → 0; `call_indirect` → 6 and 20; `global.get` → 7;
  `i32.const &var` → 4; `i32.const &func` → 1; a load or store offset → 3.
- **In data**: a pointer to a variable → 5; a pointer to a function → 2.
- **In debug sections**: 8, 9, 5, 26.
- A **pointer to a function is a table slot number**, not a function index.
  The linker gives every function whose address is taken a slot in
  `__indirect_function_table`; `call_indirect` looks it up there.

The example's relocations:

```
reloc.CODE, section 6:
  0f  MEMORY_ADDR_LEB     #2 counter  +0     load offset
  18  MEMORY_ADDR_LEB     #3 fp       +0     load offset
  1e  TYPE_INDEX_LEB      type 0             call_indirect type
  23  TABLE_NUMBER_LEB    #4 table           call_indirect table
  2b  FUNCTION_INDEX_LEB  #5 ext             call
  35  FUNCTION_INDEX_LEB  #6 sys             call

reloc.DATA, section 7:     07 02 | 05 0e 07 01 | 02 17 00
  0e  MEMORY_ADDR_I32     #7 msg      +1     p = msg + 1
  17  TABLE_INDEX_I32     #0 twice           fp = twice
```

(Section 6 is CODE: type, import, function, export, element, datacount,
code — counting from 0.)

## 10. Archives

A library `libx.a` is a Unix `ar` archive of objects:

```
"!<arch>\n"
member*          60-byte header, then data, padded to an even length
```

Header, all ASCII, space-padded:

| Bytes | Field                   |
|-------|-------------------------|
| 16    | name                    |
| 12    | date                    |
| 6     | owner                   |
| 6     | group                   |
| 8     | mode (octal)            |
| 10    | size (decimal)          |
| 2     | `` `\n ``               |

Special members, GNU style (what `llvm-ar` writes here):

- `/` — **symbol index**: a count, that many member offsets (both 32-bit
  **big-endian**), then that many NUL-terminated symbol names. Says which
  member defines which global symbol.
- `//` — long names, each ending `/\n`. A member named `/123` has the name
  at offset 123 here. Short names end with `/`: `io.o/`.
- `/SYM64/` — like `/` with 64-bit offsets.

BSD style (macOS `ar`): a name `#1/20` means the real name is the first 20
bytes of the data. `!<thin>\n` is a thin archive: members are elsewhere on
disk.

**A member is linked only if it is needed**: when an undefined symbol is
defined by some member, that whole member is added, which may need further
members, and so on until nothing changes.

## 11. The executable module

The linker's output has no `linking` and no `reloc.*` sections, and every
number is final. Laid out for Braam (`c4.wasm`):

```
type        function types, each once
import      env.memory                  the process's memory
            kernel.sys_async, kernel.sys
function    types of all kept functions
table       1 funcref table, min = max = slots + 1
global      __stack_pointer: mut i32 = top of stack
export      _alloc _free _sig _start _resume
element     00 41 01 0b vec(funcidx)    slots 1.., slot 0 stays null
code        all kept bodies, numbers patched
data        final segments at final addresses
name        function, global and segment names
producers
target_features
braam       5 × u32le: magic "bram", ABI, flags, initial pages, max pages
```

**Memory map** (`--stack-first`, stack size 128 KiB):

```
0            stack bottom  ─┐
             stack grows ↓  │ 131072 bytes
131072       stack top  ◄───┘  __stack_pointer starts here
             .rodata           constants, string literals
             .data             initialised variables
             .bss              zero-initialised variables
__data_end
__heap_base  (aligned 16)      malloc's memory starts here
             ...heap grows up to the end of memory
```

Symbols the linker defines itself:

| Symbol                      | Value                                 |
|-----------------------------|---------------------------------------|
| `__stack_pointer`           | global, initially the stack top       |
| `__indirect_function_table` | the table                             |
| `__wasm_call_ctors`         | function calling all init functions   |
| `__data_end`                | end of `.bss`                         |
| `__heap_base`               | start of the heap                     |
| `__global_base`             | start of data                         |
| `__dso_handle`              | 0; used by C++ `atexit` machinery     |

What changes when the linker writes the example into an executable:

- `call 0` becomes `call <final index of ext's definition>`;
- `counter`'s load offset `8c 80 80 80 00` becomes its final address, still in
  5 bytes;
- `fp`'s data `01 00 00 00` becomes `twice`'s final table slot;
- `p`'s data becomes `msg`'s final address + 1;
- the segments' `i32.const` addresses become final addresses.

**Imported memory and zeros.** With `--import-memory`, wasm-ld cannot know
the memory is fresh, so it makes the segments passive and adds a start
function `__wasm_init_memory` that copies them in and fills `.bss` with
zeros. A Braam process always gets a new, zeroed memory, so active segments
and no `.bss` bytes give the same result.

## 12. Reading a file yourself

Tools that print everything in this document:

```
llvm-objdump -h file.o          sections
llvm-objdump -t -r file.o       symbols and relocations
llvm-readobj --symbols --relocations --sections file.o
wasm-objdump -x file.o          (wabt) all sections in detail
wasm2wat file.wasm              the code as text
```

And node, for a quick look at imports and exports:

```
node -e 'const m = new WebAssembly.Module(require("fs").readFileSync("f.o"));
         console.log(WebAssembly.Module.imports(m))'
```
