# make — a plan for Braam's POSIX make

`devel/make` is V7 make 2.58 being grown into **Braam's POSIX make**:
POSIX.1-2024's `make`, then the half of GNU make that most makefiles lean on,
on V7's code. Phases A to C are done and in `2.58-r3`: all of POSIX's `make`
but `-j`, and the everyday half of GNU make, which [README.md](README.md) has
the tables of. Phase D, the rest that makefiles reach for, is done and in
`2.58-r4`.

## 1. Rules for every phase

1. **POSIX wins, V7's code stays.** Behaviour follows POSIX wherever the two
   disagree, with no switch to get V7's back. What stays is V7's code, its
   identifiers and structure, its comments, and its own text where POSIX does
   not specify any — the `Make: ... Stop.` messages, `` `x' is up to
   date.``, `-p` and `-d`. A case's expectation changes only where POSIX
   demands it, and the commit that changes it says which rule required it.
2. **POSIX.1-2024 first, then GNU.** POSIX's `make` is done, and so is the
   GNU make most makefiles are written for; phase D is the rest of it. BSD make's dialect
   (`.if`, `:M`) is not a goal.
3. **Await only where something parks.** A `co_await` is a call and not a
   tail call, so a loop that awaits without parking grows the native stack
   until the process traps. `yyparse()` is a coroutine and awaits `include`
   and `!=` itself, both of which park on a system call; the lexer,
   `nextlin()` and `subst()` stay plain, since they run once per token, line
   and macro reference and almost never need to wait. §3 has the
   mechanisms; §3.3 is how `-j` drives `doname()`.
4. **The README keeps up.** Each addition goes in its "Beyond V7" table with
   the standard it comes from, and each POSIX difference in "Where POSIX
   overrules V7".
5. **Version:** `2.58-r4` after phase D. There is no
   upstream version to move, so only the revision counts up.

## 2. Phases

### Phase D — the rest of GNU make that makefiles use

| Feature | Where | Notes |
|---|---|---|
| `$(value)`, `$(origin)`, `$(info)`, `$(warning)`, `$(error)`, `$(call)` | `funcs.cpp` | §2.1 |
| `$(eval)` | `funcs.cpp`, `yyparse()`, `docom()` | §2.2 |
| Target-specific and pattern-specific macros | `nextlin()`, `doname()` | §2.3 |
| Long options and `--help` | `make_main` | `--directory`, `--file`, `--makefile`, `--jobs`, `--keep-going`, `--dry-run`, `--just-print`, `--silent`, `--quiet`, `--ignore-errors`, `--touch`, `--question`, `--print-data-base`, `--environment-overrides`, `--no-builtin-rules`, each `=value` or the next word |
| `.ONESHELL` | `docom()` | A rule's lines are one script to one shell, its first line's `@`, `-` and `+` for the whole, as GNU has it |
| Chained pattern rules, `.INTERMEDIATE`, `.SECONDARY` | `implicit()`, `doname()`, `make_main` | §2.4 |
| `-j [n]` | `doname()`, `docom()`, `dosys.cpp` | §3.3. `.WAIT` and `.NOTPARALLEL` begin to mean something |

`.SILENT:` per target is already there (phase A), and so is
`-include $(OBJS:.o=.d)`; what automatic dependencies still wait on is `cc`
learning `-MD`, which is `devel/cc`'s work and not this phase's.

#### 2.1 Functions without side effects to speak of

`$(value v)` is the value unexpanded. `$(origin v)` is `undefined`,
`default`, `environment`, `environment override`, `file`, `command line`,
`override` or `automatic`, so a macro remembers where it was set: a field set
by `setvar()` from what its caller is doing — reading `builtin[]`, the
environment, the command line, an `override` line. `$(call v,a,b,...)` binds
`$(0)` to `v` and `$(1)`... to the arguments, as `$(foreach)` binds its name,
and expands `v`'s value.

`$(info)`, `$(warning)` and `$(error)` are the first functions with an effect,
and an expansion may be done twice — `substa()` growing its buffer, or a line
done again for §3.2. So what `info` and `warning` say is held while an
expansion runs and said only when the outermost `substa()` finishes with
nothing left to wait for; a pass that is done again throws it away.
`$(error)` stops make, which is the same however often it happens. `warning`
says the line, as `yyerror()` does.

#### 2.2 `$(eval)`

Its argument, expanded, is makefile text, held as `info` is (§2.1) and
handed on when the expansion finishes. Met while the makefile is read, it is
pushed as one more input buffer (§3.1), so `yyparse()` reads it before the
next line, which is where a line of nothing but `$(eval ...)` wants it. Met
while a rule's commands are expanded, `docom()` awaits `yyparse()` over it
before the next command is expanded, as GNU's are, one after another. A `$(foreach)` of evals is one buffer, in order. Unlike
GNU, a macro an `eval` sets is not seen by the rest of the line it is on.

#### 2.3 Target-specific and pattern-specific macros

`target...: name op value` and `%.o: name op value`, with any operator and
`override` or `export` before the name, found by `nextlin()` as a rule line
whose part after the separator is an assignment. Each target keeps a list
of them, and the pattern ones a list of their own. `doname()` puts a
target's into effect when it begins — the pattern ones that match it first,
in the order read, then its own — and puts the old values back when it
returns, so they are in effect for its prerequisites too, as GNU has it.
`:=` is expanded when read, `+=` appends to the value in effect, `?=` sets
only what has none, and a command-line macro still wins without `override`.

#### 2.4 Chains

A pattern rule's prerequisite that is neither a file nor named may be made by
another pattern rule, to a depth of ten: `prog` from `prog.o` from `prog.c`
where only `prog.c` is there. Such a file is *intermediate*, as is anything
`.INTERMEDIATE` names. A missing intermediate is not made when what needs it
is newer than the intermediate's own prerequisites, and those that were made
are removed at the end, with one `rm` line, unless `.SECONDARY` names them,
or names nothing and so means all, or `.PRECIOUS` does.

**Done** in `2.58-r4`, with what the plan did not say. A rule's commands are
expanded one after another, so an `$(eval)` in one is seen by the next, as in
GNU. A missing intermediate is still not made when a target needing it is
remade for some other reason, where GNU would make it. A long option in a
parent's `MAKEFLAGS` was being read as a string of letters —
`--no-print-directory` as `-n` among them — and is now understood or passed
over. A
pattern rule with no prerequisites had lost its commands. And `^C` under `-j`
is tested by `cases.mjs`'s `jobsint` rather than by `jobs.mjs`, as a compile
under the harness never waits long enough for a key.

**Done when:** each feature has cases in `cases.mjs`, compared against host
GNU make 4.4's `-n` where it is GNU's (§4); and `test/jobs.mjs` builds a
program of several sources with `cc` on Braam under `-j 4`, the same
program as under `-j 1`, with a failing command under `-j` waiting for the
jobs already running before it stops, and `^C` under `-j` stopping them
all.

Not planned: `--output-sync`, `.SECONDEXPANSION`, grouped targets (`&:`),
`load`, Guile, the jobserver protocol between separate `make` processes —
so a sub-make runs its own jobs one at a time — `-B`, and BSD make's
dialect.

## 3. The mechanisms

### 3.1 A line that has to wait: `include` and `!=`

`yyparse()` is a `Task`, awaited by `rddescf()`, over a plain `yylex()`. Both
lines are whole lines, found by `nextlin()` between two `comline`s: it
returns a token, `INCLUDE` or `SHELLDEF`, with what the line said saved, and
`yyparse()` takes it at the start of a `comline`, after finishing the rule
before it:

```cpp
case INCLUDE:  co_await includef(incnames, incopt); continue;
case SHELLDEF: co_await runset(); continue;
```

`includef()` reads each file whole and `pushin()` puts it on a stack of input
buffers, which `getfin()` pops at its end. `runset()` expands the command,
runs it with its stdout on a pipe, reads it to the end, and sets the macro.
Each await parks on a read or a spawn, so the parse loop's stack goes back to
nothing at every one of them. A `!=` on the command line or in `MAKEFLAGS` is
run the same way by `make_main()`.

### 3.2 An expansion that has to wait: `$(wildcard)` and `$(shell)`

`subst()` stays plain, and is called from plain code. A blocking function it
meets is looked up in a cache keyed by the function and its expanded
argument. On a miss it records the request, puts nothing, and sets
`substneed`; whatever was being done with the expansion is not done. The
coroutine above performs the requests, fills the cache, and expands the same
text again, which now hits. Each pass does at least one request, so the
number of passes is the number of distinct calls in the line. `substa()`
already expands again when its buffer fills; this is the same loop with a
second reason, one level up.

The coroutines above are three. For a makefile line, `nextlin()` keeps the
line as read and returns a `NEED` token in place of what it would have
returned; `yyparse()` takes it, as it takes `INCLUDE`, awaits the requests and
has the line processed again. `docom()` does the same for a command,
`runset()` for a `!=`, and `make_main()` for a macro on the command line.
Before a line is done again nothing of it has been: `assign()` sets nothing
from an expansion that needs, and a conditional is not pushed.

The cache is emptied after every command runs, since a command can create
files and change what a shell prints. That makes two identical `$(shell)`s
within one line or one rule the same result, where GNU runs them twice; the
README will say so.

### 3.3 Jobs: `-j`

V7's `doname()` makes a target by walking its prerequisites and awaiting each
command, so one runs at a time. Under `-j n` a target's commands are a *job*
instead: `doname()` expands them, as `docom()` already does all of a rule's
at once, spawns the first, marks the target running and returns. A target
with a prerequisite running is left as not looked at, and returns too. The
walk over the goals is then done again whenever a job's command ends, until
nothing is running and nothing is left:

```
for (;;) {
    walk the goals: doname() starts what it can, up to n jobs
    if no job is running: break
    reap one: wait_child(SYS_WAIT_ANY), then its job's next line, or its
    target done -- its time read, done = 2, or 3 if the line failed
}
```

A walk awaits a `stat` per target it looks at, which parks, so its stack
goes back to nothing each time; the cost is a walk per command, which is
small beside a command. A `.WAIT` stops the walk along that list while
anything before it is running, and `.NOTPARALLEL` makes `n` one. A failure
without `-k` starts nothing new, waits for the running commands, and stops; a
`^C` is sent to every child, and each running target is removed as one is
now. A `::` rule, and anything under `-n`, `-t` or `-q`, runs as it does
without `-j`. The children write to make's own output as they go, which is
GNU's default; `--output-sync` is not planned.

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

## 5. Order inside phase D

The functions first, since the later steps' cases use them, with `$(eval)`
after the others, as it needs their held effects. Then target-specific
macros, long options and `.ONESHELL`, each small; then chains, which are
`implicit()`'s; and `-j` last, as the one that changes how `doname()` is
driven, with `jobs.mjs` the phase's real build.
