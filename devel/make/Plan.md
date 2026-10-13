# make — a plan for Braam's POSIX make

`devel/make` is V7 make 2.58 ([README.md](README.md)). It builds with `cc` on
Braam, but a makefile written in the last forty years will usually stop at
its first line. This plan makes it **Braam's POSIX make**: POSIX.1-2024's
`make`, then the half of GNU make that most makefiles lean on, grown from
V7's code. It is no longer V7's program once this starts, and nothing on
Braam has a V7 makefile to keep working, so where V7 and POSIX disagree,
POSIX wins. Four phases, ordered by how soon a real makefile reaches for each
feature; each ends green and is published on its own.

## 1. Rules for every phase

1. **POSIX wins, V7's code stays.** Behaviour follows POSIX wherever the two
   disagree, with no switch to get V7's back; phase A starts by settling every
   such difference. What stays is V7's code, its identifiers and structure,
   its comments, and its own text where POSIX does not specify any — the
   `Make: ... Stop.` messages, `` `x' is up to date.``, `-p` and `-d`. The 39
   cases in `test/` change only where POSIX demands it, and each commit that
   changes an expectation says which rule required it.
2. **POSIX.1-2024 first, then GNU.** Phases A and B are POSIX's `make`, and
   phase C is the GNU make most makefiles are written for. BSD make's dialect
   (`.if`, `:M`) is not a goal.
3. **Await only where something parks.** A `co_await` is a call and not a
   tail call, so a loop that awaits without parking grows the native stack
   until the process traps. `yyparse()` becomes a coroutine and awaits
   `include` and `!=` itself, both of which park on a system call; the lexer,
   `nextlin()` and `subst()` stay plain, since they run once per token, line
   and macro reference and almost never need to wait. §3 has the two
   mechanisms.
4. **The README is rewritten in phase A** to say what the program now is:
   V7's code, made a POSIX make. The port's account stays, and a section
   lists each addition and the standard it comes from. The package's `T=`
   says "POSIX make" from then on.
5. **Version:** `2.58-r1` after phase A, `-r2` after B, and so on. There is
   no upstream version to move, so only the revision counts up.

## 2. Phases

### Phase A — POSIX where V7 differs, and what nearly every makefile uses

First the differences, since everything after is written against POSIX's
behaviour and not V7's:

| V7 | POSIX, and from phase A on | Where |
|---|---|---|
| Backslash-newline in a command becomes a space | Kept, and passed to the shell | `nextlin` |
| `.out` rules: `make hello` needs a target `hello.out` | Single-suffix `.c:` and `.s:` rules; `.SUFFIXES: .o .c .s .a` | `builtin[]`, `implicit()` |
| The environment is not read | Environment variables are macros (below) | `make_main` |
| `.SILENT` and `.IGNORE` with prerequisites act on everything | Only on those targets; without prerequisites, on everything | `docom()` |
| `-q` exits 255 when not up to date, and every error exits 1 | `-q` exits 1; an error exits 2 | `doname()`, `fatal()` |
| `No suffix list.` on every run under `-r` | Nothing: an empty list is not worth saying | `make_main` |

Then the features:

| Feature | Where | Notes |
|---|---|---|
| `include f...`, `-include`, `sinclude` | `nextlin`, `yyparse()` | §3.1. Nested files; `-` ignores a missing one |
| `$(VAR:a=b)` substitution references | `subst()` | Suffix form only, as POSIX defines it |
| `+=`, `?=`, `:=`, `::=` | `eqsign()`, `setvar()` | `:=` expands once; a `varblock` flag says which |
| Environment as macros | `make_main` | `proc_env_count()`/`proc_env_at()`; makefile beats environment, command line beats both |
| `$(MAKE)`, `MAKEFLAGS` | `make_main`, `run()` | `MAKEFLAGS` carries flags and command-line macros to a child through `spawn`'s `env` |
| `.PHONY` | `doname()` | A phony target is always remade and never `stat`ed |
| `$^`, `$<` in explicit rules | `doname()` | GNU's, but used everywhere; `$<` is the first prerequisite |

**Done when:** every feature has cases in `cases.mjs`, and a new
`test/portable.mjs` builds a three-directory C program with `cc` from
makefiles that use only phase-A features: `cd dir && $(MAKE)` into each
directory, an `include`d configuration file, and `.PHONY: all clean`.
Each fixture's `make -n` output is also checked against host GNU make's
(§4).

### Phase B — the rest of POSIX, and the limits

| Feature | Where | Notes |
|---|---|---|
| Hash table, `INMAX`, `OUTMAX`, `QBUFMAX`, `NLEFTS` grow | `misc.cpp`, `gram.cpp` | A real project passes 509 names; growing buffers replace the fixed ones |
| `$(@D)`, `$(@F)`, and `D`/`F` on `< * ? %` | `subst()` | |
| `$%`, `lib.a(m.o)` targets, the `.c.a` rule | `doname()`, `files.cpp` | Built-in `ARFLAGS` is `rvU`, for the reason README.md's "Archives" gives |
| `!=` | `nextlin`, `yyparse()` | §3.1, like `include` |
| `+` command prefix | `docom()` | Runs under `-n`, `-t`, `-q` |
| `-e`, `-C dir` | `make_main` | `-C` is not POSIX but every make has it |
| `.POSIX` | `gram.cpp` | Accepted with no effect: POSIX is already what this make does |
| `.WAIT`, `.NOTPARALLEL`, `.SCCS_GET` | `gram.cpp` | Accepted and ignored until phase D's `-j` |
| Built-in macros | `builtin[]` | `AR`, `ARFLAGS`, `LDFLAGS`, `MAKE`, as POSIX lists them, for Braam's tools |

**Done when:** each has cases, and `test/posix.mjs` runs a set of makefiles
using only POSIX syntax under both this make and host GNU make with
`.POSIX:`, comparing `make -n` output.

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
  phase A's `-include`; the rest waits on `cc` learning `-MD`, which it does
  not have.
- **`--help`, and long options** where GNU has them (`--directory`,
  `--jobs`, `--keep-going`, `--dry-run`).

Not planned: `.SECONDEXPANSION`, grouped targets (`&:`), `load`, Guile, the
jobserver protocol between separate `make` processes, and BSD make's
dialect.

## 3. The two mechanisms

### 3.1 A line that has to wait: `include` and `!=`

`yyparse()` becomes a `Task`, awaited by `rddescf()`, over the same plain
`yylex()`. Both lines are whole lines, found by `nextlin()` between two
`comline`s: it returns a new token, `INCLUDE` or `SHELLDEF`, with the
expanded rest of the line saved, and `yyparse()` takes it as the start of a
`comline`, after finishing the rule before it:

```cpp
case INCLUDE:  co_await pushfile(name); continue;   // read, push a buffer
case SHELLDEF: co_await runset(macro, cmd); continue;
```

`pushfile()` reads the file whole and pushes it on a stack of input buffers
(`fin`, `finend`, `yylineno`, the file's name); `getfin()` pops a buffer at
its end and goes on with the one below, so nested includes need nothing
more. `runset()` runs the command as `$(shell)` does and sets the macro. Each
await parks on a read or a spawn, so the parse loop's stack goes back to
nothing at every one of them, however many lines the files have. `rddescf()`
reads the top file as it does now and pushes it the same way.

### 3.2 An expansion that has to wait: `$(wildcard)` and `$(shell)`

`subst()` stays plain, and is called from plain code. A blocking function it
meets is looked up in a cache keyed by the function's expanded argument. On a
miss it records the request, stops, and its caller reports "need": the
coroutine above (`rddescf()` for a makefile line, `docom()` for a command)
performs it, stores the result, and expands the same text again, which now
hits. Each pass does at least one request, so the number of passes is the
number of distinct calls in the line.

The cache is emptied after every command runs, since a command can create
files and change what a shell prints. That makes two identical `$(shell)`s
within one line or one rule the same result, where GNU runs them twice; the
README will say so.

## 4. How each phase is checked

- **Cases** in `cases.mjs`, one `.args`/`.mk`/`.expected` per behaviour, the
  way the 39 now are. Plain `make test` stays the gate.
- **Differential against host GNU make.** For a makefile whose behaviour GNU
  and POSIX agree on, `gmake -n -r` on the host and `make -n -r` on Braam
  print the same commands. A new `test/gmake.mjs` runs each fixture in a
  directory list both ways, with the same file times set on both sides, and
  diffs. The commands must agree; the messages, which stay V7's, are not
  compared.
- **A real build** at the end of A and C: `cc` on Braam builds from the
  phase's makefiles, as `build.mjs` does today.
- **Stack and frames:** every new loop over a list is plain or parks at each
  await, and no new coroutine frame holds a buffer, per rule 3 in §1 and
  CLAUDE.md. A case includes a few hundred files in a row, so a parse loop
  that awaited without parking would trap there.

## 5. Order inside phase A

The POSIX differences first, with the expectations they move, so that every
later case is written once against the behaviour that stays. Then
`$(VAR:a=b)` and the assignment operators: they are contained in
`subst()` and `eqsign()`, and everything after uses them. Then environment
and `MAKEFLAGS`, then `.PHONY`, `$^` and single-suffix rules, and `include`
last, since it turns `yyparse()` into a coroutine (§3.1), which `!=` reuses
in phase B.
