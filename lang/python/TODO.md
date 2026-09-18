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
  so that golden was blessed with `--shard=3/4`, as `make test` runs it. The
  fix is in braam-core: return a class's empty spans to the free runs.
- **Deep structures and the recursion limit.** `pickle.py` spends four
  frames on each level of a list, so a structure nested past about fifty
  levels raises `RecursionError`, where CPython's limit of 1000 takes 250.
  The limit is what the native stack holds; this is the cost of it.

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

21. **`tarfile`, the archive half of `shutil`, and `zipfile`'s test.**
    `zipfile` is already here, since `importlib.resources` imports it, and
    handles stored members until `zlib` exists. `tarfile` guards `pwd` and
    `grp`. `shutil` is already shipped, so `make_archive` and
    `unpack_archive` start working once `tarfile` is here. Tests:
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

25. **The `_socket` floor, importable but unable to connect.** `pdb` imports
    `socket` at the top, and `doctest` imports `pdb`, so none of the three
    loads without it. `_socket` provides the constants, the exception types,
    `gethostname` (`"localhost"`) and a `socket` type whose constructor
    raises `OSError(EAFNOSUPPORT)`. Then ship `socket.py` byte for byte.
    `select` and `selectors` are already here, for `subprocess`. §10
    "Because Braam has no such thing" still holds for sockets and says what
    the floor is for. `test_subprocess` imports `socket` and `sysconfig` at
    the top, so it runs once this and task 30 are done.

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
    event. `pstats` saves and loads through `marshal`, which writes
    CPython's format. The harness clock is frozen, so the tests can check
    only the structure. Tests:
    `test_profile`, `test_profiling/test_tracing_profiler.py`, `test_pstats`.

32. **`pdb`.** Now a working debugger: `run`, `runcall`, `post_mortem`,
    `pm`, `set_trace` through the monitoring backend, and every command.
    It reads its commands from stdin, through the key ring when stdin is the
    console. `breakpoint()` now starts it. Leave `f_lineno` jumps
    until there is a reason. Tests: `test_pdb`, `test_bdb`.

33. **`symtable`.** A native `_symtable` that exposes what `symtab.cpp`
    already computes: one table per scope, with id, name, type, lineno,
    children and a symbol-to-flags dict. The flags use CPython's `DEF_*`
    bits and scope values, so that `symtable.py` can be shipped byte for
    byte. Test: `test_symtable`.

34. **`pydoc`, `help()`.** `pydoc` needs `sysconfig` (task 30), `pkgutil`,
    `platform` and `inspect`, which are here. It falls back to its plain
    pager when `_pyrepl` is missing. Its server half (`http.server`) stays
    out. `help()`, which `site` installs, then works, both on an object and
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

## Stage 8 — waiting on descriptors

38. **`Sys::Poll` in braam-core.** This is kernel work, released as a new SDK
    before task 39 can start. It adds a syscall number, so `PROC_ABI` rises
    and every package here is rebuilt.
    - Shape: `Poll = 86`. The payload is a timeout in milliseconds
      (`0xffffffff` waits for ever) and then `u32 fd, u32 events` pairs; the
      reply is a `u32 revents` for each pair. Events are `IN`, `OUT` and
      `HUP`.
    - Mechanism: `Source::Read` (`src/user/prog.h`) over N channels. One
      `Waiter`, one token, armed with `park_receiver` for `IN` or
      `park_sender` for `OUT` on every pipe named, and disarmed on all of
      them on resume. A channel that fires after the task has already woken
      finds nothing waiting, which `sched_wake` already treats as a late
      event.
    - Readiness: a read end is ready when `pend` is non-empty, the ring is
      non-empty or the writer has closed. A write end is ready when the ring
      is not full or the reader has hung up. Descriptors 0-2 answer through
      their `Source` and `Stream` when they are pipes. A file is always
      ready.
    - Busy flags: hold `busy_r`/`busy_w` on each handle for the length of the
      poll. A second waiter would displace the first on `park_receiver` and
      panic on `park_sender`. A handle already busy is `Err(Busy)`, and a
      `Read` during the poll gets `Err(Perm)`, as it already does.
    - Timeout: fix the scheduler first. A waiter that is both timed and
      listed is resumed twice today, because `sched_tick` does not remove it
      from the wake table and `sched_wake` does not remove it from the timer
      queue. Each path must remove the other registration.
    - Signals: `Poll` joins `Read`, `KeyRead`, `Sleep`, `Wait` and `ClipRead`
      as a call a signal abandons with `Err(Intr)`.
    - Not in scope: sockets, fetch bodies and keys wait on host calls, not
      channels, and are `Err(Unsupported)` until something needs them.
    - Also: `poll_fds()` in `proc/io.h`, `poll()` in the kit, and the
      syscall in `System_Calls.md` and `Concept.md` §3.5. Test: a case in
      `test/system/` for two pipes written in turn by two children, a
      timeout, `^C` during a poll, and `Err(Busy)`.

39. **`select.poll`, `select.select` on pipes, `communicate()`.**
    - `selectmod.cpp` gains a `poll` object (`register`, `modify`,
      `unregister`, `poll`), and `select.select` is rewritten over the same
      call. The wait is a request the driver performs, as `Wait` is.
    - `selectors.py` is already byte for byte and picks `PollSelector` by
      itself once `select.poll` exists. So does `subprocess._PopenSelector`.
      `communicate()` over two or three pipes, `capture_output=True` and
      `communicate(timeout=)` then work without touching `lib/`.
    - Remove the "Waiting for a descriptor" entry from §10 of `Manual.md`,
      and the `communicate()` limit from its `subprocess` section.
    - The harness clock is frozen, so a timeout expires only when the test
      passes a later `now` to `run()`.
    - Tests: `test_select`, `test_selectors` and the `communicate` cases of
      `test_subprocess`, without the socket ones.

## Not planned

- **`.pyc` files.** Compiling from source is fast enough that a cache costs
  more than it saves, and `marshal`'s code format is this interpreter's. `-B`
  and `PYTHONDONTWRITEBYTECODE` are accepted and change nothing.
