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
Stage 4 and tasks 19 to 24 were compression.

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
- **A `MemoryError` that is not caught crashes the process** while it is
  being reported. `tempfile.mkdtemp(dir=b"/tmp")` shows it: `map` is eager,
  so mapping `os.fsencode` over the endless name generator runs out of memory,
  and then the report traps. That is also why `test_tempfile` cannot run.
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
  so that golden is blessed under the shard `make test` runs it in --
  `--shard=2/4` today, and it moves whenever a test file is added.
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
- **Deep structures and the recursion limit.** `pickle.py` spends four
  frames on each level of a list, so a structure nested past about fifty
  levels raises `RecursionError`, where CPython's limit of 1000 takes 250.
  The limit is what the native stack holds; this is the cost of it.

## Stage 5 — `email` and `xml`

25. **`email`.** The whole package, pure Python. It needs `urllib.parse` and
    `quopri`, `calendar`, `datetime` and `base64`. `socket` is
    imported only inside `make_msgid`, so that one function waits for
    task 28. Test: `test_email/`.

26. **`xml`, without a parser.** `xml.etree.ElementTree` and `ElementPath`,
    `xml.dom.minidom`, `xml.dom.minicompat`, `xml.sax.saxutils`, `handler`
    and `xmlreader`. All of these import `expat` lazily, so building a tree,
    searching it and serialising it all work now. `fromstring`, `parse` and
    `minidom.parseString` raise `ImportError` until task 27. Tests: the
    non-parsing cases of `test_xml_etree` and `test_minidom`, and
    `test_xml_dom_minicompat`.

27. **`pyexpat`.** A native module with the surface that `ElementTree`,
    `expatbuilder` and `expatreader` use: `ParserCreate`, the handler
    attributes, `Parse` and `ParseFile`, `buffer_text`, `ordered_attributes`,
    `ExpatError` with `lineno` and `offset`, and `errors` and `model`.
    Recommendation: build libexpat's C as a separate `PORT` library rather
    than write a new parser. The tests check expat's own error messages and
    positions, and only expat produces those. Tests: `test_pyexpat`,
    `test_xml_etree`, `test_minidom`, `test_sax`.

## Stage 6 — debugging and profiling

28. **The `_socket` floor, importable but unable to connect.** `pdb` imports
    `socket` at the top, and `doctest` imports `pdb`, so none of the three
    loads without it. `_socket` provides the constants, the exception types,
    `gethostname` (`"localhost"`) and a `socket` type whose constructor
    raises `OSError(EAFNOSUPPORT)`. Then ship `socket.py` byte for byte.
    `select` and `selectors` are here and wait on pipes, so this floor is the
    last thing two of their tests need: `test_selectors` imports `socket` at
    the top and `test_subprocess` imports `socket` and `sysconfig`, so the
    first runs once this is done and the second once task 33 is too. Both
    rows are in `test/cpython.txt` already, as `fail import`. §10 "Because
    Braam has no such thing" still holds for sockets and says what the floor
    is for.

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
    33. A trace function is a Python call made from inside the instruction
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
    scheme paths that point into the library directory. Test: `test_trace`.

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
