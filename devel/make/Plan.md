# make — a plan for Braam's POSIX make

`devel/make` is V7 make 2.58 being grown into **Braam's POSIX make**:
POSIX.1-2024's `make`, then the half of GNU make that most makefiles lean on,
on V7's code. Phases A and B are done and in `2.58-r2`: all of POSIX's
`make` but `-j`, which [README.md](README.md) has the tables of. Phase C, the
everyday half of GNU make, is done and in `2.58-r3`. Phase D is left; each
phase ends green and is published on its own.

## 1. Rules for every phase

1. **POSIX wins, V7's code stays.** Behaviour follows POSIX wherever the two
   disagree, with no switch to get V7's back. What stays is V7's code, its
   identifiers and structure, its comments, and its own text where POSIX does
   not specify any — the `Make: ... Stop.` messages, `` `x' is up to
   date.``, `-p` and `-d`. A case's expectation changes only where POSIX
   demands it, and the commit that changes it says which rule required it.
2. **POSIX.1-2024 first, then GNU.** POSIX's `make` is done, and phase C is
   the GNU make most makefiles are written for. BSD make's dialect
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
5. **Version:** `2.58-r3` after phase C, `-r4` after D. There is no
   upstream version to move, so only the revision counts up.

## 2. Phases

### Phase C — GNU make, the everyday half

| Feature | Where | Notes |
|---|---|---|
| `ifeq`, `ifneq`, `ifdef`, `ifndef`, `else`, `endif` | `nextlin()` | §2.1 |
| `define`/`endef`, `override`, `export`, `unexport` | `nextlin()`, `assign()`, `dosys.cpp` | §2.1 |
| Text functions | `funcs.cpp`, from `subst()` | §2.2 |
| `$(wildcard)`, `$(shell)` | `funcs.cpp`, their awaiting callers | §3.2 |
| Order-only prerequisites, `a: b \| c`, and `$\|` | `yylex()`, `yyparse()`, `doname()` | Made first, never newer than the target |
| `$+` | `doname()` | Every prerequisite in order, repeats kept |
| `.DELETE_ON_ERROR`, `MAKECMDGOALS`, `.DEFAULT_GOAL` | `docom()`, `make_main` | A failed command removes `$@` as `^C` does |
| Pattern rules, `%.o: %.c`, and static patterns | `rule()`, `implicit()`, `doname()` | §2.3 |
| `VPATH`, `vpath` | `exists()`, `implicit()`, `doname()` | §2.4 |

#### 2.1 Directives

Each is a whole line, found by `nextlin()` before anything else looks at it,
and never one starting with a tab. A conditional keeps a stack of states:
inside a false branch nothing is read but the next `else` or `endif` and the
nesting of other conditionals. `else ifeq ...` chains, as GNU's does. `ifeq`
takes `(a,b)`, `"a" "b"` and `'a' 'b'`, each side expanded; `ifdef` asks
whether the named macro has a value that is not empty, without expanding it.

`define NAME [op]` takes the lines up to its `endef` as they are — comments
and blanks too, nested `define`s counted — and assigns them with the
operator, `=` if none. A value with newlines used in a command is one command
per line, each with its own `@`, `-` and `+`, which is a canned recipe.

`override` sets a macro the command line set, and keeps it from the
makefile's later plain assignments. `export` and `unexport` mark macros, and
`export` alone marks them all. A child's environment is built for each
command: this process's, with every macro from the environment or the command
line and every exported one at its value now, expanded, and the unexported
ones out. GNU does the same.

#### 2.2 Text functions

`$(name args)`, a known name followed by a blank. The arguments are split at
commas outside nested parentheses, the last taking the rest; each is
expanded only when the function wants it, so `$(if)`, `$(or)` and `$(and)`
expand only the branch they take. `$(foreach v,list,text)` binds `v` for each
word and puts it back after. The set:
`subst patsubst strip findstring filter filter-out sort word wordlist words
firstword lastword dir notdir suffix basename addsuffix addprefix join if or
and foreach`, plus the two of §3.2. All are plain code over `putn()`, so a
full buffer is `substa()`'s loop as it is now.

#### 2.3 Pattern rules

A target with a `%` is a pattern rule, kept on a list of its own in the order
read and never the default goal. A pattern rule with no commands cancels the
one before it with the same target and prerequisites. For a target with no
commands and not `.PHONY`, `implicit()` tries the pattern rules first, then
the suffix rules as now. A rule applies when its target pattern matches —
against the file part when the pattern has no slash, the directory going back
on the stem — and each prerequisite, the stem put in for its `%`, is a file,
or a target, or found through §2.4. `implicit()` then returns them all, the
first being `$<`, and `$*` is the stem. A rule is not chained through an
intermediate file another pattern rule would make; that is phase D.

A static pattern, `objs: %.o: %.c`, is a rule per target: the stem matched,
the prerequisites written out and added to the target's own, and `$*` the
stem when its commands run. A target the pattern does not match is an error,
as in GNU.

#### 2.4 VPATH

`VPATH` names directories, a colon or a blank apart; `vpath pattern dirs`
names them for the names `pattern` matches, `vpath pattern` forgets them, and
`vpath` alone forgets all. A prerequisite that is not a file here is looked
for in them, the `vpath`s first, and found is its path: `$<`, `$^`, `$?` and
`$+` name the path. A target found through them that is out of date is made
here, under its own name, as GNU does without `GPATH`. `implicit()` looks for
a source the same way.

**Done** in `2.58-r3`, with three things the plan did not say. A rule's
commands are all expanded before the first runs, as GNU does, so `$(wildcard)`
in one does not see what the one before made. A directory's time on Braam
may be 0, and `exists()` takes it as there and as old as can be, which is
what an order-only directory wants. And `$?` has each name once, as in GNU,
now that `$+` keeps the repeats.

**Done when:** each feature has cases in `cases.mjs`, compared against host
GNU make 4.4's `-n` where it is GNU's (§4); and `test/gnu.mjs` builds a C
program with `cc` on Braam from a GNU-style makefile — pattern rules,
`$(wildcard *.c)`, `$(patsubst)`, a conditional on `DEBUG`, `VPATH` for the
sources, order-only for the object directory — then makes nothing, and
remakes one object when its source moves.

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

## 5. Order inside phase C

The directives first, being all of `nextlin()`: conditionals, `define`,
`override`, `export`. Then the text functions, which every later step's cases
use; then `$(wildcard)` and `$(shell)`, the one new mechanism; then the small
ones — order-only, `$+`, `.DELETE_ON_ERROR`, `MAKECMDGOALS`, `.DEFAULT_GOAL`.
Pattern rules after, as the largest, and `VPATH` last, since it changes what a
pattern rule's prerequisite is; `gnu.mjs` is the phase's real build.
