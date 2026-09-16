# Python for Braam — a development plan

Python 3 written for Braam: our own bytecode VM, our own compiler, our own
object model. The interpreter is not a port and stays that way. What is
borrowed is measured, and named here.

**The language stands, the built-in types have their methods, a file can be
imported, text can be formatted, and CPython's own tests are a ruler beside
MicroPython's.** Phases 0 to 13 built the lexer, the parser, the compiler, the
VM, the object heap and its collector, exceptions, functions and closures,
classes with the whole type system, the method tables, the module loader, the
`unittest` and `test.support` shims every CPython test stands on, and the one
format engine that `format()`, `__format__`, `str.format`, `%` and f-strings
all reach. They are done, and their record is the git history — `python: phase
0` through `python: phase 13` — not this file, which from here describes only
what is left.

Where that leaves us, measured against MicroPython's suite: **302 of the 339
tests in [test/manifest.txt](test/manifest.txt)**, and **331 of the 480** in
`tests/basics/` once the bigint, generator, async and t-string families are set
aside. The largest single causes of the rest are bignums and generators, which
are the next two phases, and the modules, which are phase 18. None stop at the
object model.

Measured against CPython's, which is the harder ruler: **five of the twelve in
[test/cpython.txt](test/cpython.txt) run, and thirteen test methods of
thirty-four pass.** `node test/pycases.mjs --survey` runs the whole of
`Lib/test/` and counts what stops each of the 391 files: 213 an unwritten
module, 41 complex numbers, 39 an integer past 2³⁰, 34 other syntax, 31 a lone
surrogate in a literal, 13 `async`, 12 `\N{...}`, and 5 that run.

Phase 13 is what that count is for. F-strings were 124 of those files and are
now none of them, and the wall moved from the compiler to the modules — which
is phase 18, and a good deal further off than the next two. The survey is how
each wave is chosen, and it only means something read beside what it said
last time.

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
  `test_grammar.py` the syntax, `test_str.py`/`test_dict.py`/`test_list.py`
  the built-in types, and `list_tests.py`, `seq_tests.py`, `mapping_tests.py`
  and `string_tests.py` are shared behaviour suites several of them mix in.
  These are unforgiving in a way MicroPython's are not, and they are the
  measure this plan ends on.
- **[tmp/cpython/Lib/](tmp/cpython/Lib/)** — the standard library, most of it
  pure Python written against a small C floor. `re/` is 3,258 lines of Python
  over an `_sre` whose whole Python-visible surface is eight names;
  `collections`, `functools`, `heapq`, `json`, `datetime` and `decimal` each
  carry an `except ImportError` fallback for the day their C accelerator is
  missing, which is our day. **A Python that runs CPython's own library is a
  real Python**, and writing that library again would be both enormous and
  worse. So we implement the floor and take the rest verbatim.

Taking it has two prices. The first is paid: CPython's licence is the PSF
licence, not MIT, and [LICENSE](LICENSE) now carries both and says which files
each covers — phase 12 owed it the moment the first test file was copied in,
and the library will owe it again.

The second is partly paid, and **the library decides the syntax**: it is
written in the Python of its own day, not 3.9's. `dataclasses.py` has 92
f-strings in it and a `match` statement; `functools.py` has 29 f-strings. Phase
13 settled the f-strings, which were the prerequisite for borrowing anything at
all; what the library still wants is `match`, `except*` and the rest of phase
24. The syntax phases below are ordered by what the modules we want actually
use, which is measurable: `pycases.mjs --survey` says what stops each of
CPython's own test files, and the answer moves as each phase lands.

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
and value stack, the module globals, the intern table, the type objects, and an
RAII `Root` chain for C++ code holding a value across an allocation. Mark from
those, sweep the `next` list, collect on bytes-allocated pressure.

**`Type` is a struct of slots, and a type is an object.** A built-in type is a
`TypeObj` wrapping the static `Type` its instances point at; a `class` is a
`TypeObj` carrying a `Type` of its own. A slot cannot call Python, so a special
method written in Python is not in the slot table: `type_lookup` finds it and
the VM makes the call at the opcode, which is the one place a frame can be
pushed. The slots hold only what a native base already answers.

**The AST is an index arena.** Links are `u32` indices, not pointers, because
the arrays reallocate as the parse grows. `sh/parse.h` states the rule.

## Phases

Numbering continues from the core, so a commit message and a phase still name
the same thing. Test names are real files under
[tmp/cpython/Lib/test/](tmp/cpython/Lib/test/) unless they say otherwise.

### Phase 14 — arbitrary-precision integers, and complex

Everything above assumes them, and `test_int.py` and `test_long.py` are
unrunnable without them. Together with `complex`, which the plan did not have
and the survey found, they are what 80 of `Lib/test/`'s files stop on — the
largest cause left that is not a module.

- [ ] Our own bignum: 32×32→64 limbs and long division, because `__int128`
      division needs a compiler-rt builtin this target does not have.
- [ ] The small-int fast path stays: a `Value` with bit 0 set is still a
      31-bit int, and promotion happens at the overflow the current code
      already detects and raises on.
- [ ] Add, subtract, multiply, floor-divide, modulo, power, the bitwise
      operators and the shifts; comparison; `hash` that agrees with the small
      case; decimal, hex, octal and binary conversion both ways; `int(str)`
      with any base.
- [ ] `float` interworking: exact comparison, `int(float)`, `float(int)` with
      overflow to `inf`, and `int.__truediv__` correctly rounded.
- [ ] The three methods phase 10 left raising `OverflowError`:
      `int.to_bytes` past eight octets, `int.from_bytes` of more than eight
      significant ones, and `float.as_integer_ratio` of most values.
- [ ] **`complex`**, which the plan did not have and the survey found: `2j` is
      a `SyntaxError` today and 41 of `Lib/test/`'s files stop on it, more than
      any other single type. The literal, the arithmetic, `real`/`imag`/
      `conjugate`, `abs`, and the repr — but not `cmath`, which is phase 18.
- [ ] The format engine takes them both: `format_int_by` in
      [format.cpp](format.cpp) is written against `i64` and `radix()` over it,
      and `%d` of a bignum goes the same way.

Tests: `test_int.py`, `test_long.py`, `test_complex.py`, and MicroPython's 25
`int_big_*`.

### Phase 15 — generators

The frames are already heap objects chained through `back`, which is most of
what a generator is.

- [ ] `yield` and the generator object: a frame that is parked rather than
      popped, with its own value stack and block stack intact.
- [ ] `send`, `throw`, `close`, `GeneratorExit`, and `StopIteration.value`.
- [ ] `yield from`, delegating send and throw through.
- [ ] Generator expressions, which the compiler already builds as nested code
      objects for comprehensions.
- [ ] The interaction with the driver: a generator resumed from C++ is the
      same callback problem as a special method, so `map`, `filter`, `zip` and
      `sum` over a generator all go through the continuation.

Tests: `test_generators.py`, `test_genexps.py`, `test_yield_from.py`, and
MicroPython's `generator*` and `gen_yield_from*` families.

### Phase 16 — `eval`, `exec`, `compile`, and the namespaces

`collections.namedtuple`, `dataclasses` and `enum` all build classes by
compiling source at run time, so the library needs this.

- [ ] `compile()` to a code object, `eval()` and `exec()` over one or over
      source, with explicit `globals` and `locals` mappings.
- [ ] `globals()`, `locals()`, `vars()`, `dir()`, `__builtins__`. Four of
      upstream's import tests wait on `globals()` alone.
- [ ] The code object made Python-visible: `__code__`, `co_varnames`,
      `co_consts`, `co_argcount`, `co_flags`, `co_filename`, `co_firstlineno`.
- [ ] Function attributes: `__name__`, `__qualname__`, `__doc__`,
      `__defaults__`, `__globals__`, `__closure__`, `__module__`, and
      assignment to them.

Tests: `test_compile.py`, `test_eval.py`, `test_exec.py`, `test_builtin.py`,
`test_funcattrs.py`, and MicroPython's `fun_code*`.

### Phase 17 — the rest of the type system

Phase 9 built the half the language uses daily; this is the half the library
uses.

- [ ] Metaclasses: `class C(metaclass=M)`, `type.__call__`, `__prepare__`, and
      the keyword arguments `__build_class__` currently refuses.
- [ ] `__slots__`, and the instance layout without a dict.
- [ ] The full descriptor protocol on any object — `__get__`, `__set__`,
      `__delete__`, and data descriptors taking precedence over the instance
      dict. Phase 9 does only `property`, which is one instance of it.
- [ ] `__getattribute__`, `__setattr__` and `__delattr__` as overridable hooks.
- [ ] `__init_subclass__`, `__set_name__`, `__class_getitem__`,
      `__mro_entries__`.
- [ ] `__del__`, and `weakref` with callbacks — both of which make the
      collector's sweep observable and need a resurrection rule.
- [ ] `__hash__ = None`, and the `__eq__`/`__hash__` interaction.
- [ ] **A comparison made from C++.** `py_cmp` and `py_eq` cannot push a
      frame, so a class with `__lt__` cannot be sorted and one with `__eq__`
      cannot be found by `list.index`. `sorted`, `min` and `max` have had this
      since phase 8 and `list.sort` joined them in phase 10. The fix is a
      continuation that owns the merge itself, or a `py_cmp` that can suspend.
- [ ] `abc`, `__instancecheck__` and `__subclasscheck__`, which is what
      `collections.abc` stands on.

Tests: `test_descr.py` above all, then `test_class.py`, `test_super.py`,
`test_property.py`, `test_weakref.py`, `test_abc.py`, `test_metaclass.py`.

### Phase 18 — the primitive modules, written natively

The floor CPython's library stands on. Each is small; together they are the
difference between borrowing the library and not.

- [ ] `sys` in full: `argv`, `path`, `modules`, `stdin`/`stdout`/`stderr`,
      `exc_info`, `maxsize`, `version_info`, `implementation`, `getsizeof`,
      `setrecursionlimit`, `exit`.
- [ ] `builtins` as a real module.
- [ ] `_collections` (deque, defaultdict, OrderedDict), `_functools`
      (`reduce`, `partial`, `lru_cache`), `itertools`, `operator`, `_random`
      (Mersenne Twister), `_struct`, `array`, `math` and `cmath` over
      `braam::math`, `time` over `proc_now`, `errno`, `gc`, `_weakref`.
- [ ] `memoryview`'s `itemsize`, `format` and strides, which only `array`
      gives meaning to. Phase 10's is one octet wide and refuses a step.
- [ ] Each is checked against the pure-Python fallback the library already
      carries beside it, which is a free oracle.

Tests: `test_itertools.py`, `test_operator.py`, `test_struct.py`,
`test_array.py`, `test_math.py`, `test_random.py`, `test_time.py`,
`test_sys.py`, `test_gc.py`.

### Phase 19 — `_sre`, and the whole of `re`

The best return of any phase here. `re/` is 3,258 lines of Python we do not
write; what it stands on is one module whose Python-visible surface is
`compile`, `template`, `MAGIC`, `CODESIZE`, `MAXREPEAT`, `MAXGROUPS` and four
case-folding helpers.

- [ ] The `_sre` opcode VM: the pattern is a `u32` array `re/_compiler.py`
      emits, and the matcher walks it with an explicit backtracking stack —
      explicit because ground rule 4 leaves it no other choice.
- [ ] The `Pattern` and `Match` objects: `match`, `search`, `fullmatch`,
      `findall`, `finditer`, `split`, `sub`, `subn`, `group`, `groups`,
      `groupdict`, `span`, `expand`.
- [ ] `re/*.py` taken verbatim, with its provenance recorded.
- [ ] Not `braam::regex`: it is POSIX leftmost-longest, and Python's is
      leftmost-first with back-references, lazy quantifiers and lookaround.
      The two engines answer different questions.

Tests: `test_re.py`.

### Phase 20 — the library, verbatim

With `import`, f-strings, generators, `exec` and `re` in hand, the pure-Python
half of CPython's library can simply be copied.

- [ ] First wave, which needs nothing but the language: `types`, `operator`,
      `abc`, `functools`, `collections`, `collections.abc`, `contextlib`,
      `heapq`, `bisect`, `copy`, `reprlib`, `enum`, `string`, `textwrap`,
      `keyword`, `warnings` (`_py_warnings.py`).
- [ ] Second wave: `json`, `csv`, `base64`, `binascii`, `hashlib`, `random`,
      `statistics`, `fractions`, `decimal` (`_pydecimal.py`), `datetime`
      (`_pydatetime.py`), `pprint`, `difflib`, `shlex`, `dataclasses`,
      `traceback`, `argparse`.
- [ ] Each module is a row in a manifest with the CPython commit it came from,
      and arrives with its own `test_*.py`. A module that needs syntax we do
      not have yet waits rather than being edited.
- [ ] `importlib`, `__spec__`, `__loader__` and reloading. Phase 11's loader is
      C++ and the only thing that finds a module: there is no `sys.meta_path`,
      no `sys.path_hooks`, and nothing for a library module to hook.
- [ ] The packaging question: `share/lib/` against a 4 MiB compressed package
      limit, and whether the whole library or a chosen set ships.

### Phase 21 — `io`, `os`, and the file system

Where ground rule 1 meets the library: every read and write is a `Req`, so the
whole of `io` is continuations.

- [ ] `open()` and the three layers — `RawIOBase` over Braam's descriptors,
      `BufferedReader`/`BufferedWriter`, and `TextIOWrapper` with its codec
      and its newline translation.
- [ ] `sys.stdin`, `sys.stdout` and `sys.stderr` as real file objects, which
      replaces the buffer the VM prints into today.
- [ ] `os`: `listdir`, `stat`, `mkdir`, `remove`, `rename`, `getcwd`, `chdir`,
      `environ`, `urandom`; `os.path` and `posixpath` verbatim; `stat`,
      `fnmatch`, `glob`, `tempfile`, `pathlib`, `shutil`.
- [ ] `signal` over `sig_catch`, and what `KeyboardInterrupt` means once a
      program can install a handler of its own.

Tests: `test_io.py`, `test_fileio.py`, `test_os.py`, `test_posixpath.py`,
`test_pathlib/`, `test_tempfile.py`.

### Phase 22 — Unicode in full

Until here, `str` is codepoints with an ASCII fast path and a range table for
case. The library and `test_str.py` want more.

- [ ] `unicodedata`: the category, the case mappings, the numeric values and
      the names, as a generated table whose size is measured before it ships.
- [ ] `str.upper`/`lower`/`title`/`casefold` and the `is*` predicates by
      category rather than by range. Phase 10 left `casefold` as `lower`,
      `isdecimal` and `isnumeric` as `isdigit`, and `'ß'.isalpha()` False,
      because the range table has no one-codepoint upper for it.
- [ ] `codecs`: `utf-8`, `utf-16`, `utf-32`, `latin-1`, `ascii`, the error
      handlers (`strict`, `ignore`, `replace`, `surrogateescape`,
      `backslashreplace`), and `str.encode`/`bytes.decode` over them.
- [ ] `\N{...}` escapes in the lexer, and identifiers by XID_Start and
      XID_Continue rather than "anything above U+0080", which is a known
      difference today. NFKC normalisation of an identifier goes with it, and
      it is what `test_unicode_identifiers.py` stops on.
- [ ] **A lone surrogate in a string literal.** `"\ud800"` is refused by the
      lexer and CPython allows it; 31 of `Lib/test/`'s files stop there, which
      is second only to f-strings among the lexer's refusals. It needs the
      whole `surrogatepass`/`surrogateescape` question answered, not just the
      range check relaxed.
- [ ] Normalisation, if the table cost is bearable.

Tests: `test_str.py`, `test_unicodedata.py`, `test_codecs.py`.

### Phase 23 — `async` and `await`

Coroutines are generators with a different protocol, so this lands on phase 15;
what makes it interesting here is that Braam already *is* an event loop.

- [ ] `async def`, `await`, `async for` and `async with` — all four of which
      the parser already accepts and the compiler refuses with a `SyntaxError`
      that says so.
- [ ] The coroutine object, `__await__`, `__aiter__`/`__anext__`,
      `__aenter__`/`__aexit__`, and async generators.
- [ ] `asyncio`: the event loop is `braam.cpp`'s park. A `Req` is what the loop
      waits on and `proc_spawn` is what a task is — the mapping is closer than
      it is on a POSIX host, and the selector layer CPython's `asyncio` assumes
      is the part to replace rather than borrow.
- [ ] `contextvars`, which `asyncio` and `unittest` both want.

Tests: `test_coroutines.py`, `test_asyncgen.py`, `test_await.py`, and the
`asyncio` suite as far as it reaches.

### Phase 24 — the syntax since 3.9

The parser was written to 3.9. The library is written to 3.16.

- [ ] `match`, with all seven pattern kinds — literal, capture, wildcard,
      value, sequence, mapping, class — plus guards. `dataclasses.py` and
      `traceback.py` both use it, so it gates the second library wave.
- [ ] `except*` and the exception groups: `BaseExceptionGroup`,
      `ExceptionGroup`, `split`, `subgroup`, and the unwinding rule.
- [ ] `@` as an operator, with `__matmul__` and `__imatmul__`.
- [ ] `X | Y` as a type union, PEP 695's `type` statement and generic syntax,
      and whatever else the modules we actually ship turn out to use.
- [ ] Decide and record the version this tracks, because "3.16" and "the
      subset the shipped library needs" are not the same promise.

Tests: `test_patma.py`, `test_exception_group.py`, `test_syntax.py`,
`test_grammar.py`.

### Phase 25 — annotations and typing

Annotations are discarded today: `x: int = 1` compiles as `x = 1`, and a
parameter annotation costs nothing at `def` time.

- [ ] `__annotations__` on modules, classes and functions, under PEP 649's
      lazy evaluation — what 3.14 onwards does and what `annotationlib.py`
      implements.
- [ ] `typing` verbatim: 3,955 lines that need `__class_getitem__`,
      `__mro_entries__` and a working `functools`, all of which land earlier.
- [ ] `dataclasses`, which is the first thing most code wants annotations for.

Tests: `test_annotations.py`, `test_type_annotations.py`, `test_typing.py`,
`test_dataclasses.py`.

### Phase 26 — the REPL

- [ ] `python` with no arguments, `-i`, `sys.ps1`/`sys.ps2`, and the
      incomplete-input rule `codeop` states.
- [ ] `python -m <module>`, which needs `runpy` or its own small version of it.
- [ ] The line editor, and the keyboard-ownership problem `mbasic` had to
      solve: a key ring has one receiver and there is no non-blocking key
      read, so the editor holds it at the prompt and gives it back the moment
      a program runs.
- [ ] History, and a traceback that reads well at a prompt.

### Phase 27 — shipping

- [ ] `share/lib/` with the library that fits, and `share/` examples written
      in it. Phase 11 already puts that directory on `sys.path`, found through
      the `/pkg/bin` link.
- [ ] `Manual.md`, and the "what had to change" half of
      [README.md](README.md) — which for this program is "what was borrowed,
      and from where".
- [ ] [LICENSE](LICENSE) carrying both the MIT and the PSF terms, saying which
      files each covers.
- [ ] A version that is not `0.1-r0`, `make index`, and one commit in
      `braamix.github.io`.

## What is deliberately not here

- **Threads.** A Braam process is one Web Worker. `_thread` can be a stub that
  raises and `threading` the shim `asyncio` needs, but real concurrency here is
  `proc_spawn` and message passing, not shared memory.
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
