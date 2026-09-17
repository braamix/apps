# Python for Braam — a development plan

Python 3 written for Braam: our own compiler, bytecode VM and object model,
and CPython's library taken byte for byte over a native floor. Phases 0 to 26
are done; their record is the git history and [README.md](README.md). This
file describes only what is left.

Where it stands: **427 of the 449 MicroPython tests** in
[test/manifest.txt](test/manifest.txt) pass, and **59 of the 87 CPython tests**
in [test/cpython.txt](test/cpython.txt) run, 1,513 of their 1,920 methods
passing. `node test/pycases.mjs --survey` says what stops each file of
`Lib/test/`; most stop at a module that is not here yet, which is how each
phase is chosen.

## The two upstreams

Both clones live under [tmp/](tmp/) and neither is committed; what we take from
them is.

**MicroPython** — [tmp/micropython/](tmp/micropython/), HEAD `52b5fbc`. Its
`tests/basics/` is 578 small, self-contained programs that print and compare,
and it has been the ruler for ten phases. It stays the ruler for the language
core, and it is MIT ([LICENSE](LICENSE)).

**CPython** — [tmp/cpython/](tmp/cpython/), HEAD `82952e3`, version 3.16.0a0.
It is the ruler for everything after the core, and it is two distinct things:

- **[tmp/cpython/Lib/test/](tmp/cpython/Lib/test/)** — 519 entries, the real
specification of the language. `test_descr.py` is the type system,
`test_grammar.py` the syntax, `test_str.py`/`test_dict.py`/`test_list.py` the
built-in types, and `list_tests.py`, `seq_tests.py`, `mapping_tests.py` and
`string_tests.py` are shared behaviour suites several of them mix in. These are
unforgiving in a way MicroPython's are not, and they are the measure this plan
ends on. - **[tmp/cpython/Lib/](tmp/cpython/Lib/)** — the standard library,
most of it pure Python written against a small C floor. `re/` is 3,258 lines of
Python over an `_sre` whose whole Python-visible surface is a dozen names;
`collections`, `functools`, `heapq`, `json`, `datetime` and `decimal` each
carry an `except ImportError` fallback for the day their C accelerator is
missing, which is our day. **A Python that runs CPython's own library is a real
Python**, and writing that library again would be both enormous and worse. So
we implement the floor and take the rest verbatim.

Taking it has two prices. The first is paid: CPython's licence is the PSF
licence, not MIT, and [LICENSE](LICENSE) now carries both and says which files
each covers — phase 12 owed it the moment the first test file was copied in,
and the library will owe it again.

The second is not paid yet, and **the library decides the syntax**: it is
written in the Python of its own day, not 3.9's. `dataclasses.py` has 92
f-strings in it and a `match` statement; `typing.py` has fifteen PEP 695
generics; `argparse.py` has nine `lazy` imports. Phase 13 settled the
f-strings; phases 19 and 20 settled the rest, and they came before the library
because the library cannot be compiled without them. `pycases.mjs --survey`
says what stops each of CPython's own test files, and the answer moves as each
phase lands.

**The host's CPython is 3.14** — `/opt/homebrew/bin/python3`, first on
`PATH` — and every golden a CPython wrote was regenerated under it in phase 20.
`tools/pyref.py` is where the `mk*` tools get their interpreter: `$PYTHON`, or
`python3` on `PATH`; the manifest's `exp` column and
[test/goldens.txt](test/goldens.txt) record which one wrote each golden, and a
golden is not rewritten by another one without `--regen`. The `lazy` cases are
the exception: 3.14 has no PEP 810, so they come from the clone built out of
tree at `tmp/cpython-build/`.

**The version this tracks is 3.14's language**, which is what
`sys.version_info` says, plus two things from CPython's main branch that the
library written there already uses: PEP 810's `lazy` imports, and a `+`
before a number in a pattern. Nothing else from 3.15 is taken until a library
module or a test needs it; PEP 798's unpacking in comprehensions is the first
such thing the survey names, in `test_listcomps.py`.

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
   dispatch loop continues. A C++ builtin that must call back into Python
   parks its state in a `ContObj` ([call.h](call.h)) and returns it; the VM
   records the continuation on the frame it pushes, and `Return` brings the
   answer back. It does not re-enter the loop.
3. **There is no `longjmp` and nothing like it.** An error is a sticky pending
   exception plus a sentinel return, unwound a frame at a time. `editors/vi`
   and `lang/mbasic` arrived at the same answer for the same reason.
4. **The parser is the only recursive thing.** It runs on the 128 KiB native
   stack and carries a nesting bound, the way
   `../braam-core/src/cmd/sh/parse.h` does with `MAX_NEST`.
5. **Braam idiom, not a libc port.** No `PORT`: `kernel/vec.h`, `string.h`,
   `str.h`, `hash.h`, `result.h`, `text.h`, `alloc.h`, and `LIBS braam::math`
   for `math/math.h` and `math/ftoa.h`. Never `new` — `heap_new` and
   `heap_delete`. Namespace-scope globals stay trivially destructible.
6. **Nothing lives in a coroutine frame.** Only `braam.cpp` has frames at all,
   and they hold a pointer to the interpreter state and nothing else.
7. **No phase depends on a later one.** A library module that needs something
   not written yet waits for the phase that writes it, and the module is never
   edited to get round it. A phase that finds a new forward dependency moves
   the dependency earlier or itself later, and says so here.

The limits to build against, with their sources: `PROC_MAX_PAGES = 1600`, so
100 MB of linear memory (`../braam-core/src/kernel/sysabi.h`); a 128 KiB shadow
stack; `PROC_TASKS = 8`. The repository's [CLAUDE.md](../../CLAUDE.md) said
16 MB until phase 18 corrected it.

## The design

**`Value` is a 32-bit tagged word.** Bit 0 set means a 31-bit small integer;
otherwise it is a pointer to an `Obj`, which is at least 4-byte aligned
(`heap_alloc`'s smallest size class is 16). `None`, `True` and `False` are the
addresses of static PODs.

An integer past that word is a `BigObj` whose type is `int_type`, so the two
shapes are one type from Python. The rule that keeps them from disagreeing is
that **a BigObj never holds a value a small int could hold**: every operation
ends at `big_make`, which hands back a small Value when it can, so two equal
integers are always the same shape and hash the same way.

**`Obj` is `{ const Type *type; Obj *next; Obj *grey; u32 flags; }`** —
sixteen bytes, the smallest size class. `next` threads every live object onto
one list, because `kernel/alloc.h` has no heap iterator and the sweep needs
something to walk; `grey` threads the marker's worklist, so marking neither
allocates nor recurses.

**Collection is precise mark-and-sweep.** Conservative scanning is not
available here: there is no `__builtin_frame_address`, no exported stack base,
and wasm keeps pointers in locals that a scan of linear memory cannot see. So
the roots are exhaustive and explicit — the frame stack, each frame's locals
and value stack, the module globals, the intern table, the type objects, and an
RAII `Root` chain for C++ code holding a value across an allocation. Mark from
those, sweep the `next` list, collect on bytes-allocated pressure.

**`Type` is a struct of slots, and a type is an object.** A built-in type is a
`TypeObj` wrapping the static `Type` its instances point at; a `class` is a
`TypeObj` carrying a `Type` of its own, and a class made by a metaclass points
at *that* metaclass's slots, so `type(C) is M` falls out of the layout. A slot
cannot call Python, so a special method written in Python is not in the slot
table: `type_lookup` finds it and the VM makes the call at the opcode, which is
the one place a frame can be pushed. The slots hold only what a native base
already answers.

**An attribute lookup may be a call, so it hands back a continuation.** A
getter, a `__get__`, a `__getattribute__` and a `__getattr__` are all Python,
and ground rule 2 says the lookup cannot make that call itself. The same is
true of a comparison a sort has to make and of a finalizer the sweep owes: each
is a state machine the VM drives, never a nested interpreter.

**The AST is an index arena.** Links are `u32` indices, not pointers, because
the arrays reallocate as the parse grows. `sh/parse.h` states the rule.

## Phases

Numbering continues from the core, so a commit message and a phase still name
the same thing. Test names are real files under
[tmp/cpython/Lib/test/](tmp/cpython/Lib/test/) unless they say otherwise.

The order is the layers that need the whole library (27–30); the language
phases, the floor, both waves of the library, `re` and the file system are
done. Each phase
lists only what the phases before it have made possible.

After Phase 26: **the library ships as the package's `lib/`**, exactly the
rows of [lib/manifest.txt](lib/manifest.txt) — 178 files, 1.9 MB
compressed with the binary. The whole of CPython's `Lib/` less its tests is
3.5 MB compressed, so the 50 MiB limit decides nothing; what does is the rule
that a module ships once it has been copied and runs. **importlib is
CPython's, and the C++ loader stays the fast path**: importing `importlib`
installs the three finders and the path hook and gives every module its
`__spec__`, and a finder or hook the program adds sends each import after it
through `importlib._bootstrap._find_and_load`. **A traceback is an object** —
`__traceback__`, `tb_next`, `tb_frame`, `co_positions()` — but carries lines
only, so `traceback` prints no carets until `ast` is here. **`_colorize` is
native and colourless** until `dataclasses` is. **`sysconfig` waits**: it
imports a `_sysconfigdata_*` module a CPython build generates, which is not a
copy this tree can take. **Of the plan's tests**, `test_base64.py`,
`test_binascii.py`, `test_math.py`, `test_calendar.py`, `test_strptime.py` and
`test_contextlib.py` run; `test_datetime.py` runs none, its `load_tests` not
being called; `test_hashlib.py` and `test_time.py` stop at `sysconfig`,
`test_decimal.py` at `pickle`, `test_statistics.py` at `doctest`,
`test_fractions.py` at `typing`, `test_traceback.py` at `inspect`,
`test_codeop.py` at `ast`, and `test_argparse.py` and `test_hmac.py` at
`unittest.mock`. **`test_importlib/` skips whole**, as it does on any CPython
built without the `_testmultiphase` test extension; `test/stdlib/imports.py`
measures importlib against CPython instead. `importlib.invalidate_caches()`,
`importlib.resources`, `importlib.abc` and `importlib.metadata` import
`typing` (phase 27).

### Phase 27 — annotations and typing

Annotations are discarded today: `x: int = 1` compiles as `x = 1`, and a
parameter annotation costs nothing at `def` time. The compiler half has no
dependency; the library half imports `ast`, which is why the phase is here.

- [ ] `__annotations__` and `__annotate__` on modules, classes and functions,
      under PEP 649's lazy evaluation — what 3.14 onwards does.
- [ ] `_ast`, so `ast.py` can be copied: the node classes, and `compile()`
      with `PyCF_ONLY_AST` answering them from the parser's arena.
      `annotationlib.py` imports `ast` at the top; `inspect.py` does too.
- [ ] `annotationlib`, `typing` (3,955 lines over the `_typing` phase 20
      wrote), `dis` and `opcode` over a native `_opcode`, `inspect`, and
      `dataclasses`, which is the first thing most code wants annotations for.
- [ ] Hand `typing.py` what `_typing` and the union answer natively today, as
      CPython does: a string in a union or a `Generic` becomes a
      `ForwardRef` through `typing._type_check`, and substitution, unpacking
      a `TypeVarTuple` and `Generic.__class_getitem__` are typing's.
- [ ] `doctest` and the real `unittest`, which is where the shims end.

Tests: `test_annotations.py`, `test_type_annotations.py`, `test_typing.py`,
`test_dataclasses/`, `test_inspect/`; and now the ones earlier phases
deferred — `test_coroutines.py`, `test_asyncgen.py`, `test_patma.py`,
`test_grammar.py`, `test_type_params.py`, `test_collections.py`,
`test_fractions.py`, `test_traceback.py`, `test_codeop.py`,
`test_functools.py`, `test_enum.py`, `test_itertools.py`, `test_syntax.py`,
`test_posixpath.py`, `test_random.py`, `test_shlex.py`, `test_shutil.py`,
`test_pprint.py`, and `test_json/` and `test_io/` with the `load_tests`
protocol and `import_fresh_module`.

### Phase 28 — `asyncio`

The language half was phase 19. What makes this interesting here is that
Braam already *is* an event loop.

- [ ] `asyncio`: the event loop is `braam.cpp`'s park. A `Req` is what the loop
      waits on and `proc_spawn` is what a task is — the mapping is closer than
      it is on a POSIX host, and the selector layer CPython's `asyncio` assumes
      is the part to replace rather than borrow.
- [ ] `sys.set_asyncgen_hooks` and `get_asyncgen_hooks`, so the loop hears
      when an async generator starts and when one is dropped unfinished.
- [ ] `threading` as the shim `asyncio` needs over phase 22's `_thread`.
- [ ] `contextvars.py` over phase 22's `_contextvars`, and the context each
      task runs in.

Tests: the `asyncio` suite as far as it reaches, and `test_contextlib.py`,
which imports `threading`.

### Phase 29 — the REPL

- [ ] `python` with no arguments, `-i`, `sys.ps1`/`sys.ps2`, and the
      incomplete-input rule `codeop` states.
- [ ] `python -m <module>`, which needs `runpy` or its own small version of it.
- [ ] The line editor, and the keyboard-ownership problem `mbasic` had to
      solve: a key ring has one receiver and there is no non-blocking key
      read, so the editor holds it at the prompt and gives it back the moment
      a program runs.
- [ ] History, `sys.displayhook` and `builtins._`, and a traceback that reads
      well at a prompt.

### Phase 30 — shipping

- [ ] Examples written in Python, beside the library the package already
      ships as `lib/`.
- [ ] `Manual.md`, and the "what had to change" half of
      [README.md](README.md) — which for this program is "what was borrowed,
      and from where".
- [ ] [LICENSE](LICENSE) carrying both the MIT and the PSF terms, saying which
      files each covers.
- [ ] A version that is not `0.1-r0`, `make index`, and one commit in
      `braamix.github.io`.

## What is deliberately not here

- **Threads.** A Braam process is one Web Worker. `_thread` is a stub whose
  locks are never contended and `threading` the shim `asyncio` needs, but real
  concurrency here is `proc_spawn` and message passing, not shared memory.
- **C extension modules.** There is no `dlopen` and no stable ABI to offer.
  Anything CPython writes in C is either implemented natively here or taken
  from the pure-Python fallback beside it.
- **`pickle` of arbitrary objects**, `marshal` compatibility, `ctypes`,
  `socket`, `ssl`, `subprocess` — each wants something the platform has not
  got.

## Conventions

- Comments are terse: what, not why.
- Markdown wraps at 80 columns; C++ follows the root
  [.clang-format](../../.clang-format).
- Every test driver gets its own line in the `TESTS` variable at the head of
  the top [Makefile](../../Makefile).
- A test copied from upstream is copied byte for byte. Its provenance — the
  upstream path and the commit it came from — is recorded in the manifest, not
  in the file. The same rule holds for a library module taken from CPython.
- A phase is done when its tests are in a manifest and green, and when
  [README.md](README.md) says the new number.
