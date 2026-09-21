# The test suite

Thirty-one node drivers over a Braam kernel booted in-process, and about eight
hundred Python programs. There is no CPython to link against and no test
runner inside the interpreter: a driver boots a kernel, plants this `python`
as a command, runs a shell line, and compares what came back.

[../README.md](../README.md) is the implementation, [../Manual.md](../Manual.md)
the language, [../TODO.md](../TODO.md) what is known to be wrong.

## Running

    make test                                   # the whole tree
    make test TESTS=lang/python/test/pysmoke.mjs
    make longtest                               # the three slow drivers
    make test STRESS=1                          # and the collector pass

    node lang/python/test/pystdlib.mjs          # one driver by hand
    node lang/python/test/pystdlib.mjs jsons.py # one case of it

`make` first — a driver needs `build/lang/python/python.wasm` — and node
22.12. The kernel and the rootfs come from
[../../../test/sdk.mjs](../../../test/sdk.mjs): `$BRAAM_SDK`, or the SDK the
top Makefile fetched. A driver prints one `ok` line and exits 0, or prints the
first difference and exits 1.

Arguments a driver takes: a bare word names one case to run, where the driver
has a directory of them, and

| | |
| --- | --- |
| `--shard=i/n` | every nth case from i, so a long list runs as n tests side by side |
| `--bless` | rewrite this run's goldens, where a driver keeps them (`pylex`, `pyast`, `pydis`, `pycases`, `pyunit`, `pyrepl`) — read the diff |
| `--full` | `pyunicode` only: every codepoint against the host CPython |
| `--survey` | `pycases` only: run the whole CPython clone and tally what stops each file |
| `--kernel=`, `--rootfs=`, `--binary=` | override what is booted and planted |

Each driver is its own line in the `TESTS` variable at the head of the top
[Makefile](../../../Makefile), and the ones that shard are cut into four there
so none of them sets the length of the whole run.

Three of them are not in `TESTS` but in `LONGTESTS`, which `make longtest`
runs: [pycases.mjs](pycases.mjs), [runcases.mjs](runcases.mjs) and
[pyast.mjs](pyast.mjs). `pycases` is nearly all of it — seven minutes against
everything else in the tree costing a hundred seconds put together. Its shards
stay four whatever that costs, because a golden is blessed under the shard it
runs in.

## How a test runs

[pylib.mjs](pylib.mjs) is the harness every driver imports. `boot()` starts a
kernel, plants the binary as `/bin/py` (`exec` takes any path carrying a
well-formed stamp) and copies [../lib/](../lib/) to
`/pkg/store/python-0/lib`, where the installed package keeps it. Then:

```js
const r = run("/tmp/c.py", stdin, "VAR=x");  // a shell line, streams to files
r.out; r.err; r.status;                      // /tmp/o, /tmp/e, the exit status
```

`put(path, text)` and `get(path)` reach the store, `script(src)` is `put` plus
`run`, `same(what, got, want)` reports the first differing line, and
`golden(file, text)` compares against a file beside the driver or, with
`--bless`, writes it.

Four things follow from the harness and explain most of what looks odd here:

- **Output goes down a pipe, not to the grid.** Nothing echoes, so the
  transcript is exactly what the program printed — which is what makes it
  comparable with CPython's byte for byte.
- **A command line has sixty-three characters.** The harness keyboard is a
  64-key channel and `type()` posts a whole line without checking. Hence `py`
  rather than `python`, and a program planted as `/tmp/c.py` rather than
  passed with `-c`.
- **The clock is driven, not read.** `run()` pumps the kernel until it is
  idle, so nothing measures real time, and a program that computes without
  parking finishes inside one call — which is why `pyint.mjs` sends `^C` as a
  `kill -INT` from a second queued command line rather than typing it.
- **One boot serves every case in a driver.** Cases are planted into `/tmp`
  and run one after another, so what a case sees of the heap depends on what
  ran before it. That is why `pycases` goldens are blessed under the shard
  they run in.

## The four rulers

A test is only as good as what it is compared against. In descending order of
strength:

| ruler | used by | where |
| --- | --- | --- |
| **CPython runs the same program** | our own cases | `ast/`, `coro/`, `exec/`, `format/`, `gen/`, `lazy/`, `module/`, `number/`, `stdlib/`, `type/`, `unicode/` |
| **upstream's own expected output** | MicroPython's suite | `cases/` |
| **unittest's own listing** | CPython's test files | `cpython/` |
| **blessed from this interpreter** | what has no CPython to compare to | `dis/`, `repl.log`, `shim/selfcheck.res` |

The first is why our own cases are written to run unchanged on both
interpreters, and why a case may not use what this one has not got. The last
catches a change nobody meant; it cannot catch a difference from CPython, and
`dis/` is there because the bytecode is this implementation's own.

## The drivers

**The front end** — each case is a source file and a golden listing, and a
name ending `_err.py` is a source that must be refused, its golden the
complaint.

| | |
| --- | --- |
| [pylex.mjs](pylex.mjs) | `--dump-tokens` against goldens [tools/mklex.py](../tools/mklex.py) wrote with CPython's own `tokenize` |
| [pyast.mjs](pyast.mjs) | `--dump-ast` against goldens [tools/mkast.py](../tools/mkast.py) wrote with CPython's own `ast` |
| [pydis.mjs](pydis.mjs) | `--dis` against goldens blessed from this compiler |

`pylex` and `pydis` also push every upstream case through the tokenizer and
the compiler: a refusal there is a hole even where the case cannot run yet.

**The language, against CPython** — each case is a program that prints, run by
both interpreters and compared byte for byte.

| | |
| --- | --- |
| [pynumber.mjs](pynumber.mjs) | the number tower: long division, rounding, bitwise over infinite two's complement |
| [pyformat.mjs](pyformat.mjs) | the format-spec mini-language, `%`, `str.format`, f-strings |
| [pygen.mjs](pygen.mjs), [pycoro.mjs](pycoro.mjs) | generators and coroutines: delegation, `throw`, `asend`/`athrow`/`aclose` |
| [pytype.mjs](pytype.mjs) | descriptors, `__slots__`, metaclasses, finalizers, abstract bases |
| [pyexec.mjs](pyexec.mjs) | `compile`, `eval`, `exec`, and what a function and a code object answer to |
| [pymodule.mjs](pymodule.mjs) | the modules written in C++, printing only what every implementation agrees on |
| [pyunicode.mjs](pyunicode.mjs) | `unicodedata`, str by Unicode's rules, the codecs, source text; `--full` for every codepoint |
| [pylazy.mjs](pylazy.mjs) | PEP 810 lazy imports, against CPython 3.16 — the first to have the statement |

**The implementation, asserted in JavaScript** — what no printed program can
say, because the answer is about the native stack, the collector or the
process.

| | |
| --- | --- |
| [pygc.mjs](pygc.mjs) | `python --selftest`: the object heap and the collector, checked from inside the program |
| [pyfun.mjs](pyfun.mjs) | a builtin calling back into Python four thousand times without growing the native stack |
| [pyclass.mjs](pyclass.mjs) | a special method written in Python, under depth, garbage and a raise |
| [pymeth.mjs](pymeth.mjs) | the built-in types' methods, through subclasses, and `sort(key=)` as a continuation |
| [pyimport.mjs](pyimport.mjs) | the search path, `__file__`, `__path__`, the cache, and an import inside an import |
| [pyio.mjs](pyio.mjs) | `sys.stdin` and `input()` over a redirected file, files open at exit, signals during a sleep |
| [pyselect.mjs](pyselect.mjs) | where `select` differs from CPython's, which is `Sys::Poll` showing through |
| [pycompress.mjs](pycompress.mjs) | lzma and zstd, which the reference CPython was built without |
| [pyint.mjs](pyint.mjs) | `^C` reaching a running program, and a program catching it |

**The program around the language.**

| | |
| --- | --- |
| [pysmoke.mjs](pysmoke.mjs) | that it starts, answers its command line, and reports a status |
| [pyvm.mjs](pyvm.mjs) | the three ways a program arrives, what an error leaves on stderr, the exit status |
| [pyflags.mjs](pyflags.mjs) | every flag past `-c`/`-m`/`-i`, and the `PYTHON*` variables |
| [pyrepl.mjs](pyrepl.mjs) | the prompt: a session at the console and the same session down a pipe |
| [pyexamples.mjs](pyexamples.mjs) | the three demos in [../examples/](../examples/), run as a user would |

**The upstream suites.**

| | |
| --- | --- |
| [runcases.mjs](runcases.mjs) | MicroPython's `tests/`, against upstream's own expected output |
| [pycases.mjs](pycases.mjs) | CPython's `Lib/test/`, under CPython's own `unittest` |
| [pyunit.mjs](pyunit.mjs) | `unittest` and the shims themselves, before anything stands on them |
| [pystdlib.mjs](pystdlib.mjs) | CPython's library as we ship it, against the CPython it came from |
| [pystress.mjs](pystress.mjs) | every MicroPython case again under a collector that never waits |

## Where the cases live

| | | |
| --- | --- | --- |
| [cases/](cases/) | 449 | MicroPython's `tests/`, byte for byte, with a `.exp` beside each |
| [cpython/](cpython/) | 151 | CPython's `Lib/test/`, byte for byte, with a `.res` listing beside each |
| [stdlib/](stdlib/) | 57 | programs over the library in `lib/`, against CPython's output |
| [module/](module/) | 23 | the native modules, likewise |
| [ast/](ast/), [lex/](lex/), [dis/](dis/) | 25, 22, 24 | sources and their token, tree and bytecode listings |
| [exec/](exec/), [type/](type/), [gen/](gen/), [coro/](coro/), [format/](format/), [number/](number/), [unicode/](unicode/), [lazy/](lazy/) | 40 | one area apiece, each against CPython |
| [shim/](shim/) | | `test.support` and its helpers, written here — not a copy of anyone's code |

A case that imports something has the modules it imports beside it
(`cases/import/`, `lazy/mods/`), and the directory is planted whole. The data
files some CPython tests open are listed in
[cpython/data.txt](cpython/data.txt). `unicode/full/` is what `--full` runs,
including Unicode's own `NormalizationTest.txt` from `tmp/ucd/`.

## The three manifests

Neither upstream is committed here; what is taken from them is, with its
provenance in a manifest rather than in the file.

| | |
| --- | --- |
| [manifest.txt](manifest.txt) | MicroPython's cases: state, case, which interpreter wrote the golden, commit, upstream path |
| [cpython.txt](cpython.txt) | CPython's cases: state, case, result, commit, upstream path |
| [goldens.txt](goldens.txt) | which CPython wrote each golden of our own cases |

`state` is `pass` or `fail`. **A failing row is listed, not dropped** — that
way both a regression and an unpromoted fix are caught, and the driver refuses
to move a row for you. For a `pass` row of CPython's, `result` is
`<passing>/<ran>` test methods; for a `fail` row it is one word for what
stopped it (`import`, `syntax`, `crash`, `runtime`, …) and the `.res` golden
holds what it printed instead.

**A golden is not rewritten by an interpreter other than the one recorded**,
so a host upgrade cannot move the ruler by accident;
[../tools/pyref.py](../tools/pyref.py) keeps that record and `--regen` is the
override. `$PYTHON` names the reference interpreter, and `python3` on `PATH`
is the default.

## Adding a test

| | |
| --- | --- |
| [tools/mkexp.py](../tools/mkexp.py) `basics/andor.py` | copy a MicroPython test into `cases/`, write its `.exp`, add a `fail` row |
| [tools/mkcpy.py](../tools/mkcpy.py) `test_unary.py` | copy a CPython test into `cpython/`, add a row; `--with-package` brings its data files |
| [tools/mkfmt.py](../tools/mkfmt.py) `test/format/spec.py` | write the golden of one of our own cases with the host CPython |
| [tools/mklex.py](../tools/mklex.py), [tools/mkast.py](../tools/mkast.py) | write a token or tree golden with CPython's own `tokenize` and `ast` |
| `node test/pycases.mjs --bless` | fill in a `.res` listing and its row from this interpreter |

Then run the driver. A new row starts at `fail` and is moved to `pass` when it
passes; a copy is **never edited** to make it pass, and a case that needs
something this interpreter has not got stays as it is and says so.

A whole new driver needs three things: a file here that imports
[pylib.mjs](pylib.mjs), a line in the Makefile's `TESTS` — or `LONGTESTS`, if
it runs for more than a few seconds — and a row in the table above.

## The stress pass

`make test STRESS=1` sets `PY_STRESS`, which makes every case run a second
time with `PY_GC_STRESS=1` in its environment — a collector that collects at
**every** allocation — and adds [pystress.mjs](pystress.mjs) to the list.

This is the one test for the rule that C++ holding an object across an
allocation must pin it with a `Root`. A missing pin is invisible until a
collection lands in the gap; closing every gap turns it into a wrong answer.
Nine were found that way. A collection walks the whole live heap, so this is
minutes where the plain pass is seconds — ask for it after touching the
interpreter's C++, and let the plain run be the ruler the rest of the time.

## Reading a failure

- A difference prints `want` and `got` for the first line that differs. The
  rest of the case is not compared.
- `pycases` reports a row whose state or count changed as `broke` or `fixed`
  and does not touch the manifest; blessing is a separate, deliberate run.
- An address, a pid-derived temporary name and an elapsed time are normalised
  out of a `unittest` listing before it is compared. Nothing else is.
- `make test` writes the whole run to `test.log` and `make longtest` to
  `longtest.log`, and the last line of each names the failures.

## Licence

[cases/](cases/) is MicroPython's, MIT. [cpython/](cpython/) is CPython's,
under the PSF licence. [shim/](shim/), the drivers and our own cases are this
repository's, MIT. [../LICENSE](../LICENSE) carries all of them.
