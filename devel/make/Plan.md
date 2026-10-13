# make — a plan for Braam's POSIX make

`devel/make` is V7 make 2.58 being grown into **Braam's POSIX make**:
POSIX.1-2024's `make`, then the half of GNU make that most makefiles lean on,
on V7's code. Phase A is done and in `2.58-r1` — where POSIX overrules V7, and
what nearly every makefile uses — and phase B in `2.58-r2`, the rest of POSIX
but `-j`; [README.md](README.md) has the tables. Phases C and D are left,
ordered by how soon a real makefile reaches for each feature; each ends green
and is published on its own.

## 1. Rules for every phase

1. **POSIX wins, V7's code stays.** Behaviour follows POSIX wherever the two
   disagree, with no switch to get V7's back. What stays is V7's code, its
   identifiers and structure, its comments, and its own text where POSIX does
   not specify any — the `Make: ... Stop.` messages, `` `x' is up to
   date.``, `-p` and `-d`. A case's expectation changes only where POSIX
   demands it, and the commit that changes it says which rule required it.
2. **POSIX.1-2024 first, then GNU.** Phase B finishes POSIX's `make`, and
   phase C is the GNU make most makefiles are written for. BSD make's dialect
   (`.if`, `:M`) is not a goal.
3. **Await only where something parks.** A `co_await` is a call and not a
   tail call, so a loop that awaits without parking grows the native stack
   until the process traps. `yyparse()` is a coroutine and awaits `include`
   and `!=` itself, both of which park on a system call; the lexer,
   `nextlin()` and `subst()` stay plain, since they run once per token, line
   and macro reference and almost never need to wait. §3 has the two
   mechanisms.
4. **The README keeps up.** Each addition goes in its "Beyond V7" table with
   the standard it comes from, and each POSIX difference in "Where POSIX
   overrules V7".
5. **Version:** `2.58-r2` after phase B, `-r3` after C, and so on. There is no
   upstream version to move, so only the revision counts up.

## 2. Phases

### Phase B — the rest of POSIX, and the limits

| Feature | Where | Notes |
|---|---|---|
| Growing limits | `misc.cpp`, `gram.cpp`, `doname.cpp`, `files.cpp`, `dosys.cpp` | §2.1 |
| `$(@D)`, `$(@F)`, and `D`/`F` on `< * ? % ^` | `subst()` | Each word of the value; a word with no `/` has `.` for its directory |
| `+` under `-t` and `-q` | `docom()` | Runs, as under `-n`; `-t` then touches the target |
| `-C dir` | `make_main` | `b_chdir()` before any makefile is read, in order with other `-C`s; not passed in `MAKEFLAGS`. Not POSIX, but every make has it |
| `.WAIT` | `doname()` | A prerequisite named `.WAIT` is skipped; it orders nothing until phase D's `-j` |
| `.POSIX`, `.NOTPARALLEL`, `.SCCS_GET` | — | Already harmless special targets; each gets a case that says so |
| `!=` | `eqsign()`, `nextlin`, `yyparse()` | §3.1: the command is expanded, run by `sh -c`, and its output's newlines become spaces, a trailing one dropped |
| `:::=` | `eqsign()`, `assign()` | POSIX.1-2024: expanded now, each `$` of the result doubled, and kept as an ordinary `=` macro |
| `lib.a(m.o)` targets, `$%`, the `.c.a` rule | `doname()`, `implicit()`, `files.cpp` | §2.2 |
| Built-in macros | `builtin[]` | `AR=ar`, `ARFLAGS=-rvU`, `CC=cc`, `CFLAGS=`, `LDFLAGS=`, `AS=as`. `-U` because `ar` dates members 0 without it (README.md, "Archives") |

#### 2.1 Growing limits

Every fixed buffer a makefile can fill, and what replaces it:

- **The name table**, 509 slots of open addressing: doubled and rehashed at
  two-thirds full, so `Hash table overflow` goes.
- **Expansion**: `subst()` reports a full buffer instead of stopping, and a
  plain `substa(text)` expands into a heap buffer, doubling and expanding
  again until it fits. It serves `docom()`'s `OUTMAX` line, `nextlin()`'s
  dependency line (`templin`), `assign()`'s `:=` and `+=`, `substref()`, and
  `include`'s names. Expanding twice is safe because expansion has no side
  effect but `used`, which is idempotent — and stays so in phase C, where §3.2
  caches what would not be.
- **Input lines**: `yytext` and `word` grow as `nextlin()` and `yylex()` fill
  them, so `line too long` and `word too long` go.
- **`$?` and `$^`**: `mkqlist()` builds a heap string, so `$? list too long`
  goes, and a list no longer shares one static buffer with the next.
- **`NLEFTS`**: the targets of one rule line grow, so `Too many lefts` goes.
- **Names**: `doname()`'s and `implicit()`'s 100-byte name buffers are sized
  from the name, so a long name gets its inference rule; `srchdir()`'s too.
- **`doexec()`'s 200 arguments**: counted first and allocated.

`MAXSUBST` (100 nested references) and `MAXINCL` (100 nested includes) stay:
they stop a macro or a file that refers to itself, and no makefile is near
either.

#### 2.2 Library members

POSIX's `lib(member)` as a *target*: making it makes the member, and `$@` is
`lib`, `$%` is `member`. The inference rule for one is found from the
member's suffix to `.a` — `lib.a(x.o)` from `x.c` by `.c.a` — and the
built-in one is POSIX's:

```
.c.a:
	$(CC) -c $(CFLAGS) $<
	$(AR) $(ARFLAGS) $@ $*.o
	rm $*.o
```

with `rm` and not `rm -f`, Braam's `rm` having no `-f`. `exists()` already
dates a member through `lookarch()`, so `lib.a: lib.a(a.o) lib.a(b.o)` is
remade member by member. A member's date is whole seconds; `lookarch()` takes
the end of the second, which keeps a member just archived from looking older
than its object.

**Done** in `2.58-r2`, with four things the plan did not say. A member
prerequisite is its member's name in `$?` and `$^`, as GNU has it, so that
`$(AR) $(ARFLAGS) $@ $?` names the objects. A `lib(member)` whose library is
not there yet is a member not there, where V7 stopped. An assignment that
expands at once is not a use of what it reads, so V7's "changed after being
used" warning no longer fires on the change it was written for. And the
comparison with GNU make runs it with `SHELL=/bin/sh`, without which GNU runs a
command-line `!=` with no shell at all.

**Done when:** each feature has cases in `cases.mjs`, those whose behaviour
POSIX fixes beginning `.POSIX:` and compared against host GNU make's `-n`
(§4); and `test/library.mjs` builds a library with `cc` and `ar` on Braam
through `.c.a`, links a program against it, rebuilds one member when its
source moves, and makes nothing on the run after.

### Phase C — GNU make, the everyday half

| Feature | Where | Notes |
|---|---|---|
| Pattern rules, `%.o: %.c`, and static patterns | `implicit()`, `gram.cpp` | The largest change: searched before suffix rules, as GNU does |
| Conditionals: `ifeq`, `ifneq`, `ifdef`, `ifndef`, `else`, `endif` | `nextlin` | A stack of skip states; tested on the expanded line |
| Text functions | `subst()` | `subst patsubst strip findstring filter filter-out sort word words firstword lastword dir notdir suffix basename addsuffix addprefix join if or and foreach` |
| `$(wildcard)`, `$(shell)` | `subst()`, its callers | §3.2 |
| `define`/`endef`, `export`, `unexport`, `override` | `nextlin`, `eqsign()` | Exported macros go through `spawn`'s `env` |
| `VPATH`, `vpath` | `exists()`, `doname()` | A found prerequisite's path is what `$<` names |
| Order-only prerequisites, `a: b \| c` | `gram.cpp`, `doname()` | |
| `.DELETE_ON_ERROR`, `MAKECMDGOALS`, `.DEFAULT_GOAL` | `doname()`, `make_main` | |
| `$+`, `$\|`, `$(@:...)` forms | `subst()` | |

**Done when:** each has cases checked against host GNU make 4.4 (§4), and
`test/gnu.mjs` builds a C program from an unedited GNU-style makefile:
pattern rules, `$(wildcard *.c)`, `$(patsubst)`, conditionals on a `DEBUG`
macro.

### Phase D — less often needed

- **`-j n`**, parallel jobs: `wait_child(SYS_WAIT_ANY)`, a slot count, held
  output per job written whole as each finishes, `^C` sent to every child.
  `.WAIT` and `.NOTPARALLEL` start to mean something.
- **`$(call)`, `$(eval)`, `$(value)`, `$(origin)`, `$(error)`, `$(warning)`,
  `$(info)`.** `$(eval)` is the hardest: plain `subst()` pushes its text as
  one more input buffer (§3.1), and `yyparse()` parses it before the next
  line of the file — or, met while a command is expanded, `docom()` awaits
  `yyparse()` over it before running the command.
- **Target-specific and pattern-specific variables.**
- **`.ONESHELL`, `.SECONDARY`, `.INTERMEDIATE`, `.SILENT:` per target.**
- **Automatic dependencies** with `-include $(OBJS:.o=.d)`. The make side is
  there; the rest waits on `cc` learning `-MD`, which it does not have.
- **`--help`, and long options** where GNU has them (`--directory`,
  `--jobs`, `--keep-going`, `--dry-run`).

Not planned: `.SECONDEXPANSION`, grouped targets (`&:`), `load`, Guile, the
jobserver protocol between separate `make` processes, and BSD make's
dialect.

## 3. The two mechanisms

### 3.1 A line that has to wait: `include` and `!=`

`yyparse()` is a `Task`, awaited by `rddescf()`, over a plain `yylex()`. Both
lines are whole lines, found by `nextlin()` between two `comline`s: it
returns a token, `INCLUDE` or `SHELLDEF`, with what the line said saved, and
`yyparse()` takes it at the start of a `comline`, after finishing the rule
before it:

```cpp
case INCLUDE:  co_await includef(incnames, incopt); continue;   // in now
case SHELLDEF: co_await runset(shellname, shellcmd); continue;  // phase B
```

`includef()` reads each file whole and `pushin()` puts it on a stack of input
buffers, which `getfin()` pops at its end. `runset()` expands the command,
runs it with its stdout on a pipe, reads it to the end, and sets the macro.
Each await parks on a read or a spawn, so the parse loop's stack goes back to
nothing at every one of them. A `!=` on the command line or in `MAKEFLAGS` is
run the same way by `make_main()`.

### 3.2 An expansion that has to wait: `$(wildcard)` and `$(shell)`

`subst()` stays plain, and is called from plain code. A blocking function it
meets is looked up in a cache keyed by the function's expanded argument. On a
miss it records the request, stops, and its caller reports "need": the
coroutine above (`rddescf()` for a makefile line, `docom()` for a command)
performs it, stores the result, and expands the same text again, which now
hits. Each pass does at least one request, so the number of passes is the
number of distinct calls in the line. Phase B's `substa()` already expands
again when its buffer fills; this is the same loop with a second reason.

The cache is emptied after every command runs, since a command can create
files and change what a shell prints. That makes two identical `$(shell)`s
within one line or one rule the same result, where GNU runs them twice; the
README will say so.

## 4. How each phase is checked

- **Cases** in `cases.mjs`, one `.args`/`.mk`/`.expected` per behaviour.
  Plain `make test` stays the gate.
- **Differential against host GNU make.** A case named in `GMAKE_CASES` also
  runs under `-n` on Braam and under the host's `gmake -n`, over the same
  files at the same times, and the commands must agree line for line; the
  messages, being V7's, are not compared.
- **A real build** at the end of each phase: `cc` on Braam builds from the
  phase's makefiles, as `build.mjs` and `portable.mjs` do now.
- **Stack and frames:** every new loop over a list is plain or parks at each
  await, and no new coroutine frame holds a buffer, per rule 3 in §1 and
  CLAUDE.md. `incmany` includes three hundred files in a row, so a parse loop
  that awaited without parking would trap there.

## 5. Order inside phase B

The limits first: `substa()` is what `!=`, `:::=` and the `D`/`F` forms
expand with, and a long line or name is better found failing early than
late. Then the small ones — `D`/`F`, `+` under `-t` and `-q`, `-C`, the
special targets, the built-in macros — and `!=` and `:::=` together, since
both are `eqsign()`'s. Library members last, as the largest, with
`library.mjs` the phase's real build.
