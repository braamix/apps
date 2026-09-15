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

**Phase 4 of ten.** The object heap, its collector, the core types, a
tokenizer and a parser. `python --dump-tokens f.py` and `python --dump-ast
f.py` print what each produced, and both are checked against CPython's own
`tokenize` and `ast` modules. Nothing runs yet: the compiler is phase 5.

[TODO.md](TODO.md) is the plan: the ground rules the design is pinned to, the
ten phases, and the upstream tests each one is expected to turn green.

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
| [str.cpp](str.cpp), [bytes.cpp](bytes.cpp) | Text in codepoints, and octets |
| [tuple.cpp](tuple.cpp), [list.cpp](list.cpp) | The two sequences |
| [table.cpp](table.cpp) | The insertion-ordered table behind dict and set |
| [repr.cpp](repr.cpp) | repr for every type, quoting and all |
| [lex.h](lex.h), [lex.cpp](lex.cpp) | The tokenizer, and the `--dump-tokens` listing |
| [parse.h](parse.h), [parse.cpp](parse.cpp) | The grammar, by recursive descent into an index arena |
| [astdump.cpp](astdump.cpp) | The `--dump-ast` listing, which is the format mkast.py writes to |
| [selftest.cpp](selftest.cpp) | What `--selftest` checks |
| [test/pylib.mjs](test/pylib.mjs) | The harness: boot, plant the binary, run a command, read back what it wrote |
| [test/pysmoke.mjs](test/pysmoke.mjs) | That the program starts, answers its flags, and reports the right status |
| [test/pygc.mjs](test/pygc.mjs) | Drives `--selftest` and reads what it printed |
| [test/pylex.mjs](test/pylex.mjs) | Every source under `test/lex/`, token for token |
| [test/pyast.mjs](test/pyast.mjs) | Every source under `test/ast/`, node for node |
| [test/runcases.mjs](test/runcases.mjs) | Every case in the manifest, in one boot |
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
missing pin becomes a wrong object count rather than a rare crash.

## Known differences from CPython

Three, all recorded rather than hidden, and all in reach later:

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
  braces is parsed in a later phase; CPython builds a `JoinedStr` here.

## Why the VM is a driver

A `co_await` is a call and not a tail call: the wasm tail-call feature is off,
so entering a task and returning from it each leave a frame on the native
stack, given back only where something *suspends*. An interpreter loop that
awaited would therefore grow the stack until the process trapped.

So the VM is plain C++ that runs until it has something for its caller to do —
a read, a write, an open, an exit — and returns saying what. Only
[braam.cpp](braam.cpp) awaits. It is the shape
[emulators/simbesm](../../emulators/simbesm/) arrived at for the same reason,
and it decides much else: a Python call pushes a frame rather than recursing,
and an error is a sticky flag unwound a frame at a time, because there is no
`setjmp` here either.

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

## Licence

The interpreter is this repository's. The tests under `test/cases/` are
MicroPython's, MIT, Damien P. George — see [LICENSE](LICENSE).
