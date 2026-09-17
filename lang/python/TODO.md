# Python for Braam — a development plan

Python 3 written for Braam: our own bytecode VM, our own compiler, our own
object model. The interpreter is not a port and stays that way. What is
borrowed is measured, and named here.

**The language stands, the built-in types have their methods, a file can be
imported, text can be formatted, numbers have no width, a function can yield, a
program can compile and run more of itself, the type system is whole,
twenty-two modules are written natively, a function can be a coroutine, the
syntax is 3.14's, text is Unicode's, the floor under the library is down, its
first wave runs, and so does `re`.** Phases 0 to 24 built the lexer, the parser, the
compiler, the VM, the object heap and its collector, exceptions, functions and
closures, classes, the method tables, the module loader, the `unittest` and
`test.support` shims every CPython test stands on, the one format engine that
`format()`, `__format__`, `str.format`, `%` and f-strings all reach, the bignum
and `complex` that finish the number tower, generators with `yield from`,
`compile`/`eval`/`exec` with the namespaces and the attributes they make
visible, the half of the type system the library uses — the metaclasses, the
descriptor protocol, `__slots__`, the attribute hooks, the finalizers and weak
references, the comparisons a sort has to make from C++, and the native `_abc`
that CPython's own `abc.py` runs over — then the primitive modules: `sys` in
full, `_collections`, `_functools`, `itertools`, `operator`, `_random`,
`_struct`, `array`, `math`, `cmath`, `time`, `errno`, `gc` and `_types`, with
the protocol methods in each built-in type's namespace and the generic alias
that makes `list[int]` a value — and then `async def`, `await`, `async for`,
`async with`, the async comprehensions and async generators, with the
awaitables they make — and then everything the library's syntax asks since 3.9:
`@`, unions, `match`, `except*` and the exception groups, PEP 695 over a native
`_typing`, PEP 701's f-strings, PEP 750's t-strings and PEP 810's lazy imports
— and then the Unicode database and `unicodedata`, str by Unicode's categories
and case mappings, `_codecs` with CPython's own `codecs.py` and `encodings`
over it, PEP 263's source encodings, `\N{...}`, identifiers by XID and NFKC,
and the lone surrogate as a character — and then the rest of the native floor:
`_thread`, `_contextvars`, `_string`, the weak proxies, `range` at any width,
`FrameLocalsProxy`, `Placeholder` and `cmp_to_key`, and the implicit
`__class__` cell — and then CPython's first twenty-five library modules, over
`frozendict`, `sentinel`, `mappingproxy`, `object`'s pickle helpers, a native
`_warnings` and `atexit` — and then CPython's whole `re/` over a native `_sre`
that can stop in the middle of a match, with `textwrap`, `json`, `fractions`
and `difflib` over it, and `unicodedata.ucd_3_2_0`. They are done, and their
record is the git history — `python: phase 0` through `python: phase 24` — not
this file, which from here describes only what is left.

Where that leaves us, measured against MicroPython's suite: **424 of the 449
tests in [test/manifest.txt](test/manifest.txt)**. Of the twenty-five that do
not, most exercise what MicroPython does and CPython does not — a native base
class's `__init__` protocol, `pend_throw`, `machine` — and two want a
memoryview of more than one dimension. One, `assign_expr_syntaxerror.py`,
expects MicroPython to accept what CPython refuses, and stays as it is.

Measured against CPython's, which is the harder ruler: **thirty-two of the
forty in [test/cpython.txt](test/cpython.txt) run, and 620 test methods of 796
pass.** `node test/pycases.mjs --survey` runs the whole of `Lib/test/` and
counts what stops each of the 391 files: 351 an unwritten module, 34 that run,
5 that fail at runtime or say nothing this can read, and 1 other syntax, PEP
798's. Phase 24 moved four files to running and one past its imports.

Three walls came down in phases 13 and 14 — f-strings, complex and the bignum
were 204 files between them — a fourth in phase 19, whose fifteen `async` files
went fourteen to an import and one to other syntax, a fifth in phase 20,
whose forty-two went thirty-nine to an import and three to running, and a
sixth in phase 21, whose forty-seven — a lone surrogate or `\N{...}` in a
literal — all went to an import. **What stops
CPython's tests is an import**, and what those files stop on is the *library*,
not the modules under it. The survey is how each wave is chosen, and it only
means something read beside what it said last time.

**The library is being borrowed.** Forty-one files and 89 of
`lib/encodings/` are CPython's own, byte for byte, with their provenance in
[lib/manifest.txt](lib/manifest.txt) — `tools/mklib.py` writes a row — and the
PSF terms in [LICENSE](LICENSE). The phases after this one grow that directory,
and phase 30 ships it; the pattern is that we write the floor natively and take
the rest as it is.

## Why the phases are in this order

The plan used to put `re` next and the library after it, with the syntax the
library is written in further down. Measuring `import re` showed that order
could not be built: every phase waited on a later one.

`re/__init__.py` imports `enum`, `functools` and `copyreg` at the top, and
compiling those — not running them, compiling, since a module is compiled
whole — reaches the following, measured by running each file through
`python --dis` and then importing it:

| module | stops on | which is |
| --- | --- | --- |
| `types.py` | `async def`, in the fallback for a missing `_types` | the language |
| `_collections_abc.py` | `async def` and `await`, at module level | the language |
| `collections/__init__.py` | `lazy from copy import copy` (PEP 810) | the language |
| `operator.py` | `a @ b` | the language |
| `copyreg.py` | `int \| str`, evaluated at import | the language |
| `reprlib.py` | `import _thread` | the native floor |
| `_py_warnings.py` | `import _contextvars`, `import _thread` | the native floor |
| `string/__init__.py` | `import _string` | the native floor |
| `contextlib.py` | `import os` | the file system |

`functools` reaches `collections`, `_collections_abc`, `types`, `reprlib` and
`_thread`; `enum` reaches `types`. So `re` stands on the library, the library
stands on the language, and the order below is that order: **the language
first, then the floor, then the library in the layers its own imports make.**
The same measurement says `types.py` cannot "simply be copied" once `_types`
exists, as this file used to claim — the fallback it never runs still has to
compile.

Phase 19 has taken the first two rows away. Measured again after it:
`types.py` imports; `enum.py`, `functools.py` and `copyreg.py` compile; and
`_collections_abc.py` compiles and stops at runtime on `range(1 << 1000)`,
which phase 22 now carries. Phase 20 took the other three language rows, and
phase 22 the floor rows: `reprlib` and `string` import, and `_py_warnings.py`
gets as far as `sys.flags.context_aware_warnings`. What is left of the table
is the file system.

It also says what a phase's CPython tests can prove. A test file imports far
more than the module it tests — `test_functools.py` imports `annotationlib`,
`random`, `threading`, `typing` and `unittest.mock` — so each phase names the
tests whose imports it has satisfied, and says which ones wait.

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

The order is library by layer (25–26), then the layers that need all of it
(27–30); the language phases, the floor, the first wave and `re` are done. Each phase
lists only what the phases before it have made possible.

Phase 18 left two things for the phases that use them. `memoryview` is flat:
it has an item size, a format and a stride, and `cast()` recasts a contiguous
one, but it is one-dimensional and nothing here makes a buffer with more than
one. And `_types` is missing the names this interpreter has no type for —
`UnionType` arrived with phase 20, and `TracebackType` for whenever a traceback
stops being a string.

Phase 19 left three. MicroPython's `async_await2.py`, `async_for2.py` and
`async_with2.py` import `types` for `types.coroutine`, and pass once phase 23
copies it in; `code.replace()` is what that decorator stands on, and it is
there. A coroutine collected without ever starting does not warn, because
there is no `warnings` to warn through. And nothing is told when an async
generator starts or is dropped, because `sys.set_asyncgen_hooks` is for an
event loop.

Phase 20 left three. **The union and `Generic` refuse a string**, where CPython
hands one to `typing._type_check`; phase 27 hands them over. **A comprehension
is still a function**, where PEP 709 inlined it. And the eight CPython tests
this phase added all compile and stop at an import — `test_exception_group.py`
at `collections` and `test_except_star.py` at `textwrap`, both of which run
since phases 23 and 24, `test_syntax.py` at `doctest` (phase 27),
`test_type_aliases.py` at `pickle`,
`test_grammar.py` and `test_type_params.py` at `annotationlib`, and
`test_patma.py` at `collections` then `dataclasses` (phase 27);
`test_tstring.py` wants `test.test_string`, which is a package of tests.
`test_lazy_import/` imports `subprocess`, `threading` and `tempfile`, and was
not copied.

Phase 21 left five. **`unicodedata.ucd_3_2_0` was missing**, and phase 24
wrote it; what `stringprep` and the `idna` codec wait for now is below.
**Seven codecs in `encodings` wait for a module**: `base64_codec`,
`hex_codec`, `uu_codec` and `utf_7_imap` for `binascii`, `quopri_codec` for
`quopri` and `io`, `bz2_codec` and `zlib_codec` for compression that is not
planned; they were not copied, and neither were the CJK codecs over
`_multibytecodec`, `mbcs`, `oem` and the two Windows and iconv helpers, which
this platform will never have. **An invalid escape warns about nothing**:
CPython's SyntaxWarning waits for `warnings`, phase 23. **A program's own file
cannot name a codec written in Python in its cookie**, because it is read
before there is a VM; `compile`, `exec` and `import` can. And **`--dump-tokens`
prints a name in its NFKC form**, where `tokenize` prints it as written; a
native `_tokenize` in phase 26 has to keep both.

Phase 23 left six. **Four modules of the wave waited on a later one in part**:
`string.Template` and `locale.format_string` compiled a regular expression,
which phase 24 gave them; `warnings.deprecated` imports `inspect` (phase 27),
and `linecache` reads no file until `io` does (phase 25), so a warning prints
without its source line. **There is no `_bisect` or `_heapq`**: the pure-Python
fallbacks run, and `test_bisect.py`'s C half errors on the `None` its
`import_fresh_module` answers. **Of the plan's seven tests, four wait**:
`test_heapq.py` imports `random` and `doctest`, `test_copyreg.py`
`test.pickletester`, `test_reprlib.py` `annotationlib`, `os` and `importlib`,
and `test_weakset.py` `contextlib`, which imports `os`. **A traceback is still a
string**, so the exception-group tests that read `__traceback__` error, as does
`staticmethod.__annotations__` (phase 27). **A `mappingproxy` over a mapping
written in Python** reads it through slots, which cannot call it. And
**`test_super.py`** stops at `pickle` now, after `copy`.

Phase 24 left eight. **`stringprep` and the `idna` codec wait for Unicode
17.0.** `unicodedata.ucd_3_2_0` is written and agrees with CPython's for every
codepoint, but the `stringprep.py` of CPython's main branch is generated
against 17.0 — its B.3 table lists only where 3.2.0's case folding differs
from 17.0's, and it asserts the version at import — and the tables here are
16.0, 3.14's, on purpose. Either they move to 17.0, with every golden a 3.14
wrote for a character in between, or the two modules wait; a copy is not
edited. **A buffer is copied before it is matched**, where CPython pins it, so
`test_re.py`'s `test_keep_buffer` fails: nothing here counts exports of a
`bytearray`, and a memoryview does not pin one either. **`test_re.py`'s
`test_pickling` waits for `pickle`.** **`difflib.unified_diff` waits for
`_colorize`**, which imports `os` (phase 25) and `dataclasses` (phase 27).
**`test_fractions.py` stops at `decimal`** (phase 26), and **`test_json/`**, a
package of tests, wants `import_helper.import_fresh_module`, `doctest` and `os`
(phase 27). **`json` has no `_json`**, which CPython allows for, so a
malformed document is reported in the pure decoder's words. And **a
`Fraction` used as a dict key still hashes by identity**, as every instance
does: `hash()` now agrees with CPython's numeric hash, modulo 2³¹ − 1 as
`sys.hash_info` says, but `py_hash` cannot call a `__hash__` written in
Python, so `{0.5: 1}[Fraction(1, 2)]` misses where CPython hits.

### Phase 25 — `io`, `os`, and the file system

Where ground rule 1 meets the library: every read and write is a `Req`, so the
whole of `io` is continuations.

- [ ] `open()` and the three layers — `RawIOBase` over Braam's descriptors,
      `BufferedReader`/`BufferedWriter`, and `TextIOWrapper` with its codec
      (phase 21) and its newline translation. Native `_io`, or `_pyio.py`
      verbatim over a native floor: decide, and record why.
- [ ] `sys.stdin`, `sys.stdout` and `sys.stderr` as real file objects, which
      replaces the buffer the VM prints into today.
- [ ] `os` over a native `posix`: `listdir`, `stat`, `mkdir`, `remove`,
      `rename`, `getcwd`, `chdir`, `environ`, `urandom`; `os.path`,
      `posixpath`, `genericpath` and `stat` verbatim.
- [ ] What waited for `os`: `contextlib`, `fnmatch`, `glob`, `tempfile`,
      `shutil`, `pathlib`, `random` (`from os import urandom`), `pprint` and
      `shlex` (`io`), `csv` over a native `_csv`, and `gettext`.
- [ ] `signal` over `sig_catch`, and what `KeyboardInterrupt` means once a
      program can install a handler of its own.

Tests: `test_io.py`, `test_fileio.py`, `test_os.py`, `test_posixpath.py`,
`test_pathlib/`, `test_tempfile.py`, `test_contextlib.py`, `test_random.py`.

### Phase 26 — the library, second wave

What needs the file system, and the native accelerators the rest has no
fallback for.

- [ ] `binascii` natively, and `base64` over it; `hashlib` over native
      `_md5`, `_sha1`, `_sha2` and `_sha3`.
- [ ] `statistics`, `decimal` (`_pydecimal.py`, over `_contextvars`),
      `datetime` (`_pydatetime.py`).
- [ ] `argparse`, `traceback` (`match`, `linecache`, `contextlib`, `pathlib`,
      `textwrap`, `codeop`, `tokenize`), `tokenize` and `token` over a native
      `_tokenize`.
- [ ] `importlib`, `__spec__`, `__loader__` and reloading. Phase 11's loader is
      C++ and the only thing that finds a module: there is no `sys.meta_path`,
      no `sys.path_hooks`, and nothing for a library module to hook.
- [ ] The packaging question: `share/lib/` against a 4 MiB compressed package
      limit, and whether the whole library or a chosen set ships.

Tests: `test_base64.py`, `test_binascii.py`, `test_hashlib.py`,
`test_statistics.py`, `test_decimal.py`, `test_fractions.py` once `decimal`
is in, `test_datetime.py`,
`test_argparse.py`, `test_traceback.py`, `test_importlib/`.

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
`test_functools.py`, `test_enum.py`, `test_itertools.py`, `test_syntax.py`,
and `test_json/` with the `load_tests` protocol and `import_fresh_module`.

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

Tests: the `asyncio` suite as far as it reaches.

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
