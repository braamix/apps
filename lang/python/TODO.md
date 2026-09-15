# Python for Braam — a development plan

Python 3 written for Braam: our own bytecode VM, our own compiler, our own
object model. This is **not** a port. Nothing is taken from CPython or from
MicroPython except one thing — MicroPython's test suite, which is the best
executable specification of the language that exists at this size, and which is
MIT.

The upstream clone lives at [tmp/micropython/](tmp/micropython/) (HEAD
`52b5fbc`, September 2026) and is ignored by git. Its
[tests/](tmp/micropython/tests/) directory holds 1,651 `.py` files across 31
categories; [tests/basics/](tmp/micropython/tests/basics/) alone is 578 tests of
the language core, and that is the target. Copies of the ones we pass are
committed under [test/cases/](test/), because `tmp/` is not.

This plan is detailed through phase 9 — a working core interpreter with
functions, classes, the built-in containers and exceptions. Arbitrary-precision
integers, generators, `async`, the REPL, `import` and the standard library are
named at the end but not planned.

## Ground rules

Every phase depends on these. They are not style; each one is forced by the
platform, and getting one wrong is a rewrite.

1. **The VM is a driver, not a coroutine.** This is `emulators/simbesm`'s
   shape. Only `braam.cpp` contains `co_await`. `vm_burst()` runs plain C++
   until it has something for its caller to do — a read, a write, an open, a
   sleep, an exit — and returns a `Req` describing it; the driver performs it
   and hands the answer back. A `co_await` is a call and not a tail call, so an
   interpreter loop that awaited would grow the native stack until the process
   trapped.
2. **The VM never recurses for a Python call.** A call pushes a frame and the
   dispatch loop continues. A C++ builtin that must call back into Python —
   `sorted(key=)`, `__init__`, `__str__` — pushes a frame and returns to the
   loop with its continuation recorded on that frame. It does not re-enter the
   loop.
3. **There is no `longjmp` and nothing like it.** An error is a sticky
   `vm.pending` exception plus a sentinel return, unwound a frame at a time.
   `editors/vi` and `lang/mbasic` arrived at the same answer for the same
   reason.
4. **The parser is the only recursive thing.** It runs on the 128 KiB native
   stack and carries a nesting bound, the way
   `../braam-core/src/cmd/sh/parse.h` does with `MAX_NEST`.
5. **Braam idiom, not a libc port.** No `PORT`: `kernel/vec.h`, `string.h`,
   `str.h`, `hash.h`, `result.h`, `text.h`, `alloc.h`, and `LIBS braam::math`
   for `math/math.h` and `math/ftoa.h`. Never `new` — `heap_new` and
   `heap_delete`. Namespace-scope globals stay trivially destructible.
6. **Nothing lives in a coroutine frame.** Only `braam.cpp` has frames at all,
   and they hold a pointer to the interpreter state and nothing else.

The limits to build against, with their sources: `PROC_MAX_PAGES = 1600`, so
100 MB of linear memory (`../braam-core/src/kernel/sysabi.h`); a 128 KiB shadow
stack; `PROC_TASKS = 8`. The repository's [CLAUDE.md](../../CLAUDE.md) still
says 16 MB — check that and correct it when the first package is built.

## The design

**`Value` is a 32-bit tagged word.** Bit 0 set means a 31-bit small integer;
otherwise it is a pointer to an `Obj`, which is at least 4-byte aligned
(`heap_alloc`'s smallest size class is 16). `None`, `True` and `False` are the
addresses of static PODs.

**`Obj` is `{ const Type *type; Obj *next; Obj *grey; u32 flags; }`** —
sixteen bytes, the smallest size class. `next` threads every live object onto
one list, because `kernel/alloc.h` has no heap iterator and the sweep needs
something to walk; `grey` threads the marker's worklist, so marking neither
allocates nor recurses.

**Collection is precise mark-and-sweep.** Conservative scanning is not
available here: there is no `__builtin_frame_address`, no exported stack base,
and wasm keeps pointers in locals that a scan of linear memory cannot see. So
the roots are exhaustive and explicit — the frame stack, each frame's locals
and value stack, the module globals, the intern table, the static type objects,
and an RAII `Root` chain for C++ code holding a value across an allocation.
Mark from those, sweep the `next` list, collect on bytes-allocated pressure.
Every builtin that allocates twice pins its operands first; that discipline is
the price of collecting cycles, and Python makes cycles constantly.

**`Type` is a static struct of slots** — `repr`, `str`, `hash`, `eq`, `call`,
`getattr`, `iter`, `next`, and the number, sequence and mapping protocols — so
that a built-in type and a Python `class` answer the same dispatch.

**The AST is an index arena.** Links are `u32` indices, not pointers, because
the arrays reallocate as the parse grows. `sh/parse.h` states the rule.

## Phases

Test names below are real files under
[tmp/micropython/tests/basics/](tmp/micropython/tests/basics/).

### Phase 0 — the shell of a program — **done**

- [x] [CMakeLists.txt](CMakeLists.txt), copied from
      [../mbasic/CMakeLists.txt](../mbasic/CMakeLists.txt) including the
      standalone guard; `python` added to
      [../CMakeLists.txt](../CMakeLists.txt).
- [x] `braam_add_package(NAME python VERSION 0.1-r0 …
      FILES $<TARGET_FILE:bin_python>=bin/python)`, and
      `add_dependencies(packages pkg_python)`.
- [x] [LICENSE](LICENSE) — MIT, Damien P. George, covering the borrowed tests.
- [x] [README.md](README.md) and this file.
- [x] [braam.cpp](braam.cpp): the command line, the banner, the exit status.
      `-V`, `--version`, `-h`, `-c` and a file argument are parsed; the last
      two say there is no interpreter yet and exit 1.
- [x] [test/pylib.mjs](test/pylib.mjs) on the
      [../mbasic/test/mblib.mjs](../mbasic/test/mblib.mjs) pattern: boot, plant
      the `.wasm` at `/bin/py`, run `py … >/tmp/o 2>/tmp/e`, then read those
      back out of the store. It refuses a command line over sixty characters —
      the harness keyboard is a `Channel<Key, 64>`.
- [x] [test/pysmoke.mjs](test/pysmoke.mjs): the banner three ways in, the usage
      block, an unknown option, a valued option with nothing after it, and the
      status each one leaves.
- [x] [test/runcases.mjs](test/runcases.mjs): read
      [test/manifest.txt](test/manifest.txt), run every case in one boot,
      compare against the `.exp` beside it, and fail both on an unexpected
      failure and on a known failure that starts passing — the rule
      [ehcases.mjs](../../editors/eh/test/ehcases.mjs) follows.
- [x] Two `TESTS` lines in the top [Makefile](../../Makefile).
- [x] [tools/mkexp.py](tools/mkexp.py): copy one named upstream test into
      `test/cases/` byte for byte, write its `.exp` from upstream's own when
      there is one and from host CPython otherwise, and refuse when the host's
      version cannot produce it — the `*_cp310`, `*_py312` and `python34.py`
      family. Provenance, the upstream path and commit, goes in the manifest
      and never into the copied `.py`.
- [x] The first case, `basics/andor.py`, marked `fail`: the pipeline is proved
      end to end rather than only wired.

### Phase 1 — values, the object heap, the collector — **done**

- [x] [value.h](value.h) — the tagged `Value`: bit 0 set is a 31-bit signed
      int, otherwise a pointer; zero is Nil, which is not `None`.
- [x] [obj.h](obj.h), [obj.cpp](obj.cpp) — the 16-byte `Obj` header, the
      static `Type` descriptor with its `trace` and `fini` slots, the
      `None`/`True`/`False` singletons, and `StrObj`, `TupleObj` and `ListObj`
      — enough to have something to trace, and something that can cycle.
- [x] [gc.h](gc.h), [gc.cpp](gc.cpp) — `obj_alloc` over `heap_alloc`, the
      `next` list every object is threaded onto, mark and sweep, the `Roots`
      and `Root` pins, the allocation-pressure trigger, and `gc_stress` for
      collecting at every allocation.
- [x] [intern.cpp](intern.cpp) — the intern table, traced rather than swept,
      so an interned string lives as long as the process.
- [x] `--selftest` ([selftest.cpp](selftest.cpp)) and
      [test/pygc.mjs](test/pygc.mjs): eleven checks — values, strings,
      interning, collection, pins, tracing through tuples and lists, a cycle,
      a ten-thousand-deep chain, the stress mode, and the automatic trigger.

Two decisions worth recording, both forced:

- **The marker threads its worklist through the objects themselves**, in a
  `grey` field in the header. Marking therefore neither allocates nor recurses,
  which a ten-thousand-deep structure needs on a 128 KiB stack.
- **The `Type` descriptor is not an object.** Types become Python-visible in
  phase 9; until then there is no metatype to want, and a static descriptor
  keeps every namespace-scope global trivially destructible.

Nothing is Python-visible at the end of this phase.

### Phase 2 — the core types — **done**

- [x] [err.h](err.h), [err.cpp](err.cpp) — the error channel, needed before an
      operation can fail: a sticky kind and message, checked rather than
      thrown. `R` is `Ok`, `Err` or `NotImpl`, and every slot returns it.
- [x] [obj.h](obj.h) — the `Type` descriptor grew its protocol slots: `truth`,
      `hash`, `eq`, `order`, `repr`, `str`, `len`, `getitem`, `setitem`,
      `contains`, `binop`.
- [x] [ops.h](ops.h), [ops.cpp](ops.cpp) — the generic operations and the
      fallbacks the slots do not answer, with the number tower inside them.
      Floor division and modulo take the sign of the divisor, as in CPython.
- [x] [int.cpp](int.cpp) — small integers only; what does not fit raises
      `OverflowError` rather than wrapping, until there is a bignum.
- [x] [float.cpp](float.cpp) — `braam::math` for the arithmetic, and CPython's
      own repr rule: shortest round-trip digits, exponent form when the decimal
      point is past 16 or at or before -4, a `.0` otherwise. Twenty-eight
      values are checked against what CPython prints.
- [x] [str.cpp](str.cpp) — UTF-8 validated on the way in, counted and indexed
      in codepoints, with an ASCII flag so the common case indexes in O(1).
- [x] [bytes.cpp](bytes.cpp), [tuple.cpp](tuple.cpp), [list.cpp](list.cpp).
- [x] [table.cpp](table.cpp) — one insertion-ordered table behind dict and
      set: the entries in order, an open-addressing index over them.
- [x] [repr.cpp](repr.cpp) — repr for every type, with the quote rule, the
      escapes, and a guard so a container holding itself prints `[...]`.
- [x] Nine more `--selftest` checks — numbers, floats, compare, strtext,
      reprs, dict, set, errors, truth. Twenty in all.

Three decisions worth recording:

- **A set iterates in insertion order**, not CPython's. Matching CPython would
  mean copying its table's sizing and probing exactly. The README records it as
  a known difference.
- **An integral float hashes as the equal int**, or `{1: 'a'}[1.0]` would miss.
- **`str` validates on the way in.** `str_new` rejects malformed UTF-8;
  `str_raw` is the unchecked form, for bytes already known good.

Nothing is Python-visible at the end of this phase either: there is no syntax
yet, and phase 6 is where these types first reach a program.

### Phase 3 — the lexer — **done**

- [x] [lex.h](lex.h), [lex.cpp](lex.cpp) — the full token set (34 keywords, 47
      operators), significant indentation with `Indent`/`Dedent` and CPython's
      tab rule, implicit line joining inside brackets and the backslash join,
      string prefixes `r`/`b`/`u`/`f` and every escape, the number literals
      including underscores and the three radices.
- [x] Error positions: `err_set_at` carries a line and column, and each error
      points at the construct that is wrong — the literal, the escape, the
      character — not at wherever scanning stopped. Indentation errors carry
      CPython's own `IndentationError` and `TabError`.
- [x] `--dump-tokens`, one token per line as `line:col label value`.
- [x] [tools/mklex.py](tools/mklex.py) and [test/pylex.mjs](test/pylex.mjs):
      nine sources under [test/lex/](test/lex/) whose goldens come from
      **CPython's own `tokenize` module**, so the lexer is measured against
      CPython and not against itself — position, kind and decoded value. Nine
      more are sources it must refuse, with the complaint pinned. Every
      upstream test in the manifest must tokenize as well.
- [x] `basics/lexer.py`, `basics/string_escape.py` and
      `basics/string_escape_invalid.py` are in the manifest, marked `fail`
      until there is something to run them with.

Two things the CPython comparison caught that nothing else would have:

- **`Indent` starts at column 1**, covering the whitespace, not at the first
  token of the line.
- **A file with no final newline** puts its dedents and its endmarker on the
  line after the last, not at the end of it.

An f-string is lexed as one `FStr` token holding the body as written; what is
inside the braces is the parser's problem, in a later phase.

### Phase 4 — the parser — **done**

- [x] [parse.h](parse.h), [parse.cpp](parse.cpp) — recursive descent into the
      index arena. The whole 3.9 grammar: every expression form with its
      precedence and associativity, every statement form, comprehensions,
      decorators, `async`, target lists and unpacking, the walrus.
- [x] The node kinds and their fields **mirror CPython's `ast` module**, which
      is what makes the goldens below possible; parse.h writes the layout out.
- [x] [astdump.cpp](astdump.cpp) — `--dump-ast`, whose format is the contract
      [tools/mkast.py](tools/mkast.py) writes to.
- [x] `SyntaxError` with a line and a column, pointing at the construct that is
      wrong: `fail_node` reports at a node's own token, so `1 = 2` complains
      about the `1` rather than about what follows it.
- [x] `MAX_NEST`, counted at the bracketed forms and at each block rather than
      at every rung of the binary-operator ladder, which is a constant six
      deep. A hundred nested brackets parse; a hundred and one are refused
      cleanly rather than trapping.
- [x] [test/pyast.mjs](test/pyast.mjs): seven sources under [test/ast/](test/)
      whose goldens come from **CPython's own `ast` module**, and nine more
      that must be refused, with the complaint pinned. Every upstream test in
      the manifest must parse as well.
- [x] `basics/parser.py`, `basics/op_precedence.py` and `basics/syntaxerror.py`
      are in the manifest.

Two things had to be got right twice:

- **The kids arena is append-only**, so a node's run has to be written in one
  go. Collecting children with interleaved pushes silently captured whatever a
  nested parse had pushed in between, and a function's body turned up as a
  sibling of the function. Every list is now gathered into a local `Vec` and
  written to the arena at the end.
- **A `for` target is not an expression.** Parsed as one, `in` is taken for the
  comparison operator and swallows the iterable, so `for i in range(3)` then
  complained that it expected an `in`. There is a `target_list()` for it.

An f-string is still one `FString` node holding its body as written.

### Phase 5 — the compiler and the bytecode — **done**

- [x] [code.h](code.h), [code.cpp](code.cpp) — 74 opcodes in one X-macro table
      that generates the enum, the names and the operand kinds together, so
      they cannot drift; the code object with its constants, names, varnames,
      cellvars, freevars and run-length line table. An operand is a whole
      `u32`, so there is no `EXTENDED_ARG` and a jump is an absolute
      instruction index that patches in one store.
- [x] [symtab.h](symtab.h), [symtab.cpp](symtab.cpp) — the scope pass. One
      walk collects what each scope binds and uses, a second decides Name,
      Local, Cell, Free or Global and hands out the slot numbers, threading a
      free name up through every scope between the use and the binding.
- [x] [compile.cpp](compile.cpp) — the whole grammar the parser accepts, less
      `async` and f-strings, which are refused with a `SyntaxError` that says
      so. Comprehensions and class bodies are nested code objects; `with` and
      `try` emit the 3.10 handler shapes; the stack size is a depth-first walk
      over the finished instruction graph.
- [x] `--dis` ([dis.cpp](dis.cpp)), and [test/pydis.mjs](test/pydis.mjs) over
      eighteen sources under [test/dis/](test/) — ten listings and eight
      refusals. Every upstream test in the manifest must compile as well.
- [x] Two more `--selftest` checks, `compile` and `scopes`. Twenty-two in all.
- [x] `Ellipsis`, which phase 2 had no reason to want and a `...` constant
      does.

Three decisions worth recording:

- **Loops are not on the block stack.** A `break`, a `continue` or a `return`
  leaving a `try`/`finally` or a `with` emits that cleanup *inline* before it
  jumps, which is CPython's answer since 3.9. So the only thing the VM unwinds
  at run time is an exception, and `SetupFinally`/`PopBlock` are the whole
  mechanism. An exit from inside an inlined `finally` would recurse, and is
  refused rather than mis-compiled.
- **`SetupWith` is a separate opcode** from `SetupFinally` because the handler
  needs the manager's `__exit__` to survive the cut: it records one below the
  current depth, and the `with` handler therefore starts at `[exit, exc]`.
- **A cell that is also a parameter keeps both slots.** It is in `varnames` at
  its argument position and in `cellvars` as well, and the body starts with a
  `LoadFast`/`StoreDeref` pair per such parameter, so a frame needs no
  `cell2arg` table and the copy is visible in the listing.

Nothing runs yet.

### Phase 6 — the VM and the driver — **done**

- [x] [frame.h](frame.h), [frame.cpp](frame.cpp) — one activation, with the
      fast locals and the value stack as a single run of slots after the
      header, so a call costs one allocation.
- [x] [vm.h](vm.h), [vm.cpp](vm.cpp) — the dispatch loop over every opcode the
      compiler emits bar the exception ones; the frame stack chained through
      `back`; argument binding for the whole `def` grammar; cells and
      closures; `Req` and the flush-and-exit half.
- [x] Binary and unary operators through the slot table, subscription,
      slicing, comparison chains, `is` and `in`.
- [x] `if`, `while`, `for`, `break`, `continue`, and comprehensions with them.
- [x] [iter.h](iter.h), [iter.cpp](iter.cpp) — `slice`, `range`, and the three
      iterators every container is walked with. `Type` grew `iter`, `next`,
      `getattr` and `delitem`.
- [x] [func.h](func.h), [func.cpp](func.cpp) — cells, functions, C++ builtins
      and module objects.
- [x] [builtin.cpp](builtin.cpp) — eighteen builtins, none of which calls back
      into Python: `print` (with `sep` and `end`), `len`, `abs`, `repr`, `str`,
      `bool`, `int`, `float`, `list`, `tuple`, `dict`, `set`, `range`, `min`,
      `max`, `sum`, `all`, `any`, `ord`, `chr`. `print` buffers, and the VM
      asks for one write per four kilobytes.
- [x] The driver in [braam.cpp](braam.cpp): `python file.py`, `python -c`,
      `python -` , `sys.argv`, a traceback on stderr, and the exit status.
- [x] [test/pyvm.mjs](test/pyvm.mjs) — the three ways in, argv, four kinds of
      error, a run too long for one write, closures, and the collector under
      load and under `PY_GC_STRESS=1`.

**Seventeen upstream tests pass**, of the thirty-four now in the manifest:
`andor`, `builtin_abs`, `builtin_allany`, `builtin_len1`, `builtin_print`,
`builtin_sum`, `compare_multi`, `comprehension1`, `equal`, `for1`, `ifcond`,
`logic_constfolding`, `op_precedence`, `string_escape`,
`string_escape_invalid`, `true_value`, `while1`. Every one of the seventeen
that does not is waiting on `try`/`except`, on classes, or on the built-in
types having methods — none on the VM.

Three decisions worth recording:

- **An operand is read where it lies.** The value stack is a root, so popping
  into a C++ local and *then* computing would leave the operands unreachable
  across the allocation the operation itself makes. Every case peeks, computes
  and only then moves `sp`. `PY_GC_STRESS=1` is what proves it.
- **A cell parameter needs no `cell2arg` table.** The compiler already emits
  `LoadFast`/`StoreDeref` at the top of the body, so the frame just makes empty
  cells and the copy is ordinary bytecode.
- **`Type` gained `getattr` but nothing gained methods.** A module answers an
  attribute out of its dict; everything else raises. Bound methods are one
  mechanism, and it belongs with the descriptor protocol in phase 9 rather
  than bolted on here.

### Phase 7 — exceptions and control flow — **done**

- [x] [exc.h](exc.h), [exc.cpp](exc.cpp) — thirty-two exception types in one
      static table, each a name and a base pointer, so matching is a few
      pointer compares and no allocation; the type object `except` matches
      against and the instance that carries the arguments; `raise`,
      `raise … from`, and bare `raise`.
- [x] The error channel learned to hold an *object* as well as a kind and a
      message ([err.h](err.h)). A few hundred call sites still say
      `err_set("TypeError", …)`, and the VM materialises an exception from
      that only where an `except` might want one.
- [x] `try`/`except`/`else`/`finally`, the block stack in the frame, and the
      exits through it. `except (A, B)` and `except E as e` with the name
      deleted afterwards.
- [x] `with` and the context-manager protocol: `BeforeWith` and
      `WithExceptStart` do what [code.h](code.h) says they do. Nothing
      built-in is a context manager, so the tests for it wait for classes.
- [x] The traceback, collected frame by frame as the exception unwinds and
      printed with its cause or context first; `SystemExit`, which is the one
      exception that sets a status rather than being an error; `sys.exit`.
- [x] `KeyboardInterrupt`: `sig_catch(SIG_INT)` before the driver's first
      park, `ReqKind::Tick` when the burst's twenty thousand instructions are
      up, and `sleep_for(0)` between bursts — because a signal is delivered
      where a process parks, and a compute loop parks nowhere.
      [test/pyint.mjs](test/pyint.mjs) drives `kill -INT` at one.
- [x] Four more builtins the tests wanted and the runtime already had the
      parts for: `iter`, `next`, `ord`, `chr`. Slicing a `range` yields a
      range.
- [x] A twenty-third `--selftest` check over the hierarchy: every kind the
      error channel can raise is in the table, and every row reaches
      `BaseException`.

**Fifty-three upstream tests pass**, of the sixty-four in the manifest — up
from seventeen. The 20 `try_*` files, `exception1`, `exceptpoly`,
`exceptpoly2`, `except_match_tuple` and `sys_exit` are all green. Of the
eleven that are not, three need `exec`/`compile`, six need methods on the
built-in types, and two need `type()` and `getattr()`.

Four decisions worth recording, three of them found by upstream's tests:

- **The exception state is per frame.** A bare `raise` re-raises what the
  innermost `except` is handling, and a called function can see it — but a
  frame that dies while unwinding must not leave its handler visible to
  whatever catches next. Each frame records what was being handled when it was
  entered; `try_reraise.py` is the test that says so.
- **A `finally` clause is emitted more than once, and the copies do not start
  from the same stack.** The exception copy has the exception on it; a copy
  emitted for a `return` has the return value. A `break` out of either has to
  drop what it is standing on, and the compiler counts it.
- **An exit from inside a `finally` does not unwind that clause again**: its
  handler is popped and its body is what is running. Phase 5 refused this
  outright, which cost five of the `try_finally_*` tests.
- **`CheckExcMatch` pops the copy of the exception as well as the type.** The
  `DupTop` before it exists for exactly that, and getting it wrong inflated
  every stack-size estimate in a `try`.

Two things the phase left alone: `with` cannot be exercised until there is a
class to write `__enter__` on, and a traceback is a string collected as the
frames go rather than a `__traceback__` object.

### Phase 8 — functions, closures, calls — **done**

Phase 6 had already built most of this — `def`, `lambda`, decorators, cells,
`global`, `nonlocal`, `del`, the whole argument grammar and the recursion
limit — so what this phase owed was the *last* line of its own list, and it is
the one the previous phases had been deferring.

- [x] [call.h](call.h), [call.cpp](call.cpp) — argument binding moved out of
      [vm.cpp](vm.cpp), and beside it `ContObj`: **the builtin-callback rule**.
      A builtin that needs Python parks its state and returns the continuation;
      the VM records it on the frame it pushes and `Return` brings the answer
      back to `step`. A frame gained a `cont` slot and that is the whole of it.
- [x] [builtin.cpp](builtin.cpp) — the first three builtins to use it:
      `sorted(key=, reverse=)`, `min(key=, default=)` and `max`, with the
      keys computed one request at a time and the sort itself an iterative,
      stable merge over an index array. `enumerate` came along because
      `builtin_minmax.py` wanted it.
- [x] `DictMerge` ([code.h](code.h)) — `DictUpdate` for a call's keywords,
      where a key already present is a `TypeError`. Two `**` naming one
      parameter used to overwrite in silence; a named keyword now goes in
      through a one-entry map, which is CPython's own shape and puts every
      path under the same check.
- [x] `del` on a cell unbinds it, and `LoadDeref`/`DeleteDeref` on an unbound
      one say *which* name and that it was referenced before assignment —
      `local variable` for a cellvar, `free variable` for a freevar.
      `UnboundLocalError` reads the same way now.
- [x] [test/pyfun.mjs](test/pyfun.mjs) — the callback rule at four thousand
      turns, under `PY_GC_STRESS=1`, with an exception through it and with a
      key function that recurses into the same builtin; the duplicate keyword
      three ways; stacked decorators; a deleted cell.

**Eighty upstream tests pass**, of the 103 now in the manifest — up from 53.
The `fun_*`, `closure*`, `lambda*`, `scope*` and `del_*` families are green
bar the ones that want a `class`, a method on a built-in type, `exec`, or
`__code__`.

Three decisions worth recording:

- **A suspended builtin is not a frame.** It is an object the *callee's* frame
  points back at, so unwinding needs no new case: an exception that escapes the
  key function drops the continuation with the frame and carries on out of the
  builtin, which is what it should do.
- **Decorate, sort, undecorate is forced, not copied.** A comparison sort that
  called back would have to suspend inside its own recursion; computing every
  key first makes the callback phase a flat indexed loop, which a continuation
  can own, and leaves a sort that calls nothing.
- **`map` and `filter` still cannot be written.** Their callback is inside
  `py_next`, which returns a value rather than a request, and no continuation
  can reach it. That is the next thing this mechanism has to grow.

### Phase 9 — classes and the type system

- [ ] `class.cpp` — class bodies as code objects, the metatype, instance
      dicts.
- [ ] `type.cpp` — attribute lookup and the descriptor protocol, `property`,
      `staticmethod` and `classmethod`, `super`.
- [ ] Single inheritance, then C3 for multiple.
- [ ] Special-method dispatch for the operators and for `__str__`, `__repr__`,
      `__len__`, `__iter__`, `__call__`, `__getitem__`.
- [ ] Subclassing the built-in types; `isinstance` and `issubclass`.

Tests: the 37 `class*` files, `subclass_native*`, `special_methods.py`,
`builtin_super.py`, `builtin_property.py`, `object1.py`, `types1.py`.

At the end of this phase the README can state a number: the share of
`tests/basics/` that passes, setting aside the bigint, generator and async
families.

## Later, once the core stands

**Arbitrary-precision integers.** About 25 `int_big_*` tests plus the
`*_intbig.py` variants elsewhere. Our own bignum: `__int128` division needs a
compiler-rt builtin that does not exist on this target, so 32×32→64 limbs and
long division.

**Generators, then `async`.** The frames are already heap objects, which is
most of what a generator needs; `yield from` and then `async`/`await` on top.

**The REPL.** The line editor, and the keyboard-ownership problem `mbasic`
had to solve — a key ring has one receiver and there is no non-blocking key
read, so the editor holds it at the prompt and gives it back the moment a
program runs.

**`import`.** The module object, the search path, and `share/lib/` resolved
through the `/pkg/bin` link the way [../mbasic/epath.cpp](../mbasic/epath.cpp)
resolves its examples.

**The rest of the builtins and the standard library**: `math`, `array`,
`struct`, `json`, `re`, `time`, `os`, `collections`, `random`.

**Float `repr` and format fidelity.** `fmt_f64_shortest` in `math/ftoa.h` is
the round-trip printer; `str.format` and `%` are their own long tail.

**Unicode.** The 25 tests in `tests/unicode/`.

**Error-message wording**, which is where a suite that compares against CPython
byte for byte stops being forgiving.

**Shipping**: the `share/` examples, `Manual.md`, the "what had to change"
section of `README.md`, a version bump, `make index`, and one commit in
`braamix.github.io`.

## Conventions

- Comments are terse: what, not why.
- Markdown wraps at 80 columns; C++ follows the root
  [.clang-format](../../.clang-format).
- Every test driver gets its own line in the `TESTS` variable at the head of
  the top [Makefile](../../Makefile).
- A test copied from upstream is copied byte for byte. Its provenance — the
  upstream path and the commit it came from — is recorded in the manifest, not
  in the file.
