# How `defer` and coroutines are implemented

[Coroutines_in_C.md](Coroutines_in_C.md) is the tutorial: what the two extensions mean
and how to use them, with the frame ABI in its §10. This document is for whoever works
on the compiler: the design rules, where each part lives, how both features are
lowered, and the alternatives that were weighed and rejected. [Braam.md](Braam.md)
describes the target the coroutines were made for.

## 1. Why, and the rules

A process of Braam is a WebAssembly module that the kernel *steps*. It has no event
loop of its own and nothing in it may block: it asks the kernel for something, returns
out of the step, and is entered again later with the answer (Braam.md §7.1 has the
process ABI). Braam's own programs are C++20 coroutines. A C program compiled by vcc
must likewise be able to suspend at a system call with its state in linear memory,
return out of `_start`/`_resume`, and continue there. Stackless coroutines give that;
`defer` makes early returns and cancellation safe for the resources such a program
holds.

Five rules drive the design:

1. **Stackless.** A coroutine lowers to a frame in linear memory plus a function that
   dispatches on a state number. No separate stack, no stack switching, no Asyncify,
   no JSPI.
2. **No hidden allocation.** The compiler never calls an allocator. The program
   supplies frame storage, or it comes off the shadow stack or the task's arena in
   LIFO order.
3. **No customization points.** No promise type, no awaiter protocol, no library type
   the compiler must know about. The language gives suspension and delegation;
   schedulers, I/O and cancellation policy are ordinary C. `await` is defined entirely
   by `yield` (the equivalence in the tutorial's §5), so the language has no notion of
   an I/O operation or of readiness: a yielded value is whatever the runtime says it
   is.
4. **No undefined behaviour in the new features.** Every misuse is a compile-time
   error or a defined trap (the tutorial's §7), but for storage given to `co_init`
   going away while its frame is suspended, which only the lint (§6) partly catches.
5. **Fixed surface.** Four keywords, two type constructors and ten operations.

Out of scope: coroutines on targets other than wasm32 (the lowering is
target-neutral, but enabled only where a runtime exists), symmetric transfer, awaiting
anything but a coroutine, exceptions.

## 2. Where each part lives

```
scanner     _Defer, _Coro, _Yield, _Await, _Coro_frame, _Coro_ptr, the ten __co_*
parser      the specifier, the statement, the expressions, the types
ast         STMT_DEFER, EXPR_YIELD/AWAIT/CO_OP, FUNC_SPEC_CORO, the frame types
semantic    coroutine-ness on the symbol, Y and T, every compile-time rule, the lint
            (coroutines.c); the jumps past a defer or co_alloca (defer.c)
translator  defer as inline cleanup on every exit edge, shared when large (stmt.c);
            a coroutine body as an ordinary TAC function over an opaque suspension
            call, the co_* operations as calls of the runtime (coro.c)
optimizer   unchanged: the suspension is a call it may not move or drop; it learned
            JUMP_TABLE
coro split  after the optimizer: liveness, frame layout, spills, the dispatch, f$init,
            the descriptor f$co (coro.c)
genwasm     a dispatch node per irreducible region and JUMP_TABLE as br_table
            (structure.c)
every gen   co_alloca's three stack builtins, in place (call.c); otherwise the
            coroutine reaches them as ordinary functions and calls (avr-as wants
            f$resume quoted, emit.c)
runtime     libc/common/co.c in every target's libc.a (libvcc.a on the hosted
            ones); libc/wasm32/braam for Braam
driver      the target wasm32-braam (cc/cc.c)
```

The lowering is in shared code, and every target but BESM-6 has it.
`Target.no_coroutines`, set for BESM-6 alone, gates it: `lower -t besm6` of a program
with `_Coro` says "coroutines are not supported on target besm6". Two more fields say
what the backend can do: `jump_tables` (§5.2), wasm32's alone. Every backend expands
the stack builtins of §5.1 (BESM-6, which has no coroutines, `__builtin_alloca` alone,
in its `intrinsics.c`). `defer` has no target dependency and is not gated. BESM-6 output does not
change, since no BESM-6 program uses it and the shared cleanup (§4) is off there.

## 3. Front end

### 3.1 Scanner and parser

- The keywords are rows of the sorted `keywords[]` table in `scanner/scanner.c`.
  `cpp` does not know keywords; it predefines `__vcc_coroutines__` on every target but
  BESM-6 so headers can test for the feature.
- `_Coro(Y)` is a function specifier with an argument, parsed in
  `parse_declaration_specifiers` beside `_Alignas`. `FunctionSpec` has
  `FUNC_SPEC_CORO` with a `Type *yield_type`.
- `_Defer stmt` is `STMT_DEFER{body}` in `parse_statement`.
- `_Yield [expr]` and `_Await expr` are unary-level expressions, `EXPR_YIELD{expr?}`
  and `EXPR_AWAIT{expr}`. `_Await` takes a cast-expression: `await f(x) + 1` is
  `(await f(x)) + 1`. `_Yield` takes a *relational-expression*, so it binds tighter
  than `==`: `yield i == CO_CANCEL` means `(yield i) == CO_CANCEL`, and `yield a + b`
  yields the sum. It has no operand when the next token cannot begin an expression.
- The ten operations are one `EXPR_CO_OP{op, args}` kind with an enum `CoOp`; the
  parser checks the argument counts. The coroutine an operation names is parsed as an
  expression and judged in semantic.
- `_Coro_frame(Y, T)` is a type specifier that builds a `TYPE_STRUCT` tagged
  `__co_frame` carrying `Y` and `T` in `struct_t.frame_yield`/`frame_result`. It is
  never defined, so it is incomplete. `compatible_type` compares two frame types by
  `Y` and `T`, not by the tag. `_Coro_ptr(Y, T)` is the same with the tag `__co_desc`
  (§6). No new `TypeKind`, so the type switches across the compiler are untouched.
- The binary AST has no version word; `parse` and `lower` are rebuilt together.
- `grammar/c11.y`, `c11.l`, `c11.asdl` and [C_Grammar.md](C_Grammar.md) have the
  extensions in marked sections.

### 3.2 Semantic

In `semantic/coroutines.c`, but for the jumps:

- **Coroutine-ness is on the symbol.** `Symbol.u.func` has `coro` and `yield_type`,
  beside `noret`, and a redeclaration must agree on both. The C function type stays
  `T f(params)`, so argument checking is shared by calls, `co_init`, `co_alloca` and
  the arena `await` (`typecheck_call_args`).
- **Context.** The yield type of the coroutine being checked, set by
  `typecheck_fn_decl` around the body; the depth of deferred statements and of loop
  heads, kept by `statements.c`.
- **Types.** `yield e` converts `e` to `Y` as `return` does to `T`, and has type `int`
  (`co_signal`). `await e` is either a call whose callee is a coroutine (the arena
  form), checked as a call, its value `T`; or a `co_frame(Y', T') *` (the explicit
  form), its value `T'`; either way `Y'` must be `Y`. `co_init` and `co_alloca` give
  `co_frame(Y, T) *`; `co_resume`, `co_cancel`, `co_destroy` and `co_done` `int`;
  `co_value` `Y`; `co_result` `T`; `co_sizeof` and `co_alignof` a `size_t` that is
  not a constant. A coroutine named by an operation carries its function type on the
  `EXPR_VAR`, as a callee does.
- **Jumps.** `semantic/defer.c` counts each `co_alloca` as a `defer` registered after
  the declaration or expression statement holding it, or, in the head of an `if` or
  `switch`, before the body, so a `goto` or `case` past one is the same error. A
  `co_alloca` may not be in a loop's head, which runs more than once per entry of the
  block.
- **`main`** may not be a coroutine, except on `wasm32-braam`, where it must be
  `coro(braam_call *) int main(int, char **)` (`Target.braam`).
- `eval_const` and `const_convert.c` never see the new expressions: none is a constant
  expression.

## 4. Lowering `defer`

In the translator only. Cleanup is emitted inline on every edge that leaves a scope,
with no run-time stack, so there is no library dependency, no allocation, no hidden
state, and the optimizer sees all of it. The rule: every control-flow edge leaving one
or more lexical scopes passes through the cleanup of those scopes, innermost first.

- **The scope stack.** `TacCtx` holds a stack of `TacScope`. Each C11 block pushes
  one: a compound statement, `for`'s own scope, and every substatement of `if`,
  `switch`, `while`, `do` and `for`, braced or not. So `if (x) defer f();` lowers to
  `f()` right there. Each holds the **exit actions** registered so far: a deferred
  `Stmt *` (`EXIT_DEFER`), or a `co_alloca` release (`EXIT_CO_RELEASE`, §5.1).
  `STMT_DEFER` appends to the top scope and emits nothing; a `co_alloca` appends its
  release after emitting the allocation (`tac_scope_add`).
- **Running them.** `run_exits` lowers the actions of every scope from the innermost
  down to a depth, each scope's in reverse order: a deferred statement by `gen_stmt`,
  a release as its few instructions. It runs at the end of a compound statement when
  the end is reachable, before `return` for every scope, before `break`/`continue`
  for the scopes down to the loop's or switch's body, and before `goto` for the scopes
  down to the common ancestor of the `goto` and its label, whose scope path the
  semantic pass records.
- **Re-walking.** A deferred statement is lowered once per exit edge, re-walking the
  same AST subtree, relabelled each time (`label_loops_stmt`). String constants and
  temporaries are minted per emission. A static local inside a deferred statement is
  emitted once, since it is keyed by the declaration, and every copy names it.

### 4.1 Shared cleanup

Lowering per exit duplicates code in a block with many exits. So an exit whose cleanup
is large shares one copy per block, as C++ compilers share landing pads (`leave`,
`emit_chain` and `gen_finish` in `translator/stmt.c`):

- **The test.** A size estimate: three per statement in the deferred statements and
  twelve per `co_alloca` release, at least 12 (`CHAIN_MIN`). A coroutine's destroy
  paths, one per suspension point, share from 6 (`CHAIN_MIN_DESTROY`).
- **The chain.** The block's actions are lowered once more at its end, after a jump,
  with an entry label in front of each. Then come tests of a "where next" variable for
  the exits that end there (a label, a return of the one return variable, a
  coroutine's end), then a jump into the chain of the next block out.
- **The exit** sets the variable and jumps into the chain of the innermost block it
  leaves, at the action it has reached.
- **Why the chain can be static.** An exit from inside a block always goes on into the
  same place outside it: an enclosing block's actions cannot change while the inner
  block is open.
- **The fall-through** keeps its own copy, so the common path pays nothing.
- **Off** on BESM-6, whose code must not change, and under `lower --no-shared-cleanup`.
  A `goto` back over a defer of the common block keeps its own copy, and so does a
  return of an aggregate or a `long double`.
- **Measured.** A test program with large cleanups is 10% smaller (1391 to 1250 bytes
  of code). Programs whose cleanups are one call, the Braam library for one, do not
  change.

Tests: `translator/test/defer_tests.cpp` (YAML goldens: fall-off, return, break,
continue, goto out across two scopes, nested LIFO, a loop body's defer per iteration,
a `defer` in a `for` init scope, the shared chains), `semantic/test/defer_tests.cpp`
(the forbidden jumps), and `backend/wasm/test/defer_tests.cpp` (runs, the order
printed).

## 5. Lowering coroutines

`translator/coro.c` and `libc/common/co.c`.

### 5.1 The result

`coro(Y) T f(A a, B b)` becomes two functions and a descriptor in its unit, global for
a global `f` and local for a `static` one:

```
int    f$resume(char *fp)              the body as a state machine; returns co_status
void   f$init(char *fp, A a, B b)      stores the arguments in the frame
size_t f$co[2] = { size, align }       the frame's size and alignment
```

A coroutine taking `(void)` or `(void *)` has a four-word `f$co`, `{ size, align,
init, resume }` (§6), and for `(void)` a thunk `f$initp`; its words are `size_t`s. The
frame starts with a header that is ABI (the tutorial's §10 has it): two `unsigned`
and four pointers as the target lays them out (`co_header_size`), 24 bytes on ILP32,
40 on LP64, 12 on AVR and MSP430; then the value, the
result, the parameters, and the names that live across a suspension and every object
in memory (address taken, aggregate, `long double`), by decreasing alignment. The
offsets of the value and the result depend only on `Y` and `T` (`coro_layout`), so a
holder of a `co_frame(Y, T) *`, such as a scheduler, the `await` expansion or
`co_value`, reads them with no knowledge of `f`. The alignment is the largest of the
members' (each at the target's alignment of its type, `type_align`), at least the
header's, at most 16 as the shadow stack's is. The compiler plants no
pointer into the frame, so a frame may be moved while suspended if nothing in it had
its address taken, but the language promises nothing, and the arena never moves
anything.

The runtime routines (`libc/common/co.c`, ordinary C compiled by vcc; named
`__coro_*` because `__co_*` are the operations' keywords):

- `__coro_setup(storage, bytes, desc, resume, parent)` checks `storage` against the
  descriptor's alignment and `bytes` against its size (`CO_TRAP_STORAGE`), clears the
  header, sets `resume`, `top = storage + size` and `limit = storage + bytes`, and
  `task` to `storage` for a root (no parent) or to the parent's task for an arena
  `await`'s frame. `co_init(storage, bytes, f, args)` is that with `f$co`, `f$resume`
  and no parent, then `f$init(storage, args)`.
- `__coro_resume(p, signal)` is `co_resume`, `co_cancel` and `co_destroy`, with
  signals 0, 1 and 2. It traps on a finished frame (`CO_TRAP_FINISHED`) and a running
  one (`CO_TRAP_REENTRANT`). A destroy of a frame never started marks it destroyed,
  there being no `defer` to run. Otherwise it sets `flags = RUNNING | signal << 1`,
  calls `p->resume(p)` (a `call_indirect` on wasm32), clears the flags and returns
  the status.
- `__coro_done(p)`: the state is DONE or DESTROYED.
- `__coro_value(p, off)` and `__coro_result(p, off)` check the state
  (`CO_TRAP_NO_VALUE` unless suspended, `CO_TRAP_NOT_DONE` unless done) and return
  `p + off`, which the caller reads as `Y` or `T`.
- `__coro_push(fp, bytes, align, name)` takes `bytes` aligned to `align` off the arena
  of `fp`'s task, or traps with `CO_TRAP_NO_SPACE: name`, `name` being the coroutine
  started there (a string constant at each site). `__coro_pop(fp, p)` sets the task's
  `top` back to `p`.

**An arena `await` of a named coroutine** does not go through `__coro_resume`: it sets
the flags itself and calls `g$resume` directly. Its frame is private to the await,
which never resumes it once it is done or while it runs, and destroys it only after it
has suspended, so the checks cannot fail. `co_resume(p)` keeps the checks, which are
what catch a misused frame.

**`co_alloca(f, extra, args)`** gets its memory one of two ways, then does what
`co_init` does on it with `bytes = n = (size + extra + 15) & -16`:

- **In an ordinary function**: `%sp = __builtin_stack_save()`, `%p =
  __builtin_alloca(n)`. The release, run at the end of the block (§4), is `if (%p) {
  if (!__coro_done(%p)) __coro_resume(%p, 2); __builtin_stack_restore(%sp); %p = 0;
  }`. Every backend expands the three builtins inline (§5.3,
  [X86_64_Backend.md](X86_64_Backend.md#alloca),
  [Aarch64_Backend.md](Aarch64_Backend.md#alloca),
  [Riscv_Backend.md](Riscv_Backend.md#alloca),
  [Arm32_Backend.md](Arm32_Backend.md#alloca), [Avr_Backend.md](Avr_Backend.md#alloca),
  [Msp430_Backend.md](Msp430_Backend.md#alloca),
  [Mmix_Backend.md](Mmix_Backend.md#alloca), [Wasm_Backend.md](Wasm_Backend.md#alloca)).
  Until they did, the other targets took the memory from a static arena of the
  runtime, `libc/common/costack.c` (64 KiB), taken and given back in LIFO order: a
  fixed size, and a `longjmp` past one left it taken. It went once every backend had
  the builtins.
- **In a coroutine**: `%p = __coro_push(fp, n, 16, "f")`, from the arena of the task
  the coroutine belongs to; the release is the same with `__coro_pop(fp, %p)` in place
  of the restore. A coroutine's shadow stack is unwound at every suspension, and the
  arena is not. The coroutine's own `co_destroy` reaches the release, since it is one
  of the exit actions active at the suspension point.

Either way `__coro_setup` makes `%p` the root of its own task, the `extra` bytes its
arena. A `co_alloca` in an operand of `&&`, `||` or `?:` may be skipped while its
block runs on, so `%p` is set to null at the block's entry (`tac_scope_entry`) and the
release tests it; the optimizer drops the test where the allocation always runs. One
inside a `sizeof` is never evaluated and registers nothing.

### 5.2 The two-stage translation

The optimizer must see a coroutine body as ordinary code, and the frame must be laid
out from *optimized* code, or every temporary the source mentions ends up in it. So
the translator makes a **provisional** function, the optimizer runs, and a **split
pass** finishes the job: LLVM's CoroSplit, scaled to this compiler.

**Stage 1, in the translator** (`translate_fn` with a coroutine symbol):

- The TAC function is `f$resume` with the parameters `(%.fp, %a, %b)`: the frame
  pointer, then the user's parameters *as ordinary TAC parameters*, which the split
  pass moves. Its return type is `int`, and a structure result never goes through
  sret.
- `yield e` is a store of `e` at `fp + value_off` (an aggregate by
  `gen_aggregate_copy`), then `%s = __coro_suspend(%fp)`, then the destroy branch: if
  `%s == 2`, the exit actions of every open block, the state DESTROYED and `RETURN 1`.
  The expression's value is `%s`, 0 or 1. `__coro_suspend` is declared by an EXTERN,
  so the verifier is content, and the optimizer treats it as an opaque call: never
  dead, killing every memory fact, staying where it is. Nothing in `optimize/` knows
  about it. The EXTERN goes once the split has replaced every call.
- `return e` stores `e` at `result_off` first, then runs the exit actions, then sets
  the state DONE and returns 1 (`CO_DONE`). The end of a `void` body is the same
  without the store.
- `await` (`gen_await`) is the loop of the tutorial's §5, in TAC. The arena form:
  `%sub = __coro_push(fp, g$co[0], g$co[1], "g")`, `__coro_setup(%sub, g$co[0], g$co,
  g$resume, fp)` (the sub-frame joins the awaiter's task), `g$init(%sub, args)`. The
  explicit form copies the operand into `%sub`, evaluated once. Then: `%sig = 0; L:
  %st = resume(%sub, %sig); if %st goto done; copy sub->value to fp->value; %s =
  __coro_suspend(fp); if (%s == 2) { destroy %sub; pop; <every block's exit actions>;
  state = DESTROYED; RETURN 1 } %sig = %s; goto L; done: the result from sub->result;
  pop`. The pops are in the arena form only: an explicit frame is the program's, and
  destroying the awaiter leaves it suspended, as the equivalence says. Forwarding a
  cancel is `%sig` itself. `%sub` and `%sig` live across the suspension, so the split
  puts them in the frame.
- The `co_*` operations are calls of the runtime (§5.1). `co_sizeof(g)` loads
  `g$co[0]` and `co_alignof(g)` `g$co[1]`. A unit names `g$co`, `g$init` and
  `g$resume` through EXTERNs with TAC types (`tac_record_extern_tac`), since they have
  no symbol; a name the unit defines itself goes with the unit's externs
  (`note_own_type`).

**Stage 2, the split pass** (`coro_split`, called from `translate()` after
`optimize_function`, then `optimize_function` once more and the verifier):

1. **Liveness** over the TAC body (`optimize/liveness.c`, shared with
   `dead_store.c`). The names that go into the frame: those live just after a
   `__coro_suspend` but for its own result; every `ALLOCATE_LOCAL` name and every
   `GET_ADDRESS` source (an address may be held across a suspension through memory
   the analysis cannot see, and the shadow stack, where the backend would put them, is
   unwound at every return); and the user's parameters, the only channel from
   `f$init` to the body.
2. **Layout:** after the value and the result, the parameters in order, then the rest
   by decreasing alignment. The size, rounded up to the alignment, and the alignment
   go into `f$co`.
3. **Rewrite.** Every use of a moved scalar becomes a `LOAD` from `fp + off` into a
   fresh temporary (`%co.N`) just before the instruction, every definition a `STORE`
   just after. An `ALLOCATE_LOCAL` goes, `GET_ADDRESS` becomes `ADD_PTR fp, off`, and
   `COPY_TO_OFFSET`/`COPY_FROM_OFFSET` a `STORE`/`LOAD` at the member's address. An
   object in memory used whole (a structure argument, a call's structure result) goes
   through a shadow copy in the shadow-stack frame, copied in before the use and out
   after the definition. The second optimizer round forwards the stores to the loads
   inside a block and prunes the dead names.
4. **Suspension points.** The *k*-th `__coro_suspend` becomes `STORE k → state; RETURN
   0; LABEL %co.resumek.f; %s = LOAD flags >> 1` (the signal); the label carries the
   coroutine's name, since the native assemblers have one namespace for a unit's
   labels. The dispatch goes in front of the body: for one or two points a compare
   chain, `if (state == k) goto %co.resumek.f`; from three (`coro_table_min`), on a
   target with `jump_tables`, a `JUMP_TABLE` on the state, to `%co.start.f` for 0 and as
   the default, `%co.resumek.f` for *k*; elsewhere the chain at any length.
   `JUMP_TABLE(index, targets, default)` is made only here. The optimizer and the shared
   CFG (`backend/common/flow.c`, whose blocks hold any number of successors) take it, a
   constant index folds to a `JUMP`, the wasm backend makes it a `br_table` (§5.3), and
   the other backends never see one (BESM-6 and MSP430 reject it). On the Braam programs it saves 0.4% of the code,
   1% on the files with the most suspension points. State 0 falls into the body's
   first instruction.
5. **Parameters** are dropped from `f$resume`'s list. `f$init(%.fp, %a, %b)` is made
   as a function storing each at its offset (an aggregate in chunks), and `f$co` as a
   static variable; both follow `f$resume` in the chain `translate()` returns. Nothing
   new reaches the backend but `JUMP_TABLE`.
6. **The verifier** sees ordinary TAC: `fp` is a `char *`, and the frame accesses are
   `ADD_PTR` with `LOAD`/`STORE`.

Why the body stays correct across the two optimizer rounds: before the split, no name
crosses a suspension in the optimizer's eyes, since the suspension is a call and a
call returns to the same point; after it, the only names that cross a `RETURN` are in
memory. `dead_store` would otherwise delete the last store to a local before a
`RETURN` (exit blocks are seeded only with observable and address-taken names); step 3
is what prevents it.

A loop whose every iteration suspends needs no back edge in `f$resume`: each pass
returns at the `yield` and comes back through the dispatch, so a generator's state
machine is acyclic and the wasm backend structures it as is.

### 5.3 What the wasm backend sees

- `f$resume` is a function with one `i32` parameter. Where a loop runs some iterations
  without suspending, the dispatch enters it in the middle, so the graph is
  irreducible there and that loop gets a dispatch node of its own; the rest stays
  structured ([Wasm_Backend.md](Wasm_Backend.md), "Irreducible graphs").
- Frame accesses are `i32.load off`/`i32.store off` on the parameter; the constant
  addends fold into memarg offsets where the address is not shared.
- A `JUMP_TABLE` becomes a `br_table`. Each of its targets counts as a merge node, so
  each has a block to leave. A target that the regional dispatch reroutes through a
  dispatch node must set the state on the way, which a `br_table` entry cannot; that
  entry leaves a block of its own, a trampoline, after which the state is set and the
  jump made. Under the skeleton (`--no-structure`) the table is a chain of compares.
- `__coro_resume` calls `f$resume` through `call_indirect`; `f$resume` gets a table
  slot because `co_init` and `co_alloca` pass its address.
- `co_alloca`'s builtins are expanded in `call.c` as `__va_start` is:
  `__builtin_stack_save()` is `global.get __stack_pointer`,
  `__builtin_stack_restore(p)` is `global.set __stack_pointer`, and
  `__builtin_alloca(n)` subtracts `n` from `__stack_pointer`, masks with `-16`, sets it
  and yields it. They take no calls' area and no `.functype`. A function that
  allocates always has a frame, 16 bytes with no slots, so that its epilogue resets
  `__stack_pointer` from the frame pointer.

Nothing else: the backend does not know what a coroutine is.

### 5.4 The runtime

`co.c` is compiled into every target's `libc.a` but BESM-6's (and `wasm32-braam`'s),
and into `libvcc.a` on the hosted targets, macOS included. A program that uses no coroutine carries none of it. The header is private
to `co.c` and the translator (`coro.c`'s offsets, from the target's `int` and pointer
sizes, matching `co.c`'s `struct co_header` as the target lays it out); `<coro.h>` has
the user-facing macros and the two enums only. A trap prints `coroutine trap: <name>`
through `putchar` and exits with status 255; on Braam `exit` reports the status and
traps, so the kernel sees a crash.

### 5.5 Tests

- `semantic/test/coro_tests.cpp`: each compile-time rule, `coro_ptr`, the lint.
- `translator/test/coro_tests.cpp`: the provisional function, the split, the
  dispatch, `co_alloca`, the descriptor.
- `optimize/test/jump_unreachable_tests.cpp`: `JUMP_TABLE` folding and its labels.
- `backend/common/test/coro/coro_run_tests.cpp`: runs on every target but BESM-6
  (a `CoroTest` fixture per backend, `coro_test.h`): generators, frames by `co_init`
  and `co_alloca`, the releases, two units, each trap, `await` in both forms against a
  scheduler, recursion, cancel and destroy through a chain, `co_alloca` in a coroutine
  cascading, an arena overflow, `coro_ptr`.
- `backend/wasm/test/coro_tests.cpp`: wide dispatches, as `br_table`, under node.
- `backend/wasm/test/flow_tests.cpp`: the dispatch nodes, random irreducible graphs
  run against the skeleton.

## 6. `coro_ptr` and the lint

**`coro_ptr(Y, T)`** points to a coroutine that takes `(void)` or `(void *)`, so a
scheduler can hold a table of coroutines of different code.

- **The type** is `_Coro_ptr(Y, T)`: a pointer to a struct tagged `__co_desc`. Like a
  frame type's struct it is never defined, and two are the same when their `Y` and
  `T` are (`is_coro_struct`, `coro_desc_target`).
- **Conversion.** The name of such a coroutine, used as a value, converts to its
  `coro_ptr`, in a static initializer too (`initializers.c` names `f$co`). The value is
  the address of `f$co`, which then holds four words: size, alignment, init and
  resume. For `(void)` the init is the thunk `f$initp`, which takes the `void *` and
  ignores it, so every descriptor's init has the type `void (char *, void *)`. The
  parser's `(void)` parameter list is stripped to an empty one, which counts as
  pointable.
- **The operations.** `co_init`, `co_alloca`, `co_sizeof`, `co_alignof` and an arena
  `await p(arg)` take a `coro_ptr` where they take a name, with zero or one argument,
  converted to `void *` (`typecheck_coro_ptr_call`). They read the descriptor's words
  (`desc_word`, `setup_frame_by_ptr`); an `await` through a pointer goes through
  `__coro_resume`, its callee unknown.
- **Why only one `void *`.** A thunk for any parameter list would need the arguments
  packed by the caller in the coroutine's own layout. One `void *` lets a program pass
  whatever it likes, and a coroutine with other parameters gets an error saying it has
  no `coro_ptr`.

**The lint** (the end of `semantic/coroutines.c`) flags the two plain cases of storage
given to `co_init` that the language cannot check. Each is a `warning: <function>: …`
on stderr, and compilation goes on. Storage counts as automatic when it is a local
array, decayed, or `&x` of a local; a local pointer's value is memory from elsewhere,
so it does not count.

- **The frame outlives its storage:** the frame is stored where it outlives the
  storage (a global, a static, a variable of an outer block, through a pointer or into
  a member), or returned (`coro_lint_bind`).
- **The frame may be left suspended:** it was resumed by a statement that drops the
  status (`co_resume(p);`), and nothing in the block destroys it, asks `co_done`, reads
  `co_result` or awaits it (`coro_lint_statement`, `lint_settled`).

Its state is per block level and freed at scope exit and at the end of a function.

## 7. Alternatives considered

| Alternative | Chosen | Reason |
|---|---|---|
| `coro_frame(f)`, a complete object type per coroutine, and `co_sizeof(f)` an integer constant expression | `co_frame(Y, T)`, incomplete; `co_sizeof` a load from the descriptor `f$co` the defining unit emits | The library's coroutines are awaited from units that never see their bodies, and the frame is laid out after the optimizer, which is what keeps it small. A fixed-size object type would fix the layout at the declaration. An absolute symbol would cost no load, but clang's wasm assembler cannot write one (§8). |
| A restricted VLA, `char buf[co_sizeof(f)]`, to put a frame in a local of its own size | `co_alloca(f, extra, args)` | A link-time constant is not an array size, so this would need run-time-sized arrays in the type system and in `sizeof`. `co_alloca` gives the same memory with none of that, and adds the cleanup a plain array cannot have. |
| `co_alloca` scoped to the function, as C's `alloca`, and banned in coroutines | Scoped to the block, usable everywhere: in a function the shadow stack (wasm32) or the runtime's arena, in a coroutine the task's arena | A coroutine that starts coroutines needs it; a function-scoped allocation would grow without bound in a loop; and the end of a block is where an unfinished sub-coroutine can be destroyed, which closes the "storage goes away while suspended" hole for these frames. |
| Frames never nested: every `await` on a frame the awaiter declared as a local | Also `await f(args)`: the callee's frame from the task's arena, LIFO | Without compile-time sizes a sub-frame cannot be a local. An await chain is a stack, so a bump pointer per task costs nothing and permits recursion and separate compilation. The storage is still the program's: the root's block. |
| Cancellation only through `co_cancel`, cleanup entirely the coroutine's | `co_destroy` as well | It runs the `defer`s active at the suspension point and nothing else, so a scheduler can drop a suspended task without its cooperation, and a resource registered with `defer` is released exactly once however the scope ends. |
| Cancellation by unwinding | Cooperative: `yield` returns `CO_CANCEL`, and the coroutine returns | C has no exceptions, and an unwind path for cancellation would be a second control-flow mechanism. |
| A `coro_ptr` for any coroutine | Only for `(void)` and `(void *)` | A thunk for any parameter list would need the arguments packed by the caller in the coroutine's own layout; one `void *` carries whatever the program likes. |
| A direct `co_resume` when the frame comes from a visible `co_init` | Only an arena `await` calls `g$resume` directly | An await knows its callee and owns its frame, so its checks cannot fail. A `co_resume` would need the frame traced through variables, and its checks are what catch a misused frame. |
| `yield` at the precedence of a unary operator, with parentheses required around `yield e == …` | `_Yield` takes a relational-expression and binds above `==` | So `if (yield i == CO_CANCEL)` means what it looks like. |
| The whole frame layout part of the ABI | The header is ABI; the rest is the defining unit's | Only the header crosses units. The rest may change with the optimizer. |
| A trap for resuming an uninitialised frame | None | A frame comes only from `co_init` or `co_alloca`; storage that was never initialised is garbage the language cannot recognise. The reentrancy and finished checks stay. |
| Eager start (the body runs to its first suspension inside `co_init`) | Lazy: `co_init` and `co_alloca` run nothing | Otherwise the point at which side effects happen depends on the call site rather than the source. |
| A `defer` whose run-time "active" flag a jump may skip, so `goto` and `case` into its scope are allowed | Such jumps are compile-time errors | The active set stays a static fact, cleanup is inline code, and `co_destroy` knows from the state number alone what to run. The flag remains possible later without breaking programs. |
| `defer` running at a suspension (releasing a lock across `await`) | Suspension is not scope exit | A suspended scope is alive; a resource held across `await` stays held, and the programmer who does not want that closes the scope before the `await`, which keeps lifetimes visible in the source. |
| Each exit lowers its own copy of the cleanup | Large cleanups shared through one copy per block, the fall-through keeping its own | Duplication grows with exits times cleanup size; the shared chain costs a store and a jump on the exits that use it and nothing on the common path. |
| Symmetric transfer, awaiting arbitrary "awaitables", task groups, allocator hooks | None | Library concerns, or specification weight for little gain; a `select` is a scheduler that resumes several frames. |

## 8. Open questions

- **Absolute symbols** (settled). The first design made `co_sizeof`/`co_alignof` the
  absolute symbols `f$size`/`f$align`, from `.set`, costing no load. clang's wasm
  assembler stops on one ("absolute addressing not supported"), so the descriptor
  `f$co` took their place, at one load each.
- **Code size of the dispatch.** A compare chain or a `br_table` per resume, and a
  `LOAD`/`STORE` per frame access. The second optimizer round and the memarg folding
  keep it moderate.
- **The `defer`-past-`case` rule** may surprise. The alternative, a run-time active
  flag per `defer` that a jump may skip, can come later without changing programs that
  compile today.
- **A `defer` that suspends.** A deferred statement may not suspend, so `defer await
  close(fd)` and `defer await fflush(f)` are errors, and on Braam a descriptor and a
  buffered stream are the resources a program most wants released. Braam's C++ has
  the same limit: a destructor cannot `co_await`. Lifting the rule only turns an error
  into accepted code, so it waits for evidence from real programs. What it would take:
  on ordinary exits nothing new, since the cleanup is code in the coroutine body and
  the split handles its suspension like any other; on `co_destroy`, destruction
  becomes an unwind that can wait, so `co_destroy` may return `CO_SUSPENDED` and
  whoever runs the coroutine resumes it until `CO_DONE`; the end-of-block destroy of a
  `co_alloca` frame forwards the suspensions in a coroutine, as `await` does, and must
  trap in an ordinary function, which has nobody to forward them to.
- **`exit()` as a trap** is Braam's own limitation. If it matters, the runtime can make
  `exit` set a sticky status and have every library coroutine return an error
  afterwards, so the program unwinds by its own returns, the pattern Braam's `vi` port
  uses.
- **`main(void)` on Braam.** `main` must be `coro(braam_call *) int main(int, char
  **)`; a wrapper for the `(void)` form, or for a plain `main` that never waits, is
  small if wanted.
- **Deep chains.** An `await` depth of *n* costs *n* resume calls per suspension, as
  in Braam's C++. Braam has no tail calls, so nothing here relies on them.
- **The second optimizer round** on code with many frame loads: CSE kills every load
  at a `STORE` through a pointer, so two frame members in one block cost a reload
  each. A "same base, different constant offset, cannot alias" rule in `alias.c` would
  help.
- **Liveness granularity.** The spill set is per name over the whole body, not per
  suspension point: a local live across one `yield` is in the frame everywhere. Locals
  that live across no suspension stay out of it.
