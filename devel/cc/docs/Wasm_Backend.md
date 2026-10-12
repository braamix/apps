# The WebAssembly backend

`genwasm` turns the compiler's intermediate code (TAC) into WebAssembly for the 32-bit
address space, **wasm32**. The code follows **clang's C ABI for
`wasm32-unknown-unknown`**, so it calls, and is called by, code that clang compiles,
with the same wasm features. The output is LLVM's wasm assembly: clang assembles it into
a relocatable object and `wasm-ld` links that into a module. Programs run under node,
with a small host of ours, `libc/wasm32/run.mjs`. This is the first target here for a
stack machine, and the first with no registers and no `goto`.

## The target

- **Machine:** the WebAssembly MVP with five features on clang's default CPU (Braam's
  set, from braam-core's `cmake/wasm32-unknown-unknown.cmake`):
  `-mreference-types -mbulk-memory -msign-ext -mmutable-globals -mnontrapping-fptoint`.
  CMake keeps the list in `WASM32_FEATURES`. The assembler, `vcc` and every clang
  reference build in the tests take it, so all objects carry the same
  `target_features` section and `wasm-ld` links them together. clang's default CPU
  adds `multivalue`, `bulk-memory-opt` and `call-indirect-overlong`, which leave the
  C ABI alone. Of the features, the code uses:
  - `i32.extend8_s`/`extend16_s` (sign-ext), for narrow signed values;
  - `*.trunc_sat_*` (nontrapping-fptoint), for every float-to-integer conversion, as
    clang does: out of range saturates, it never traps;
  - `memory.copy` and `memory.fill` (bulk memory), for aggregate copies, `memcpy`,
    `memmove` and `memset`;
  - `call_indirect __indirect_function_table, (sig)`, the reference-types spelling.

  No `externref` reaches C, and nothing returns more than one value.
- **Data model:** ILP32. `char` 1 byte and **signed**, `short` 2, `int`, `long` and
  pointers 4, `long long` 8. `_Bool` 1 byte. `size_t` is `unsigned long`, `wchar_t` is
  `int`. Natural alignment, at most 16. Little-endian. Bit-fields are laid out as on
  System V, where clang counts each bit-field as a field of its own (see
  [Calls](#function-calls)).
- **Floating point:** `float` is binary32 and `double` binary64, in hardware (`f32`,
  `f64`, `f64.sqrt` included). `long double` is IEEE binary128, 16 bytes aligned to 16,
  in software: each operation is a call of `libc/common/float128.c`, under libgcc's
  names (`__addtf3`, `__lttf2`, …).
- **Value types:** an integer or pointer of 32 bits or less is an `i32`, a `long long`
  an `i64`, `float` an `f32`, `double` an `f64`. A narrow integer is kept **extended** in
  its `i32`, by `extend8_s`/`extend16_s` when signed and an `and` mask when not, so a
  load of one needs no fixing and a truncation does it.
- **Output:** a `.s` file in LLVM's wasm assembly: `.functype` signatures,
  `local.get`/`i32.add`/`end_function` instructions, a `.section .text.NAME` per
  function and a section per variable. It is assembled by
  `clang --target=wasm32 --no-default-config <features> -c` and linked by
  `wasm-ld --stack-first -z stack-size=1048576`. There are no GNU binutils for wasm.
- **Host:** `run.mjs` under node. The module imports two functions, `env.putch(i32)`,
  which writes a byte to stdout, and `env.exit(i32)`, which ends the program. The
  linear memory holds the shadow stack first (1 MiB, growing down from `__stack_pointer`),
  then the data, then the heap from `__heap_base`, which `memory.grow` extends.

**Not supported:**
- `setjmp`/`longjmp`. They need wasm exception handling or stack switching, which this
  feature set lacks. `<setjmp.h>` declares them, so a program that never calls them
  compiles; one that does fails to link.
- A call through a function type other than the callee's own traps, in
  `call_indirect` or in `wasm-ld`'s stub for a mismatched direct call, as with clang.
- Automatic variables aligned to more than 16 get 16.
- `_Complex` and atomics, as on every target here.

## Design choices

- **LLVM's assembly, not a binary module.** clang assembles it into a relocatable object,
  and `wasm-ld` links it. That gives sections, relocations, symbols, static libraries,
  dead-section removal and linking with clang's objects for nothing; writing the
  binary format directly would mean writing a linker too.
- **Braam's feature set,** no more. A module this backend builds must run where Braam
  runs. Every object carries the same `target_features` section, which `wasm-ld`
  compares across the objects it links.
- **Two host imports,** `env.putch` and `env.exit`, and nothing else. WASI would bring a
  file system and a clock this C library does not use. The host is about 50 lines of
  JavaScript, and any engine can supply the two functions.
- **clang's ABI, checked, not read.** Every rule under [Calls](#function-calls) was
  established by compiling probes with `clang -S` and is held in place by the interop
  tests, which link our code with clang's both ways.
- **A correct translation first.** The dispatch skeleton handles any control flow, so
  every test ran before the structured translation existed. It stays as the last
  fallback for an irreducible graph and as a check (`--no-structure`) of the structured
  one.

## How code is generated

For each function, in this order (`codegen.c`):

1. **Where each name lives** (`frame.c`): a wasm local, or a slot in the frame on the
   shadow stack.
2. **Selection and control flow** together (`structure.c` drives `instr.c` and
   `call.c`): each basic block's instructions are selected in the place the structured
   translation puts them.
3. **An `unreachable` at the end** of a function with a result whose control can run off
   it (after an infinite loop, say).
4. **The rewrites of the finished code** (`peephole.c`): stackify, tees, dead values,
   peephole rules, then local coalescing (`locals.c`) and the rules once more.

There is no register allocator: wasm has none to give. The engine's compiler allocates
registers when it compiles the module, so the backend's job is fewer locals, fewer
instructions and code the engine can read as expressions.

To see the code without the rewrites, give `genwasm` `--no-peephole`, `--no-stackify`
and `--no-coalesce` (each turns off its part). `--no-structure` gives every function the
dispatch skeleton below, and `--no-regional` every function with an irreducible graph.

### Locals and the frame

A parameter, automatic variable or temporary is a **wasm local** when it is a scalar
that only its own name reaches. It lives in a **frame slot** when:
- its address is taken, it is an `ALLOCATE_LOCAL` object, or it is `volatile`
  (`Flow.in_memory`);
- its type is an aggregate or `long double`, which have no value type. A structure
  result of a call is a temporary with no `ALLOCATE_LOCAL`, so the type decides.

Parameters are the first locals, in the order of the signature. One that lives in a
slot is stored there by the prologue. One passed by reference is used where it is,
through its pointer.

### Selection

Each TAC instruction becomes stack code: push the operands (`local.get`, a constant, or
a load from a slot or a static object), compute, and pop the result into the destination
(`local.set`, or a store whose address was pushed before the operands). The value type of
an operand always comes from the operator or the destination, never from the kind of a
constant. A shift count of the other width is wrapped or extended. Integer conversions
truncate or extend in place.

The byte and "fat pointer" kinds every target sees (`LOAD_BYTE`, `GET_ADDRESS_DECAY`,
`PTR_DIFF`, …) are their plain forms on a byte-addressed machine. A static object's
address is `i32.const sym`, a slot's `fp + offset`, a function's its index in the table
(the same `i32.const sym`, with a table relocation). An access to a static object puts
the symbol into the access itself: `i32.const 0`, then `i32.load g+4`.

Aggregates and `long double` values move by their bytes, with `memory.copy`. A `long
double` operation is a call of the runtime through the ordinary call path, its operands
two `i64` each and an arithmetic result through memory. Each routine it calls is
declared by a `.functype` just before the function that uses it.

### Structured control flow

TAC has labels, `JUMP` and `JUMP_IF_[NOT_]ZERO`, and one multiway jump: `JUMP_TABLE`,
which only the coroutine split makes, for the dispatch of a coroutine with three
suspension points or more. (`switch` is a chain of comparisons.) Wasm
has no `goto`, only `block`, `loop` and `if`, and branches out of them by depth.
`structure.c` translates one into the other by Norman Ramsey's method ("Beyond
Relooper", ICFP 2022):

1. Number the reachable blocks in reverse postorder and find their dominators
   (Cooper, Harvey and Kennedy, over predecessor lists).
2. A block some later block jumps back to is a **loop header**; one that two or more
   earlier blocks jump to is a **merge node**.
3. Translate the dominator tree from the entry. A node's merge children, in reverse
   postorder, each get a `block` that its code ends inside, so that a jump to one is a
   `br` out of its block, after which its code follows. A loop header is wrapped in a
   `loop`, so that a jump back to it is a `br` to the loop's start.
4. A jump to a merge node or a loop header is a `br` of the depth the context stack
   gives. A jump to any other block is that block's code, in place.
5. A conditional jump is a `br_if` when one of its ways is such a `br`, and the other way
   follows. Otherwise it is an `if`/`else` with a way in each arm. When both ways go to
   one block it is a plain jump. Falling off the last block returns from a `void`
   function, and is `unreachable` in any other.
6. A jump table is a `br_table`. Each of its targets counts as a merge node, so it has
   a block to leave.

#### Irreducible graphs

The method needs a **reducible** graph. A backward jump to a block that does not
dominate its source (a `goto` into a loop, Duff's device, a coroutine resumed inside a
loop it does not suspend at every turn) makes the graph irreducible. Such a graph is
first made reducible, as LLVM's `FixIrreducibleControlFlow` does:

1. Find the strongly connected regions (Tarjan) of the reachable blocks. One with a
   single entry is a loop; its header's incoming edges are set aside and the regions
   inside it are looked at the same way.
2. A region with several entries gets a **dispatch node**, a `local.get state;
   br_table` over its entries. A jump into an entry from outside the region, and a jump
   inside it that a depth-first walk from the dispatch node finds going back, set
   `state` to the entry's index and go to the dispatch node instead. A forward jump
   inside the region stays as it is: the dispatch node dominates its target anyway, so
   Duff's cases still run into one another. The region is now a loop headed by the
   dispatch node, and its inner regions are looked at in turn.
3. In the rare graph where that is not enough (a region inside entered by the dispatch
   node in two ways), every jump to an entry is redirected and the work starts again.

Ramsey's translation then runs over that graph; each entry of a dispatch node counts as
a merge node, so the `br_table` leaves the block in front of it. A jump table whose
entry goes to such a dispatch node must set `state` on the way, which a `br_table`
entry cannot. That entry leaves a block of its own instead, a trampoline, after which
`state` is set and the jump made. One `state` local
serves every dispatch node, since each reads it right after the jump that set it. Code
outside the irreducible regions keeps its structure.

The dispatch nodes came with the coroutines, whose resume dispatch enters a loop in
the middle when the loop does not suspend at every turn. Before them, such a function
got the whole-function skeleton below. Code-section bytes, with every rewrite:

| Program | Skeleton (`--no-regional`) | Dispatch nodes |
|---|---|---|
| a generator whose loop does not always suspend | 243 | 215 |
| Duff's device and a `goto` into a loop | 484 | 470 |
| a region nested in another, every jump redirected | 243 | 224 |
| the 607 book programs (all reducible) | 137 384 | 137 384 |

Should that fail (a region entered at the function's very start), the function gets
the **dispatch skeleton**, which is correct for any graph: a `state` local, and `loop {
block … block; br_table }` with a block per basic block. A jump sets `state` and
branches to the loop; a fall-through runs straight on into the next block. The skeleton
came first, and got every test running before the structured translation existed.

#### An example

For

```c
int sum(int *a, int n) { int s = 0; for (int i = 0; i < n; i++) s += a[i]; return s; }
```

the loop rotated by the translator, and its index strength-reduced to a pointer, become

```
	block
	i32.const	0
	local.set	3
	local.get	0
	local.get	1
	i32.const	2
	i32.shl
	i32.add
	local.set	2
	local.get	1
	i32.const	0
	i32.le_s
	br_if	0          # n <= 0: leave the block, to the return
	loop
	local.get	3
	local.get	0
	i32.load	0
	i32.add
	local.set	3
	local.get	0
	i32.const	4
	i32.add
	local.tee	0
	local.get	2
	i32.lt_s
	br_if	0          # back to the loop's start
	end_loop
	end_block
	local.get	3
	end_function
```

### Coroutines

The backend does not know what a coroutine is. The translator makes `coro(Y) T f(...)`
an ordinary function `f$resume(fp)` over a frame in memory, a function `f$init` that
stores the arguments, and a static array `f$co`, the descriptor
([Coroutines_in_C.md](Coroutines_in_C.md) §10 has the ABI;
[Coroutines_Internals.md](Coroutines_Internals.md) the lowering). What reaches the
backend from it:

- `f$resume` starts with a dispatch on the frame's state: a chain of comparisons for
  one or two suspension points, a `JUMP_TABLE`, so a `br_table`, from three. A
  suspension is a store of the state and a `return`.
- The frame's members are loads and stores at constant offsets from `fp`, which the
  peephole rules fold into the memargs.
- A loop that does not suspend at every turn is entered in the middle by the dispatch,
  and gets a dispatch node of its own (above).
- `co_alloca` in a function uses three builtins that move `__stack_pointer` (the
  stack frame, [below](#alloca)).
- `co_resume` and the rest are calls of `libc/common/co.c`; `__coro_resume` calls the
  frame's `f$resume` through `call_indirect`, so `f$resume` is in the table.

### Stackify and the peephole rules

Selection moves every value through a local. The rewrites (`peephole.c`) work on the
finished code, round after round until nothing changes:

- **Stackify.** A local set once and read once, later in the same straight run of code
  (no structured instruction between), stays on the operand stack. Either:
  - the code between the set and the read leaves the stack as it found it and never
    reaches below it. Then the value waits on the stack beneath that code, and nothing
    moves, so a call or a store between needs no care. `int a = *p; *p = 5; return a +
    *p;` keeps `*p`'s first value on the stack across the store;
  - or the expression that computed it has no effect (no store, call, division or
    `volatile` access), and nothing between writes a local it reads or, if it loads,
    any memory. Then it moves down to the read.

  The instructions of a `volatile` access are marked as barriers. Nothing with a load
  moves across one, and none moves at all.
- **Tees.** A set read at once and again later becomes `local.tee`.
- **Dead values.** A local never read is no longer set: the set becomes a `drop`, and a
  value dropped as soon as made goes with the code that made it, when that has no
  effect. A call's unused result stays a `call; drop`.
- **Tests:**
  - `x == 0` becomes `eqz`;
  - `x != 0`, or `eqz` twice, before a test disappears;
  - an integer comparison followed by `eqz` becomes the opposite comparison (not a
    floating one, whose opposite differs on a NaN);
  - a comparison's 0 or 1 compared against 0 again is itself.
- **Addresses.** A constant added to an address goes into the offset of the load or
  store that uses it. That is how `p->y` becomes `i32.load 4`.
- **Constants:**
  - a multiplication by a power of two becomes a shift;
  - constant stores side by side through one local merge into a store twice as wide, up
    to an `i64.store`. They are a structure's or array's initializer, mostly. The
    merged store claims natural alignment only through the frame pointer, which is
    16-aligned; elsewhere it keeps the alignment of the stores it replaces. In wasm the
    alignment is only a hint either way.
- **Control:**
  - code after a `br`, `return` or `unreachable`, up to the end of its construct, goes;
  - a `br` to where control would run on to anyway goes, through the `end`s that follow
    and from an `if`'s first arm to its end;
  - a `block` or `loop` no branch names is just its code;
  - an empty `else` goes;
  - the `return` at the very end goes, since the end returns what is on the stack.

### Local coalescing

`locals.c` solves liveness over the finished code itself, an instruction at a time: a
`br` goes to the end of the `block` or `if` it names, or to the start of the `loop`.
Two locals interfere when one is written while the other is live, except at a copy
(`local.get y; local.set x`, or a `local.tee y` before the set), which leaves them
equal. At the entry, every parameter and every local read before it is written (a wasm
local starts at zero) is live at once.

The locals of each copy that never interfere then merge into one group, whose conflicts
are those of all its members; a group holds one parameter at most. The groups are
coloured greedily: a parameter's group keeps its number, and another group takes the
first local of its type that no group it interferes with holds, a dead parameter
included. A copy inside a group becomes `local.get x; local.set x`, which the rules
remove. `int t = a; a = b; b = t; return a - b;` compiles to `local.get 1; local.get 0;
i32.sub`. Last, the locals no instruction names are dropped and the rest renumbered.

## Stack frame

The frame is on the shadow stack: linear memory below `wasm-ld`'s global
`__stack_pointer`, which grows down and keeps 16-byte alignment. A function with no
slots has no frame and never touches `__stack_pointer`. One with slots has a local
holding the frame's address:

```
	global.get	__stack_pointer
	i32.const	32              # the frame's size, a multiple of 16
	i32.sub
	local.tee	1               # the frame pointer
	global.set	__stack_pointer
```

Before each return it puts `fp + size` back. Slots are laid out from offset 0 in the
order of the parameters and then the locals, each aligned to its type, at most 16.
Above them, 16-aligned, is the **calls' area**, as big as the largest
call needs: the variable arguments' buffer at its start, then the copies of arguments
passed by reference, then a result slot for a call whose structure or `long double`
result has nowhere of its own to go. Every call in the function shares it.

### alloca

`alloca(n)` (`<alloca.h>`) and the coroutines' `co_alloca` move `__stack_pointer` in the
middle of a function, through three builtins the translator calls and `call.c` expands
in place (no call, no calls' area, no `.functype`): `__builtin_stack_save()` is `global.get __stack_pointer`,
`__builtin_stack_restore(p)` is `global.set __stack_pointer`, and `__builtin_alloca(n)`
lowers it by `n` rounded to 16 and yields it. A function that allocates always has a
frame, 16 bytes when it has no slots, so that its epilogue puts `__stack_pointer` back
from the frame pointer. The calls' area is addressed from the frame pointer too, so the
memory needs no room left below it. On `wasm32-braam`, `main` is a coroutine and may not
call `alloca` (a coroutine's frame outlives the stack); a function it calls may. The
coroutines themselves need nothing of the backend: the
translator makes each an ordinary function `f$resume(fp)` over a frame in memory
([Coroutines_Internals.md](Coroutines_Internals.md) §5).

## Function calls

The signature is clang's (checked against `clang -S`):

- A scalar argument is one parameter of its value type, and a scalar result is the
  function's result. A `char` or `short` travels extended in an `i32`.
- A structure or union holding exactly **one scalar** after flattening travels as that
  scalar, whatever its nesting or one-element arrays (`struct { int a[1]; }`, `struct {
  struct { double d; } s; }`), but only when the scalar's size is the structure's. A
  structure of one bit-field travels as an unsigned integer of the structure's size.
  Unnamed `:0` fields do not count. `tac_wasm32_scalar` in `tac/tac_abi.c` decides,
  and the same rule answers `__builtin_va_class`.
- An **empty structure** is no argument at all, and as a result gives `() -> ()`.
- **Any other aggregate** is passed as the address of a copy the caller makes in its
  calls' area. The callee uses it in place, and may write to it.
- An aggregate result that is not a scalar, and every `long double` result, go through
  a hidden first parameter, the address of the result (sret). The function then returns
  nothing.
- A **`long double`** argument, or a structure of one, is two `i64` parameters, the low
  half first.
- A function pointer is the callee's index in `__indirect_function_table`, and a call
  through one is `call_indirect __indirect_function_table, (params) -> (results)` with
  the signature of the pointer's type. A `_Noreturn` call is followed by `unreachable`.

The unit begins with a `.functype` for every function it defines or names, and with
`.globaltype __stack_pointer, i32` and `.tabletype __indirect_function_table, funcref`:
the assembler rejects a symbol used before its `.functype`.

**`main`** is named as clang names it, or crt0 cannot call it. A `main(void)` is emitted
as `__original_main`, with a `main(i32, i32)` that calls it and an alias `__main_void`.
A `main(int, char **)` is `__main_argc_argv`, and there is no `main` at all. crt0 calls
`__main_void`, which libc defines weak as a call of `__main_argc_argv(0, 0)`.

### Variadic functions

A variadic function's signature is its named parameters and one more `i32`, the
address of a buffer the caller fills with the variable arguments. Each argument takes a
slot of at least 4 bytes, aligned to its type (a `double` 8, a `long double` 16). A
structure that is not a scalar takes a 4-byte slot holding the address of a copy; an
empty one takes none. A call with no variable arguments passes 0.

`va_list` is a `char *` that walks the buffer. `va_start(ap, last)` is
`__va_start(&ap)`, which the backend expands in place: it stores the hidden parameter
into `ap`. No `.functype` is emitted for it. `va_arg` is a macro, which asks
`__builtin_va_class(T)` whether a structure came by reference.

## The assembler's peculiarities

- **No quoted names**, and `inf`, `nan` and `infinity` read as float literals wherever
  an operand may be one. A symbol of those names (`float128.c` has a function `infinity`)
  gets a `.vcc` suffix: `wasm_name` in `emit.c`.
- **Float constants** are spelled `infinity`/`-infinity`, `nan` for the default quiet
  NaN, and `nan:0x…` for another payload, with the sign in front. `inf` is rejected.
- **A duplicate `.functype`** is accepted, so a runtime routine declared again before
  each function that calls it is harmless.

## The runtime library

`libc/wasm32/` holds:
- `crt0.S` with `_start`, `wasm-ld`'s default entry. It runs `__wasm_call_ctors` (which
  `wasm-ld` makes), calls `__main_void` and passes its result to `exit`. Built with
  `PRINT_STATUS` as `crt0-status.o`, for the book run tests, it prints the result as
  `%d\n` first.
- `console.s`: `putbyte` and `putch` call the imported `env.putch`; `flush` does nothing;
  `exit` calls `env.exit`.
- `memory.s`: `memcpy` and `memmove` are `memory.copy` (which handles an overlap), and
  `memset` is `memory.fill`. These replace the C versions of `libc/common`. It also has
  `memory.size` and `memory.grow` for the allocator.
- `sqrt.s`: `sqrt` and `sqrtf`, as `f64.sqrt` and `f32.sqrt`.
- `main.s`: the weak `__main_void`.
- `malloc.c`: a bump allocator from `__heap_base` up, growing the memory by the pages a
  block needs; `free` does nothing.
- `co.c`: the coroutine runtime: `__coro_setup` (a frame on given storage),
  `__coro_resume` (resume, cancel and destroy, through the frame's `f$resume`),
  `__coro_done`, `__coro_value`, `__coro_result`, `__coro_push` and `__coro_pop` (a
  task's arena, for `await` and for `co_alloca` in a coroutine), and the traps, which print
  `coroutine trap: <name>` and exit with 255 ([docs/Coroutines_in_C.md](Coroutines_in_C.md)).
- The C library of `libc/common` (`printf` over `doprnt`, `<string.h>`, `float128.c`, …)
  and `frexp`, `ldexp` and `modf` of `libc/ilp32`, compiled by our own passes. The
  `long long` helpers of `libc/ilp32` are left out: `i64` arithmetic is native.
- `run.mjs`, the host: `node run.mjs prog.wasm`. `env.putch` writes to a buffered stdout.
  `env.exit` throws, and the runner then flushes, prints `[exit N]` on stderr and exits
  with N. A trap prints its message and exits with 255 without that line, so it cannot
  pass for `return 255`.

clang assembles the `.s` files, and `llvm-ar` archives the objects into `libc.a`. The
headers are `libc/wasm32/include` (`float.h` for binary128, `limits.h` for the signed
`char`, `setjmp.h`, `stdarg.h`, `stddef.h`, `stdint.h`), then `libc/ilp32/include`
(`inttypes.h`, `math.h`), then `libc/common/include`. Everything installs to
`share/vcc/wasm32/`, `run.mjs` into its `lib/`.

### A process of Braam

The target `wasm32-braam` uses this backend unchanged, with a runtime of its own in
`libc/wasm32/braam` ([Braam.md](Braam.md) §7): `exports.s`
(`crt0.o`: the five exports `_start`, `_resume`, `_alloc`, `_free`, `_sig`, and the
imports `kernel.sys` and `kernel.sys_async`), `rt.c` (a table of tasks, the root a
coroutine that awaits `main` and flushes, the others from `braam_spawn`, each run by
`_start` or `_resume` when its call is answered; the signal bits `_sig` records), `sys.c` (`braam_sys`,
the descriptor, `stat` and path calls, coroutines), `stdio.c` (streams buffered both
ways, `fflush`, `fgetc`, `fgets`, `fread`, `fopen`, `fclose` and `fseek` coroutines),
`malloc.c` (first fit, since Braam frees every reply block) and
`strerror.c`, with this library's objects but the console, `main.s`, the bump
allocator and `strerror`. Its headers come first: `braam.h`, `unistd.h`, `fcntl.h`,
`errno.h`, `stdio.h`, `stdlib.h`, `signal.h`, `poll.h`, `sys/types.h`, `sys/stat.h`. `run.mjs` there is a fake
kernel for node, which checks the process ABI before it runs a program; the ctest
`braam-system` runs programs on Braam itself when a built braam-core is at hand.
[Braam_Example.md](Braam_Example.md) works a program through, built and run both
ways, and [Braam.md](Braam.md) is the target's reference.

## Costs against clang

`scripts/bench_wasm.sh` compiles C files by our passes, as they are and with the
rewrites off, and by clang `-O2` and `-Os` with the same features, and adds up the bytes
of each object's Code section. With no files it takes the 607 book programs that
`wasm32-tests` leaves in `build/backend/wasm`.

| Code section bytes | ours | ours, no rewrites | clang `-O2` | clang `-Os` |
| --- | --- | --- | --- | --- |
| the C files of `libc/common` (31; clang cannot compile `doprnt.c`, whose `va_arg` uses our builtin) | 25 717 | 36 619 | 22 138 | 17 066 |
| the book programs (607) | 137 384 | 204 280 | 79 760 | 76 977 |

- On the library ours is 1.16 times clang `-O2`; the rewrites took off 30%.
- The book programs overstate the difference: clang folds many of them to a constant
  `return`.
- Most of what is left is in `float128.c`, where clang keeps a `long double`'s two halves
  in `i64` locals while ours stays in memory, and in initializers that TAC stores
  member by member.
- A 16-byte copy by `memory.copy` is shorter than clang's two `i64` loads and stores,
  though an engine may run it slower.

## Running a program by hand

You need a clang with the WebAssembly target, `wasm-ld` and `llvm-ar` (Homebrew's
`llvm` and `lld` have them), and node. `wasm-validate` and `wasm-objdump`, from WABT,
help.

After `make install`, which installs into `~/.local`, the driver does it all:

```sh
vcc -t wasm32 -o hello.wasm hello.c
node ~/.local/share/vcc/wasm32/lib/run.mjs hello.wasm
```

The program's output goes to stdout, `[exit N]` to stderr, and node exits with `main`'s
result. `wasm-validate hello.wasm` checks the module. `wasm-objdump -x hello.o` lists an object's
features under `target_features`, the same eight for ours as for clang's. `wasm-objdump -x -j Import
hello.wasm` shows its two imports, `env.putch` and `env.exit`, and `wasm-objdump -d`
disassembles it.

`vcc -v` prints each step's command. By hand, the same steps are:

```sh
P=~/.local
F="-mreference-types -mbulk-memory -msign-ext -mmutable-globals -mnontrapping-fptoint"
vcpp -t wasm32 -nostdinc -I$P/share/vcc/wasm32/include hello.c hello.i  # preprocess
vparse hello.i hello.ast                                            # parse
vlower -t wasm32 hello.ast hello.tac                                # check and lower
vgenwasm hello.tac hello.s                                          # generate assembly
clang --target=wasm32 --no-default-config $F -c -o hello.o hello.s
L=$P/share/vcc/wasm32/lib
wasm-ld --stack-first -z stack-size=1048576 -o hello.wasm $L/crt0.o hello.o $L/libc.a
```

Without installing, use `build/parse`, `build/lower`, `build/backend/genwasm`, the
headers in `libc/wasm32/include`, `libc/ilp32/include` and `libc/common/include`, the
library in `build/libc/wasm32/`, and `libc/wasm32/run.mjs`.

## Things found on the way

- **A symbol must have its `.functype` before its first use,** or the assembler rejects
  it. Hence every signature at the top of the unit, and the runtime routines a `long
  double` operation calls declared before each function that calls them.
- **`call infinity` does not assemble:** the operand reads as a float. `float128.c` has
  such a function, hence the `.vcc` suffix.
- **clang counts each bit-field as a field of its own** when it decides whether a
  structure holds one scalar, so wasm32 lists a storage unit per bit-field
  (`bitfield_unit_per_field` in the target descriptor), and a structure of one
  bit-field travels by value.
- **clang's freestanding `main(argc, argv)` is plain `main`**; only a hosted one is
  `__main_argc_argv`. The interop test builds clang's with `-fhosted`.
- **A rotated loop at the very end of a function** ends in a conditional jump whose
  fall-through is the end of the function. The structured translation first looked for
  a block there.
- **A function called through another signature traps.** Book programs that declare
  the runtime's `void putch(unsigned)` as `int putch(int)` cannot run here, compiled by
  clang or by us.

## Outside the backend

The target needed little from the shared code:
- **`semantic/target.c`:** a `wasm32` descriptor, appended to the table so that the index
  of the default target stays as it was. It has a signed `char`, aggregates aligned to
  their members (`aggregate_align` 1), `struct_return_max` `SIZE_MAX` (the front end
  never lowers a structure result; the backend classifies it), `hw_sqrt` for `f64.sqrt`,
  a binary128 `long double`, System V bit-fields and `bitfield_unit_per_field`.
- **`tac/tac_abi.c`:** `tac_wasm32_scalar` (the one-scalar rule), `tac_wasm32_empty` and
  `tac_wasm32_class` (the answer of `__builtin_va_class`).
- **`cpp/cpp.c`:** the target's predefined macros: `__wasm__`, `__wasm`, `__wasm32__`,
  `__wasm32`, `__ILP32__`, `_ILP32`, clang's `__wasm_<feature>__` for each feature, and
  `__vcc_coroutines__`, as every target but BESM-6 has. They are checked in `cpp/test/test_predefined_macros.cpp`.
- **`translator/test/wasm32_tests.cpp`:** sizes, layouts and bit-fields against clang's.
- **`scripts/CrossTools.cmake`:** `vcc_find_cross` takes `LD` (a linker other than
  `ld.lld`) and `LLVM_ONLY` (no search for binutils), since there are no binutils for
  wasm.
- **`cc/cc.c`:** a target's own LLVM linker (`llvm_ld`, `llvm_ld_flags`), `wasm-ld`
  recognised as an LLVM tool by its name, and the `wasm32` entry with its features and no
  linker script.

## How it was built

In phases, each ending with the wasm32 tests green and a commit:
1. the target entry, macros, headers, cross tools, driver, host and crt0, and a `genwasm`
   that compiled `return 42` (book chapter 1);
2. integers, locals, calls and static data, all control flow through the dispatch
   skeleton (chapters 2 to 12);
3. the shadow-stack frame, pointers, arrays, strings and the first of the C library,
   with `float` and `double` (chapters 13 to 17);
4. structures and clang's calling convention, variadics, function pointers and
   bit-fields (chapter 18 and the interop tests);
5. `long double` through the binary128 runtime, `printf` and the full C library (the
   whole book suite);
6. the structured translation;
7. stackify, the peephole rules and coalescing, with the default-pipeline goldens and the
   size comparison with clang;
8. this document;
9. later, for the coroutines ([Coroutines_Internals.md](Coroutines_Internals.md)): `co_alloca`'s builtins, a
   dispatch node per irreducible region in place of the whole-function skeleton,
   the target `wasm32-braam`, and `JUMP_TABLE` as a `br_table`.

`git log --grep=wasm` shows each phase.

## Tests

`build/backend/wasm/wasm32-tests` checks the generated assembly and runs programs under
node:
- **Goldens:**
  - the emitter (forms, float specials);
  - `main`'s names, the target features and the declarations;
  - integers, frames, pointers, static data, calls, structure signatures and variadics;
  - the structured translation (if/else, merge nodes, loops, a dispatch per irreducible
    region for a `goto` into a loop, Duff's device and a generator, and the skeleton).

  These run with all rewrites off (`NaiveSelection()`). `peephole_tests.cpp` has the
  goldens of the default pipeline.
- **Runs:**
  - narrow and `long long` arithmetic, control flow of every kind, Duff's device and a
    `goto` into a loop (with and without `--no-regional`), irreducible graphs of random
    `goto`s, checked against the skeleton;
  - recursion and frames, pointers, statics, structures by value and by reference,
    function pointers and variadics;
  - the libc (`printf`, `<string.h>`, the `mem*` functions, `malloc`, math);
  - binary128 against exact results, and the rewrites;
  - `defer` (`defer_tests.cpp`) and coroutines (`coro_tests.cpp`: generators, `await`,
    `co_alloca`, `coro_ptr`, cancel and destroy, each trap);
  - `wasm32-braam` programs on the fake kernel (`braam_tests.cpp`).
- **Against clang:**
  - our code is linked with clang's both ways, over a table of signatures, structures
    of each class, `long double`, variadics and bit-fields;
  - clang's `main(argc, argv)` runs on our crt0;
  - the headers agree with clang's;
  - every book program's output and exit status is compared with clang's `-O0` build.

The fixture (`test/wasm_test.h`) is the shared `QemuTest` with node and `run.mjs` in place
of qemu, and `[exit ` as the report of a clean exit. It gives:
- `CompileToWasm` (the unit's assembly, compiled in the test's process);
- `Code` (its instruction lines alone, for goldens) and `EXPECT_CODE`;
- `CompileAndRunWasm` and `CompileAndRunBook` (with `crt0-status.o`);
- `CompileAndRunWithClang` (half the program by clang `-O1`) and `ClangRunBook` (a book
  program by clang `-O0`).

Book programs that expect a 64-bit `long`, or declare `putch` with another signature,
are skipped with the reason in `test/book_test.h`; `signed_char_tests.cpp` runs versions
of those that expect an unsigned plain `char`. Tests that need clang, `wasm-ld` or node
are skipped when they are missing. The `wasm32-headers` CTests check that every header
preprocesses and parses.
