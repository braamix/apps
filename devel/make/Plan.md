# make — a plan for Braam's make

`devel/make` is V7 make 2.58 grown into **Braam's make**: POSIX.1-2024's
`make`, then GNU make, on V7's code. Phases A to D are done and in
`2.58-r4` — all of POSIX's `make`, `-j` included, and of GNU's what most
makefiles use; [README.md](README.md) has the tables. Phase E, the rest of
GNU make that real projects reach for now that `cc` writes dependencies, is
done and in `2.58-r5`. BSD make's dialect (`.if`, `.for`, `:M`) is not a
goal.

## 1. Rules for every phase

1. **POSIX wins, V7's code stays.** Behaviour follows POSIX wherever the two
   disagree, with no switch to get V7's back. What stays is V7's code, its
   identifiers and structure, its comments, and its own text where POSIX does
   not specify any — the `Make: ... Stop.` messages, `` `x' is up to
   date.``, `-p` and `-d`. A case's expectation changes only where POSIX
   demands it, and the commit that changes it says which rule required it.
2. **POSIX, then GNU.** Where the two read the same text two ways, POSIX
   decides, as phases B to D have it.
3. **Await only where something parks.** A `co_await` is a call and not a
   tail call, so a loop that awaits without parking grows the native stack
   until the process traps. `yyparse()` is a coroutine and awaits `include`
   and `!=` itself, both of which park on a system call; the lexer,
   `nextlin()` and `subst()` stay plain, since they run once per token, line
   and macro reference and almost never need to wait. §3 has the
   mechanisms.
4. **The README keeps up.** Each addition goes in its "Beyond V7" table with
   the standard it comes from, and each POSIX difference in "Where POSIX
   overrules V7".
5. **Version:** `2.58-r5` after phase E. There is no upstream version to
   move, so only the revision counts up.

## 2. Phases

### Phase E — the rest of GNU make that projects use

| Feature | Where | Notes |
|---|---|---|
| Automatic dependencies | nothing new; `test/deps.mjs` | §2.1 |
| Remaking makefiles | `make_main`, `rddescf()` | §2.1, §3.4 |
| `$(abspath)`, `$(realpath)`, `$(flavor)`, `$(intcmp)`, `$(let)`, `$(file)` | `funcs.cpp` | §2.2 |
| `undefine` | `nextlin()` | With `override`; `$(origin)` is `undefined` after it |
| `MAKELEVEL`, `-w`, `--print-directory`, `--no-print-directory` | `make_main`, `writemakeflags()` | `make: Entering directory 'd'` and `Leaving`, said for a sub-make and under `-C`, as GNU does |
| `-B`, `--always-make`; `-W f`, `--what-if=f`; `-o f`, `--old-file=f` | `doname()` | Every target out of date; `f` taken as new; `f` taken as old and never remade |
| `-O`, `--output-sync` | the job loop | §3.5 |
| A missing intermediate for a target remade for some other reason | `doname()` | §2.3 |

#### 2.1 Dependencies, and makefiles that are targets

`cc` now writes GCC's dependency rules: `-MD` and `-MMD` beside the object,
`-MF`, `-MT`, `-MQ`, `-MP` and `-MG` as GCC has them. The make side has been
there since phase A — `-include $(OBJS:.o=.d)` reads what exists and passes
over what does not — so what this step adds is the proof:
`test/deps.mjs` builds with `CFLAGS += -MMD -MP` and checks that a changed
header remakes exactly the objects that include it, that a header deleted
and dropped from the sources does not stop make (`-MP`'s empty rules), and
that a first build with no `.d` files at all builds everything.

GNU's older way writes `.d` files with a rule, `%.d: %.c`, and needs make to
**remake its makefiles**: after reading, each makefile and each included
file that is a target is brought up to date, and if any was remade make
starts again from the beginning, so it reads the new ones. A file named by
`-include` that cannot be made is passed over, as now. §3.4 is how it starts
again; `MAKE_RESTARTS` counts the starts, and a makefile remade twice in a
row with nothing changed stops it, as GNU's loop guard does.

#### 2.2 Functions

`$(abspath x)` and `$(realpath x)` make each word absolute against the
directory make is in; `realpath` also resolves `.`, `..` and links and drops
a name that is not there, so it is a §3.2 request, as `$(wildcard)` is.
`$(flavor v)` is `undefined`, `recursive` or `simple`. `$(intcmp a,b,lt,eq,gt)`
compares integers, and `$(let a b,list,text)` binds names as `$(foreach)`
does, the last taking the rest of the list. `$(file >f,text)`, `>>f` and
`<f` write, append and read: an effect, so a write is held as `$(info)` is
(§3.2, and phase D's held effects) and done by the coroutine above once the
expansion is done for good, and a read is a request.

#### 2.3 An intermediate that has to be made after all

Phase D does not make a missing intermediate when the target that needs it
is newer than its sources. When that target is then remade for another
reason — another prerequisite moved — its commands may well need the
intermediate. GNU makes it then. `doname()` will do the same: a target
about to run its commands first makes any missing intermediate among its
prerequisites that the first walk passed over, which is a second walk of
those prerequisites alone.

**Done** in `2.58-r5`, with what the plan did not say. The loop guard on
starting again is twenty starts, not a comparison of what was remade. A
command-line macro survives `override undefine`, as it does in GNU make
4.4. Two `$(file)`s in one expansion are done in the order written, which
is why requests are now kept in the order asked. `-d` says nothing about
the makefiles being remade, but `-p` after it lists them as names. A
`define` whose first line is empty lost that line. And under `-O`, at most
32 jobs run, as one poll takes 64 descriptors.

**Done when:** each feature has cases in `cases.mjs`, compared against host
GNU make's `-n` where it is GNU's (§4), and `test/deps.mjs` (§2.1) is green
with both `-MMD` and a `%.d: %.c` rule.

Not planned: `.SECONDEXPANSION`, grouped targets `&:`, `load` and Guile,
the jobserver between separate `make`s, which needs a descriptor passed to a
child where a spawn passes only the three streams, and BSD make's dialect.

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
README says so. Phase E adds to the requests — `$(realpath)` and
`$(file <f)` — and nothing to the loop.

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
GNU's default; §3.5 is `--output-sync`.

### 3.4 Starting again: remade makefiles

Make's state is everywhere — the name table, the macros, the rules, every
static in the parser — and none of it was written to be emptied. So make
does not start again in the process: it spawns itself, `makename0` with the
same arguments and `MAKE_RESTARTS` one higher in the environment, waits for
it as for any command, `^C` included, and exits with its status. A Braam
process is a fresh instance, so that is what GNU's `exec` of itself is
here, at the cost of one process more while the second runs.

### 3.5 Output kept whole: `-O`

Under `-O` each command's standard output and error go to two pipes of its
own instead of to make's — two, as a descriptor given to a child is moved
into it and one cannot be both. The job loop then waits on
`poll_fds()` over every running job's pipes rather than on `wait_child()`:
what a pipe has is added to that job's buffer, and when both of a job's
pipes are at their end its child is reaped by pid, which no longer parks
long, and the buffer is written whole. `-O target`, the default, writes when
a target's last command ends; `-O line` after each command. A command's
output and error lose their order against each other, which GNU keeps by
sending both to one file; the README says so.

## 4. How each phase is checked

- **Cases** in `cases.mjs`, one `.args`/`.mk`/`.expected` per behaviour.
  Plain `make test` stays the gate.
- **Differential against host GNU make.** A case named in `GMAKE_CASES` also
  runs under `-n` on Braam and under the host's `gmake -n`, over the same
  files at the same times, and the commands must agree line for line; the
  messages, being V7's, are not compared.
- **A real build** at the end of each phase: `cc` on Braam builds from the
  phase's makefiles, as `build.mjs`, `gnu.mjs` and `jobs.mjs` do now.
- **Stack and frames:** every new loop over a list is plain or parks at each
  await, and no new coroutine frame holds a buffer, per rule 3 in §1 and
  CLAUDE.md. `incmany` includes three hundred files in a row, so a parse loop
  that awaited without parking would trap there.

## 5. Order inside phase E

Dependencies first, as the reason for the phase and a test with no code
under it, then remaking makefiles, which the `%.d` half of `deps.mjs`
needs. The functions and `undefine` next, each small; then `MAKELEVEL` and
`-w`, since a sub-make's messages change every recursive case; then `-B`,
`-W` and `-o`, the intermediate, and `-O` last, the one that changes the job
loop.
