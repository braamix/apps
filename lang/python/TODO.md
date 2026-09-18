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

Two items in the list are not planned. They are at the end, with the reason.

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

## Stage 2 — compiler, command line, `site`

10. **`__debug__`.** A builtin constant, `True`. The compiler folds
    `if __debug__:`, and assigning to the name is a `SyntaxError`, as in
    CPython. Nothing else changes until task 12 adds `-O`.

11. **`breakpoint()` and `sys.breakpointhook`.** The builtin, the hook,
    `sys.__breakpointhook__` and `PYTHONBREAKPOINT` (`0` turns it off, and
    `mod.func` names another hook). Until task 32, the default hook's
    `import pdb` fails. CPython answers that failure with a `RuntimeWarning`
    and returns, and so will this. Test: the `breakpoint` cases in
    `test_builtin`.

12. **Command-line flags.** `OptParse` in `braam.cpp` takes `-V -i -c -m`
    today. Add:
    - `-O` and `-OO`: drop asserts and fold `__debug__` to `False`; `-OO`
      also drops docstrings. Sets `sys.flags.optimize`.
    - `-B`, `-s`: accepted, and set their `sys.flags`. Nothing writes
      bytecode, and there is no user site.
    - `-E`, `-P`, `-I`: `-P` leaves the script's directory off
      `sys.path`; `-I` means `-E -P -s`.
    - `-u`: unbuffered stdout and stderr.
    - `-q`: no banner at the prompt.
    - `-v`: one line to stderr for each import.
    - `-W arg`: appends to `sys.warnoptions`, which `warnings` already reads.
    - `-X opt`: fills `sys._xoptions`. Implement `utf8`, `dev`,
      `int_max_str_digits`, `warn_default_encoding` and `importtime`, and
      accept and record the rest.
    - `-b` and `-bb`: `BytesWarning` from `str(bytes)` and from comparing
      bytes with str. This one needs checks in the VM, so do it last.

13. **`PYTHON*` environment variables.** Read through `proc_env`, and ignored
    under `-E`/`-I`. `PYTHONPATH`, `PYTHONSTARTUP` (at the prompt only),
    `PYTHONWARNINGS`, `PYTHONOPTIMIZE`, `PYTHONUNBUFFERED`, `PYTHONVERBOSE`,
    `PYTHONSAFEPATH`, `PYTHONINSPECT`, `PYTHONINTMAXSTRDIGITS`,
    `PYTHONDONTWRITEBYTECODE`, `PYTHONNOUSERSITE`, `PYTHONUTF8`, and
    `PYTHONBREAKPOINT` from task 11. The shell's `VAR=x python …` prefix is
    how a user sets them.

14. **`site` and `_sitebuiltins`.** Imported at startup unless `-S` is
    given. Brings `exit`, `quit`, `copyright`, `credits` and `license`, and
    the `.pth` files in the library directory. `help` is installed too, but
    it only works after task 34, because it imports `pydoc` when called.
    Watch the startup cost: `site` pulls in `os` and `stat`. Test: `test_site`,
    minus the Windows and user-site cases.

15. **`faulthandler`, partly.** A native module. `dump_traceback()`,
    `enable()`, `disable()`, `is_enabled()`. A wasm trap ends the Worker
    before any handler can run, so `enable()` only records the request and
    §10 says so. `dump_traceback_later` and `register` need a timer thread
    and signals that do not exist here, so they raise `NotImplementedError`.
    Test: the parts of `test_faulthandler` that do not start a subprocess.

## Stage 3 — `pickle`

16. **`__reduce__` for every native type.** `copy` works, but most native
    types have no `__reduce__` yet, so `object.__reduce_ex__` loses their
    contents: `set`, `frozenset`, `range`, `slice`, `complex`, `bytearray`,
    `deque`, `defaultdict`, `OrderedDict`, `array`, every exception
    (`BaseException.__reduce__` and `__setstate__`), `SimpleNamespace`, and
    the `functools`/`operator` objects that do not have one yet. Also check
    `object.__getstate__` and `copyreg.__newobj_ex__`. Test: `test_copy` and
    the reduce cases in each type's own test.

17. **`pickle`, `_compat_pickle`, `pickletools`.** The pure-Python pickler
    (`_pickle` is guarded). It needs `struct`, `codecs`, `io`,
    `itertools.batched`, `copyreg`, and `whichmodule` over `sys.modules`. A
    `PickleBuffer` is not needed below protocol 5. Tests: `test_pickle`
    (through `pickletester`), `test_pickletools`.

18. **`marshal` in CPython's format, for data.** The Manual says marshal
    uses this interpreter's own format. Write CPython's format for `None`,
    bool, int, float, complex, str, bytes, tuple, list, dict, set and
    frozenset, including references. Code objects stay this interpreter's,
    and loading a CPython code object is a `ValueError`. `pstats` needs this
    (task 31). Test: `test_marshal`, without the code-object cases.

## Stage 4 — compression

19. **`zlib`, native.** `crc32` and `adler32`; `compress` and `decompress`;
    `compressobj` and `decompressobj`, streaming, with `wbits` choosing raw,
    zlib or gzip framing; `flush` modes, `unused_data`, `unconsumed_tail`,
    `eof`, and the constants. Write both directions in C++ as plain
    functions, so that the VM can call them from a slot without an await.
    The kernel's `inflate` syscall does not fit: it is one-shot, it blocks,
    and its input is capped at `SYS_STAGE_MAX`. Compressed output must be
    valid DEFLATE, but it need not match zlib's bytes. Report
    `ZLIB_VERSION` as this implementation's. `binascii.crc32` and `zlib.crc32`
    should share one table. Test: `test_zlib`, without the cases that
    compare against zlib's exact output.

20. **`compression`, `gzip`.** The `compression` package (`_common._streams`,
    `zlib`, `gzip`), `gzip`, and `python -m gzip`. Test: `test_gzip`.

21. **`zipfile`, `tarfile`, and the archive half of `shutil`.** `zipfile`
    guards its `bz2`, `lzma` and `zstd` imports. `tarfile` guards `pwd` and
    `grp`. `shutil` is already shipped, so `make_archive` and
    `unpack_archive` start working once these two are here. Tests:
    `test_zipfile/`, `test_tarfile`, and the archive cases of `test_shutil`.

## Stage 5 — `email` and `xml`

22. **`email`.** The whole package, pure Python. It needs `urllib.parse` and
    `quopri`, `calendar`, `datetime` and `base64`. `socket` is
    imported only inside `make_msgid`, so that one function waits for
    task 25. Test: `test_email/`.

23. **`xml`, without a parser.** `xml.etree.ElementTree` and `ElementPath`,
    `xml.dom.minidom`, `xml.dom.minicompat`, `xml.sax.saxutils`, `handler`
    and `xmlreader`. All of these import `expat` lazily, so building a tree,
    searching it and serialising it all work now. `fromstring`, `parse` and
    `minidom.parseString` raise `ImportError` until task 24. Tests: the
    non-parsing cases of `test_xml_etree` and `test_minidom`, and
    `test_xml_dom_minicompat`.

24. **`pyexpat`.** A native module with the surface that `ElementTree`,
    `expatbuilder` and `expatreader` use: `ParserCreate`, the handler
    attributes, `Parse` and `ParseFile`, `buffer_text`, `ordered_attributes`,
    `ExpatError` with `lineno` and `offset`, and `errors` and `model`.
    Recommendation: build libexpat's C as a separate `PORT` library rather
    than write a new parser. The tests check expat's own error messages and
    positions, and only expat produces those. Tests: `test_pyexpat`,
    `test_xml_etree`, `test_minidom`, `test_sax`.

## Stage 6 — debugging and profiling

25. **The `_socket` and `select` floors, importable but unable to connect.**
    `pdb` imports `socket` and `selectors` at the top, and `doctest` imports
    `pdb`, so none of the three loads without them. `_socket` provides the
    constants, the exception types, `gethostname` (`"localhost"`) and a
    `socket` type whose constructor raises `OSError(EAFNOSUPPORT)`. `select`
    provides `select()` over regular files, which are always ready; any
    other descriptor is an `OSError`. Then ship `socket.py` and `selectors.py`
    byte for byte. §10 "Because Braam has no such thing" still holds for
    sockets and says what the floors are for. This also removes `selectors`
    from the "not written" list.

26. **The `sys.monitoring` namespace.** `bdb` reads `sys.monitoring.events`
    at import. Add the tool registry (`use_tool_id`, `get_tool`,
    `free_tool_id`, `clear_tool_id`), `register_callback`, `set_events`,
    `get_events`, `set_local_events`, `restart_events`, `DISABLE`, `MISSING`,
    and the event constants. All of it is bookkeeping, and no event fires
    until task 29.

27. **`bdb`, `doctest`.** With tasks 25 and 26 done, `pdb` imports. It
    cannot trace yet, but `doctest` does not trace unless asked to. Ship
    `bdb`, `pdb` (import only) and `doctest`. `doctest.DocTestSuite` then
    plugs into `unittest`. Test: `test_doctest/`.

28. **`sys.settrace`, `sys.setprofile`.** The VM work, together with task
    29. A trace function is a Python call made from inside the instruction
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

29. **`sys.monitoring` events.** Built on the same event points as task 28.
    Implement the events `bdb` and `pdb.set_trace()` use: `PY_START`,
    `PY_RESUME`, `PY_RETURN`, `PY_YIELD`, `LINE`, `INSTRUCTION`, `JUMP`,
    `CALL`, `RAISE`, `EXCEPTION_HANDLED` and `PY_UNWIND`. Also per-code local
    events and `DISABLE`. Test: `test_monitoring`, as far as its
    CPython-specific parts allow.

30. **`sysconfig`, `trace`.** `trace` imports `sysconfig` at the top.
    `sysconfig` needs a small `_sysconfig` floor (`config_vars`) and the
    scheme paths that point into the library directory. Test: `test_trace`.

31. **`profile`, `pstats`, `cProfile`.** `profile` runs on `setprofile`.
    `cProfile` is `profiling.tracing`, which stands on `_lsprof`: write
    `_lsprof` natively on the task 28 event points, with no Python calls per
    event. `pstats` saves and loads through `marshal` (task 18). The harness
    clock is frozen, so the tests can check only the structure. Tests:
    `test_profile`, `test_profiling/test_tracing_profiler.py`, `test_pstats`.

32. **`pdb`.** Now a working debugger: `run`, `runcall`, `post_mortem`,
    `pm`, `set_trace` through the monitoring backend, and every command.
    It reads its commands from stdin, through the key ring when stdin is the
    console. `breakpoint()` (task 11) now starts it. Leave `f_lineno` jumps
    until there is a reason. Tests: `test_pdb`, `test_bdb`.

33. **`symtable`.** A native `_symtable` that exposes what `symtab.cpp`
    already computes: one table per scope, with id, name, type, lineno,
    children and a symbol-to-flags dict. The flags use CPython's `DEF_*`
    bits and scope values, so that `symtable.py` can be shipped byte for
    byte. Test: `test_symtable`.

34. **`pydoc`, `help()`.** `pydoc` needs `sysconfig` (task 30), `pkgutil`,
    `platform` and `inspect`, which are here. It falls back to its plain
    pager when `_pyrepl` is missing. Its server half (`http.server`) stays
    out. `help()` from task 14 then works, both on an object and
    interactively. The real work is `inspect.signature` on builtins, which
    needs a `__text_signature__` on each of them. Test: `test_pydoc/`,
    without the server and browser cases.

## Stage 7 — the rest of compression

35. **`bz2`.** A native `_bz2` on libbzip2, built as a `PORT` library.
    libbzip2 is about 5k lines of portable C. Then `compression.bz2` and
    `bz2`, and `zipfile`/`tarfile`/`shutil` pick it up. Test: `test_bz2`.

36. **`lzma`.** A native `_lzma` on liblzma from xz-utils. It is several
    times the size of libbzip2 and adds a lot to the binary. Do it only if
    someone needs `.xz` files. Test: `test_lzma`.

37. **`tracemalloc`.** The allocator would record a traceback for each live
    object and the collector would drop it on sweep. That costs a word or
    more per object while tracing is on, plus a snapshot type. `start`,
    `stop`, `get_traced_memory`, `take_snapshot` and `get_object_traceback`
    are enough for `tracemalloc.py`. Low value here, so it is last. Test:
    `test_tracemalloc`.

## Not planned

- **`.pyc` files.** Compiling from source is fast enough that a cache costs
  more than it saves, and `marshal`'s code format is this interpreter's. `-B`
  and `PYTHONDONTWRITEBYTECODE` are accepted (task 12) and change nothing.
- **A `selectors` that waits on anything.** Braam has no poll call. Task 25
  makes the module importable, and that is all it can be.
