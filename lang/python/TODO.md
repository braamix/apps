# TODO

What `Manual.md` §10 lists as "not written yet", in the order to write it:
cheap and useful first, then the tasks that need new native code, then the ones
that need new machinery in the VM. Each task names its floor (the native module
or VM work it stands on) and the CPython test that says it is done.

The same rules as the rest of `lib/`: a module is taken from
`tmp/cpython/Lib` byte for byte with a row in `lib/manifest.txt`, and a module
that needs something missing waits for it rather than being trimmed. Its test
comes in through `tools/mkcpy.py`. Each task ends by moving its entry out of
§10 of `Manual.md`.

One item in the list is not planned. It is at the end, with the reason.

**A number is a name, not a position.** A stage or a task that is finished is
deleted and everything left keeps the number it had, so the list has gaps.
Stage 4 and tasks 19 to 24 were compression; Stage 5 and tasks 25 to 27 were
`email`, `xml` and `pyexpat`; task 28 was the `_socket` floor.

What `email` still cannot do is the one thing this system has not got: **the
CJK codecs are not written**. `encodings` here is the single-byte pages, the
UTF forms and the transforms, and `euc-jp`, `shift_jis`, `iso-2022-jp`,
`gb2312` and `cp949` each stand on a C codec — `_codecs_jp` and its four
siblings — that nobody has written. Six of `test_email`'s eight remaining
methods and three of `test_contentmanager`'s are that, and
`test_asian_codecs` is a `fail` row for it. `Manual.md` §10 says so.

**`pyexpat` is expat 2.8.4 rewritten in C++**, under `src/expat/`, taken from
CPython's own `Modules/expat` -- the version these tests were written
against, and the same tree `lib/` comes from. It keeps upstream's identifiers
and structure, because what the tests check is expat's own error codes,
messages and positions. What it does not keep: the two `#include`d bodies are
a template over the encoding, `XML_UNICODE` and `XML_MIN_SIZE` are gone with
the C, the accounting keeps its limits and loses its reporting, and the
entropy is `proc_random()`. `src/pyexpatmod.cpp` drives it the way
`emulators/simbesm` drives a machine: a handler queues its event and suspends
the parse with `XML_StopParser`, and a `ContObj` makes the Python call and
resumes. `test/stdlib/xmls.py` is 244 lines of that, identical to CPython's.

Four changes to expat were needed for a driver that leaves the parser to make
a call, and each is marked in the source. `XML_ERROR_SUSPEND_PE` is gone --
upstream refuses to suspend a parameter entity's parse because `XML_Parse` on
such a parser is called from inside the parent's handler, and here it never
is. The four position accessors answer from where the handler stood rather
than from where the parse stopped, and the position cache is not advanced
past a suspension. `XmlFailSuspendedParse` fails a stopped parse with the
error a handler's refusal would have caused, because the three handlers that
steer the parse by what they return get their answer a suspension too late.
And an external parameter entity's `dtd->paramEntityRead` is read on the
resume rather than the moment the handler returns, for the same reason. One
corner is left: where a non-standalone document's external parameter entity
was read and a `NotStandaloneHandler` is set, that handler is not called.

What compression could not do is recorded rather than left open. **lzma's high
presets are what the process can spare**: nothing is refused in advance —
liblzma is asked, and CPython's default preset of 6 wants 94 MiB of a 100 MB
process, which it gets when it asks first and not once the program holds much
else. So a preset nobody named comes down until it fits, and one that was
named is reported, which also puts `zipfile.ZIP_LZMA` out of reach. And **zstd's
dictionaries are untrained**: `zdict.h`, which holds zstd's COVER trainer, is
not in the library, so `train_dict` and `finalize_dict` build a content-only
dictionary — real, and better than none, but with no entropy tables and so no
`dict_id`. `Manual.md` §6 says both.

## Found along the way

- **A `__hash__` written in Python, for dict keys and set members.** `py_hash`
  is C++ and cannot call Python, so an instance hashes by identity and two
  equal instances are two keys. `ipaddress.collapse_addresses` gives the wrong
  answer because of it. The fix is VM work: dict and set operations whose key
  is such an instance park in a continuation, call `__hash__` and then `__eq__`
  on each collision, and finish in C++. This is the most useful item in this
  file that is not a module.
- **`__index__` returning a non-int** says "an integer is required" where
  CPython says `__index__ returned non-int (type str)`.
- **A slice's bounds do not call `__index__` either**, and for the reason the
  note below gives: `slice_resolve` is plain C++ reached from the middle of
  the VM, so it cannot park to make a Python call. `lst[X():]` where `X`
  writes `__index__` is a TypeError here. Ten of `test_xml_etree`'s methods
  are that one thing.
- **A `MemoryError` that is not caught crashes the process** while it is
  being reported. `tempfile.mkdtemp(dir=b"/tmp")` shows it: `map` is eager,
  so mapping `os.fsencode` over the endless name generator runs out of memory,
  and then the report traps. That is why `test_tempfile` cannot run, and why
  `test_xml_etree` cannot either: its module is 5,400 lines of classes and a
  hundred megabytes is not much room left over, so one test out of four
  hundred takes the last of it and the run ends where the report would be.
  Both rows say `crash`.
- **`importlib.invalidate_caches()` fails**: it imports `importlib.metadata`,
  which is not shipped.
- **`python <directory>`** says "is a directory" instead of running the
  directory's `__main__.py`, and **`python -m`** refuses a module name that is
  not ASCII. `test_argparse` shows both.
- **Reserved memory ratchets up to the cap.** braam-core's allocator never
  gives a span back once a size class has taken it, so a burst of small
  objects leaves spans that later multi-megabyte buffers cannot use.
  `test_pickle` gets to 102 MB reserved with 11 MB in use, and its framing
  tests then raise `MemoryError`. Which subtest tips over depends on the pid,
  so that golden is blessed under the shard `make test` runs it in, and it
  moves whenever a test file is added. Adding `email`'s fourteen cases moved
  `test_bz2` from 102 of 103 methods to 80: nothing about bzip2 changed, only
  what had run in that worker before it.
  `test_tarfile` is the same story and worse: alone it runs 781 methods, and
  in `--shard=4/4`, after twenty-nine cases have been through the same boot,
  its `setUpModule` cannot build a `.tar.xz` at all and the file reports
  nothing. Its golden is blessed in that shard, so what `make test` compares
  is what `make test` produces. The fix is in braam-core: return a class's
  empty spans to the free runs.
- **`bytes(array.array(...))` takes the items, not the buffer.** CPython
  reads the object's buffer and answers the raw bytes; this answers one byte
  per item, so `bytes(array("I", [1,2,3]))` is three bytes and not twelve.
  `memoryview(a)` is right, so the buffer is there and only `bytes()` does not
  ask for it. `_io`'s `write` refuses an array outright, because `buffer_arg`
  in [src/bytemeth.cpp](src/bytemeth.cpp) uses `bytes_like` where
  `buffer_like` would take one. `test_gzip` shows both.
- **`memoryview.cast()` takes no keywords**, so `m.cast("B", shape=[8, 8, 4])`
  is a TypeError. `test_gzip` again.
- **Native argument parsing does not call a Python `__index__`.** A class that
  writes one is an integer to CPython wherever a number is wanted; here only
  the builtins that spell out `redo_converted` take it, and a *keyword*
  argument never does. `zlib`'s six entry points do it positionally; nothing
  else does. Related to the `__index__` note above.
- **`os.utime` does nothing** — there is no call to set a file's times — and
  **a directory has no modification time at all**, which `os.stat` reports as
  0. Between them they are most of `test_tarfile`'s remaining failures, and
  they are why `shutil.make_archive(..., "zip", ...)` over a tree raises
  "ZIP does not support timestamps before 1980".
- **A codec object holds megabytes until the collector sweeps it**, so the
  next one may find no room: an lzma encoder is 94 MiB at preset 6 and a
  bzip2 compressor 7.6 MB. Each of the four modules therefore collects once
  before reporting no memory, and lzma collects before a decompressor's first
  step, because liblzma builds its dictionary there and **a coder that has
  answered an error cannot be stepped again** — there is no retrying after
  the fact. The real fix is the span ratchet above.
- **A binary operator tries the right side's dunder too late** where the
  right side is a subclass of a built-in and the left is a plain one.
  `[1, 2] + sub` used to raise before `sub.__radd__` was asked, because the
  delegating slot raised rather than saying it had nothing; it says so now.
  What is still CPython's and not this interpreter's is the other half of the
  rule: where `type(b)` is a *proper subclass* of `type(a)`, CPython asks
  `b.__radd__` **first**. So `1 + IntSub()` and `[1] + ListSub()` answer with
  the built-in's operator here and with the subclass's there.
- **Character data arrives in different pieces than CPython's**, where
  `buffer_text` is off. Expat may report a run of text in several calls and
  says so; suspending the parse at a handler moves where it chooses to break,
  so the same document can give `('hi', ' there')` here and `('hi there',)` in
  CPython. Every handler that matters concatenates, `buffer_text` hides it
  altogether, and it is what a driver costs.
- **Deep structures and the recursion limit.** `pickle.py` spends four
  frames on each level of a list, so a structure nested past about fifty
  levels raises `RecursionError`, where CPython's limit of 1000 takes 250.
  The limit is what the native stack holds; this is the cost of it.

## Stage 6 — debugging and profiling

29. **The `sys.monitoring` namespace.** `bdb` reads `sys.monitoring.events`
    at import. Add the tool registry (`use_tool_id`, `get_tool`,
    `free_tool_id`, `clear_tool_id`), `register_callback`, `set_events`,
    `get_events`, `set_local_events`, `restart_events`, `DISABLE`, `MISSING`,
    and the event constants. All of it is bookkeeping, and no event fires
    until task 32.

30. **`bdb`, `doctest`.** With tasks 28 and 29 done, `pdb` imports. It
    cannot trace yet, but `doctest` does not trace unless asked to. Ship
    `bdb`, `pdb` (import only) and `doctest`. `doctest.DocTestSuite` then
    plugs into `unittest`. Test: `test_doctest/`.

31. **`sys.settrace`, `sys.setprofile`.** The VM work, together with task
    33. `sys.gettrace` and `sys.getprofile` are already here and answer None,
    which is the true answer while nothing can be installed; the setters are
    what is missing. A trace function is a Python call made from inside the
    instruction
    loop, so it must be a pushed frame, like any other call. When it
    returns, the loop resumes the instruction it was called from, with no
    native recursion (the same rule as `ContObj`). Events:
    - `call`, `line`, `return` and `exception`, plus `opcode` when
      `f_trace_opcodes` is set.
    - For the profiler, `c_call`, `c_return` and `c_exception` around
      builtins.

    Line events come from the code's line table. While the tracer runs,
    tracing is off. Also: `frame.f_trace`, `f_trace_lines`, `f_trace_opcodes`,
    `sys.gettrace`/`getprofile`, and `threading.settrace`/`setprofile`.
    Leave jump by assigning `f_lineno` for later. Tests: `test_sys_settrace`,
    `test_sys_setprofile`.

32. **`sys.monitoring` events.** Built on the same event points as task 31.
    Implement the events `bdb` and `pdb.set_trace()` use: `PY_START`,
    `PY_RESUME`, `PY_RETURN`, `PY_YIELD`, `LINE`, `INSTRUCTION`, `JUMP`,
    `CALL`, `RAISE`, `EXCEPTION_HANDLED` and `PY_UNWIND`. Also per-code local
    events and `DISABLE`. Test: `test_monitoring`, as far as its
    CPython-specific parts allow.

33. **`sysconfig`, `trace`.** `trace` imports `sysconfig` at the top.
    `sysconfig` needs a small `_sysconfig` floor (`config_vars`) and the
    scheme paths that point into the library directory. Tests: `test_trace`,
    and `test_pyexpat`, which imports `sysconfig` at its own top for one
    method that looks for CPython's build directory and is a `fail import`
    row until it can. `test/stdlib/xmls.py` is what tests `pyexpat` until
    then.

34. **`profile`, `pstats`, `cProfile`.** `profile` runs on `setprofile`.
    `cProfile` is `profiling.tracing`, which stands on `_lsprof`: write
    `_lsprof` natively on the task 31 event points, with no Python calls per
    event. `pstats` saves and loads through `marshal`, which writes
    CPython's format. The harness clock is frozen, so the tests can check
    only the structure. Tests:
    `test_profile`, `test_profiling/test_tracing_profiler.py`, `test_pstats`.

35. **`pdb`.** Now a working debugger: `run`, `runcall`, `post_mortem`,
    `pm`, `set_trace` through the monitoring backend, and every command.
    It reads its commands from stdin, through the key ring when stdin is the
    console. `breakpoint()` now starts it. Leave `f_lineno` jumps
    until there is a reason. Tests: `test_pdb`, `test_bdb`.

36. **`symtable`.** A native `_symtable` that exposes what `symtab.cpp`
    already computes: one table per scope, with id, name, type, lineno,
    children and a symbol-to-flags dict. The flags use CPython's `DEF_*`
    bits and scope values, so that `symtable.py` can be shipped byte for
    byte. Test: `test_symtable`.

37. **`pydoc`, `help()`.** `pydoc` needs `sysconfig` (task 33), `pkgutil`,
    `platform` and `inspect`, which are here. It falls back to its plain
    pager when `_pyrepl` is missing. Its server half (`http.server`) stays
    out. `help()`, which `site` installs, then works, both on an object and
    interactively. The real work is `inspect.signature` on builtins, which
    needs a `__text_signature__` on each of them. Test: `test_pydoc/`,
    without the server and browser cases.

## Stage 7 — the allocator

38. **`tracemalloc`.** The allocator would record a traceback for each live
    object and the collector would drop it on sweep. That costs a word or
    more per object while tracing is on, plus a snapshot type. `start`,
    `stop`, `get_traced_memory`, `take_snapshot` and `get_object_traceback`
    are enough for `tracemalloc.py`. Low value here, so it is last. Test:
    `test_tracemalloc`.

## Not planned

- **`.pyc` files.** Compiling from source is fast enough that a cache costs
  more than it saves, and `marshal`'s code format is this interpreter's. `-B`
  and `PYTHONDONTWRITEBYTECODE` are accepted and change nothing.
