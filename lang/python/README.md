# python — Python 3, written for Braam

Not a port. Every other program in this tree is somebody else's source with
the lines that touch the OS replaced; this one is a Python implementation
written from nothing — its own lexer, parser, compiler, bytecode and virtual
machine, 132k lines of C++. The standard library is CPython's, byte for byte,
over a floor of native modules written here.

```
$ python --version
Python 3.14.0 on Braam
$ python -c 'import json; print(json.dumps({"n": 2**100}))'
{"n": 1267650600228229401496703205376}
```

| | |
| --- | --- |
| [src/](src/) | the interpreter, and [src/expat/](src/expat/), the one thing here that is a port |
| [lib/](lib/) | CPython's library, 335 files, with [lib/manifest.txt](lib/manifest.txt) saying where each came from |
| [test/](test/) | the suite — [test/README.md](test/README.md) explains it |
| [examples/](examples/) | three demos; the package ships them and `Manual.md` as `share/` |
| [Manual.md](Manual.md) | the language: what runs, what does not, and why |
| [TODO.md](TODO.md) | what is known to be wrong, and what would fix it |

`make` at the top of the tree builds it, `make test` runs the suite.

## What the platform forces

Six rules, none of them style. Each follows from Braam, and getting one wrong
is a rewrite rather than a patch.

1. **The VM is a driver, not a coroutine.** Only [src/braam.cpp](src/braam.cpp)
   contains `co_await`. `vm_burst()` runs plain C++ until it has something for
   its caller to do — a read, a write, an open, a sleep, an exit — and
   returns a `Req` describing it; the driver performs it and answers. A
   `co_await` is a call and not a tail call, so an interpreter loop that
   awaited would grow the native stack until the process trapped.
   `emulators/simbesm` has the same shape for the same reason.
2. **The VM never recurses for a Python call.** A call pushes a frame and the
   dispatch loop continues. A C++ builtin that must call back into Python
   parks its state in a `ContObj` ([src/call.h](src/call.h)) and returns it;
   the VM drives the continuation and `Return` brings the answer back.
3. **There is no `longjmp`.** An error is a sticky pending exception plus a
   sentinel return, unwound a frame at a time. `editors/vi` and `lang/mbasic`
   arrived at the same answer.
4. **The parser is the only recursive thing**, on a 128 KiB native stack and
   with a nesting bound.
5. **Braam idiom, not a libc port.** No `PORT`: `kernel/vec.h`, `str.h`,
   `result.h`, `alloc.h` and the rest, plus `LIBS braam::math`. Never `new` —
   `heap_new` and `heap_delete`. Namespace-scope globals stay trivially
   destructible.
6. **Nothing lives in a coroutine frame.** Only `braam.cpp` has frames, and
   they hold a pointer to the interpreter state and nothing else.

The limits to build against: 100 MB of linear memory, a 128 KiB shadow stack,
`PROC_TASKS = 8`. The recursion limit is 200 because that is what the native
stack holds.

## The design

**`Value` is a 32-bit tagged word.** Bit 0 set means a 31-bit small integer;
otherwise it points at an `Obj`. `None`, `True` and `False` are static PODs.
An integer too big for the word is a `BigObj` of the same `int` type, and a
`BigObj` never holds what a small int could: every operation ends at
`big_make`, so two equal integers are always the same shape.

**`Obj` is sixteen bytes** — `{ type, next, grey, flags }`, the smallest size
class. `next` threads every live object onto one list, because
`kernel/alloc.h` has no heap iterator; `grey` threads the marker's worklist,
so marking neither allocates nor recurses.

**Collection is precise mark-and-sweep.** Conservative scanning has no
mechanism here: no `__builtin_frame_address`, no exported stack base, and wasm
keeps pointers in locals that a scan of linear memory cannot see. So every
root is named — the frame stack, each frame's locals and value stack, the
module globals, the intern table, the type objects, and an RAII `Root` chain
for C++ holding a value across an allocation:

    Root s{ obj_value(str_new("x")) };   // survives the tuple below
    TupleObj *t = tuple_new(1);

Forget the `Root` and the string is freed under you; `make test STRESS=1` is
what catches it. Inside the VM the same discipline takes a second form: an
operand is read *where it lies* and `sp` moved only after.

**`Type` is a struct of slots, and a type is an object.** A built-in type is a
`TypeObj` wrapping the static `Type` its instances point at; a `class` carries
a `Type` of its own, and one made by a metaclass points at that metaclass's
slots, so `type(C) is M` falls out of the layout.

**A slot cannot call Python**, which is rule 2 seen from the object model. A
special method written in Python is not in the slot table: `type_lookup` finds
it and the VM makes the call at the opcode, the one place a frame can be
pushed. Anything else that might call Python — a `__get__`, a comparison a
sort needs, a finalizer the sweep owes — hands back a continuation the VM
drives, never a nested interpreter.

**Exception state is per frame and there is no exception table.** A `try` is
`SetupFinally`/`PopBlock` over a block stack the frame carries, so unwinding
is popping blocks. That is what lets a generator suspend inside a `try` and
resume into it.

**The AST is an index arena**: links are `u32` indices, because the arrays
reallocate as the parse grows.

## The library

CPython's `Lib/` is taken verbatim — most of it is pure Python over a small C
floor, and that floor is what is written here. `re/` is 3,258 lines of Python
over an `_sre` whose Python-visible surface is a dozen names; `collections`,
`functools`, `heapq`, `json`, `datetime` and `decimal` each carry an
`except ImportError` fallback for the day their C accelerator is missing,
which is our day.

**The library decided the syntax.** It is written in the Python of its own
day: `dataclasses.py` has 92 f-strings and a `match` statement, `typing.py`
fifteen PEP 695 generics, `argparse.py` nine `lazy` imports. Each had to land
before the module that needed it could compile at all.

**The version is 3.14's**, which is what `sys.version_info` says, plus two
things from CPython's main branch that the library already uses: PEP 810's
`lazy` imports, and a `+` before a number in a pattern. The sources are
CPython at `82952e3` (3.16.0a0) and, for the tests, MicroPython at `52b5fbc`;
neither clone is committed, only what is taken from it.

## Differences from CPython

Each is a consequence of the design above, not a gap. [Manual.md](Manual.md)
§10 is the user's view of the same list, and [TODO.md](TODO.md) holds what is
merely unfinished.

**Because there is no refcount**

- A finalizer runs when the collector gets to it, not when the last name goes.
  So does the report for a coroutine that was never awaited.
- `id()` is an address and is reused after a collection.

**Because a slot cannot call Python**

- An instance used as a dict key or set member hashes by identity, even where
  its class writes `__hash__`.
- `map` and `filter` are eager, and a builtin handed a generator drains it
  first: `zip(g, [1, 2])` reads the whole of `g`. `itertools`' `takewhile`,
  `dropwhile`, `filterfalse`, `starmap`, `accumulate` and `groupby` do the
  same, in a continuation.
- `cmp_to_key`'s key is a class, so a sort compares through its methods.

**The compiler and the object model**

- A `SyntaxError` is raised where CPython raises one and says something else;
  there is no `SyntaxWarning` at all.
- A comprehension is a function of its own — PEP 709 inlined them in 3.12.
- `dis` is this implementation's: an instruction is an opcode and a whole
  `u32`, so CPython's `dis.py` would read the wrong bytes, and `opcode` and
  `_opcode` do not exist.
- A set iterates in insertion order; CPython's order falls out of its probing.
- `locals()` in a function is a fresh snapshot, PEP 667's behaviour.
- `hash()` is CPython's for a 32-bit `Py_hash_t`, so
  `hash(Fraction(1, 2)) == hash(0.5)` still holds.

**Because the platform has no such thing**

- One thread, so a lock never waits and `threading` is real but never
  contended.
- No sockets: `_socket` is constants, exception types and arithmetic, and
  exists to be imported, since `socket` carries `http.client`,
  `urllib.request` and `xml.sax`, which `pdb` and `doctest` reach through.
- A child is one spawn, not a fork and an exec: `subprocess` has no
  `preexec_fn`, hands a child 0, 1 and 2 only, and reports 130 for a killed
  one. `select` is `Sys::Poll` — `POLLIN`, `POLLOUT`, `POLLHUP`, 64
  descriptors a call. asyncio has no streams, subprocesses or sockets, and a
  loop with nothing to do raises rather than blocking.
- There are no modes, owners, inodes or second links; a path's hash stands in
  for an inode. The store keeps one time per file and can only move it to now.
- The local zone is the browser's offset, with no daylight-saving table.
- lzma's default preset wants 94 MiB of a 100 MB process, so a preset nobody
  named comes down until it fits and one you name is reported;
  `zipfile.ZIP_LZMA` is out of reach. zstd's dictionaries are content-only,
  so `dict_id` is 0. What the four compression modules write is otherwise the
  real tool's bytes, the SDK's libraries being the real ones.
- `email` is whole and pure Python; the one thing it cannot do is a CJK
  charset, which has no codec here.

**Written here rather than borrowed**

- `pyexpat` drives a rewritten libexpat: a handler suspends the parse with
  `XML_StopParser` and a `ContObj` makes the Python call. `ElementTree`,
  `minidom`, `pulldom` and `xml.sax` all read through it.
- `sys.settrace`, `sys.setprofile` and `sys.monitoring` are the VM's. A tracer
  is a pushed frame like any other call; what unwinding owes is queued and
  fired at the next instruction boundary, `dispatch` being plain C++. Every
  code object begins with a `Nop` — CPython's `RESUME` — so a frame that has
  not run has a position to report. `_lsprof` is told through a function
  pointer, so a profile costs no Python call per event.
- The loader is C++ until the program adds a finder. Importing `importlib`
  installs the three finders and the path hook and gives every module a
  `__spec__`; a finder added after that sends every later import through
  `importlib._bootstrap._find_and_load`.
- Unicode is 16.0, and the streams are in UTF-8 mode: `surrogateescape` in,
  `backslashreplace` on stderr. The regular-expression engine parks every 2¹⁸
  steps, which is where a `^C` is taken.

## Rules a change follows

[CLAUDE.md](../../CLAUDE.md) has the repository's conventions;
[test/README.md](test/README.md) has the suite's. This one is the program's:
**anything copied from upstream — a library module or a test — is copied
byte for byte**, its provenance recorded in the manifest beside it and never
in the file. A module that needs something this interpreter has not got waits; it is
not trimmed to fit.

## Licence

The interpreter is this repository's, MIT, and so is the `test.support` under
`test/shim/`. [src/ucddb.cpp](src/ucddb.cpp) is generated from the Unicode
Character Database, under the Unicode licence. [src/sre.cpp](src/sre.cpp) and
[src/sremod.cpp](src/sremod.cpp) follow CPython's `_sre`, which Secret Labs
wrote, under CNRI's Python 1.6 licence and the PSF's. `lib/` and
`test/cpython/` are CPython's, under the PSF licence; `test/cases/` is
MicroPython's, MIT. [LICENSE](LICENSE) carries all of them and says which
files each covers.
