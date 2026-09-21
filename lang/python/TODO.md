# TODO

Known defects and the enhancements that would answer them, each stated as what
is wrong, where the fix goes, and which tests it moves. `test/cpython.txt` is
the scoreboard: a `fail` row does not run at all, and a `pass` row's ratio is
how many of its methods answer.

Working rules. A module is taken from `tmp/cpython/Lib` byte for byte with a
row in `lib/manifest.txt`, and one that needs something missing waits for it
rather than being trimmed; its test comes in through `tools/mkcpy.py`; a task
ends by blessing the goldens and moving what it fixed out of §10 of
`Manual.md`. **A number is a name, not a position** — a finished task is
deleted and the rest keep their numbers, so the list has gaps. 1 to 39 are
spent.

## The interpreter

Five of these are one root cause: **plain C++ reached from the middle of the
VM cannot call Python**, because there is no way to park. Each needs a
`ContObj` and an arm in the opcode that reaches it, the way `cmp_seq` already
works for `==`.

40. **A `__hash__` written in Python, for dict keys and set members.**
    `py_hash` is C++, so an instance hashes by identity and two equal objects
    are two keys. Dict and set operations whose key is such an instance should
    park, call `__hash__`, then `__eq__` on each collision, and finish in C++.
    `ipaddress.collapse_addresses` gives the wrong answer for it, and so do
    five of `test_tracemalloc`'s nine failures, whose `_group_by` keys a dict
    by a `Traceback`. The most useful item in this file.

41. **A tuple or a list does not order by an item's Python `__lt__`.**
    `sorted([C(1), C(2)])` works because `cmp_sort` owns the loop and parks;
    `(0, C(1)) < (0, C(2))` raises `TypeError`, because `seq_order`
    ([src/tuple.cpp:176](src/tuple.cpp#L176)) is reached from the `order` slot.
    The fix is `cmp_seq`'s mirror — a `cmp_seq_order` beside it and one more
    arm in `Compare`. 23 of `test_pprint`'s 39 failures are this one thing:
    `_safe_key` writes `__lt__` and pprint sorts tuples of them.

42. **A native that takes an iterable refuses a generator.** `gen_type` has an
    `iter` slot and no `next` — only the VM can resume a generator — so
    `py_next` ([src/ops.cpp:442](src/ops.cpp#L442)) answers "'generator' object
    is not an iterator" for one. `set.difference_update(x for x in ...)` is
    where it shows, and twelve of `test_weakset`'s failures are that. The fix
    is a continuation the VM drives for native iteration, which every builtin
    taking an iterable would then reach.

43. **`__index__` is not called from C++.** Three places: a slice's bounds
    (`slice_resolve`, [src/iter.h:25](src/iter.h#L25)), native argument parsing
    except where a builtin spells out `redo_converted`
    ([src/call.h:138](src/call.h#L138)), and a *keyword* argument never. So
    `lst[X():]` is a TypeError where `X` writes `__index__`, which is ten of
    `test_xml_etree`'s methods. The message is wrong too — "an integer is
    required" where CPython says `__index__ returned non-int (type str)`.

44. **A binary operator does not try a proper subclass's reflected dunder
    first.** Where `type(b)` is a proper subclass of `type(a)`, CPython asks
    `b.__radd__` before `a.__add__`; here the built-in's operator answers, so
    `1 + IntSub()` and `[1] + ListSub()` differ. `test_binop` and
    `test_richcmp` show it.

45. **Assigning `frame.f_lineno` does not jump.** Ninety-eight of
    `test_sys_settrace`'s methods are `JumpTestCase`, and pdb's `jump` command
    waits on the same thing.

46. **The line an implicit `return None` is reported at.** CPython's compiler
    duplicates the return into each branch that falls into it, so each copy
    carries that branch's last line; there is one copy here carrying the last
    line the body emitted code for. Most of what `test_sys_settrace` still
    fails on besides the jumps.

47. **An uncaught `MemoryError` crashes the process while it is being
    reported.** `tempfile.mkdtemp(dir=b"/tmp")` is the shortest way to it: the
    eager `map` exhausts the heap and the report then traps. Reporting has to
    run without allocating. Both `test_tempfile` and `test_xml_etree` are
    `crash` rows for it.

48. **`sys.monitoring`'s `DISABLE` is honoured only for `PY_START` and
    `PY_RESUME`.** Everywhere else it names one line or one instruction and
    there is nowhere to record that, so a tool that returns it to go faster
    does not. Recording a per-instruction disable set on the code object is
    the work.

49. **The recursion limit is 200**, which is what the native stack holds.
    `pickle.py` spends four frames per level of a list, so a structure nested
    past about fifty deep raises `RecursionError`. Either cut what a Python
    frame costs in native stack, or find the frames `pickle` need not spend.

50. **`__length_hint__` is not answered by the built-in iterators**, so
    `operator.length_hint(iter([1] * 10))` is 0 rather than 10. Twenty-one of
    `test_iterlen`'s 22 failures.

51. **`property` carries no `__doc__` or `__name__`.** `test_property` is 12
    of 31 for it, mostly `AttributeError: __name__ is not set` and a docstring
    that does not copy.

## Memory

52. **Reserved memory ratchets up to the cap.** braam-core's allocator never
    returns a span once a size class has taken it, so a burst of small objects
    leaves spans that a later multi-megabyte buffer cannot use: `test_pickle`
    reaches 102 MB reserved with 11 MB in use and its framing tests then raise
    `MemoryError`. Which subtest tips over depends on what ran in that worker
    before it, so those goldens are blessed per shard and move whenever a case
    is added — `test_tarfile` in `--shard=4/4` cannot build a `.tar.xz` in
    `setUpModule` at all. The fix is in braam-core: return a class's empty
    spans to the free runs. It also retires the workarounds each codec module
    carries — a collection before reporting no memory, and lzma's before a
    decompressor's first step — and the preset that comes down until it fits,
    which is what puts `zipfile.ZIP_LZMA` out of reach.

## Objects and buffers

53. **`bytes(array.array(...))` takes the items, not the buffer.**
    `bytes(array("I", [1, 2, 3]))` is three bytes here and twelve in CPython.
    `memoryview(a)` is right, so the buffer is there and only `bytes()` does
    not ask. `_io`'s `write` refuses an array for the same reason:
    `buffer_arg` ([src/bytemeth.cpp:484](src/bytemeth.cpp#L484)) uses
    `bytes_like` where `buffer_like` would take one. `test_gzip` shows both.

54. **`memoryview.cast()` takes no keywords**, so `m.cast("B", shape=[8, 8,
    4])` is a TypeError. `test_gzip` again.

## The OS layer

55. **`os.utime` does nothing.** `Sys::Touch` moves a file's mtime to now,
    which answers `os.utime(path)` and `os.utime(path, None)`; an arbitrary
    time needs a kernel call that does not exist yet. Most of
    `test_tarfile`'s remaining failures.

56. **A directory has no modification time**, which `os.stat` reports as 0 —
    a braam-core task. It is why `shutil.make_archive(..., "zip", ...)` over a
    tree raises "ZIP does not support timestamps before 1980".

## Modules

57. **The CJK codecs are not written.** `encodings` here is the single-byte
    pages, the UTF forms and the transforms; `euc-jp`, `shift_jis`,
    `iso-2022-jp`, `gb2312`, `big5` and `cp949` each stand on a C codec —
    `_codecs_jp` and its four siblings — that nobody has written. So
    `email.charset.Charset('euc-jp')` raises. Six of `test_email`'s eight
    remaining methods, three of `test_contentmanager`'s, and the whole of
    `test_asian_codecs`.

58. **Replace `src/colorizemod.cpp` with CPython's `_colorize.py`.** The C++
    stand-in dates from before `dataclasses` shipped, and its `_NoColor`
    answers every attribute with itself — so `default_theme.traceback.items()`
    is not iterable and `test_traceback` cannot even import. `dataclasses.py`
    is in `lib/` now, so the module can be upstream's, byte for byte.

59. **`importlib.metadata` is not shipped**, so `importlib.invalidate_caches()`
    fails on the import it does.

60. **`_bisect` does not exist.** `bisect.py` falls back to its Python half,
    and most of the 23 of `test_bisect`'s 46 methods that fail do so with
    `AttributeError: 'NoneType' object has no attribute 'bisect_right'`.

61. **`_symtable` has no `DEF_IMPORT`**: an import binds the way an assignment
    does in `symtab.cpp` and nothing records which it was, so
    `Symbol.is_imported()` answers False. `_symtable.symtable` also takes only
    source and not an AST object, which is eight of `test_symtable`'s methods.

62. **`sysconfig`'s scheme paths are CPython's, not this library's.**
    `get_path('stdlib')` expands to `/pkg/lib/python3.14` because the template
    is `{installed_base}/{platlibdir}/python{py_version_short}`, one component
    longer than `<store>/lib`. `sys._stdlib_dir` is where the library really
    is, and `pydoc` and `trace` use the first to decide what is library code,
    so neither recognises one here.

63. **A native carries no `__text_signature__`**, so `inspect.signature` has
    nothing to read and `help()` on a builtin says `(...)`. It is a body of
    data rather than of code, and the largest thing `test_pydoc` fails on.

64. **zstd's dictionaries are untrained.** `zdict.h`, which holds the COVER
    trainer, is not in the library, so `train_dict` and `finalize_dict` build
    a content-only dictionary — real, but with no entropy tables and so no
    `dict_id`.

65. **One expat corner is left**: where a non-standalone document's external
    parameter entity was read and a `NotStandaloneHandler` is set, that
    handler is not called. The four other changes a suspending driver needed
    are marked in `src/expat/`.

## The command line

66. **`python <directory>`** says "is a directory" instead of running the
    directory's `__main__.py`, and **`python -m`** refuses a module name that
    is not ASCII. `test_argparse` shows both.

## The test suite

67. **Widen `test/shim/test/support/`.** Eight `fail` rows are test plumbing
    and nothing else: `is_resource_enabled` (`test_decimal`),
    `hashlib_helper` (`test_hashlib`, `test_hmac`), `LARGEST`
    (`test_ipaddress`), `run_code` (`test_type_aliases`, `test_type_params`),
    and a case importing a sibling test module — `test_genericpath`
    (`test_posixpath`), `test.test_string` (`test_tstring`),
    `test.typinganndata` (`test_grammar`). The last three want `mkcpy.py` to
    plant the sibling the way `--with-package` plants a package.

68. **`test_pstats` crashes** four methods in, and nothing says why yet.

69. **`test_subprocess` is out of the suite.** It imports now that `sysconfig`
    does, but two hundred of its methods spawn a child apiece and the harness
    does not reach the end, so there is no golden and the run stops instead.
    What it waits on is a run that ends.

## Differences that are not tasks

These follow from the design and are recorded so they are not rediscovered.
`Manual.md` §10 states each one.

- **There is no refcount.** An object dies at a collection, so a `__del__`
  runs late, a generator abandoned unfinished is closed by the collector
  rather than at the last name, a coroutine never awaited is reported where
  the program then was, and `tracemalloc.get_traced_memory` falls at the
  sweep. Two of `test_sys_setprofile`'s methods and two of
  `test_tracemalloc`'s are that.
- **`tracemalloc` traces objects, not raw blocks**, and a traced size is
  `sys.getsizeof`'s — the size class the object was rounded up to — so
  `test_get_traces`, which looks a trace up by its exact size, finds none.
- **Expat reports character data in different pieces than CPython**, where
  `buffer_text` is off: suspending the parse at a handler moves where it
  breaks a run of text. Every handler that matters concatenates.
- **The profilers count calls, not time.** `_lsprof`'s timer is `proc_now()`
  at whole milliseconds, and a timer written in Python is refused because it
  would be a call per event.
- **`.pyc` files.** Compiling from source is fast enough that a cache costs
  more than it saves, and `marshal`'s code format is this interpreter's. `-B`
  and `PYTHONDONTWRITEBYTECODE` are accepted and change nothing.
- **Threads, sockets, `ctypes`, `mmap` and C extension modules**, for which
  Braam has nothing underneath.
