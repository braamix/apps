# make — a plan for what V7 did not have

`devel/make` is V7 make 2.58 ([README.md](README.md)). It runs V7's makefiles
and builds with `cc` on Braam, but a makefile written in the last forty years
will usually stop at its first line. This plan adds what such makefiles
expect, in four phases ordered by how soon a real makefile reaches for each
feature. Each phase ends green and is published on its own.

## 1. Rules for every phase

1. **V7 stays V7.** The 39 cases in `test/` keep their expectations byte for
   byte. A new feature takes syntax V7 rejected or ignored, so no V7 makefile
   changes meaning. The one exception is §3's backslash in commands, which
   only `.POSIX` turns on.
2. **POSIX.1-2024 first, then GNU.** Phases A and B are POSIX's `make`, and
   phase C is the GNU make most makefiles are written for. BSD make's dialect
   (`.if`, `:M`) is not a goal.
3. **The parser stays plain.** Whatever has to wait — reading an included
   file, running `!=` or `$(shell)`, listing a directory for `$(wildcard)` —
   is done by the coroutine above the parser, in the driver shape simbesm and
   mbasic use: the plain code stops and reports what it needs, the caller
   does the waiting, and the plain code goes on. §3 has the two mechanisms.
4. **The README's account grows.** A section "Beyond V7" lists each addition
   and the standard it comes from, and the V7 sections stay as written.
5. **Version:** `2.58-r1` after phase A, `-r2` after B, and so on. There is
   no upstream version to move, so only the revision counts up.

## 2. Phases

### Phase A — what nearly every makefile uses

| Feature | Where | Notes |
|---|---|---|
| `include f...`, `-include`, `sinclude` | `nextlin`, `rddescf` | §3.1. Nested files; `-` ignores a missing one |
| `$(VAR:a=b)` substitution references | `subst()` | Suffix form only, as POSIX defines it |
| `+=`, `?=`, `:=`, `::=` | `eqsign()`, `setvar()` | `:=` expands once; a `varblock` flag says which |
| Environment as macros | `make_main` | `proc_env_count()`/`proc_env_at()`; makefile beats environment, command line beats both |
| `$(MAKE)`, `MAKEFLAGS` | `make_main`, `run()` | `MAKEFLAGS` carries flags and command-line macros to a child through `spawn`'s `env` |
| `.PHONY` | `doname()` | A phony target is always remade and never `stat`ed |
| Single-suffix rules, `.c:` | `implicit()` | So `make hello` builds from `hello.c`; built-in `.c:` and `.s:` rules |
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
| `!=` | `nextlin`, driver | §3.1, like `include` |
| `+` command prefix | `docom()` | Runs under `-n`, `-t`, `-q` |
| `-e`, `-C dir` | `make_main` | `-C` is not POSIX but every make has it |
| `.POSIX` | `nextlin`, `dosys` | Backslash-newline kept in commands; `-e` shell as now |
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
| `$(wildcard)`, `$(shell)` | `subst()`, driver | §3.2 |
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
  `$(info)`.** `$(eval)` re-enters the parser from inside expansion, the
  hardest of these.
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

Both are whole lines, found by `nextlin()` between two `comline`s. `nextlin`
returns a new token, `INCLUDE` or `SHELLDEF`, with the expanded rest of the
line saved. `yyparse()` takes it as the start of a `comline`, finishes the
rule before it, and returns 2 with the request. `rddescf()` performs it — it
reads the named file and pushes it on a stack of input buffers (`fin`,
`finend`, `yylineno`, the file name), or runs the command and sets the macro
— and calls `yyparse()` again. A buffer is popped at its end, so `fin` and
`finend` become the top of that stack; nothing of the parse is in flight
across the call, which is why this is safe.

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
  diffs. Where this make differs on purpose (V7's `No suffix list.`, the
  messages), the difference is listed beside the fixture.
- **A real build** at the end of A and C: `cc` on Braam builds from the
  phase's makefiles, as `build.mjs` does today.
- **Stack and frames:** every new loop over a list is plain or suspends, and
  no new coroutine frame holds a buffer, per rule 3 in §1 and CLAUDE.md.

## 5. Order inside phase A

`$(VAR:a=b)` and the assignment operators first: they are contained in
`subst()` and `eqsign()`, and everything after uses them. Then environment
and `MAKEFLAGS`, then `.PHONY`, `$^` and single-suffix rules, and `include`
last, since it brings §3.1's mechanism that `!=` reuses in phase B.
