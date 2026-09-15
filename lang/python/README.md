# python — Python 3, written for Braam

Not a port. Every other program in this tree is somebody else's source with the
lines that touch the OS replaced; this one is a Python implementation written
from nothing — its own lexer, its own parser, its own compiler, its own
bytecode and its own virtual machine.

One thing is borrowed, and it is the reason the rest can be trusted:
**MicroPython's test suite**. It is the best executable specification of the
language at this size, it is MIT, and a test in it either matches CPython byte
for byte or says in an `.exp` file what it expects instead. The copies that
`make test` runs are under [test/cases/](test/), with their provenance in
[test/manifest.txt](test/manifest.txt); the upstream clone they came from sits
in `tmp/` and is not committed.

```
$ python --version
Python 0.1 on Braam
```

## Status

**Phase 11.**

```
$ python -c 'print(sum([i * i for i in range(10)]))'
285
$ python -c 'print("-".join(sorted("the quick brown fox".split())))'
brown-fox-quick-the
$ cat greet.py
NAME = "world"
$ python -c 'import greet; print(greet.NAME, greet.__file__)'
world ./greet.py
$ python -c 'class P:
    def __init__(self, x): self.x = x
    def __repr__(self): return "P(" + str(self.x) + ")"
print(sorted([P(3), P(1)], key=lambda p: p.x))'
[P(1), P(3)]
```

Expressions, `if`, `while`, `for`, comprehensions, `def` and `lambda` with the
whole argument grammar, decorators, closures, `global`, `nonlocal` and `del`,
slicing, unpacking, forty-odd builtins, `class` with multiple inheritance, the
descriptor protocol and the special methods, the exception hierarchy with
`try`/`except`/`else`/`finally`, `with`, `raise … from` and exceptions of one's
own, `sys.argv`, `sys.exit`, a `^C` that becomes a catchable
`KeyboardInterrupt`, and a traceback on the way out.

The built-in types have their methods: str's forty-odd, bytes and a new
`bytearray`, `memoryview`, list, tuple, dict with its three views, set and a
new `frozenset`, and int and float. `divmod`, `round(x, n)`, `pow(a, b, m)`,
`reversed`, `zip`, `map` and `filter` came with them.

**A program can import another file**: modules and packages, `import a.b.c`,
`from x import y`, `as`, `from x import *`, relative imports, namespace
packages, `sys.modules`, `sys.path` and `__import__`. The shipped library will
live in the package's own `share/lib/`, which the binary finds through the
`/pkg/bin` link.

**284 of MicroPython's own tests pass unchanged**, and over the whole of
`tests/basics/` — setting aside the bigint, generator, async and t-string
families — **313 of 480**, up from 198 at phase 9. What stops most of the rest
is `str.format` and the `%` operator, which are the next phase; generators,
f-strings and bignums are the others.

`python --dump-tokens f.py`, `python --dump-ast f.py` and `python --dis f.py`
print what the lexer, the parser and the compiler produced; the first two are
checked against CPython's own `tokenize` and `ast` modules, and the third
against goldens of its own, the bytecode being ours.

[TODO.md](TODO.md) is the plan: the ground rules the design is pinned to, the
phases still to come, and the upstream tests each one is expected to turn
green.

## Files

| | |
| --- | --- |
| [braam.cpp](braam.cpp) | The platform. The command line, and every `co_await` in the program |
| [value.h](value.h) | A value in one 32-bit word: a 31-bit int, or a pointer |
| [obj.h](obj.h), [obj.cpp](obj.cpp) | The object header, the type descriptor and its slots, the singletons |
| [gc.h](gc.h), [gc.cpp](gc.cpp) | The object heap: allocation, precise mark and sweep, the pins |
| [intern.cpp](intern.cpp) | The intern table, which is a root |
| [err.h](err.h), [err.cpp](err.cpp) | The error channel: sticky, checked, not thrown |
| [ops.h](ops.h), [ops.cpp](ops.cpp) | The generic operations and the number tower |
| [int.cpp](int.cpp), [float.cpp](float.cpp) | The two number types, and CPython's float repr |
| [str.cpp](str.cpp), [bytes.cpp](bytes.cpp) | Text in codepoints, and octets both immutable and not |
| [tuple.cpp](tuple.cpp), [list.cpp](list.cpp) | The two sequences |
| [table.cpp](table.cpp) | The insertion-ordered table behind dict and set |
| [method.h](method.h), [method.cpp](method.cpp) | The method mechanism: a static table becomes a built-in type's namespace |
| [strmeth.cpp](strmeth.cpp) | str's methods |
| [bytemeth.cpp](bytemeth.cpp) | bytes', bytearray's and memoryview's |
| [seqmeth.cpp](seqmeth.cpp) | list's, tuple's and slice's |
| [mapmeth.cpp](mapmeth.cpp) | dict's and set's, frozenset, and the three views |
| [nummeth.cpp](nummeth.cpp) | int's and float's |
| [repr.cpp](repr.cpp) | repr for every type, quoting and all |
| [lex.h](lex.h), [lex.cpp](lex.cpp) | The tokenizer, and the `--dump-tokens` listing |
| [parse.h](parse.h), [parse.cpp](parse.cpp) | The grammar, by recursive descent into an index arena |
| [astdump.cpp](astdump.cpp) | The `--dump-ast` listing, which is the format mkast.py writes to |
| [code.h](code.h), [code.cpp](code.cpp) | The opcode table, the instruction, the code object, the line table |
| [symtab.h](symtab.h), [symtab.cpp](symtab.cpp) | The scope pass: local, cell, free or global |
| [compile.h](compile.h), [compile.cpp](compile.cpp) | The emitter, jump patching, and the blocks an exit unwinds |
| [dis.cpp](dis.cpp) | The `--dis` listing |
| [frame.h](frame.h), [frame.cpp](frame.cpp) | One activation: locals and the value stack in one block |
| [vm.h](vm.h), [vm.cpp](vm.cpp) | The dispatch loop, and the `Req` it hands the driver |
| [func.h](func.h), [func.cpp](func.cpp) | Cells, functions, builtins written in C++, and modules |
| [type.h](type.h), [type.cpp](type.cpp) | Type objects, instances, the MRO and the descriptors |
| [call.h](call.h), [call.cpp](call.cpp) | Argument binding, and the continuation a suspending builtin parks in |
| [exc.h](exc.h), [exc.cpp](exc.cpp) | The exception hierarchy, and the two objects it needs |
| [iter.h](iter.h), [iter.cpp](iter.cpp) | Slices, ranges and the three iterators |
| [builtin.h](builtin.h), [builtin.cpp](builtin.cpp) | The builtins namespace, `sys` and `builtins` |
| [import.h](import.h), [import.cpp](import.cpp) | The module cache, the search path, and the loader |
| [selftest.cpp](selftest.cpp) | What `--selftest` checks |
| [test/pylib.mjs](test/pylib.mjs) | The harness: boot, plant the binary, run a command, read back what it wrote |
| [test/pysmoke.mjs](test/pysmoke.mjs) | That the program starts, answers its flags, and reports the right status |
| [test/pygc.mjs](test/pygc.mjs) | Drives `--selftest` and reads what it printed |
| [test/pylex.mjs](test/pylex.mjs) | Every source under `test/lex/`, token for token |
| [test/pyast.mjs](test/pyast.mjs) | Every source under `test/ast/`, node for node |
| [test/pydis.mjs](test/pydis.mjs) | Every source under `test/dis/`, instruction for instruction |
| [test/pyvm.mjs](test/pyvm.mjs) | The driver: the three ways in, `sys.argv`, tracebacks, the collector under load |
| [test/pyfun.mjs](test/pyfun.mjs) | Calls: the callback rule at four thousand turns, decorators, a deleted cell |
| [test/pyclass.mjs](test/pyclass.mjs) | Classes: the diamond, the special methods, exceptions of one's own, all under gc stress |
| [test/pyint.mjs](test/pyint.mjs) | That a `^C` reaches a running program, and that it may catch it |
| [test/pymeth.mjs](test/pymeth.mjs) | Methods through a subclass, `sort(key=)`, the views, and the new types |
| [test/pyimport.mjs](test/pyimport.mjs) | Fifty nested imports, the cache, the search path, the store |
| [test/runcases.mjs](test/runcases.mjs) | Every case in the manifest, in one boot |
| [test/pystress.mjs](test/pystress.mjs) | Every case again, collecting at every allocation |
| [tools/mkexp.py](tools/mkexp.py) | Copies one upstream test in and writes its expected output |
| [tools/mklex.py](tools/mklex.py) | Writes a token golden out of CPython's own tokenizer |
| [tools/mkast.py](tools/mkast.py) | Writes a tree golden out of CPython's own ast module |

The table grows a row per phase.

## Why every root is explicit

Conservative stack scanning has no mechanism on this target: there is no
`__builtin_frame_address`, no exported stack base, and wasm keeps pointers in
locals that a scan of linear memory cannot see. So the collector is precise and
every root is named — and C++ that holds an object across an allocation has to
pin it:

    Root s{ obj_value(str_new("x")) };   // survives the tuple below
    TupleObj *t = tuple_new(1);

Forget the `Root` and the string is freed under you. `--selftest`'s `stress`
check is there to catch exactly that: it collects at *every* allocation, so a
missing pin becomes a wrong object count rather than a rare crash. A whole
Python program can be run the same way:

    PY_GC_STRESS=1 python prog.py

which is slow and is the point — [test/pyvm.mjs](test/pyvm.mjs) drives one
program through it every run.

Inside the VM the discipline takes a second form, because the value stack is
already a root: an operand is read *where it lies*, the result computed, and
only then is `sp` moved. Popping first and computing after would leave the
operands unreachable across the allocation that the operation itself makes.

## Why the bytecode has no exception table

CPython 3.11 moved exception handling to a side table and made the happy path
free. This does not: `SetupFinally` pushes a handler, `PopBlock` pops it, and
the VM cuts the value stack back to the depth the block recorded. The 3.10
shape is what a first VM can be written against and read, and it is what the
opcode comments in [code.h](code.h) state exactly. The block stack lives in the
frame, sized by a count the compiler worked out, so it is one allocation with
everything else.

What is *not* in the block stack is loops. A `break`, a `continue` or a
`return` that leaves a `try`/`finally` or a `with` emits that cleanup inline
before it jumps, which is what CPython has done since 3.9 — so the only thing
the VM has to unwind at run time is an exception.

That inlining is also where the hard cases are, and the tests upstream wrote
for them are unkind on purpose. Two rules make them come out:

- **A `finally` clause is emitted more than once**, and the copies do not start
  from the same stack. The exception copy has the exception on it, waiting for
  the `Reraise` at the end; a copy emitted on behalf of a `return` has the
  return value on it. So a `break` or a `continue` *out of* a clause has to
  drop whatever those copies are standing on — the compiler counts it.
- **An exit from inside a clause does not unwind that clause again.** Its
  handler has already been popped and its body is what is running, so the
  compiler marks it busy and skips it. Refusing this, which is what phase 5
  did, costs five of the `try_finally_*` tests.

## Why the exception state is per frame

`raise` with no arguments re-raises what the innermost `except` is handling,
and a function called from inside that clause can see it too. So the VM keeps
one "currently handling" value — but a frame that dies while unwinding must not
leave its own handler visible to whatever catches next. Each frame therefore
records what was being handled when it was *entered*, and returning or
unwinding past it puts that back. `try_reraise.py` is the test that says so:
without it, a bare `raise` at module level re-raises an exception a function
had finished with.

## Known differences from CPython

All recorded rather than hidden, and all in reach later:

- **A set iterates in insertion order.** CPython's order falls out of its hash
  table's size and probing, and matching it exactly would mean copying that
  table. Anything that prints a set directly will differ; anything that prints
  `sorted(s)` will not.
- **`repr` of a string keeps every codepoint from U+00A0 up as itself.**
  CPython escapes the ones Unicode calls unprintable, which needs a
  printability table this does not carry yet.
- **An identifier may hold any codepoint from U+0080 up.** CPython follows
  Unicode's XID_Start and XID_Continue, which is another table. So this accepts
  some names CPython rejects, and rejects none it accepts.
- **An f-string is one node holding its body as written.** What is inside the
  braces is parsed in a later phase; CPython builds a `JoinedStr` here, and the
  compiler refuses one rather than pretending.
- **An annotation is neither evaluated nor recorded.** `x: int = 1` compiles as
  `x = 1`, and a parameter annotation costs nothing at `def` time. CPython
  evaluates both and keeps `__annotations__`.
- **`async` is refused by the compiler.** The parser accepts the whole 3.9
  grammar; `async def`, `async for`, `async with` and `await` stop at the
  compiler with a `SyntaxError` that says so.
- **`str.format`, `format()` and `%` are not there.** They are one engine —
  the format-spec mini-language — and it is the next phase, together with
  f-strings.
- **A class repr has no module in it.** CPython prints
  `<class '__main__.C'>`; this prints `<class 'C'>`, there being one module.
- **`__set_name__` and `@` are not there.** The first is a descriptor hook the
  class body would have to call; the second is an operator the parser does not
  know.
- **`map` and `filter` are eager.** CPython calls the function at each `next`;
  these call it over the whole input first and hand back an iterator on the
  result. The two differ only where the input is endless or the function has an
  effect the program watches for. See below for why.
- **A sort, a `min` or an `index` cannot call a Python `__lt__` or `__eq__`.**
  The comparison happens inside C++, which cannot push a frame. So
  `[A(1), A(2)].sort()` on a class with `__lt__` raises `TypeError` where `a <
  b` on the same two works. `sorted`, `min` and `max` have had this since
  phase 8; `list.sort` and `list.index` join them.
- **A method that would need a bignum raises `OverflowError`.**
  `int.to_bytes` past eight octets, `int.from_bytes` of more than eight
  significant ones, and `float.as_integer_ratio` of most values. Phase 14.
- **`memoryview` is one octet wide and has no stride.** A slice of a step
  other than 1 raises `NotImplementedError`, and `itemsize` and `format` are
  not there; `array` is what would give them meaning, and it is phase 18.
- **`'ß'.isalpha()` is False.** The case table is by range and has no
  one-codepoint upper for it, so it is not counted as a letter. Phase 22
  replaces the ranges with the Unicode categories, and `isdecimal`,
  `isnumeric` and `casefold` stop being aliases of `isdigit` and `lower` at
  the same time.
- **A namespace package can shadow a module on a later path entry.** CPython
  scans the whole of sys.path for a real module before settling for a
  directory; this settles per entry, so `a/` on the first entry wins over
  `a.py` on the second.
- **There is no `__spec__`, no `importlib` and no import hook.** The loader is
  C++ and the only thing that finds a module. `sys.path_hooks`,
  `sys.meta_path`, `__loader__` and reloading are not there.
- **Nothing is cached in compiled form.** A module is parsed and compiled every
  time the program runs: 1.8 ms for 1,667 lines against 3.6 ms to start the
  process at all, measured under the harness, so a marshalled code object would
  buy little.
- **`python -m` is not there.** A program is a file, `-c` or stdin.
- **A traceback is a string, not an object.** It is collected as the frames go
  and printed at the end; there is no `__traceback__` to read.

## Why the VM is a driver

A `co_await` is a call and not a tail call: the wasm tail-call feature is off,
so entering a task and returning from it each leave a frame on the native
stack, given back only where something *suspends*. An interpreter loop that
awaited would therefore grow the stack until the process trapped.

So the VM is plain C++ that runs until it has something for its caller to do —
a write, a file to read, an exit — and returns saying what. Only
[braam.cpp](braam.cpp) awaits. It is the shape
[emulators/simbesm](../../emulators/simbesm/) arrived at for the same reason,
and it decides much else: a Python call pushes a frame rather than recursing,
and an error is a sticky flag unwound a frame at a time, because there is no
`setjmp` here either.

The whole of the driver is a dozen lines:

    for (;;) {
        Req r = vm_burst();
        if (r.kind == ReqKind::Exit)
            co_return r.status;
        if (r.kind == ReqKind::Read) {
            Result<String> got = co_await read_file(r.path);
            vm_read_done(got.is_ok(), false, got.value().str());
            continue;
        }
        vm_write_done(!(co_await write_all(r.fd, r.data)).is_err());
    }

`ReqKind::Read` is what an `import` runs on. The loader cannot read a file
inside an opcode, so it parks the continuation, the burst ends, the driver
reads, and the next burst resumes the step with the source — or with `None`,
which means try the next candidate. A name ending in `/` asks whether that is a
directory, which is how a namespace package is found.

`print` therefore does not write. It appends to a buffer the VM owns, and the
VM asks for one write when that buffer passes four kilobytes or the program
ends — which is also why a syscall per `print` never happens.

Because frames are heap objects rather than C++ ones, the recursion limit is
ours to choose and ours to *report*: two hundred deep is a `RecursionError`
with a traceback, not a trap.

## How a builtin calls back into Python

`sorted(xs, key=f)` is written in C++ and `f` is not, so somewhere the one has
to call the other — and the same rule that keeps the VM out of the native stack
keeps a builtin out of it. It **must not re-enter the dispatch loop**.

So it does not call `f` at all. It parks what it knows in a `ContObj`
([call.h](call.h)) and returns that as its result; the VM sees it, records the
continuation on the frame it pushes for `f`, and returns to the loop. `Return`
brings the answer back to `ContObj::step`, which asks for the next call or says
it is finished. Nothing nests, and four thousand key calls cost the native
stack exactly what one does — which is what
[test/pyfun.mjs](test/pyfun.mjs) measures.

It also decides how `sorted` is written: the keys are computed first, one
request each, and only then is the list sorted — decorate, sort, undecorate,
which is CPython's own answer. The sort itself calls nothing back, so it is an
ordinary iterative merge over an index array, stable and with no recursion on a
128 KiB stack.

It is also how a class works. A special method written in Python is a call, so
it cannot be in the slot table at all: the VM looks it up on the class and makes
the call **at the opcode**, which is the one place a frame can be pushed. So
`a + b` on a class, `obj[k]`, `for x in obj` and `print(obj)` are all
continuations, and a thousand of them cost the native stack what one does.

Two of them need more than a return value. `for x in obj` ends when `__next__`
raises `StopIteration`, which is control flow, not an answer — so a
continuation can name an exception it will **catch** (`ContObj::catching`), and
`dispatch` hands it back instead of unwinding past it. And `del obj[k]` wants
no answer at all, so a continuation can say its result is to be dropped.

The one place a special method is *not* reached at an opcode is inside a
container: `print([obj])` asks a list for its repr, and that repr is C++ all the
way down. So the objects that need Python are gathered out of the structure
first, rendered one call at a time, and put back — into a **copy**, with each
one replaced by the text it answered. The list the program holds is untouched,
and what gets printed is what CPython prints.

What the mechanism does *not* reach is a callback from inside the iterator
protocol. `py_next` returns a value, not a request, so `map` and `filter` —
whose function is called at each `next` — have nowhere to suspend. They are
eager instead: one continuation runs the function over the whole input, and
what comes back is an iterator over the list it built. The type is still `map`
or `filter`, so it is once-only and not a list; only the timing differs.

Nor does it reach a comparison. `list.sort()` merges inside C++, so a class
with `__lt__` cannot be sorted — the same limit `sorted`, `min` and `max` have
had since phase 8. A key function is fine, because a key is called from a loop
the continuation owns.

`import` is the one continuation that needs both halves. It asks the driver for
a file, and then asks the VM to run the module body — and a module that imports
a module that imports a module nests neither the native stack nor the driver.
[test/pyimport.mjs](test/pyimport.mjs) runs a chain of fifty.

## Testing

`make test` from the top of the tree runs everything; one file at a time:

    make test TESTS=lang/python/test/pysmoke.mjs

All three need node and a built `../braam-core`. To bring one more upstream
test into the suite:

    tools/mkexp.py basics/andor.py

which copies it into `test/cases/`, writes the expected output beside it —
upstream's own `.exp` when there is one, host CPython otherwise — and adds a
row to the manifest marked `fail`. Move the row to `pass` when it passes.

A new tokenizer or parser case is a `.py` under `test/lex/` or `test/ast/`
plus

    tools/mklex.py test/lex/<name>.py
    tools/mkast.py test/ast/<name>.py

which write the golden from CPython's own `tokenize` and `ast` modules, so both
are measured against CPython rather than against themselves. A case whose name
ends in `_err` is one that must be refused, and its golden holds the complaint;
`node test/pylex.mjs --bless` and `node test/pyast.mjs --bless` rewrite those,
after reading the diff.

A compiler case is a `.py` under `test/dis/` and nothing else: the bytecode is
this implementation's, so there is nothing to generate the golden from and
`node test/pydis.mjs --bless` writes what the compiler printed. **Read the diff
before blessing** — that golden is the only thing standing between a change and
a silent regression.

`PY_GC_STRESS=1` in a program's environment collects at every allocation, which
turns a missing `Root` from a rare crash into a wrong answer.
[test/pystress.mjs](test/pystress.mjs) runs the whole manifest that way and
compares; it costs a second and a half, and it found nine of them.

## Licence

The interpreter is this repository's. The tests under `test/cases/` are
MicroPython's, MIT, Damien P. George — see [LICENSE](LICENSE).
