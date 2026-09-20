# python — Python 3, written for Braam

Not a port. Every other program in this tree is somebody else's source with the
lines that touch the OS replaced; this one is a Python implementation written
from nothing — its own lexer, parser, compiler, bytecode and virtual machine,
132k lines of C++ in [src/](src/). One corner is a port after all:
[src/expat/](src/expat/) is libexpat 2.8.4 rewritten in C++, because what
`pyexpat`'s callers check is expat's own error codes, messages and positions,
and only expat's own code produces those.

The other half is borrowed whole: **CPython's standard library**, 313 files
byte for byte as [lib/](lib/), over a floor of native modules written here. A
Python that runs CPython's own library is a real Python, and writing that
library again would be both enormous and worse.

[Manual.md](Manual.md) is the language: what runs, what does not, and why.
This file is the implementation.

```
$ python --version
Python 3.14.0 on Braam
$ python -c 'import json; print(json.dumps({"n": 2**100}))'
{"n": 1267650600228229401496703205376}
```

## Building and testing

`make` at the top of the tree builds it; `make test` runs the suite, several
tests at a time. One of them:

    make test TESTS=lang/python/test/pysmoke.mjs

The two longest lists are cut into shards — `pycases.mjs,--shard=1/4` is one
entry of four — so that neither sets the length of the whole run. A shard is
every nth row rather than a block, because what a row costs varies by two
orders of magnitude.

    make test STRESS=1

runs every case again under a collector that collects at **every** allocation.
It found nine missing pins; ask for it after touching any of this C++.

The tests need node 22.12; see the top README.

## The two upstreams

Neither upstream is committed here; what is taken from them is.

**MicroPython** — HEAD `52b5fbc`, MIT. Its `tests/basics/` is 578 small,
self-contained programs that print and compare, and it is the ruler for the
language core. The copies are in [test/cases/](test/cases/), their provenance
in [test/manifest.txt](test/manifest.txt).

**CPython** — HEAD `82952e3`, version 3.16.0a0, PSF. It is the ruler for
everything after the core, and it is two things:

- **`Lib/test/`** — the real specification of the language, unforgiving in a
  way MicroPython's tests are not. The copies are in
  [test/cpython/](test/cpython/), their rows in
  [test/cpython.txt](test/cpython.txt). Every one imports `unittest`, which is
  CPython's own here, and most import `test.support`, which is
  [test/shim/](test/shim/)'s — written against what this interpreter has.
- **`Lib/`** — the library, most of it pure Python over a small C floor. `re/`
  is 3,258 lines of Python over an `_sre` whose Python-visible surface is a
  dozen names; `collections`, `functools`, `heapq`, `json`, `datetime` and
  `decimal` each carry an `except ImportError` fallback for the day their C
  accelerator is missing, which is our day. So the floor is written here and
  the rest taken verbatim, a row each in [lib/manifest.txt](lib/manifest.txt).

**The library decided the syntax.** It is written in the Python of its own day:
`dataclasses.py` has 92 f-strings and a `match` statement, `typing.py` fifteen
PEP 695 generics, `argparse.py` nine `lazy` imports. Each had to land before
the module that needed it could be compiled at all.

**The version is 3.14's**, which is what `sys.version_info` says, plus two
things from CPython's main branch that the library written there already uses:
PEP 810's `lazy` imports, and a `+` before a number in a pattern.

## Ground rules

Not style. Each is forced by the platform, and getting one wrong is a rewrite.

1. **The VM is a driver, not a coroutine.** `emulators/simbesm`'s shape. Only
   [braam.cpp](src/braam.cpp) contains `co_await`. `vm_burst()` runs plain C++
   until it has something for its caller to do — a read, a write, an open, a
   sleep, an exit — and returns a `Req` describing it; the driver performs it
   and hands the answer back. A `co_await` is a call and not a tail call, so an
   interpreter loop that awaited would grow the native stack until the process
   trapped.
2. **The VM never recurses for a Python call.** A call pushes a frame and the
   dispatch loop continues. A C++ builtin that must call back into Python parks
   its state in a `ContObj` ([call.h](src/call.h)) and returns it; the VM
   records the continuation on the frame it pushes, and `Return` brings the
   answer back. It does not re-enter the loop.
3. **There is no `longjmp` and nothing like it.** An error is a sticky pending
   exception plus a sentinel return, unwound a frame at a time. `editors/vi`
   and `lang/mbasic` arrived at the same answer for the same reason.
4. **The parser is the only recursive thing.** It runs on the 128 KiB native
   stack and carries a nesting bound.
5. **Braam idiom, not a libc port.** No `PORT`: `kernel/vec.h`, `string.h`,
   `str.h`, `hash.h`, `result.h`, `text.h`, `alloc.h`, and `LIBS braam::math`.
   Never `new` — `heap_new` and `heap_delete`. Namespace-scope globals stay
   trivially destructible.
6. **Nothing lives in a coroutine frame.** Only `braam.cpp` has frames at all,
   and they hold a pointer to the interpreter state and nothing else.

The limits to build against: 100 MB of linear memory (`PROC_MAX_PAGES` in
`../../../braam-core/src/kernel/sysabi.h`), a 128 KiB shadow stack,
`PROC_TASKS = 8`. The recursion limit is 200 because that is what the native
stack holds.

## The design

**`Value` is a 32-bit tagged word.** Bit 0 set means a 31-bit small integer;
otherwise it is a pointer to an `Obj`, which is at least 4-byte aligned.
`None`, `True` and `False` are the addresses of static PODs.

An integer past that word is a `BigObj` whose type is `int_type`, so the two
shapes are one type from Python. **A BigObj never holds a value a small int
could hold**: every operation ends at `big_make`, which hands back a small
Value when it can, so two equal integers are always the same shape and hash the
same way.

**`Obj` is `{ const Type *type; Obj *next; Obj *grey; u32 flags; }`** — sixteen
bytes, the smallest size class. `next` threads every live object onto one list,
because `kernel/alloc.h` has no heap iterator and the sweep needs something to
walk; `grey` threads the marker's worklist, so marking neither allocates nor
recurses.

**Collection is precise mark-and-sweep**, because conservative scanning has no
mechanism here: there is no `__builtin_frame_address`, no exported stack base,
and wasm keeps pointers in locals that a scan of linear memory cannot see. So
every root is named — the frame stack, each frame's locals and value stack, the
module globals, the intern table, the type objects, and an RAII `Root` chain
for C++ holding a value across an allocation:

    Root s{ obj_value(str_new("x")) };   // survives the tuple below
    TupleObj *t = tuple_new(1);

Forget the `Root` and the string is freed under you; that is what `STRESS=1`
exists to catch. Inside the VM the discipline takes a second form: an operand
is read *where it lies* and `sp` moved only after, because popping first would
leave it unreachable across the allocation the operation itself makes.

**`Type` is a struct of slots, and a type is an object.** A built-in type is a
`TypeObj` wrapping the static `Type` its instances point at; a `class` carries
a `Type` of its own, and one made by a metaclass points at *that* metaclass's
slots, so `type(C) is M` falls out of the layout. **A slot cannot call
Python**, so a special method written in Python is not in the slot table:
`type_lookup` finds it and the VM makes the call at the opcode, which is the
one place a frame can be pushed.

**So anything that might call Python hands back a continuation.** An attribute
lookup reaching a `__get__` or a `__getattr__`, a comparison a sort has to
make, a finalizer the sweep owes: each is a state machine the VM drives, never
a nested interpreter.

**Exception state is per frame and there is no exception table.** A `try` is
`SetupFinally`/`PopBlock` over a block stack the frame carries, so unwinding is
popping blocks rather than searching a side table — which is what lets a
generator be suspended inside a `try` and resumed into it.

**The AST is an index arena.** Links are `u32` indices, not pointers, because
the arrays reallocate as the parse grows.

## Differences from CPython

Deliberate, and each is a decision rather than a gap. [Manual.md](Manual.md)
§10 has the user's view of the same list, with what is simply absent.

**The compiler and the parser**

- A `SyntaxError`'s message and column are this parser's; it raises where
  CPython raises, and says something else.
- The pieces of an f-string carry the whole literal's position.
- No `SyntaxWarning`: `x is "s"` and an invalid escape compile quietly.
- `from __future__ import` sets no flag in `co_flags`.
- A comprehension is a function of its own — PEP 709 inlined them in 3.12.
- A `+` before a number in a pattern is accepted, as CPython's main branch
  takes it.
- `--dump-tokens` prints a name in its NFKC form; the lexer normalizes as it
  scans.
- `dis` is this port's: the instruction stream is an opcode and a whole `u32`,
  so CPython's `Lib/dis.py` would read the wrong bytes, and `opcode` and
  `_opcode` do not exist.

**The object model**

- A set iterates in insertion order. CPython's order falls out of its hash
  table's probing, and matching it would mean copying that table.
- A finalizer runs when the collector gets to it, not when the last name goes.
- An instance used as a dict key hashes by identity unless it is unhashable:
  `py_hash` is C++ and cannot call a `__hash__` written in Python.
- `locals()` in a function is a fresh snapshot — PEP 667's behaviour.
- `object`'s defaults are skipped by the special-method lookup; they are there
  to be called by name.
- `dir()` names `__class__`, `__dict__` and `__weakref__` without their being
  entries.
- The method and descriptor types are one type.
- An operator dunder in a built-in type's namespace is a wrapper, not a slot
  wrapper.
- `hash()` is CPython's for a 32-bit `Py_hash_t`: numbers hash as their value
  modulo 2³¹ − 1, so `hash(Fraction(1, 2)) == hash(0.5)` holds.
- `gc` counts bytes, not generations. `memoryview` is flat.

**Where a slot cannot call Python**

- `map` and `filter` are eager, and a builtin handed a generator drains it
  first: `zip(g, [1, 2])` reads the whole of `g`.
- `itertools`' `takewhile`, `dropwhile`, `filterfalse`, `starmap`,
  `accumulate` and `groupby` do the same, in a continuation.
- `cmp_to_key`'s key is a class, so a sort compares through its methods.

**The library**

- A lock never waits — one thread, so nobody else can release a held one.
- A context is a dict, so `copy_context()` copies rather than shares.
- An async generator has no hooks, and `aclose()` throws `GeneratorExit` the
  way `close()` does rather than into what is awaited.
- asyncio has no streams, no subprocesses and no sockets; a loop with nothing
  to do raises rather than blocking, and it counts what it slept where the
  harness clock is frozen.
- A child is one spawn, not a fork and an exec, so `subprocess` has no
  `preexec_fn` and hands a child 0, 1 and 2 only, and a child killed by a
  signal returns 130 rather than `-9`. `select` waits over `Sys::Poll`, whose
  events are `POLLIN`, `POLLOUT` and `POLLHUP` and whose bound is 64
  descriptors a call.
- The four compression libraries are the SDK's, so what each writes is the
  real tool's bytes. lzma is where the 100 MB process shows: its default
  preset wants 94 MiB, which it gets when it asks first and not once the
  program holds much else, so a preset nobody named comes down until it fits
  and one you name is reported. `zipfile.ZIP_LZMA` names an 8 MiB dictionary
  and so cannot be used. zstd's dictionaries are content-only, `zdict.h`'s
  trainer not being in the library, so `dict_id` is 0.
- `email` is the whole package and pure Python, so the only thing it cannot do
  is the one this system has not got: a CJK charset has no codec.
- `_socket` is a floor with nothing under it. There are no sockets and will be
  none, so a `socket()` raises `OSError(EAFNOSUPPORT)`; the constants and the
  exception types are real, `gethostname` is `"localhost"`, and the byte-order
  and address-text calls are arithmetic and whole. It exists to be imported:
  `socket` carries `http.client`, `urllib.request` and so `xml.sax`, and it is
  what `pdb` and `doctest` reach through.
- `pyexpat` is libexpat rewritten, and it is a driver: expat suspends itself
  at a handler with `XML_StopParser` and a `ContObj` makes the Python call and
  resumes, because a native here may not call Python. `ElementTree`,
  `minidom`, `pulldom` and `xml.sax` all read through it.
- `sys.settrace` and `sys.setprofile` are the VM's, not a module's: a tracer
  is a Python call made from inside the instruction loop, so it is a pushed
  frame like any other and the loop resumes the instruction it was called
  from. Unwinding cannot make that call at all -- `dispatch` is plain C++ --
  so what each frame is owed is queued and fired at the next instruction
  boundary. Every code object begins with a `Nop`, CPython's `RESUME`, so
  that a frame that has not run has a position for a `call` event to report.
- A coroutine never awaited is reported when the collector finds it.
- `json` is the pure-Python one, so a malformed document is reported in
  `json.decoder`'s words.
- `errno`'s numbers are musl's. `_warnings` is the default filters and the
  lock; `warn` is `_py_warnings.py`'s.
- An `atexit` callback that raises is reported without a traceback, as every
  "Exception ignored" here is.
- There is no `site`, so no `help`, `exit`, `quit` or `copyright`.

**Text, codecs and regular expressions**

- Unicode is 16.0, which is 3.14's, and `unidata_version` says so.
- The streams are in UTF-8 mode: `surrogateescape` in, `backslashreplace` on
  stderr.
- The native codecs answer when `encodings` cannot be imported; a source whose
  cookie names a codec written in Python cannot start a program.
- A buffer is copied before it is matched, since a function `sub()` calls
  could resize it underneath.
- A regular expression can be interrupted: the engine parks every 2¹⁸ steps,
  which is where a `^C` is taken.

**The file system and the clock**

- There are no modes, owners, inodes or second links. A path's hash stands in
  for an inode, so `samefile` agrees with itself.
- The store keeps one time per file and can only move it to now, so the time
  `os.utime` is given is not kept.
- The local zone is the browser's offset and has no daylight-saving table.
  `time.time()` counts on from one wall reading taken before the program
  starts, with a monotonic clock that cannot name a day.

**Imports**

- A namespace package can shadow a module on a later path entry; CPython scans
  the whole of `sys.path` for a real module first.
- The loader is C++ until the program adds a finder. Importing `importlib`
  installs the three finders and the path hook and gives every module its
  `__spec__`; a finder the program adds sends every import after it through
  `importlib._bootstrap._find_and_load`.

## Files

| | |
| --- | --- |
| [braam.cpp](src/braam.cpp), [edit.cpp](src/edit.cpp) | The platform: the command line, the prompt, its line editor, and every `co_await` in the program |
| [value.h](src/value.h), [obj.cpp](src/obj.cpp), [type.cpp](src/type.cpp) | The value word, the object header, the type descriptor and its slots |
| [gc.cpp](src/gc.cpp), [intern.cpp](src/intern.cpp), [weak.cpp](src/weak.cpp) | The heap: allocation, precise mark and sweep, the pins, weak references |
| [err.cpp](src/err.cpp), [exc.cpp](src/exc.cpp), [egroup.cpp](src/egroup.cpp), [traceback.cpp](src/traceback.cpp) | The error channel — sticky, checked, not thrown — the exception hierarchy and the traceback |
| [int.cpp](src/int.cpp), [bigint.cpp](src/bigint.cpp), [float.cpp](src/float.cpp), [complex.cpp](src/complex.cpp), [ops.cpp](src/ops.cpp) | The number tower, integers of any width, and CPython's float repr |
| [str.cpp](src/str.cpp), [bytes.cpp](src/bytes.cpp), [ucd.cpp](src/ucd.cpp), [ucddb.cpp](src/ucddb.cpp) | Text in codepoints, octets, and the Unicode database `tools/mkucd.py` writes |
| [codec.cpp](src/codec.cpp), [codecsmod.cpp](src/codecsmod.cpp) | The codecs and their error handlers, as a run the program can interrupt |
| [tuple.cpp](src/tuple.cpp), [list.cpp](src/list.cpp), [table.cpp](src/table.cpp), [range.cpp](src/range.cpp), [iter.cpp](src/iter.cpp) | The containers, the insertion-ordered table behind dict and set, the iterators |
| [method.cpp](src/method.cpp), [strmeth.cpp](src/strmeth.cpp) … [slotmeth.cpp](src/slotmeth.cpp) | A static table becomes a built-in type's namespace; one file per family of methods |
| [format.cpp](src/format.cpp), [formatgr.cpp](src/formatgr.cpp), [repr.cpp](src/repr.cpp) | The format-spec mini-language, `%` and `str.format`, and repr for every type |
| [lex.cpp](src/lex.cpp), [parse.cpp](src/parse.cpp), [symtab.cpp](src/symtab.cpp), [compile.cpp](src/compile.cpp), [code.cpp](src/code.cpp) | Source to bytecode: the tokenizer, the grammar into an index arena, the scopes, the compiler, the code object |
| [astmod.cpp](src/astmod.cpp), [astpos.cpp](src/astpos.cpp), [astdump.cpp](src/astdump.cpp), [dis.cpp](src/dis.cpp) | `_ast` and the exact positions CPython reports, and the `--dump-ast` and `--dis` listings |
| [vm.cpp](src/vm.cpp), [frame.cpp](src/frame.cpp), [call.cpp](src/call.cpp), [gen.cpp](src/gen.cpp) | The dispatch loop, the frame, argument binding, the continuation a suspending builtin parks in, generators and coroutines |
| [attr.cpp](src/attr.cpp), [func.cpp](src/func.cpp), [abc.cpp](src/abc.cpp), [patma.cpp](src/patma.cpp), [compare.cpp](src/compare.cpp) | Attribute lookup, functions and classes, abstract bases, `match`, and comparisons that call Python |
| [annot.cpp](src/annot.cpp), [lazy.cpp](src/lazy.cpp), [typevar.cpp](src/typevar.cpp), [union.cpp](src/union.cpp), [genalias.cpp](src/genalias.cpp) | PEP 649 lazy annotations, PEP 810 lazy imports, and the typing machinery |
| [import.cpp](src/import.cpp), [module.cpp](src/module.cpp), [impmod.cpp](src/impmod.cpp) | The module cache, the search path, the loader, and where importlib takes over |
| [io.h](src/io.h), [iobase.cpp](src/iobase.cpp), [iofile.cpp](src/iofile.cpp), [iobuf.cpp](src/iobuf.cpp), [iotext.cpp](src/iotext.cpp), [iomem.cpp](src/iomem.cpp) | `_io`: the abstract layers, the raw descriptor, the buffer, the text wrapper, `BytesIO` and `StringIO` |
| [posixmod.cpp](src/posixmod.cpp), [sysmod.cpp](src/sysmod.cpp), [timemod.cpp](src/timemod.cpp), [signalmod.cpp](src/signalmod.cpp), [selectmod.cpp](src/selectmod.cpp), [socketmod.cpp](src/socketmod.cpp) | `posix` and `_posixsubprocess`, `sys`, `time`, `_signal`, `select` — the system-call turn every module takes — and `_socket`, which has no call to make |
| [zlibmod.cpp](src/zlibmod.cpp), [bz2mod.cpp](src/bz2mod.cpp), [lzmamod.cpp](src/lzmamod.cpp), [zstdmod.cpp](src/zstdmod.cpp) | `zlib`, `_bz2`, `_lzma` and `_zstd` over the SDK's four compression libraries |
| [reduce.cpp](src/reduce.cpp), [picklemod.cpp](src/picklemod.cpp) | What pickle and copy need of the native types: `__reduce__` for the builtins and iterators, and `_pickle`'s `PickleBuffer` |
| [sre.cpp](src/sre.cpp), [sremod.cpp](src/sremod.cpp) | The regular-expression engine, Secret Labs', able to stop mid-match |
| [expat/](src/expat/), [pyexpatmod.cpp](src/pyexpatmod.cpp) | libexpat 2.8.4 rewritten in C++, and `pyexpat` over it: a handler suspends the parse and a `ContObj` makes the Python call |
| [builtin.cpp](src/builtin.cpp), and the other `*mod.cpp` | The builtins namespace, and one file per native module |
| [lib/](lib/) | CPython's library, byte for byte, with [lib/manifest.txt](lib/manifest.txt) saying where each file came from |
| [Manual.md](Manual.md), [examples/](examples/) | The reference manual and three demos; the package ships both as `share/` |

## The tests

| | |
| --- | --- |
| [test/pylib.mjs](test/pylib.mjs) | The harness: boot, plant the binary and the library, run a command, read back what it wrote |
| [test/runcases.mjs](test/runcases.mjs) | MicroPython's suite — [test/cases/](test/cases/) against upstream's `.exp` files |
| [test/pycases.mjs](test/pycases.mjs) | CPython's own test files, under CPython's own `unittest` |
| [test/pystdlib.mjs](test/pystdlib.mjs) | [test/stdlib/](test/stdlib/) — programs whose output is identical to the host CPython's, line for line |
| [test/pylex.mjs](test/pylex.mjs), [test/pyast.mjs](test/pyast.mjs), [test/pydis.mjs](test/pydis.mjs) | The three listings, against CPython's own `tokenize` and `ast` and against goldens |
| [test/pysmoke.mjs](test/pysmoke.mjs), [test/pyflags.mjs](test/pyflags.mjs), [test/pyrepl.mjs](test/pyrepl.mjs), [test/pyexamples.mjs](test/pyexamples.mjs) | The command line, its options and the `PYTHON*` variables, the prompt, and the demos |
| [test/pyio.mjs](test/pyio.mjs), [test/pyselect.mjs](test/pyselect.mjs), [test/pyimport.mjs](test/pyimport.mjs), [test/pygc.mjs](test/pygc.mjs) | What needs a stream, a signal, a pipe to wait on, the import system or the collector |
| [test/pycompress.mjs](test/pycompress.mjs) | lzma and zstd, which the reference CPython was built without, and the three places compression differs |
| the other `test/py*.mjs` | One driver per area — types, numbers, functions, classes, generators, coroutines, formatting, Unicode, modules |
| [test/pystress.mjs](test/pystress.mjs) | The whole manifest again under `STRESS=1` |

To bring one more upstream test into the suite:

    tools/mkexp.py basics/andor.py

which copies it into `test/cases/`, writes the expected output beside it —
upstream's own `.exp` when there is one, host CPython otherwise — and adds a
row to the manifest marked `fail`. Move the row to `pass` when it passes.
`tools/mkcpy.py` does the same for one of CPython's.

## Rules a change follows

The repository's own conventions are in [CLAUDE.md](../../CLAUDE.md); these
three are this program's.

- Every test driver gets its own line in the `TESTS` variable at the head of
  the top [Makefile](../../Makefile).
- A test or a library module copied from upstream is copied **byte for byte**.
  Its provenance — the upstream path and the commit it came from — goes in the
  manifest beside it, never in the file. A module that needs something this
  interpreter has not got waits; it is not trimmed to fit.
- A golden is written by the CPython [tools/pyref.py](tools/pyref.py) finds —
  `$PYTHON`, or `python3` on `PATH` — and which one wrote it is recorded, in
  the manifest's `exp` column or in [test/goldens.txt](test/goldens.txt).
  Another interpreter does not overwrite it without `--regen`, so a host
  upgrade cannot move a golden by accident.

## Licence

The interpreter is this repository's, MIT, and so is the `test.support` under
`test/shim/`, which is not a copy of anyone's code.
[ucddb.cpp](src/ucddb.cpp) is generated from the Unicode Character Database,
under the Unicode licence. [sre.cpp](src/sre.cpp) and
[sremod.cpp](src/sremod.cpp) follow CPython's `_sre`, which Secret Labs wrote,
under CNRI's Python 1.6 licence and the PSF's. `lib/` and `test/cpython/` are
CPython's, under the PSF licence; `test/cases/` is MicroPython's, MIT.
[LICENSE](LICENSE) carries all of them and says which files each covers.
