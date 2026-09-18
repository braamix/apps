# Python 3 — Reference Manual

For `python` on Braam: a Python 3.14 interpreter written from scratch for this
system — its own lexer, parser, compiler, bytecode and virtual machine, with
CPython's own standard library on top of it.

Everything here is what the interpreter actually does. §10 is the list of what
it does not do, and why.

---

## 1. Running it

| Command | What it does |
| --- | --- |
| `python` | read commands at a prompt |
| `python prog.py [arg]...` | run that program |
| `python -c '<code>' [arg]...` | run the code |
| `python -m <module> [arg]...` | run a module as `__main__` |
| `python - [arg]...` | read the program from stdin |
| `python -i ...` | keep the prompt when the program ends |
| `python -V`, `--version` | print the version |

A bare `python` is a prompt only when stdin is a terminal. Redirected, it reads
the whole of stdin as a program — as CPython does, and for the same reason: a
pipe is a script, not a session. `-i` asks for the prompt either way.

`sys.argv[0]` is the script's path, `-c`, or the module's file; the rest of the
command line follows it. Options stop at `-c` and `-m`, so everything after the
code or the module name belongs to the program.

Exit status is 0, the value `sys.exit()` was given, 1 for an uncaught
exception, and 130 for `KeyboardInterrupt`.

`sys.path` is the program's own directory (`.` for `-c`, `-m` and a pipe),
then `PYTHONPATH`, then the library shipped with the package. `site` is
imported before the program runs, as in CPython, and adds `exit`, `quit`,
`help`, `copyright`, `credits` and `license` to the builtins. It finds no
site-packages, since there are none.

Three demos ship as `share/` in the package: `hello.py`, `fizzbuzz.py` and
`guess.py`.

### Options

CPython's, and they mean what they mean there. Letters may be bundled:
`-OOq` is `-O -O -q`.

| Option | What it does |
| --- | --- |
| `-b`, `-bb` | a `BytesWarning` for `str(bytes)` and for comparing bytes with `str` or `int`; `-bb` makes it an error |
| `-B`, `-s` | accepted: nothing writes bytecode, and there is no user site-packages |
| `-E` | ignore every `PYTHON*` variable |
| `-I` | isolated: `-E`, `-P` and `-s` together |
| `-O`, `-OO` | drop `assert` and make `__debug__` false; `-OO` drops docstrings too |
| `-P` | leave the program's directory off `sys.path` |
| `-q` | no banner at the prompt |
| `-S` | do not import `site` |
| `-u` | stdout and stderr are written as each write happens |
| `-v`, `-vv` | say, on stderr, what each import loads and from where |
| `-W <arg>` | a warnings filter, appended to `sys.warnoptions` |
| `-X <opt>` | `dev`, `utf8`, `importtime`, `int_max_str_digits=<n>`, `warn_default_encoding`; any other is kept in `sys._xoptions` |

`sys.flags` says what was asked for.

### Environment

Read unless `-E` or `-I` is given. The shell's `NAME=value python …` prefix
sets one for a single run.

| Variable | Means |
| --- | --- |
| `PYTHONPATH` | directories, colon-separated, put on `sys.path` before the library |
| `PYTHONSTARTUP` | a file run before the first prompt, when there is no program |
| `PYTHONWARNINGS` | filters, comma-separated, before those `-W` gives |
| `PYTHONOPTIMIZE` | `-O`, or a number of them |
| `PYTHONVERBOSE` | `-v`, or a number of them |
| `PYTHONUNBUFFERED` | `-u` |
| `PYTHONSAFEPATH` | `-P` |
| `PYTHONINSPECT` | `-i` |
| `PYTHONDEVMODE` | `-X dev` |
| `PYTHONPROFILEIMPORTTIME` | `-X importtime` |
| `PYTHONINTMAXSTRDIGITS` | `-X int_max_str_digits` |
| `PYTHONWARNDEFAULTENCODING` | `-X warn_default_encoding` |
| `PYTHONBREAKPOINT` | what `breakpoint()` calls; `0` makes it do nothing |

`PYTHONDONTWRITEBYTECODE`, `PYTHONNOUSERSITE` and `PYTHONUTF8` are accepted
and change nothing: nothing writes bytecode, there is no user site-packages,
and text is always UTF-8. `PYTHONHOME` is not read; the library is found where
the package put it.

### Extra flags

Not CPython's; they print what the compiler saw and exit.

| Flag | What it prints |
| --- | --- |
| `--dump-tokens <f>` | the token stream |
| `--dump-ast <f>` | the parse tree, one node per line |
| `--dis <f>` | the bytecode |
| `--selftest` | the interpreter's own checks |

---

## 2. The prompt

```
$ python
Python 3.14.0 on Braam
>>> 2 ** 64
18446744073709551616
>>> _ // 3
6148914691236517205
```

The banner and both prompts go to **stderr**, so a redirected stdout holds only
what the commands printed; `-q` leaves the banner out. `sys.ps1` and `sys.ps2` are ordinary names and
changing one changes the prompt.

The value of an expression is printed through `sys.displayhook`, which also
binds `_`. `None` prints nothing.

A command is read a line at a time until it is complete. A suite stays open
until a blank line closes it — the same rule `codeop` states, so an open
bracket, an unterminated string, a trailing `\` and an unfinished block all
mean "keep typing" rather than "wrong".

`^D` on an empty line ends the session, as do `exit()`, `quit()` and
`sys.exit()`. A traceback does
not: the prompt comes back and `__main__` keeps what it had.

### Keys

The shell's keys, so they are the ones you already know.

| Key | Effect |
| --- | --- |
| `←` `→`, `^B` `^F` | move by a character |
| `Home` `End`, `^A` `^E` | start, end of line |
| `Backspace`, `^H` | delete the character before the cursor |
| `Alt`+`Backspace`, `^W` | delete the word before it |
| `Delete` | delete the character under the cursor |
| `^U` | delete to the start of the line |
| `^K` | delete to the end of the line |
| `^L` | clear the screen |
| `↑` `↓` | the last 32 lines typed |
| `^C` | abandon the line |
| `^D` | on an empty line, leave |

While a command runs the keyboard belongs to the console again, so `^C` there
is a `KeyboardInterrupt` in the program and not an edit.

---

## 3. The language

All of it, through Python 3.14, unless §10 says otherwise.

**Statements.** `if`/`elif`/`else`, `while`, `for`, `break`, `continue`,
`else` on a loop, `match` with every pattern (class, mapping, sequence, `|`,
guards, capture), `try`/`except`/`else`/`finally`, `except*` over exception
groups, `raise … from`, `with` and `async with` (several items,
parenthesised), `assert`, `del`, `pass`, `global`, `nonlocal`, `import` in
all its forms —
including PEP 810's `lazy import`, with `sys.lazy_modules` — `def`, `class`,
`lambda`, `return`, `yield`, `yield from`, `await`, and the `:=` walrus.

**Functions.** Defaults, `*args`, `**kwargs`, keyword-only after `*`,
positional-only before `/`, annotations, decorators, closures, `functools`
wrappers, and recursion to a limit of 200 frames.

**Classes.** Multiple inheritance with C3 linearisation, `super()` with and
without arguments, metaclasses, `__slots__`, properties, class and static
methods, descriptors, `__init_subclass__`, `__set_name__`, `__mro_entries__`,
abstract base classes, and every special method — arithmetic, comparison,
container, context-manager, `__getattr__`/`__setattr__`, `__call__`,
`__format__`, `__index__`, `__buffer__`.

**Generators and coroutines.** Generators, generator delegation, generator
expressions, `send`/`throw`/`close`, `async def`, `await`, async generators
with `async for` and `async with`, asynchronous comprehensions, and PEP 525's
asyncgen hooks.

**Comprehensions** over lists, sets, dicts and generators, with several `for`
and `if` clauses, `async for`, and a scope of their own.

**Types and annotations.** PEP 649 lazy annotations — an annotation is not
evaluated until something asks, through `__annotate__` and `annotationlib`; PEP
695 `type X = …`, `def f[T]()` and `class C[T]`; `typing` with `Generic`,
`Protocol`, `TypedDict`, `NamedTuple`, `overload`, `get_type_hints` and the
rest.

**Text.** f-strings with conversions, `=`, and specs nested inside specs;
t-strings (PEP 750); the whole format-spec mini-language behind `format()`,
`str.format`, `%` on `str` and on `bytes`; `str` is Unicode by codepoint, with
`unicodedata` over UCD 16.0.0.

**Numbers.** `int` at any width — `2**1000` is exact — `float`, `complex`,
`bool`, `//` and `%` that floor, `pow(a, b, m)`, `divmod`, bitwise operators
over infinite two's complement, `decimal`, `fractions`, `statistics`.

---

## 4. Built-in functions

```
abs aiter all anext any ascii bin bool breakpoint bytearray bytes callable chr
classmethod compile complex delattr dict dir divmod enumerate eval exec
filter float format frozendict frozenset getattr globals hasattr hash hex
id input int isinstance issubclass iter len list locals map max memoryview
min next object oct open ord pow print property range repr reversed round
sentinel set setattr slice sorted staticmethod str sum super tuple type
vars zip
```

`__import__` and `__build_class__` are there too, and `True`, `False`, `None`,
`Ellipsis`, `NotImplemented` and `__debug__` are the constants. `site` adds
`exit`, `quit`, `help`, `copyright`, `credits` and `license`, unless `-S`. `_` appears at the prompt,
once an expression has printed.

`frozendict` (PEP 814) and `sentinel` (PEP 661) come from CPython's main
branch rather than from 3.14: the library shipped here is written against
that branch and asked for them.

Every built-in type has its methods: `str`'s forty-odd, `bytes`, `bytearray`,
`memoryview`, `list`, `tuple`, `dict` with its three views, `set`,
`frozenset`, `int`, `float`, `complex`, `range`, `slice`.

The exception hierarchy is CPython's, from `BaseException` down, including
`ExceptionGroup` and `BaseExceptionGroup`, the `OSError` subclasses and the
warning categories.

---

## 5. Modules

### Written in C++, inside the binary

```
_abc _ast _blake2 _codecs _collections _colorize _contextvars _csv
_functools _imp _io _math_integer _md5 _operator _pickle
_posixsubprocess _random _sha1 _sha2 _sha3 _signal _sre _string
_struct _thread _tokenize _types _typing _warnings _weakref array
atexit binascii builtins cmath dis errno faulthandler gc itertools
marshal math posix select sys time unicodedata
```

`sys.builtin_module_names` is that list.

### CPython's own, byte for byte

235 files ship as `lib/`, each recorded in `lib/manifest.txt` with the commit
it was taken from. The ones you reach for:

| Area | Modules |
| --- | --- |
| Text | `re`, `string`, `textwrap`, `difflib`, `unicodedata`, `codecs`, `encodings`, `html`, `quopri` |
| Data | `collections`, `dataclasses`, `enum`, `heapq`, `bisect`, `copy`, `pickle`, `pickletools`, `marshal`, `pprint`, `reprlib`, `types`, `weakref`, `queue`, `graphlib`, `dbm` (`dbm.dumb` only) |
| Numbers | `decimal`, `fractions`, `statistics`, `numbers`, `random`, `struct` |
| Files | `os`, `os.path`, `pathlib`, `io`, `shutil`, `tempfile`, `glob`, `fnmatch`, `stat`, `csv`, `json`, `base64`, `binascii`, `configparser`, `tomllib`, `mimetypes`, `zipfile` (stored members only) |
| Time | `datetime`, `calendar`, `time`, `locale`, `gettext`, `sched`, `timeit` |
| Functions | `functools`, `itertools`, `operator`, `contextlib`, `abc` |
| Async | `asyncio`, `concurrent.futures`, `contextvars`, `threading` |
| Types | `typing`, `annotationlib`, `inspect`, `ast`, `tokenize`, `token`, `keyword`, `dis`, `numbers`, `copyreg` |
| Tools | `argparse`, `logging`, `unittest`, `traceback`, `warnings`, `linecache`, `platform`, `shlex`, `pkgutil`, `importlib`, `importlib.resources`, `runpy`, `codeop`, `code`, `cmd`, `optparse`, `getopt`, `site` |
| Addresses | `urllib.parse`, `ipaddress`, `uuid`, `http.cookies` |
| Crypto | `hashlib`, `hmac`, `secrets` |

**`sys.stdlib_module_names` is CPython's whole list and not this one** — it is
a frozen constant. What is importable is the native list above plus `lib/`.

Anything else is a `ModuleNotFoundError`; §10 says which of the big ones are
gone for good.

---

## 6. Files and text

`open()` in full: `r w a x` with `b`, `+`, buffering, `encoding`, `errors`,
`newline`, and `closefd`. Text is UTF-8 unless told otherwise, and the codecs
are real — `latin-1`, `cp1251`, `koi8-r`, `cp437`, `mac-roman`, `utf-16`,
`utf-32` and the rest of the `encodings` package, error handlers included.

The three streams are `_io` objects: `sys.stdout` is line-buffered to a
terminal and block-buffered to a pipe, `sys.stderr` is unbuffered and
`backslashreplace`. `sys.stdin` reads a line at a time; `input()` writes its
prompt to stdout.

Whatever is still open is flushed and closed when the program ends, after
`atexit`.

Paths are Braam's: `/` is the root, `/tmp` is emptied at boot, everything else
is in OPFS and survives a reload.

---

## 7. Processes, signals and time

`os` has what this system has: `getcwd`, `chdir`, `listdir`, `scandir`, `walk`,
`stat`, `lstat`, `mkdir`, `makedirs`, `remove`, `rmdir`, `rename`, `replace`,
`symlink`, `readlink`, `truncate`, `open`/`read`/`write`/`close`/`lseek`,
`dup`, `pipe`, `isatty`, `get_terminal_size`, `urandom`, `environ`, `getpid`,
`uname`, `kill`, `system`, `posix_spawn`, `waitpid`, `wait` and the `W*`
macros.

`subprocess` starts other programs, and so do `os.system` and `os.popen`.
Braam has no `fork`, so a child is one spawn: its program, its arguments, its
environment and its three streams, and nothing else. What that leaves out:

- **`communicate()` over two pipes or more**, including `capture_output=True`
  and `stdin=PIPE` with `stdout=PIPE`, is an `OSError`. It waits in
  `selectors`, and Braam has no call that waits for a descriptor to be ready.
  One pipe is fine: send stderr to a file, or `stderr=STDOUT`.
- **`preexec_fn`, `user`, `group`, `process_group`** and **`pass_fds`**
  above 2 raise `NotImplementedError`. A child is handed 0, 1 and 2 and no
  other descriptor, so `close_fds` is always true.
- **A signal death is not a signal.** A child that `kill()` or `terminate()`
  ended reports the status 130, as Braam records it, and `returncode` is 130
  rather than `-9`.
- **A `^C` reaches this process, not the child.** `subprocess.run` then kills
  the child, as it does for any exception.

A child starts with `os.environ` as it is, so a variable set there reaches it.

`signal.signal` takes a handler for any name, but **only `SIGINT`, `SIGTERM`
and `SIGWINCH` are ever delivered** — Braam has no others. A handler runs at
the next instruction boundary, so a `^C` inside a long computation is raised
where the loop is, and `time.sleep` is abandoned by one.

`time.time()` is the wall clock, `time.monotonic()` and `perf_counter()` are
the process clock, and `time.sleep()` parks the process. `time.strftime` and
`datetime` work off the timezone the system reports.

---

## 8. Limits

| | |
| --- | --- |
| Recursion | 200 frames (`sys.setrecursionlimit` raises it) |
| Memory | 100 MB, the kernel's cap on a process |
| `sys.maxsize` | 2\*\*31 − 1 — this is a 32-bit machine |
| `int` | unbounded; `2**31` and up is a heap object of the same type |
| `float` | IEEE double, and `repr` round-trips |
| `str` | one codepoint per character, to U+10FFFF |
| Bytecode | one opcode and one 32-bit argument per instruction |

---

## 9. Errors

A traceback reads as CPython's does, ending in the exception's own line:

```
Traceback (most recent call last):
  File "prog.py", line 3, in <module>
    main()
ZeroDivisionError: division by zero
```

A `SyntaxError` names the file, the line, the text and the column:

```
  File "prog.py", line 1
    x ===
        ^
SyntaxError: invalid syntax
```

There is no `~~~^^^` anchor line under the failing expression, and no
"did you mean" suggestion.

---

## 10. What is not here

### Because Braam has no such thing

- **Threads.** A process is one Web Worker. `threading` and `_thread` are
  there and their locks are never contended, so a program written against
  them runs — but nothing runs in parallel. Concurrency here is `asyncio`.
- **`fork` and `exec`.** A process can start another but cannot become one;
  §7 says what `subprocess` does instead.
- **Sockets and everything over them**: `socket`, `ssl`, `urllib`, `http`
  (but `http.cookies`),
  `ftplib`, `smtplib`, `socketserver`, `xmlrpc`.
- **Waiting for a descriptor.** `select.select` answers for regular files,
  which are always ready, and raises `OSError` for anything else; `selectors`
  imports and waits on nothing else either.
- **`ctypes`, `mmap`, `dlopen`, C extension modules.** There is no stable ABI
  to offer and nothing to load. Anything CPython writes in C is either
  written in C++ here or taken from the pure-Python version beside it.
- **`setenv`.** `os.environ` reads what the process was given; writing one
  reaches a child started here, but `os.putenv` alone reaches nothing.
- **`signal.alarm`, `SIGUSR1` and the rest.** Only three signals exist.
- **`curses`, `tkinter`, `turtle`, `webbrowser`, `multiprocessing`,
  `sqlite3`**, and so every `dbm` backend but `dbm.dumb`.

### Because they are not written yet

- **`zlib`, `gzip`, `bz2`, `lzma`, `tarfile`** — no compression of any
  kind, so `zipfile` reads and writes stored members only.
- **`email`, `xml`, `symtable`**, and of `urllib` only `parse`.
- **`pdb`, `doctest`, `trace`, `cProfile`, `tracemalloc`** — and
  `sys.settrace` and `sys.setprofile`, which they need. So `breakpoint()`
  finds no `pdb` and says so with a `RuntimeWarning`, as CPython does when
  `PYTHONBREAKPOINT` names something that is not there.
- **`pydoc`**, so `help()` fails when it is called.
- **`.pyc` files.** `sys.dont_write_bytecode` is true and nothing writes a
  cache; every run compiles from source, which is fast enough that the cache
  would cost more than it saves.

### Differences you can see

- **`pickle` is the pure-Python pickler.** `_pickle` holds only
  `PickleBuffer`, and answers for `Pickler`, `dumps` and the rest with
  `pickle.py`'s own. A pickle is CPython's format and loads there, with two
  exceptions: a `map` or `filter` object pickles as an iterator over what it
  has left, since both are eager here, and a structure nested more than about
  fifty deep exceeds the recursion limit.
- **`marshal` writes CPython's format for data, and no code objects**, which
  are this interpreter's own.

- **`SyntaxWarning` is never raised.** `x is "s"` and an invalid escape
  compile quietly.
- **`from __future__ import` sets no flag in `co_flags`**, so a prompt cannot
  carry a future from one command to the next.
- **`locals()` in a function is a fresh snapshot**, PEP 667's behaviour: two
  calls are two dicts, and what `exec("x = 1")` writes into one is dropped.
- **`dis` is this interpreter's**, not CPython's: the instruction stream is
  different, so the opcode names and `dis` output are too, and `opcode` and
  `_opcode` do not exist.
- **A coroutine never awaited is reported when the collector finds it**, not
  when the last name to it goes, so the line the `RuntimeWarning` names is
  where the program was then.
- **`sys.getrefcount` is not a refcount.** Collection is mark-and-sweep, so an
  object dies at a collection rather than at the last name — a `__del__` runs
  later than CPython's would.
- **`id()` is an address in this process** and is reused after a collection.
- **`faulthandler` cannot catch a fault.** A fault is a wasm trap, and a
  trap ends the process before anything can run, so `enable()` only records
  the request. `dump_traceback()` works; `dump_traceback_later()` raises,
  having no second thread to wait in; `register()` is absent, as on Windows.
- **`-v` names where a module came from**, not the loader object CPython
  prints, and `-X importtime` counts in whole milliseconds, which is the
  process clock's grain.
- **An instance used as a dict key or set member hashes by identity**, even
  where its class writes `__hash__` and `__eq__`: two equal objects are two
  keys. `ipaddress.collapse_addresses` is one casualty.
- **A wait with a timeout does not wait**, since nobody else could wake it.
  `queue.Queue.get(timeout=1)` on an empty queue spins on the clock until the
  timeout has passed, then raises `Empty`.

---

## 11. A worked example

```python
#!/pkg/bin/python
"""Word frequencies, most common first."""

import collections
import re
import sys


def counts(text):
    return collections.Counter(re.findall(r"[\w']+", text.lower()))


def main(argv):
    if len(argv) < 2:
        print(f"usage: {argv[0]} <file>...", file=sys.stderr)
        return 1
    total = collections.Counter()
    for name in argv[1:]:
        with open(name, encoding="utf-8") as f:
            total += counts(f.read())
    for word, n in total.most_common(10):
        print(f"{n:6}  {word}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
```

---

## 12. See also

- [README.md](README.md) — how the interpreter is built, and what had to
  change for this system.
- [examples/](examples/) — the three demos the package ships.
- [LICENSE](LICENSE) — MIT for the interpreter, PSF for what came from
  CPython.
