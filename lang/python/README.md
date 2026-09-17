# python — Python 3, written for Braam

Not a port. Every other program in this tree is somebody else's source with the
lines that touch the OS replaced; this one is a Python implementation written
from nothing — its own lexer, its own parser, its own compiler, its own
bytecode and its own virtual machine.

What is borrowed is the measure. **MicroPython's test suite** is the best
executable specification of the language at this size, it is MIT, and a test in
it either matches CPython byte for byte or says in an `.exp` file what it
expects instead. The copies that `make test` runs are under
[test/cases/](test/), with their provenance in
[test/manifest.txt](test/manifest.txt).

**CPython's own `Lib/test/`** is the second and the unforgiving one, under
[test/cpython/](test/cpython/) with its rows in
[test/cpython.txt](test/cpython.txt). Those tests are not self-contained the
way MicroPython's are — every one of them imports `unittest` and most import
`test.support` — so [test/shim/](test/shim/) carries a `unittest` and a
`test.support` written against what this interpreter has, and the harness
plants them beside each case. The real `unittest` pulls in `asyncio`,
`logging`, `argparse` and `inspect`, and is a much later milestone.

Both upstream clones sit in `tmp/` and are not committed.

```
$ python --version
Python 0.1 on Braam
```

## Status

**Phase 22.**

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
$ python -c 'def fib():
    a, b = 0, 1
    while True:
        yield a
        a, b = b, a + b
g = fib()
print([next(g) for _ in range(10)])'
[0, 1, 1, 2, 3, 5, 8, 13, 21, 34]
$ python -c 'async def double(n): return 2 * n
async def main(): return [await double(n) for n in range(3)]
try: main().send(None)
except StopIteration as e: print(e.value)'
[0, 2, 4]
$ python -c 'import math, itertools as it
print(math.isqrt(10**20), math.comb(52, 5))
print(list(it.islice(it.count(10, 5), 4)))'
100000000000 2598960
[10, 15, 20, 25]
$ python -c 'def area(shape):
    match shape:
        case {"w": w, "h": h}: return w * h
        case [r] | (r,): return 3 * r * r
area2 = lambda s: t"{s!r} is {area(s)}"
print(area2({"w": 2, "h": 5}).values, int | None)'
({'w': 2, 'h': 5}, 10) int | None
$ python -c 'import unicodedata as u
print("stra\N{LATIN SMALL LETTER SHARP S}e".upper(), u.name("\u20ac"), int("\u0661\u0662"))
print("\u00e9t\u00e9".encode("utf-16-le"), u.normalize("NFD", "\u00e9") == "e\u0301")'
STRASSE EURO SIGN 12
b'\xe9\x00t\x00\xe9\x00' True
$ python -c 'import _contextvars as cv
v = cv.ContextVar("v", default=0)
class B:
    def hi(self): return "B"
class C(B):
    def hi(self): v.set(5); return "C>" + super().hi() + str(v.get())
print(cv.copy_context().run(C().hi), v.get(), range(1 << 70)[-1])'
C>B5 0 1180591620717411303423
```

Expressions, `if`, `while`, `for`, comprehensions, `def` and `lambda` with the
whole argument grammar, decorators, closures, `global`, `nonlocal` and `del`,
slicing, unpacking, forty-odd builtins, `class` with multiple inheritance,
metaclasses, the descriptor protocol and the special methods, the exception
hierarchy with
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

**Text can be formatted**: the format-spec mini-language — fill, align, sign,
`#`, `0`, width, grouping, precision and type — as one engine, reached by
`format()`, by `__format__`, by `str.format` and `str.format_map`, by `%` on
str and on bytes, and by **f-strings**, with conversions, `=`, and specs
nested inside specs. `ascii()` came with them.

**Integers have no width and `complex` exists**: a `Value` with bit 0 set is
still a 31-bit int and always will be, but what does not fit becomes a
[BigObj](bigint.h) whose type is the same `int`, so the difference is
invisible from Python. `2**1000`, `//` and `%` that floor, the bitwise
operators over infinite two's complement, `int(s, base)` at any width, a
division rounded once rather than three times, and `2j`.

**A program can compile and run more of itself**: `compile()` to a code
object in all three modes, `eval()` and `exec()` over source or over one, with
the globals and locals either may be handed; `globals()`, `locals()`, `vars()`,
`dir()` and `__builtins__`. A function says what it is — `__name__`,
`__qualname__`, `__doc__`, `__defaults__`, `__kwdefaults__`, `__globals__`,
`__closure__`, `__module__`, `__dict__` — and the last four of those may be
assigned to. Under it the code object is visible too, `co_varnames` through
`co_firstlineno`, and `type(f)(code, globals)` makes a new function out of one.

**A function that yields is a generator**: `yield`, `send`, `throw`, `close`,
`GeneratorExit`, `StopIteration.value`, PEP 479, `yield from` with send, throw
and close delegated through it, and generator expressions. A generator's frame
is the frame the VM already had. It is parked rather than popped, keeping its
value stack, its block stack and its locals, and resuming pushes it back on the
chain. Everything that takes an iterable — `list`, `sum`, `sorted`, `join`,
`[*g]`, `a, b = g`, `f(*g)` — meets one and parks; see below.

**A function can be a coroutine.** `async def`, `await`, `async for`,
`async with`, the async comprehensions and async generators, with the objects
they make: the coroutine and its `__await__` wrapper, and the awaitables an
async generator's `asend`, `athrow` and `aclose` answer. `aiter()` and
`anext()` came with them. There is no event loop yet — `asyncio` is phase 28
— so a program drives one with `send`, as the loop will; a generator
`types.coroutine` has marked may be awaited, and `code.replace()` is what
lets it mark one.

**The syntax is 3.14's.** `a @ b`, `match` with all seven pattern kinds and
guards, `except*` over `ExceptionGroup` with `split`, `subgroup` and `derive`,
PEP 695's `type X = ...` and `def f[T](...)` over a native `_typing`, `X | Y`
as a union, parenthesized `with` items, `:=` in a subscript, PEP 701's
f-strings that nest the same quote, and PEP 750's t-strings, which make a
`Template` of `Interpolation`s rather than a str. Two things are taken from
CPython's main branch as well, because the library is written against it:
**PEP 810's `lazy import`**, with `sys.lazy_modules`, the filter and the mode,
and a `+` before a number in a pattern. `sys.version_info` says 3.14.

**Text is Unicode's.** A str is codepoints over the Unicode 16.0 database
CPython 3.14 carries, generated into [ucddb.cpp](ucddb.cpp) and checked against
that CPython for every one of the 1,114,112 codepoints. `unicodedata` is
there in full but for `ucd_3_2_0`; `upper`, `lower`, `title`, `casefold`,
`capitalize` and `swapcase` take the full mappings, Final_Sigma included; the
`is*` predicates, `split()`, `strip()`, `splitlines()`, `repr` and `int()`
read Unicode's categories rather than a range table. A lone surrogate is a
character like any other until something encodes it, so `"\ud800"` is a
literal, `chr(0xdc80)` a str and `surrogateescape` a round trip.

**The codecs are CPython's.** `_codecs` is written natively -- UTF-8, UTF-7,
UTF-16, UTF-32, Latin-1, ASCII, the charmap and both escape codecs, every
built-in error handler and a registry -- and CPython's own `codecs.py` and 89
modules of its `encodings` package run over it, byte for byte. So
`"\u20ac".encode("cp1252")` goes through `encodings/cp1252.py`, a handler the
program registers is called in the middle of a codec, and a source file may
say `# -*- coding: latin-1 -*-`. Identifiers are XID_Start and XID_Continue,
their NFKC form is the name, and `\N{EM DASH}` is an escape.

**The type system is whole.** A metaclass decides what a `class` statement
makes, and `__prepare__`, `__new__`, `__init__` and the class keywords all
reach it; a class is an instance of its metaclass and answers as one.
**Anything with `__get__` is a descriptor**, anything that also has `__set__`
or `__delete__` comes *before* the instance namespace, and `property`,
`staticmethod`, `classmethod`, a function and a `__slots__` member are all
instances of that one rule. `__slots__` lays an instance out without a dict;
`__getattribute__`, `__setattr__` and `__delattr__` are hooks with `object`'s
own reachable through `super()`; and `__init_subclass__`, `__set_name__`,
`__class_getitem__`, `__mro_entries__`, `__instancecheck__` and
`__subclasscheck__` all run where CPython runs them.

**An object can be finalized.** `__del__` and `_weakref.ref` with callbacks
make the collector's sweep observable, so the sweep states a rule: a weak
reference to something about to go is cleared before any finalizer sees it,
every finalizer is owed exactly once, and owing one keeps the object — and
everything it reaches — alive for that cycle, which is what makes resurrection
mean something. It is also what closes a generator dropped at a `yield`, so
its `finally` runs.

**A comparison can be made from C++.** `py_cmp` cannot push a frame, so a sort,
a fold or a search over things that compare in Python is a *continuation that
owns the loop*: the merge, the scan and the item-by-item sequence compare are
state machines that ask for one call at a time. `sorted`, `list.sort`, `min`,
`max`, `index`, `count`, `remove`, `in` and `==` between two sequences all go
through it, and the merge is the same bottom-up stable one as the plain path,
so a list of instances and a list of integers come out in the same order.

**Twenty-two modules are written in C++.** `sys` in full, `builtins`,
`_collections`, `_functools`, `itertools`, `operator`, `_random`, `_struct`,
`array`, `math`, `cmath`, `time`, `errno`, `gc`, `_types`, the `_weakref` and
`_abc` phase 17 wrote, the `_typing` phase 20 did, phase 21's `_codecs`
and `unicodedata`, and phase 22's `_thread`, `_contextvars` and `_string`.
They are the floor
CPython's own library stands on rather than that library:
`collections/__init__.py` will import this `deque`, `random.py` this Mersenne
Twister, `re/` the `_sre` phase 24 writes. Each is measured against CPython by
running the same program under both — [test/module/](test/module/), nineteen
cases, 633 lines byte for byte.

**The floor under the first wave is down.** Phase 22 wrote what CPython's
pure-Python modules were found to import and this did not have: `_thread`'s
locks and `_local` for a process with one thread, `_contextvars`, `_string`'s
view of the format grammar, `_weakref.proxy`, `_functools.Placeholder` and
`cmp_to_key`, a `range` of any width with CPython's two iterators, and a
function frame's `f_locals` as a `FrameLocalsProxy` that writes through to the
slots. **The implicit `__class__` cell is made**: a class whose methods name
`__class__` or call `super()` keeps one, the body hands it to `type.__new__`
as `__classcell__`, and zero-argument `super()` reads it rather than searching
the MRO. `reprlib`, `string`, `string.templatelib`, `numbers`, `heapq`,
`bisect`, `copyreg`, `keyword`, `linecache`, `types` and `_weakrefset` import
unchanged; `_collections_abc.py` now reads past `framelocalsproxy` and
`longrange_iterator` and stops at CPython main's new `frozendict`, which is
phase 23's first job.

**The protocol methods are in each built-in type's namespace.** `len(x)`
reaches a slot and a slot is not an entry, so `'__len__' in list.__dict__` used
to be False and `dir(list)` listed none of them; every abstract base class in
`collections.abc` decides membership by looking exactly those names up. Each is
now a small native over the generic operation, installed from the slots the
type fills, and `dict.__getitem__` is where a subclass's `__missing__` is
finally consulted.

**`list[int]` is a value.** The generic alias is the rest of PEP 560 beside
the `__class_getitem__` phase 17 wrote: `__origin__`, `__args__`,
`__mro_entries__` so `class C(list[int])` derives from `list`, and a call that
is the class's. `types.GenericAlias` is `type(list[int])`, and `_types` is the
native floor CPython's `types.py` opens by importing.

**CPython's own `abc.py`, `codecs.py` and `encodings` run here, byte for
byte**, over an `_abc` written
natively — the first module borrowed from the library rather than written.
[lib/manifest.txt](lib/manifest.txt) records where it came from; the floor
under it is `_abc_init`, `_abc_register`, `_abc_instancecheck`,
`_abc_subclasscheck` and the cache token, plus `type.mro`,
`type.__subclasses__` and the rule that an abstract class cannot be
instantiated.

**412 of MicroPython's own tests pass unchanged**, out of 449 in
[test/manifest.txt](test/manifest.txt), as at phase 20. Of the thirty-seven
that do not, most import `collections` or `struct` — the pure-Python wrappers
over phase 18's floor, which are phase 23 — and say `SKIP` until then;
`assign_expr_syntaxerror.py` expects what MicroPython accepts and CPython
refuses, and this refuses it. Every expected output a CPython wrote now comes from 3.14,
and [test/goldens.txt](test/goldens.txt) says which interpreter wrote each of
our own.

**CPython's tests are the second ruler.** Twenty-three are in
[test/cpython.txt](test/cpython.txt), fourteen of them run, and 171 test
methods of 204 pass, as at phase 21, when `test_unicode_identifiers.py` and
`test_utf8source.py` came to pass whole. The nine rows that do not run all
compile and stop at an import: `annotationlib`, `re`, `collections`,
`textwrap`, `pickle`, and `copy` for `test_super.py`, phase 22's, which the
`__class__` cell let through the compiler. Running the whole of `Lib/test/`
under this interpreter — `node test/pycases.mjs --survey`, which needs the
clone in `tmp/` — says why each of the 391 files stops:

| now | what stops it | 21 | 20 | 19 | 18 | 17 | 16 | 14 | 13 | 12 | lands in |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 373 | a module that is not written yet | 372 | 325 | 286 | 272 | 276 | 276 | 276 | 213 | 130 | phases 23–27 |
| 14 | these run | 14 | 14 | 11 | 11 | 9 | 7 | 7 | 5 | 3 | |
| 3 | a runtime error, or nothing this can read | 3 | 3 | 3 | 3 | 1 | 3 | 3 | 3 | 2 | |
| 1 | other syntax — PEP 798 | 2 | 2 | 44 | 43 | 43 | 43 | 43 | 34 | 28 | see TODO.md |
| — | a lone surrogate in a literal | — | 33 | 33 | 33 | 33 | 33 | 33 | 31 | 31 | **done** |
| — | `\N{...}` | — | 14 | 14 | 14 | 14 | 14 | 14 | 12 | 12 | **done** |
| — | `async` | — | — | — | 15 | 15 | 15 | 15 | 13 | 3 | **done** |
| — | complex numbers | — | — | — | — | — | — | — | 41 | 41 | **done** |
| — | an integer past 2³⁰ | — | — | — | — | — | — | — | 39 | 17 | **done** |
| — | f-strings | — | — | — | — | — | — | — | — | 124 | **done** |

Phase 22 moved one file: `test_super.py` compiles now that the implicit
`__class__` cell is made, and stops at `import copy`. The sixth wall went the
way the fifth did: all forty-seven files the Unicode
literals held back now stop at an import, `test_fstring.py` among them once
whitespace after `!s` was accepted. Before that, the fifth: of the forty-two
files the syntax let go, three ran and thirty-nine stopped at an import. The
one left is `test_listcomps.py`, which is written to PEP 798's
`[*x for x in y]` from 3.15. **Nothing the library is written in is refused
now**, so what stops 373 of the 391 files is the library itself, and with its
floor written the phases after this one are about taking it.

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
| [bigint.h](bigint.h), [bigint.cpp](bigint.cpp) | Integers past the value word: limbs, long division, and the whole integer arm |
| [complex.h](complex.h), [complex.cpp](complex.cpp) | complex, and the one type in the tower with no order |
| [str.cpp](str.cpp), [bytes.cpp](bytes.cpp) | Text in codepoints, and octets both immutable and not |
| [ustr.h](ustr.h) | A str's bytes: UTF-8 with the surrogates in it |
| [ucd.h](ucd.h), [ucd.cpp](ucd.cpp) | The Unicode database: properties, case, decomposition, normalization, names |
| [ucddb.h](ucddb.h), [ucddb.cpp](ucddb.cpp) | Its tables, which tools/mkucd.py writes |
| [codec.h](codec.h), [codec.cpp](codec.cpp) | The codecs and the error handlers, as a run a handler of the program's own can interrupt |
| [codecsmod.cpp](codecsmod.cpp) | `_codecs`: the registry, and a function per codec |
| [unimod.cpp](unimod.cpp) | `unicodedata` |
| [tuple.cpp](tuple.cpp), [list.cpp](list.cpp) | The two sequences |
| [table.cpp](table.cpp) | The insertion-ordered table behind dict and set |
| [method.h](method.h), [method.cpp](method.cpp) | The method mechanism: a static table becomes a built-in type's namespace |
| [strmeth.cpp](strmeth.cpp) | str's methods |
| [bytemeth.cpp](bytemeth.cpp) | bytes', bytearray's and memoryview's |
| [seqmeth.cpp](seqmeth.cpp) | list's, tuple's and slice's |
| [mapmeth.cpp](mapmeth.cpp) | dict's and set's, frozenset, and the three views |
| [nummeth.cpp](nummeth.cpp) | int's and float's |
| [repr.cpp](repr.cpp) | repr for every type, quoting and all |
| [format.h](format.h), [format.cpp](format.cpp) | The format-spec mini-language, and what str, int and float make of one |
| [formatgr.cpp](formatgr.cpp) | The other two grammars — `%` and str.format's fields — as a plan a continuation walks |
| [lex.h](lex.h), [lex.cpp](lex.cpp) | The tokenizer, PEP 263's source encodings, and the `--dump-tokens` listing |
| [parse.h](parse.h), [parse.cpp](parse.cpp) | The grammar, by recursive descent into an index arena |
| [astdump.cpp](astdump.cpp) | The `--dump-ast` listing, which is the format mkast.py writes to |
| [code.h](code.h), [code.cpp](code.cpp) | The opcode table, the instruction, the code object, the line table |
| [symtab.h](symtab.h), [symtab.cpp](symtab.cpp) | The scope pass: local, cell, free or global |
| [compile.h](compile.h), [compile.cpp](compile.cpp) | The emitter, jump patching, and the blocks an exit unwinds |
| [dis.cpp](dis.cpp) | The `--dis` listing |
| [frame.h](frame.h), [frame.cpp](frame.cpp) | One activation: locals and the value stack in one block |
| [flocals.cpp](flocals.cpp) | `FrameLocalsProxy`: a function frame's `f_locals`, written through to its slots |
| [gen.h](gen.h), [gen.cpp](gen.cpp) | The generator, the coroutine and the async generator — a frame parked rather than popped — and the awaitables that step one |
| [vm.h](vm.h), [vm.cpp](vm.cpp) | The dispatch loop, and the `Req` it hands the driver |
| [func.h](func.h), [func.cpp](func.cpp) | Cells, functions, builtins written in C++, and modules |
| [type.h](type.h), [type.cpp](type.cpp) | Type objects, instances, the MRO, the metaclasses and the class hooks |
| [attr.cpp](attr.cpp) | The descriptor protocol, the attribute algorithm and `__slots__` |
| [compare.h](compare.h), [compare.cpp](compare.cpp) | The sorts and searches that have to call Python, as continuations |
| [weak.h](weak.h), [weak.cpp](weak.cpp) | Weak references and proxies, and the callbacks the sweep owes for them |
| [abc.h](abc.h), [abc.cpp](abc.cpp) | `_abc`: the floor CPython's own abc.py stands on |
| [genalias.h](genalias.h), [genalias.cpp](genalias.cpp) | `list[int]`: the generic alias, and PEP 560's other half |
| [union.h](union.h), [union.cpp](union.cpp) | `int \| str`: the union, which is `typing.Union` and `types.UnionType` both |
| [patma.h](patma.h), [patma.cpp](patma.cpp) | What `match` asks of a subject: sequence or mapping, keys, and a class pattern's attributes |
| [egroup.h](egroup.h), [egroup.cpp](egroup.cpp) | The exception groups: `split`, `subgroup` and `derive`, and what `except*` does with them |
| [typevar.h](typevar.h), [typevar.cpp](typevar.cpp) | `_typing`: PEP 695's type parameters, the alias, `Generic`, and the intrinsics the compiler emits |
| [lazy.h](lazy.h), [lazy.cpp](lazy.cpp) | PEP 810: the proxy a lazy import binds, and what resolves it |
| [templatelib.h](templatelib.h), [templatelib.cpp](templatelib.cpp) | PEP 750: `Template` and `Interpolation`, what a t-string makes |
| [slotmeth.cpp](slotmeth.cpp) | The protocol methods in each built-in type's namespace |
| [info.h](info.h), [info.cpp](info.cpp) | The struct sequence: a tuple whose fields also have names |
| [binfmt.h](binfmt.h), [binfmt.cpp](binfmt.cpp) | One typecode's machine representation, which array and memoryview share |
| [module.h](module.h), [module.cpp](module.cpp) | The registry of modules written in C++, and the helpers each installer uses |
| [sysmod.cpp](sysmod.cpp) | `sys`: the three streams, the named tuples, and the interpreter looking at itself |
| [mathmod.cpp](mathmod.cpp) | `math` and `cmath` over braam::math, with the exact half over the bignum |
| [itermod.cpp](itermod.cpp) | `itertools`: the lazy half, and the eager half that has to call |
| [opmod.cpp](opmod.cpp) | `operator`, attrgetter, itemgetter and methodcaller |
| [collmod.cpp](collmod.cpp) | `_collections`: the deque ring, defaultdict and OrderedDict |
| [functoolsmod.cpp](functoolsmod.cpp) | `_functools`: reduce, partial and Placeholder, cmp_to_key and the lru_cache wrapper |
| [thread.cpp](thread.cpp) | `_thread`: the locks and `_local`, for one thread |
| [ctxvars.cpp](ctxvars.cpp) | `_contextvars`: ContextVar, Token and Context |
| [stringmod.cpp](stringmod.cpp) | `_string`: str.format's grammar as `string.Formatter` reads it |
| [structmod.cpp](structmod.cpp) | `_struct`: the format language, and values to octets |
| [arraymod.cpp](arraymod.cpp) | `array`: the only thing here that gives a buffer a width |
| [randmod.cpp](randmod.cpp) | `_random`: MT19937, seeded the way CPython seeds it |
| [miscmod.cpp](miscmod.cpp) | `time` over one clock reading, `errno` and `gc` |
| [typesmod.cpp](typesmod.cpp) | `_types`: the names for types that are not builtins, and SimpleNamespace |
| [lib/](lib/) | Modules taken from CPython's library, byte for byte: `abc`, `codecs` and `encodings` |
| [call.h](call.h), [call.cpp](call.cpp) | Argument binding, and the continuation a suspending builtin parks in |
| [exc.h](exc.h), [exc.cpp](exc.cpp) | The exception hierarchy, and the two objects it needs |
| [iter.h](iter.h), [iter.cpp](iter.cpp) | Slices and the iterators |
| [range.cpp](range.cpp) | range at any width, and its two iterators |
| [builtin.h](builtin.h), [builtin.cpp](builtin.cpp) | The builtins namespace, and `builtins` as a module |
| [import.h](import.h), [import.cpp](import.cpp) | The module cache, the search path, and the loader |
| [selftest.cpp](selftest.cpp) | What `--selftest` checks |
| [test/pylib.mjs](test/pylib.mjs) | The harness: boot, plant the binary, run a command, read back what it wrote |
| [test/pysmoke.mjs](test/pysmoke.mjs) | That the program starts, answers its flags, and reports the right status |
| [test/pygc.mjs](test/pygc.mjs) | Drives `--selftest` and reads what it printed |
| [test/pytype.mjs](test/pytype.mjs) | The descriptors, `__slots__`, the metaclasses, the finalizers and abc |
| [test/pylex.mjs](test/pylex.mjs) | Every source under `test/lex/`, token for token |
| [test/pyast.mjs](test/pyast.mjs) | Every source under `test/ast/`, node for node |
| [test/pydis.mjs](test/pydis.mjs) | Every source under `test/dis/`, instruction for instruction |
| [test/pyvm.mjs](test/pyvm.mjs) | The driver: the three ways in, `sys.argv`, tracebacks, the collector under load |
| [test/pyfun.mjs](test/pyfun.mjs) | Calls: the callback rule at four thousand turns, decorators, a deleted cell |
| [test/pyclass.mjs](test/pyclass.mjs) | Classes: the diamond, the special methods, exceptions of one's own, all under gc stress |
| [test/pyint.mjs](test/pyint.mjs) | That a `^C` reaches a running program, and that it may catch it |
| [test/pymeth.mjs](test/pymeth.mjs) | Methods through a subclass, `sort(key=)`, the views, and the new types |
| [test/pyimport.mjs](test/pyimport.mjs) | Fifty nested imports, the cache, the search path, the store |
| [test/pyformat.mjs](test/pyformat.mjs) | Every case under `test/format/`, against CPython and again under gc stress |
| [test/pynumber.mjs](test/pynumber.mjs) | The same for `test/number/`: the arithmetic that has one right answer |
| [test/pygen.mjs](test/pygen.mjs) | The same for `test/gen/`: the generator protocol, delegation, and every consumer |
| [test/pycoro.mjs](test/pycoro.mjs) | The same for `test/coro/`: coroutines, async generators, the async statements and a scheduler written in Python |
| [test/pyexec.mjs](test/pyexec.mjs) | The same for `test/exec/`: compile, eval, exec, the namespaces, the attributes and the syntax since 3.9 |
| [test/pylazy.mjs](test/pylazy.mjs) | The same for `test/lazy/`, against CPython 3.16, with the modules the cases import planted beside them |
| [test/pymodule.mjs](test/pymodule.mjs) | The same for `test/module/`: the modules written in C++ |
| [test/pyunicode.mjs](test/pyunicode.mjs) | The same for `test/unicode/`, with the library planted; the streams; and `--full`, every codepoint and NormalizationTest.txt |
| [test/pyunit.mjs](test/pyunit.mjs) | The shims, before anything stands on them: one of every outcome |
| [test/runcases.mjs](test/runcases.mjs) | Every case in the manifest, in one boot |
| [test/pycases.mjs](test/pycases.mjs) | Every CPython test in `cpython.txt`, and `--survey` over the whole clone |
| [test/pystress.mjs](test/pystress.mjs) | Every case again, collecting at every allocation |
| [test/shim/unittest.py](test/shim/unittest.py) | `TestCase`, the assertions, `subTest`, the skips and the loader |
| [test/shim/test/support.py](test/shim/test/support.py) | The names CPython's tests take from `test.support` |
| [test/shim/selfcheck.py](test/shim/selfcheck.py) | What `pyunit.mjs` runs: the shims measured against themselves |
| [tools/pyref.py](tools/pyref.py) | Which CPython writes a golden, and the record in `test/goldens.txt` of which one did |
| [tools/mkexp.py](tools/mkexp.py) | Copies one upstream test in and writes its expected output |
| [tools/mkcpy.py](tools/mkcpy.py) | Copies one of CPython's tests in; `pycases.mjs --bless` writes its golden |
| [tools/mkfmt.py](tools/mkfmt.py) | Runs a formatting case under the host's CPython and saves what it printed |
| [tools/mklex.py](tools/mklex.py) | Writes a token golden out of CPython's own tokenizer |
| [tools/mkast.py](tools/mkast.py) | Writes a tree golden out of CPython's own ast module |
| [tools/mkucd.py](tools/mkucd.py) | Writes ucddb.cpp from the host CPython's unicodedata and the UCD files it cannot list |

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
- **An annotation is neither evaluated nor recorded.** `x: int = 1` compiles as
  `x = 1`, and a parameter annotation costs nothing at `def` time. CPython
  evaluates both and keeps `__annotations__`.
- **`locals()` in a function is a fresh snapshot every time.** A function's
  locals are frame slots, so the mapping is built from them on the spot: two
  calls are two dicts, and what `exec("x = 1")` writes into one is dropped.
  This is what PEP 667 made CPython do in 3.13; 3.12 and before cached one dict
  on the frame, so `exec` there left the name findable through `locals()` and
  nowhere else.
- **A `single`-mode code object prints through no hook.** `PrintExpr` writes
  the repr itself; CPython calls `sys.displayhook` and sets `builtins._`, and
  both wait for the REPL in phase 29.
- **A coroutine that is never awaited says nothing.** CPython warns, with a
  `RuntimeWarning` naming the line that made it, when one is collected
  without having started; there is no `warnings` module yet to say it
  through, which is phase 23.
- **An async generator has no hooks.** `sys.set_asyncgen_hooks` is not there,
  so nothing is told when one starts or is dropped; one dropped while
  suspended is closed by the collector, as a generator is. The hooks are for
  an event loop, which is phase 28.
- **`aclose()` throws GeneratorExit the way `close()` does.** When an async
  generator is parked in an await, CPython throws it through into whatever is
  awaited; this closes that first, as for a generator's `yield from`.
- **A class repr has no module in it.** CPython prints
  `<class '__main__.C'>`; this prints `<class 'C'>`, there being one module.
- **A comprehension is a function of its own.** PEP 709 inlined them in 3.12;
  here `[x for x in y]` still pushes a frame, which shows in a traceback and in
  `locals()` inside one, and nowhere else. Zero-argument `super()` in a list,
  set or dict comprehension reads the frame that called it, which is the
  function it was written in, so that works as it does in CPython.
- **A union or a `Generic` does not take a string.** CPython hands a forward
  reference to `typing.py`, which makes a `ForwardRef` of it; there is no
  `typing.py` until phase 27, so `int | "C"` is refused.
- **A `+` before a number in a pattern is accepted.** 3.14 refuses
  `case +0:`; CPython's main branch takes it, and `test_patma.py` is written
  against that.
- **`map` and `filter` are eager.** CPython calls the function at each `next`;
  these call it over the whole input first and hand back an iterator on the
  result. The two differ only where the input is endless or the function has an
  effect the program watches for. See below for why.
- **A builtin handed a generator drains it first.** `zip(g, [1, 2])` reads the
  whole of `g` before it pairs anything, where CPython reads two items. Same
  reason as `map`: the builtin cannot step a generator, so it parks and the VM
  drains it into a list. Nothing differs unless the generator is endless or its
  effects are watched for. `for x in g` and `yield from g` are lazy, because
  those are opcodes and an opcode can suspend; so are `any` and `all`, which
  step the generator one item at a time and stop at the first that decides.
- **A finalizer runs when the collector gets to it, not when the last name
  goes.** CPython counts references, so `c = None` on the last one runs
  `__del__` there and then; this collects on allocation pressure, so a `__del__`
  and a weak reference's callback happen at the next collection and then at the
  next opcode. What runs is Python's; *when* is this port's, and a program that
  needs a finalizer at a point should use `with` instead. The rules hold either
  way: one call per object ever, a resurrected object never finalized again,
  weak references cleared before any finalizer can see them, and an exception
  out of one reported and ignored.
- **An instance used as a dict key hashes by identity unless it is
  unhashable.** `py_hash` is C++ and cannot call a `__hash__` written in
  Python. A class that writes `__eq__` and no `__hash__` is unhashable, which
  is CPython's rule and takes the common case out of harm's way; a class that
  writes both gets a dict that agrees with `is` rather than with `==`.
  `hash(x)` itself is an opcode away from a frame and does call it.
- **An operator dunder in a built-in type's namespace is a wrapper, not a slot
  wrapper.** `list.__add__` is a native over the generic operation and is
  installed only where the type really answers that operator, because
  `Type::binop` is one slot for all twelve and cannot say which. So
  `hasattr(list, '__and__')` is False here and True in CPython, where every
  type inherits a stub from `object` that returns NotImplemented; the same
  goes for `dict.__lt__`. What is asked of these names —
  `collections.abc`'s `__subclasshook__` — reads the ones that mean something,
  and those are exact.
- **`itertools` calls a function over the whole input first.** `takewhile`,
  `dropwhile`, `filterfalse`, `starmap`, `accumulate` and `groupby` park in a
  continuation, run every item through the function and hand back an iterator
  over what they kept. Same reason as `map`: a builtin cannot call Python. The
  lazy ones — `count`, `cycle`, `repeat`, `chain`, `compress`, `islice`,
  `pairwise`, `zip_longest`, `product` and the combinatorics — are lazy, so
  `islice(count(), 5)` is fine and `takewhile(p, count())` is not.
  `groupby`'s group is a list already built rather than CPython's shared
  iterator, which makes it *more* usable: it survives the next key.
- **A lock never waits.** There is one thread, so nobody else can release a
  held lock: `acquire` with a timeout, or without blocking, answers False at
  once, and a blocking `acquire` with no timeout raises `RuntimeError` where
  CPython would hang for ever. `start_new_thread` raises too, and
  `get_ident()` is the process id.
- **A context is a dict.** CPython keeps a `Context`'s variables in an
  immutable map, so `copy_context()` costs nothing; here it copies them. What
  a program sees is the same.
- **`cmp_to_key`'s key is a class.** A sort compares keys through their
  methods, since a slot cannot call the program's function, so `KeyWrapper` is
  a class whose `__lt__` and the rest are natives. `type(K)` is that class,
  and like any class here its repr has no module in it.
- **`print` writes its line in one call.** CPython calls `sys.stdout.write`
  once per argument and separator; this builds the line and writes it once. A
  program that has put an object of its own in `sys.stdout` sees one call with
  the whole line in it.
- **A module's `real` and `imag` are not on `int` and `float`.** `(3).real`
  raises; `complex` has both. Nothing here needs them until the numbers tower
  in `numbers.py`.
- **`_random` seeds a str differently.** An integer seed is spread with
  CPython's own `init_by_array`, so `seed(42)` gives CPython's stream word for
  word; a str or bytes seed is hashed here and put through `sha512` there, and
  `random.py` never reaches this path because it does that conversion itself.
- **`errno`'s numbers are musl's.** They are Linux's, which is the dialect the
  port kit's `<errno.h>` already uses and the one a modern port's `#ifdef`
  ladder is written against. A host whose libc numbers differ — macOS, where
  `EAGAIN` is 35 — disagrees on the ones past 40.
- **`gc` counts bytes, not generations.** There is one generation here, so
  `get_count()` and `get_threshold()` report the allocation pressure in
  kilobytes in their first element and zero in the other two, and
  `set_threshold(n)` sets that pressure.
- **`memoryview` is flat.** It has `itemsize`, `format`, `shape`, `strides`
  and a step, and `cast()` recasts a contiguous one; but it is one-dimensional,
  `strides` is one number, and the only things that give a buffer a width are
  `array` and `cast`.
- **`unicodedata.ucd_3_2_0` is not there.** `stringprep`, and through it the
  `idna` codec, read Unicode 3.2 through it; neither can be imported before
  `re` anyway. The delta it needs is 66 records, and phase 24 is where it
  arrives.
- **The streams are in UTF-8 mode.** `sys.flags.utf8_mode` is 1, so stdout
  and stdin use `surrogateescape` and stderr `backslashreplace`, which is what
  CPython does under `-X utf8` and not what it does by default.
- **The native codecs answer when `encodings` cannot be imported.** CPython
  imports the package at startup and cannot run without it; this imports it
  at the first lookup, and where the library is not on `sys.path` a name like
  `utf-7` or `unicode_escape` still reaches its codec rather than failing. A
  name only the library knows, `cp1252`, is then an unknown encoding.
- **A program run from a file whose cookie names a codec written in Python
  cannot start.** `compile()`, `exec()` and `import` decode such a source
  through the registry; the program's own file is read before there is a VM
  to run the codec in, so only UTF-8, Latin-1, ASCII, UTF-16 and UTF-32 are
  read there.
- **An invalid escape says nothing.** CPython warns about `"\N"` or
  `"\777"` with a SyntaxWarning; there is no `warnings` yet, and they are kept
  as CPython keeps them, silently.
- **`--dump-tokens` prints a name in its NFKC form.** The lexer normalizes
  as it scans; `tokenize` prints `ﬁx` where this prints `fix`.
- **Unicode is 16.0, 3.14's.** The CPython clone under tmp/ is 17.0; the
  version follows the language this tracks, and `unidata_version` says so.
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
or `filter`, so it is once-only and not a list; only the timing differs. The
same limit is why a builtin handed a generator drains it whole, which the
generator section below returns to.

Nor does it reach a comparison. `list.sort()` merges inside C++, so a class
with `__lt__` cannot be sorted — the same limit `sorted`, `min` and `max` have
had since phase 8. A key function is fine, because a key is called from a loop
the continuation owns.

`import` is the one continuation that needs both halves. It asks the driver for
a file, and then asks the VM to run the module body — and a module that imports
a module that imports a module nests neither the native stack nor the driver.
[test/pyimport.mjs](test/pyimport.mjs) runs a chain of fifty.

## How exec runs

`exec` and `eval` have to run Python, and a builtin may not push a frame. So
they use the mechanism `import` already had: a code object becomes a function
over the globals it was given, the continuation asks for that one call, and the
namespace the frame runs its `LoadName` against is recorded on the
continuation. `import` runs a module body exactly this way, and it is why a
module that execs a module that execs a module nests nothing.

What the two add is that the namespace is the caller's argument rather than a
fresh module dict. CPython's rule falls out of one line: neither given means
the caller's own, globals alone serves as both, and locals alone leaves the
globals the caller's. A dict handed in gets `__builtins__` put in it, because
that is where a program looks to see what it has.

`locals()` is the same question from the other side. A module or a class body
keeps a real namespace and that *is* its locals; a function's locals are frame
slots, so what comes back there is a snapshot built from `co_varnames` and the
cells. That is CPython's answer too, and it is why `exec("x = 1")` inside a
function binds nothing.

## How a generator suspends

A generator wants a frame that outlives the call that made it, and this VM
already has one: frames are heap objects chained through `back`, not C++ stack
frames. So a call to a function whose body yields binds the arguments into a
frame and stops there. The frame goes into a [GenObj](gen.h) instead of onto
the chain, and nothing of the body has run.

Resuming pushes that frame back on the chain and the loop carries on in it.
`yield` is `Return` with the frame kept: the value goes where a call's answer
would go, and the pc, the value stack, the block stack and the locals stay
exactly as they were. Running off the end raises `StopIteration` carrying the
return value, which is what the language says and what the `for` loop's
continuation was already waiting for.

One thing follows from all this and shapes the rest. **Resuming a generator
pushes a frame, and only the dispatch loop may push a frame.** So `gen.send`
cannot be a builtin: a builtin returns a value, and this has to return into the
loop. It is a small object of its own instead ([gen.h](gen.h)), which `do_call`
recognises — and because it is *callable*, a continuation can ask for the next
item the same way it asks for a `key=` function. That is what makes `list(g)`,
`sum(g)`, `sorted(g)`, `", ".join(g)` and the rest work at all: each parks, the
VM drains the generator, and the builtin is entered again over a list. The same
path answers a class that writes its own `__iter__` and `__next__`, which until
now no builtin could iterate either.

The opcodes that iterate — `[*g]`, `a, b = g`, `f(*g)`, `xs += g`, `x in g` —
do the same thing one level down: the operand is drained, the list is put where
the generator was, and the instruction runs a second time.

`yield from` is the one place that needs both directions. The delegating
generator parks *on the instruction* rather than past it, so whatever is sent
next comes back to the same opcode and goes straight through to the
sub-iterator. What comes back is either yielded onward — which parks the
delegator again — or, once the sub-iterator stops, is the value of the
expression. `throw` follows the same path, so an exception reaches the
innermost generator that can catch it. `close` is the exception to that: a
`GeneratorExit` is not thrown through the delegation but closes the
sub-iterator first, and only then reaches the `yield from` itself. That is what
lets a delegating generator run its own `except GeneratorExit`.

## How a coroutine awaits

A coroutine is a generator under another type, and `await x` is `yield from`
over whatever `x` gives to be walked: the coroutine itself, a generator
`types.coroutine` marked, or what a class's `__await__` returns, checked for
being an iterator and not another coroutine. So the delegation phase 15 wrote
is the whole of the mechanism. A value yielded at the bottom of a chain of
awaits passes up through every frame to whoever called `send`, which for now
is the program and in phase 28 will be the event loop, and what is sent back
goes down the same way.

An async generator is the one that needed more. Its body both yields values
to whoever iterates it and awaits things that yield to whoever drives it, and
the two travel the same road. So a `yield` in one is compiled with a mark on
the value — `AsyncGenWrap` — and the awaitable `asend()` answers looks at what
comes back: a marked value ends the step with `StopIteration` carrying it,
which is what makes `await agen.asend(v)` worth that value, and anything else
passes on up. `athrow()` and `aclose()` are the same awaitable in two more
modes, and all three keep the state CPython's do — not started, running,
spent — which is what refuses a second `anext()` while the first is still
awaiting.

That awaitable is a continuation, and its answer is often an exception: the
`StopIteration` above is how a step *succeeds*. So an exception now travels a
chain of continuations the way a return value always did. When a step fails,
or a frame unwinds into one, the exception is offered to each continuation
waiting behind it in turn — the `yield from` that asked for the step is
usually the one that catches it — and each one it passes is told, through its
`fail` hook, that it was abandoned. That is how an async generator learns it
is no longer running when its body raises.

## How an attribute is found

`obj.name` is not a dict lookup. It is the descriptor protocol, and phase 9
knew one instance of it -- `property` -- and looked for that by hand. The rule
[attr.cpp](attr.cpp) states instead is CPython's: a class-dict entry with a
`__get__` is a descriptor, one that also has a `__set__` or a `__delete__` is a
*data* descriptor, and only a data descriptor comes before the instance's own
namespace. Functions, `staticmethod`, `classmethod`, `property` and a
`__slots__` member are all instances of that one rule; the first three are
answered in C++ because their `__get__` cannot fail and cannot call Python.

What makes this the shape it is, rather than a function returning a value, is
that nearly every step of it *may be Python*: a property's getter, a
descriptor's `__get__`, a class's `__getattribute__`, its `__getattr__`. Ground
rule 2 says the lookup cannot make that call. So `py_attr` answers one of four
things -- the value, nothing, an error, or **a continuation the caller runs** --
and the VM, `getattr` and `hasattr` all drive it the same way. The fallback
chain lives inside that continuation: `__getattribute__` first, an
`AttributeError` out of it caught, `__getattr__` next, and only then either the
default `hasattr` was given or the exception carrying on.

`object.__getattribute__`, `object.__setattr__` and `object.__delattr__` are
real natives in `object`'s namespace, which is what lets a class override one
and still reach the default through `super()` -- the ordinary way to write a
`__setattr__` that stores after all. Finding one of them *is* finding the
default, so `type_hook` treats it as no hook at all, and the same test says
whether a class wrote an `__init__` of its own.

`__slots__` is the layout question rather than the lookup one. Each name
becomes a member descriptor over an index, and the instance's slot array sits
straight after its header, where the base's ended. A class that declares
`__slots__` and whose bases all did has no instance dict at all: `o.__dict__`
is an `AttributeError` and a name that is not a slot cannot be stored.

## How a class is made

`class C(B, metaclass=M, x=1)` is five steps, and four of them can call Python,
so [type.cpp](type.cpp)'s `build_step` is a state machine rather than a
function. A base that is not a class is asked for its `__mro_entries__` and
replaced by what it names, which may change the bases again, so that step
loops; the metaclass is then the most derived of the one asked for and the
bases' own, and a disagreement is the `TypeError` CPython raises rather than a
silent choice. `M.__prepare__` says what namespace the body runs in, the body
runs in it, and then the metaclass is *called* -- which is what makes
`M.__new__` and `M.__init__` run, and what carries the class keywords to them.

A class is an instance of its metaclass in the layout too: `is_type` is a flag
on the object rather than a compare against one descriptor, and a class made by
a metaclass points at that metaclass's slots. So `type(C) is M`, `M`'s own
methods are found on `C`, and a data descriptor on `M` comes before `C`'s
namespace the way one on `C` comes before an instance's.

The hooks a fresh class owes run inside `type.__new__`, which is where CPython
runs them: `__set_name__` over every entry in the namespace, then
`__init_subclass__` on the base with whatever keywords are left. Both are
Python, so that too is a continuation -- and because it lives in `type.__new__`
rather than in the `class` statement, `type('C', (B,), ns)` gets them as well.

A method that names `__class__` or calls `super()` closes over a cell the
class body owns, as CPython's does. The symbol table notes `__class__` wherever
`super` is read in a function, the class keeps the cell, and the body ends by
storing it as `__classcell__` and answering it. `type.__new__` fills the cell
with the class and drops the name; `build_step` then checks the cell holds the
class it made, which is how a metaclass that forgot to pass `__classcell__` on
is caught. Zero-argument `super()` is the frame's first argument -- or its
cell, where a nested scope captured it -- and what that cell holds.

## How a finalizer runs

`__del__` and a weak reference's callback make the collector's sweep
observable, and a sweep cannot call Python. So [gc.cpp](gc.cpp) does not run
them: it *owes* them. Between marking and sweeping, where everything is still
whole, [weak.cpp](weak.cpp) clears every reference whose target is about to go
and owes its callback; then every unmarked object whose class writes `__del__`
is owed one, and owing it marks the object, so this collection leaves it -- and
everything it reaches -- alone. The VM makes the calls between two opcodes,
which is the same place a `^C` is delivered and for the same reason.

Three rules fall out of that order, and they are the ones a program can see. A
finalizer is owed **once per object, ever**, so a `__del__` that stores `self`
somewhere resurrects it and is never called again. A weak reference is cleared
**before** any finalizer runs, so a `__del__` can never be handed a dangling
one. And an exception out of a finalizer is reported and goes no further: there
is no statement it came from for a handler to belong to, which the continuation
says by catching everything.

A generator carries the same flag. One dropped at a `yield` is owed a `close`
rather than a `__del__`, and that is what runs the `finally` it was sitting
inside.

## How a comparison calls Python

`py_cmp` and `py_eq` are C++ and cannot push a frame, so for eight phases a
class with a `__lt__` could not be sorted and one with an `__eq__` could not be
found. The fix is the one the plan named: a continuation that owns the loop.
[compare.cpp](compare.cpp) writes the merge, the fold, the search and the
item-by-item sequence compare as state machines over one object -- the merge's
`w`, `lo`, `i`, `j` and `o` are fields rather than locals -- and each comparison
that needs Python is one request the VM answers. A nested one, a list of lists
of instances, chains a second driver in front of the first, so the depth costs
continuations rather than native stack.

The algorithms are the same as the plain ones beside them: bottom-up,
iterative, stable, the left run winning a tie. A reverse sort swaps the
operands rather than the operator, so only `__lt__` is ever asked for, which is
what CPython's sort promises. And the reflected method is tried even between
two of the same class, because `do_richcompare` does -- that is what lets a
class with only a `__lt__` answer `>` as well, and it is the one place a
comparison differs from an arithmetic operator.

## How a str holds a surrogate

A str's bytes are UTF-8, and UTF-8 has no spelling for U+D800. So the rule is
widened by one step: a surrogate is written the way any other three-byte
codepoint is ([ustr.h](ustr.h)). Everything that walks a str -- indexing,
slicing, comparison, the methods -- already counts in lead bytes and is none
the wiser, and codepoint order is still byte order. What changed is the
boundary. Text from outside is never taken on trust: a codec decodes it, and
strict UTF-8 refuses the sequences the widened form allows. Text going out is
encoded the same way, which is why a lone surrogate on stdout becomes the byte
`surrogateescape` says, or a `UnicodeEncodeError` at the `print`.

## How a codec runs

A codec is a loop, and the loop may have to call Python in the middle: a
handler the program registered with `codecs.register_error` is asked what to
put where the codec failed, and it answers before the codec goes on. Ground
rule 2 says the loop cannot make that call. So a run is an object
([codec.cpp](codec.cpp)'s CodecObj) holding everything the loop would keep in
locals -- the position, the output so far, UTF-7's shift state -- and the loop
stops at each fault. A built-in handler settles the fault on the spot; the
program's own is a call the continuation asks for, and the run resumes from
where the answer says. Most runs never meet a fault, and those never allocate
a continuation at all.

The registry is the same shape one level up. `str.encode("cp1252")` asks each
search function in turn -- CPython's `encodings.search_function`, which
imports `encodings/cp1252.py` -- and then calls the encoder it found, and
every one of those is a request the VM makes. The package is imported at the
first lookup rather than at startup.

A source file is bytes until PEP 263 says how to read it. The lexer honours a
BOM and a coding cookie itself for UTF-8, Latin-1 and the other codecs written
here; a cookie naming one written in Python stops it with that name, and
`compile()` and `import` decode the source through the registry and try again.

## How a module is written in C++

[module.h](module.h) is a name and a function that fills a fresh module's
namespace. The loader asks the registry first — [import.cpp](import.cpp)'s
`begin_load` — and goes looking for a file only when the name is not one of
these, which is why `import time` finds this `time` with an empty `sys.path`
and a `time.py` beside the program the moment there is one.

An installer is a table and a loop:

    constexpr ModDef DEFS[] = { { "reduce", b_reduce }, ... };

    bool functools_install(DictObj *into)
    {
        return mod_defs(into, DEFS) && mod_type(into, &partial_type, b_partial);
    }

`mod_type` is the other half: a native type becomes a name in the namespace
*and* callable, because `type_set_ctor` puts the constructor in the TypeObj's
`__new__`. So `itertools.count` is a class rather than a factory,
`type(count(1))` is `itertools.count`, and `chain.from_iterable` is a static
method on it.

Three things shape what the modules can be.

**A builtin cannot call Python.** `reduce`, `partial`, `lru_cache`,
`takewhile`, `groupby`, `operator.add` on a class instance, `defaultdict`'s
`__missing__` — every one of them calls a function the program wrote, so every
one of them is a `ContObj` that asks for one call at a time and lets the VM
drive it. That is ground rule 2, and it is the same machinery `sorted(key=)`
already used. Where the call has to come from a comparison, as
`cmp_to_key`'s does, the object is an instance of a class whose methods are
natives flagged `OBJ_PYLIKE`, so the sort's own machinery sees a method it
must call rather than a slot.

**A builtin cannot step a generator.** Anything taking an iterable checks
`iter_needs_vm` and parks on `iter_park`, which drains it into a list and
enters the builtin again. The lazy iterators — `count`, `islice`, `chain` —
walk their source with `py_next` instead, which works for every *native*
iterable and is what makes `islice(count(), 5)` finite.

**Two things cannot be asked for at all.** `tty_of` and `clock_now` are
asynchronous syscalls and nothing under `vm_burst` awaits, so
[braam.cpp](braam.cpp) reads both once before the program starts and hands them
over: `sys_set_tty` and `time_set_clock`. `time.time()` counts on from that one
wall reading with `Sys::Now`, which is monotonic and cannot name a day.
`time.sleep` is the third and went the other way — it became a `Req`, because a
sleep is exactly what a driver is for, and a `^C` reaches a sleeping program
because of it.

## Testing

`make test` from the top of the tree runs everything; one file at a time:

    make test TESTS=lang/python/test/pysmoke.mjs

All three need node and a built `../braam-core`. To bring one more upstream
test into the suite:

    tools/mkexp.py basics/andor.py

which copies it into `test/cases/`, writes the expected output beside it —
upstream's own `.exp` when there is one, host CPython otherwise — and adds a
row to the manifest marked `fail`. Move the row to `pass` when it passes.

Every tool that asks CPython for a golden takes the interpreter from `$PYTHON`,
or `python3` on `PATH` when that is unset, and writes down which one answered:
the manifest's `exp` column for an upstream test, and
[test/goldens.txt](test/goldens.txt) for one of ours. A golden is not rewritten
by a different CPython unless the tool is given `--regen`, so a host upgrade
cannot move one by accident; `tools/mkexp.py --regen` rewrites every row a
CPython wrote. The `lazy` cases need 3.15 or later, which the host does not
have:

    PYTHON=tmp/cpython-build/python.exe tools/mkfmt.py test/lazy/basic.py

where `tmp/cpython-build/` is the clone configured and built out of tree.

One of CPython's goes in the same way, in two steps, because its golden is
what this interpreter printed and only the harness can produce that:

    tools/mkcpy.py test_unary.py
    node test/pycases.mjs --bless

The first copies it into `test/cpython/` and adds a row; the second runs it
with the shims planted beside it, writes the `.res` golden, and fills in the
row — the state and the `<passing>/<ran>` count together, so the manifest and
the golden cannot disagree. **Read the diff before blessing.** A test that
cannot run at all still gets a row and a golden holding the complaint, which is
how a case that starts running is noticed.

    node test/pycases.mjs --survey

runs every file in the clone under `tmp/` and counts what stopped each one.
It is not part of `make test` — the clone is not committed — and it is how the
next wave of cases is chosen.

A new tokenizer or parser case is a `.py` under `test/lex/` or `test/ast/`
plus

    tools/mklex.py test/lex/<name>.py
    tools/mkast.py test/ast/<name>.py

which write the golden from CPython's own `tokenize` and `ast` modules, so both
are measured against CPython rather than against themselves. A case whose name
ends in `_err` is one that must be refused, and its golden holds the complaint;
`node test/pylex.mjs --bless` and `node test/pyast.mjs --bless` rewrite those,
after reading the diff.

A formatting case is a `.py` under `test/format/`, a number case one under
`test/number/`, a namespace or syntax case one under `test/exec/`, a
type-system case one under `test/type/`, a module case one under
`test/module/`, a coroutine case one under `test/coro/` and a lazy-import case
one under `test/lazy/`, whose modules live in `test/lazy/mods/`; each is a
program that prints, and the golden is what CPython prints for it:

    tools/mkfmt.py test/format/spec.py

which makes the comparison as strong as it can be — the same program, the two
interpreters, byte for byte. So a case there may use nothing this interpreter
has not got: no generator, no `eval`. And it may not use anything whose answer
depends on how the *host CPython* was built, which a complex multiply of two
extreme magnitudes does — [test/number/complex.py](test/number/complex.py)
says why it leaves that out. A module case has three more of those to avoid: a
transcendental function's last ulp, which musl and the host's libm round
differently; anything an implementation is entitled to answer for itself, such
as `sys.maxsize` or a collection count; and `errno`'s numbers past 40, which
are the platform's. [test/module/mathmod.py](test/module/mathmod.py) prints to
fourteen significant digits for the first of those.

A compiler case is a `.py` under `test/dis/` and nothing else: the bytecode is
this implementation's, so there is nothing to generate the golden from and
`node test/pydis.mjs --bless` writes what the compiler printed. **Read the diff
before blessing** — that golden is the only thing standing between a change and
a silent regression.

A Unicode case is a `.py` under `test/unicode/`, written the same way; its
driver plants [lib/](lib/) beside it, so `codecs` is CPython's in both runs.
`node test/pyunicode.mjs --full` adds what is too slow for every run: a
digest of every property of every codepoint, compared with the host
CPython's, and Unicode's own `NormalizationTest.txt`, which it expects under
`tmp/ucd/16.0.0/`. The tables themselves are written by

    tools/mkucd.py --fetch

which downloads the four UCD files it reads into `tmp/ucd/`, takes everything
else from the host's `unicodedata`, and decodes each table again before
writing it. Its output is committed, so a build needs no Python of a
particular version.

`PY_GC_STRESS=1` in a program's environment collects at every allocation, which
turns a missing `Root` from a rare crash into a wrong answer.
[test/pystress.mjs](test/pystress.mjs) runs the whole manifest that way and
compares; it costs a second and a half, and it found nine of them.

## Licence

The interpreter is this repository's, and so are the shims under `test/shim/`,
which are not copies of anyone's code. [ucddb.cpp](ucddb.cpp) is generated
from the Unicode Character Database, under the Unicode licence. The tests under `test/cases/` are
MicroPython's, MIT, Damien P. George; those under `test/cpython/` are
CPython's, under the PSF licence, copyright the Python Software Foundation.
[LICENSE](LICENSE) carries both and says which files each covers.
