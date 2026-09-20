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

All four libraries are in the SDK as of 0.10.280, so none of this is a codec to
write: each task is a native module over a library that is already there, asked
for by name — `LIBS braam::zlib braam::bzip2 braam::lzma braam::zstd`. Three
things follow, and they are why this stage is now one stage and not two:

- **Every one of them is upstream's output byte for byte**, so the tests that
  compare against it run rather than being excluded.
- **The calls are synchronous**: a `step` computes and returns and never
  awaits, which is what lets the VM reach them from a slot.
- **The state is large and lives on the heap.** Each task below says how
  large. The 100 MB cap is the whole process, and braam-core's allocator never
  gives a span back once a size class has taken it, so the ratchet under
  "Found along the way" decides whether a codec buffer fits at all.

What each costs the binary is at most its archive — 60 KB for zlib, 46 KB for
bzip2, 197 KB for lzma and 467 KB for zstd, against `python.wasm`'s 2.9 MB
today — and less where `--gc-sections` drops what the module never names.
`Programming_Manual.md` §6 documents all four.

19. **`zlib`, native, on `braam::zlib`.** The library is zlib 1.3.2.1
    rewritten in C++, and deflate's output is zlib's for the same level,
    strategy, window and memory level.
    - `Deflater` and `Inflater` are `compressobj` and `decompressobj`:
      `step` over a span of input and a span of output, advancing both past
      what it used. `wbits` chooses `ZFormat::Raw`, `Zlib` or `Gzip` and the
      window bits; `ZFormat::Auto` is `wbits=47`.
    - `unused_data`, `unconsumed_tail` and `eof` are bookkeeping over what
      `step` left in `in`. `ZStatus::Stuck` is `Z_BUF_ERROR` and is not an
      error — it is a call with no input to take or no room to fill.
      `Corrupt` is `zlib.error`, with `why()` as the message.
    - `compress` and `decompress` are `zlib_compress`/`zlib_uncompress`,
      whose `limit` is what `decompress`'s `bufsize` grows into.
    - `copy()` is `copy_from`, the flush modes are `ZFlush`, and
      `set_dictionary`, `params`, `prime`, `bound` and `ZHeader` answer the
      rest of the module.
    - `crc32` and `adler32` are `crc32_update` and `adler32_update`, with
      `crc32_combine` beside them, and `binascii.crc32` moves onto the same
      pair rather than keeping a table of its own.
    - A `Deflater` is 262 KiB of heap at the defaults and an `Inflater` about
      7 KiB plus its window. Free it when the object is collected.
    - `ZLIB_VERSION` is `"1.3.2.1"`, which is what this really is.
    - Test: `test_zlib`, whole.

20. **`compression`, `gzip`.** The `compression` package (`_common._streams`,
    `zlib`, `gzip`), `gzip`, and `python -m gzip`. Test: `test_gzip`.

21. **`bz2`, native `_bz2` on `braam::bzip2`.** libbzip2 1.0.8 rewritten in
    C++, output byte for byte for the same block size.
    - `BzCompressor` and `BzDecompressor` step as zlib's pair does, with two
      rules of bzip2's own. A flush or finish answers `More` until it is
      done and must be called again with the same action and the input
      untouched, which is what `BZ2Compressor.flush()` has to loop over. And
      a decompressor stops at the end of one stream, so `bz2.decompress`
      re-`init()`s while bytes remain — that is `eof` and `unused_data`.
    - Memory is the catch. A compressor at block size 9, which is
      `BZ2Compressor`'s default and `BZ2File`'s, is 7.6 MB, and a
      decompressor 3.7 MB, or 2.4 MB in `init(true)`'s small mode. Against
      the cap that is a handful of open files at once, and `test_bz2` opens
      several.
    - `bz_crc_update` is bzip2's CRC-32, most significant bit first and not
      zlib's. Nothing in `bz2.py` needs it.
    - Then `compression.bz2` and `bz2`, and `zipfile`/`tarfile`/`shutil` pick
      it up. Test: `test_bz2`.

22. **`lzma`, native `_lzma` on `braam::lzma`.** Not a rewrite: liblzma from
    xz 5.8.4 vendored verbatim, so the output is what `xz -T1` writes.
    - `lzma/lzma.h` is liblzma's own C API whole, which is what `_lzma`
      wants: `lzma_stream`, the filter chains behind `FORMAT_RAW` and
      `_encode_filter_properties`, `lzma_str_to_filters` and the index.
      `lzma/xz.h`'s `XzEncoder`/`XzDecoder` pair is the shorter road for
      `FORMAT_XZ` and `FORMAT_ALONE`, and both headers reach the same code.
    - **The default preset does not fit.** An encoder is 93 MiB at preset 6,
      which is `lzma.PRESET_DEFAULT`, against a 100 MB cap that also holds
      the interpreter; 7 to 9 fit in nothing at all, and 0 to 3 stay under
      32 MiB. `memusage()` gives the figure before anything is allocated, so
      `_lzma` can refuse with `MemoryError` rather than trap. Then decide
      whether a compressor with no preset means 6 and fails, or means
      something lower and writes bytes CPython would not — and record the
      answer in §10 of `Manual.md` under "Differences you can see".
    - A decoder is about the size of the dictionary, 256 KiB to 64 MiB, and
      takes a memory limit as `init()`'s second argument, which is what
      `LZMADecompressor`'s `memlimit` becomes.
    - A decoder is told when the input ends: `End` comes only on the step
      carrying the last of it, because `.xz` and `.lz` may be several streams
      in a row and are read as one output.
    - There are no threads, so `lzma_stream_encoder_mt` is a compile error at
      the call and `lzma_physmem()` answers 0. `_lzma` names neither.
    - Then `compression.lzma` and `lzma`. Test: `test_lzma`, less whatever
      the preset decision excludes.

23. **`zstd`, native `_zstd` on `braam::zstd`.** libzstd 1.6.0 vendored
    verbatim, for Zstandard (RFC 8878). New in 3.14 and new in this file: it
    was not here before because the library was not.
    - `compression.zstd` (`__init__.py`, `_zstdfile.py`) over a native
      `_zstd`: `ZstdCompressor`, `ZstdDecompressor`, `ZstdDict`,
      `get_frame_info`, `get_frame_size`, `set_parameter_types`, the
      `CompressionParameter`, `DecompressionParameter` and `Strategy` enums,
      `zstd_version` and `ZSTD_CLEVEL_DEFAULT`.
    - There is no Braam-shaped pair over this one: `zstd/zstd.h` is libzstd's
      C API, a context and two cursors, called as C calls it.
    - Three things bite. `ZSTD_decompressStream` answers 0 when a frame ends
      and not after, so the loop must stop on "input used up and 0" rather
      than call once more. `ZSTD_getErrorName` returns a `const char *` and
      nothing here defines `strlen`, so the length is counted in a function
      marked `__attribute__((no_builtin("strlen")))`. And there are no
      threads, so `CompressionParameter.nb_workers` accepts 0 alone.
    - `train_dict` and `finalize_dict` are the dictionary builder
      (`zdict.h`), which is not in the library: raise rather than pretend,
      and say so in §10. `ZstdDict` itself works — `ZSTD_createCDict` and
      `ZSTD_createDDict` are there.
    - Memory and size: a compressor is 3.5 MiB at level 3, the default, and
      89.5 MiB at 19, so 20 to 22 fit in nothing. Set `ZSTD_d_windowLogMax`
      on every decompressor, or a frame claiming a 128 MiB window fails as an
      allocation instead of as `frameParameter_windowTooLarge`. Naming
      `ZSTD_compress` links every strategy's match finders, 320 KB, because
      the level is a run-time choice; `ZSTD_decompress` alone is 57 KB.
    - Test: `test_zstd`, without the dictionary-builder and thread cases.

24. **`tarfile`, the archive half of `shutil`, and `zipfile`'s test.**
    `zipfile` is already here, since `importlib.resources` imports it, and
    has handled stored members only; with tasks 19 to 23 done it gains
    `ZIP_DEFLATED`, `ZIP_BZIP2`, `ZIP_LZMA` and `ZIP_ZSTANDARD`. `tarfile`
    guards `pwd` and `grp`. `shutil` is already shipped, so `make_archive`
    and `unpack_archive` start working once `tarfile` is here, with `gztar`,
    `bztar`, `xztar` and `zstdtar` all registered. Tests: `test_zipfile/`,
    `test_tarfile`, and the archive cases of `test_shutil`.

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
