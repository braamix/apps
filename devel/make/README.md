# make — Braam's POSIX make

**This is POSIX's `make` and most of GNU's, grown from Stuart Feldman's make
from Seventh Edition Unix**, version 2.58 of 14 March 1979
([ident.cpp](ident.cpp)), Caldera's licence in [COPYRIGHT](COPYRIGHT). The
code, its identifiers, its comments and its messages are V7's; what it does is
POSIX.1-2024's wherever the two disagree, and GNU make's beside it. This is
the whole of POSIX's `make`, `-j` included, and of GNU's what makefiles use:
its conditionals, functions, `$(eval)`, pattern rules, target-specific
macros, `VPATH`, remade makefiles and `-O`. "Kept by" below is the rules it
was grown by. It came here by way of the BESM-6 port in
`~/Project/Besm-6/v7besm/cmd/make/`, which had already turned V7's into C11
and fixed six bounds V7 did not check; that port's README is the account of
those.

[Manual.md](Manual.md), which the package installs as `share/Manual.md`, is
the user's guide: the makefile language from the first rule to `-j`.

```
make [-BeinpqrstkSw] [-j [n]] [-O[type]] [-o file]... [-W file]... [-C dir]...
     [-f makefile]... [--option]... [name=value]... [target]...
```

`makefile` then `Makefile` is read when no `-f` names one, and `-f -` reads
standard input. The built-in rules are POSIX's, for `cc`, `as` and `ar`:

```
.SUFFIXES : .o .c .s .a
CC=cc  AS=as  AR=ar  ARFLAGS=-rvU  CFLAGS=  LDFLAGS=
.c              $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
.s              $(CC) $(LDFLAGS) -o $@ $<
.c.o            $(CC) $(CFLAGS) -c $<
.s.o            $(AS) -o $@ $<
.c.a            $(CC) -c $(CFLAGS) $<
                $(AR) $(ARFLAGS) $@ $*.o
                rm $*.o
```

so `make hello` builds `hello` from `hello.c` with no makefile at all, and
`lib.a: lib.a(a.o) lib.a(b.o)` builds a library. `ARFLAGS` has `-U` and
`.c.a` has `rm` and not `rm -f`, both for Braam's tools: `ar` dates a member 0
without `-U` (below, "Archives"), and `rm` has no `-f`. V7's
yacc and lex rules are gone, as the BESM port's f77 rules went before them:
neither program is on Braam. `-r` leaves them all out.

## Where POSIX overrules V7

| V7 | Here, as POSIX has it |
|---|---|
| Backslash-newline in a command became a space | Kept for the shell; the next line loses one leading tab |
| Elsewhere it joined lines past any empty ones | One space for it and the blanks around it, and no further |
| A last line with no newline was dropped | It is a line |
| `.out` rules: `make hello` wanted a target `hello.out` | Single-suffix rules, `.c:` and `.s:` |
| An inference rule was looked for for every target | Only for one with no commands of its own |
| The default goal was the first *name* met, a prerequisite too | The first *target* that is not special |
| `.SILENT` and `.IGNORE` named anywhere acted on every target | With prerequisites, on those alone |
| `-n` left out what `-s` or `.SILENT` silenced | It writes every command that would run |
| `-q` exited 255 when out of date, every error 1, `-k` 0 | 1, 2, and 2 when `-k` went past a failure |
| A `-f` file was read when its option was met | Every option first, then the files |
| A `lib(member)` whose library is not there stopped make | The member is not there either, so it is made |
| `$@` of a `lib(member)` target was the whole name | The library, with the member in `$%` |
| The environment was not read | Its variables are macros, below the makefile's |
| `No suffix list.` under `-r` | Nothing |

`$<` and `$*` in an explicit rule are the first prerequisite and the target
less a known suffix, as GNU and BSD make have them; V7 left both as the last
inference rule set them.

## Beyond V7

| What | From | Notes |
|---|---|---|
| `include`, `-include`, `sinclude` | POSIX.1-2024 | The names are expanded; nested to 100 |
| `$(name:a=b)`, `$(name:%a=%b)` | POSIX, POSIX.1-2024 | In each word; the words come out one space apart |
| `:=`, `::=`, `+=`, `?=` | POSIX.1-2024 | `:=` expands once; `+=` after one expands what it adds |
| Environment macros, `-e` | POSIX | Command line, then makefile, then environment, then built-in; `-e` puts the environment above the makefile. `SHELL` is not taken |
| `MAKE`, `MAKEFLAGS`, `SHELL` | POSIX | A child gets the flags and the command line's macros in `MAKEFLAGS` |
| `+` before a command, and a command naming `$(MAKE)` | POSIX | Run under `-n`, which is how a recursive make passes it down |
| `.PHONY` | POSIX.1-2024 | Never looked for, always remade, never removed by `^C` |
| `$^` | GNU, POSIX.1-2024 | Every prerequisite once, an inferred one first |
| `$(@D)`, `$(@F)`, and the same of `< * ? % ^` | POSIX | In each word, the directory (`.` without one) or the file |
| `lib(member)` targets, `$%`, `.c.a` | POSIX | The member less its suffix is `$*`, and the rule is from a source's suffix to `.a` |
| A member in `$?` and `$^` is its member's name | GNU | So `$(AR) $(ARFLAGS) $@ $?` names the objects |
| `!=`, `:::=` | POSIX.1-2024 | `!=` runs `sh -c` and keeps what it wrote, newlines as spaces; `:::=` expands now and doubles each `$` |
| `+` under `-t` and `-q` | POSIX | Only the `+` lines run; `-t` then touches |
| `-C dir` | GNU, BSD | In order, before any makefile; not passed on in `MAKEFLAGS` |
| `.WAIT`, `.POSIX`, `.NOTPARALLEL`, `.SCCS_GET` | POSIX.1-2024 | `.WAIT` and `.NOTPARALLEL` order what `-j` runs; the others are accepted |
| `-j [n]` | POSIX | Up to `n` commands at once, any number without one. A failure without `-k` waits for what is running and stops; `^C` goes to every child. Not passed to a sub-make, which runs one at a time; the children write as they go |
| `ifeq`, `ifneq`, `ifdef`, `ifndef`, `else`, `endif` | GNU | `else ifeq` chains; `ifdef` asks for a value that is not empty, unexpanded |
| `define`/`endef` | GNU | With any assignment operator; a value with lines is a command per line in a rule |
| `override`, `export`, `unexport` | GNU | A child gets the environment's and the command line's macros, and the exported ones, at their values now |
| Text functions | GNU | `subst patsubst strip findstring filter filter-out sort word wordlist words firstword lastword dir notdir suffix basename addsuffix addprefix join if or and foreach` |
| `$(wildcard)`, `$(shell)` | GNU | Sorted, a leading `.` matched only by one; two identical calls in one line or one rule's commands are run once |
| A rule's commands are all expanded before the first runs | GNU | So `$(wildcard)` in one does not see what the one before made; they are expanded in order, so an `$(eval)` in one is seen by the next |
| Order-only prerequisites, `a: b \| c`, and `$\|` | GNU | Made first; their times are not looked at |
| `$+` | GNU | Every prerequisite in order, repeats kept; `$?` has each once |
| `.DELETE_ON_ERROR` | GNU | A target its failed command changed is removed, as `^C` removes it |
| `MAKECMDGOALS`, `.DEFAULT_GOAL` | GNU | A makefile may set `.DEFAULT_GOAL`, and reads the default target's name in it when it does not |
| Pattern rules, `%.o: %.c` | GNU | Tried before the suffix rules; of those that apply, the shortest stem wins. A `%` matches at least one character, and one with no commands cancels |
| Chained pattern rules, `.INTERMEDIATE`, `.SECONDARY` | GNU | A prerequisite another pattern rule makes is intermediate: not made when what needs it is newer than its sources — unless that is remade for another reason — and removed once made, with one `rm` line. Ten deep, each rule once in a chain |
| Static patterns, `objs: %.o: %.c` | GNU | A rule per target, its stem `$*` |
| `VPATH`, `vpath` | GNU | A prerequisite not here is looked for through the matching `vpath`s, then `VPATH`, and is its path in `$<`, `$^` and the rest; an inference rule's source too. A target found there that is out of date is made here |
| `$(value)`, `$(origin)`, `$(call)` | GNU | `$(call)` binds `$(0)`, `$(1)`... as `$(foreach)` binds its name |
| `$(info)`, `$(warning)`, `$(error)` | GNU | Said once the expansion is done for good, never twice; `$(error)` stops make |
| `$(eval)` | GNU | Read as one more input buffer before the next line of the makefile, or, in a rule's commands, before the next command is expanded. A macro it sets is not seen by the rest of its own line |
| Target-specific and pattern-specific macros | GNU | `target: name op value`, with `override` and `export`; in effect while the target is made, and so for its prerequisites too |
| Long options, `--help` | GNU | `--directory`, `--file`, `--makefile`, `--dry-run` and the rest, each standing for its letter; a parent's long options in `MAKEFLAGS` are understood or passed over |
| `.ONESHELL` | GNU | A rule's lines are one script to one `sh -c`, without `-e`, its first line's `@`, `-` and `+` for the whole |
| Remade makefiles | GNU | Each makefile and included file that is a target is brought up to date first, under `-n` too; if one was remade, make starts itself again from the start, `MAKE_RESTARTS` counting. An `include` that is missing is an error only if it cannot be made |
| `$(abspath)`, `$(realpath)`, `$(flavor)`, `$(intcmp)`, `$(let)`, `$(file)` | GNU | `realpath` follows links and drops what is not there; `$(file)` is done when its expansion is, once however often the expansion is done |
| `undefine` | GNU | A command-line macro keeps its value, `override` or not, as GNU's does |
| `MAKELEVEL`, `-w`, `--print-directory`, `--no-print-directory` | GNU | `make[1]: Entering directory '...'` and `Leaving`, said under `-w`, and by default in a sub-make and under `-C` but for `-s` |
| `-B`, `-o file`, `-W file` and their long names | GNU | Every target with commands out of date, `$?` all of them; `file` as old as can be and never made; `file` as if just changed |
| `-O`, `--output-sync` | GNU | Under `-j`, a target's output written whole when it is made, or with `line` each command's; stdout before stderr, as each goes down a pipe of its own. At most 32 jobs, as 64 pipes are what one poll takes |
| No fixed limits | — | The name table, lines, words, expansions, `$?`, a rule line's targets and a command's arguments grow as they need to |

An assignment that expands at once — `:=`, `::=`, `:::=`, `!=` — is not a
*use* of the macros it reads, so V7's `Warning: X changed after being used`
is not said when one of them changes after: the change is what the operator
is for.

**Includes** are why `yyparse()` is a coroutine and the lexer under it is not.
`nextlin()` finds the line and expands the names; `yyparse()` awaits
`includef()`, which reads each file whole and pushes it on a stack of input
buffers; `getfin()` pops one at its end and goes on with the line after the
include. The await is a file read, which parks every time, so the stack is
back to nothing at each one however many there are — a case includes three
hundred. An await per token would not park, and would cost native stack per
token.

**`-j`** keeps V7's `doname()`, which makes a target by walking what it
needs. Under `-j` a target's commands become a job: `doname()` expands them,
starts the first, marks the target running and returns, and a target that
needs one running returns too, to be looked at again. The goals are walked
again each time a command ends, `wait_child` taking whichever ends first. Each
walk awaits a `stat` per target, which parks, so its stack is back to nothing
each time.

**`$(wildcard)` and `$(shell)`** wait, and `subst()` cannot: it is plain
code, and an await in it that did not park would cost native stack per macro
reference. So each is answered from a cache, and a call that is not there is
recorded instead, and the expansion marked as needing it. The coroutine
above — `yyparse()` for a makefile line, `docom()` for a rule's commands,
`runset()` for a `!=` — runs what was recorded and expands the same text
again, which now finds it; nothing of a line is done until it expands
whole. A call whose own argument is still waiting is not recorded until that
argument is whole, so `$(shell ls $(wildcard x*))` runs once, on the names,
and `$(error)` does not stop on half its message. The cache is emptied after
each command, which may have changed what either would say. `$(realpath)` and
`$(file)` are requests too.

**Remade makefiles** start make again, and its state — the name table, the
macros, the rules, every static in the parser — was never written to be
emptied. So make does not start again in the process: it spawns itself, with
the arguments it was given, from the directory it began in, and
`MAKE_RESTARTS` one higher, waits for it as for any command, `^C` included,
and exits with its status. A Braam process is a fresh instance, so that is
what GNU's `exec` of itself is here. Twenty starts in a row stop it.

**`-O`** gives each command's stdout and stderr a pipe each — two, as a
descriptor given to a child is moved into it and one cannot serve both. The
job loop then waits on `poll_fds()` over every running job's pipes rather
than on `wait_child()`, holds what each says, reaps a command by pid once
both its pipes are at their end, and writes the job's output whole.

## Kept by

1. **POSIX wins, V7's code stays.** Behaviour follows POSIX wherever the two
   disagree, with no switch to get V7's back. What stays is V7's code, its
   identifiers and structure, its comments, and its own text where POSIX does
   not specify any — the `Make: ... Stop.` messages, `` `x' is up to
   date.``, `-p` and `-d`. A case's expectation changes only where POSIX
   demands it, and the commit that changes it says which rule required it.
2. **POSIX, then GNU.** Where the two read the same text two ways, POSIX
   decides.
3. **Await only where something parks.** A `co_await` is a call and not a
   tail call, so a loop that awaits without parking grows the native stack
   until the process traps. `yyparse()` awaits `include` and `!=`, both of
   which park on a system call; the lexer, `nextlin()` and `subst()` stay
   plain, since they run once per token, line and macro reference and almost
   never need to wait. No coroutine frame holds a buffer.
4. **The README keeps up.** Each addition goes in the "Beyond V7" table with
   the standard it comes from, and each POSIX difference in "Where POSIX
   overrules V7".
5. **Version:** there is no upstream version to move, so only the revision
   counts up — `2.58-r8` now.

Not done, on purpose: GNU's `.SECONDEXPANSION`, grouped targets `&:`, `load`
and Guile; the jobserver between separate `make`s, which needs a descriptor
passed to a child where a spawn passes only the three streams, so a sub-make
runs one command at a time; and BSD make's dialect (`.if`, `.for`, `:M`).

## What the port changed

**The program runs other programs.** v7's `fork()` and `execvp()` are one
`spawn()`, and its `wait()` loop is `wait_child()` on that one pid. A command
with no shell metacharacter is split and spawned directly, as v7 did, and the
kernel searches `PATH` for it; one with a metacharacter goes to `/bin/sh -e -c`
— `-ce` would be `-c` and a command named `e` to this shell. `-i` or a leading
`-` drops the `-e`. A child is entered with make's own three streams and no
other descriptor, so `doclose()`, which closed the open directories in the
child before the exec, has gone. `await()` puts the exit code in the second
byte, where `docom1()` reads it, so `*** Error code 1` is v7's own line.

**`exit()` traps, so `fatal()` stops instead.** It records the message and a
sticky status, and every caller of anything that may stop tests `stopped` and
returns, a frame at a time, as vi's `error()` does. `proc_main()` says the
message and returns the status, 2 for an error and 1 for `-q`'s `exit(-1)`.
Out of memory is the one exception: every allocation would have to be tested,
so `ckalloc()` traps instead.

**Output.** A `co_await` is a call and not a tail call, so a loop that awaits
without suspending grows the native stack until the process traps, and
`printdesc()` under `-p` is such a loop over every name. So nothing prints
through an await: `mprintf()` and `diag()` are v7's `printf` and
`fprintf(stderr, ...)`, held in one buffer in the order they were said, and
`flushout()` writes it before each command runs and at exit. That also lets the
parser, `setvar()` and `subst()` stay plain functions and still warn. For the
same reason `doname()` takes a dependent that is already done without awaiting
it, and `docom()` prints a command itself and awaits only to run one, so `-n`
over a long command list never awaits at all.

**The grammar is written out by hand** in [gram.cpp](gram.cpp). There is no
yacc in this build, and gram.y's grammar is seven rules. The tokens, `yylex()`,
`nextlin()` and every action are gram.y's; the parser mirrors yacc's default
reduction, so a rules line with no separator still says `Must be a separator`
and anything else unexpected is `line N: syntax error`, the first error being
the last as before. A description file is read whole first, so only an
`include` makes the parser wait.

**Times are milliseconds.** `TIMETYPE` is 64 bits — a `long` is 32 here — and
`exists()` asks `stat_of()`, not `st_mtime`'s whole seconds, since a build here
finishes inside a second. `prestime()` is `clock_now()`.

**Directories.** A listing here is taken whole in one call and does not change
after it, so where v7 rewound an open directory to see what a command had made
since, `srchdir()` reads it again. A directory that cannot be opened has no
names in it, as in GNU make, where v7 stopped with `Directory d: Cannot
open`: an implicit-rule probe for `obj/x.d`, before a command has made
`obj/`, is an ordinary miss. `-d` prints a directory's name and no
descriptor, there being none.

**Archives** are what [devel/wasm](../wasm/)'s `ar` writes, System V's:
`!<arch>\n`, 60-byte headers of decimal fields, a long name in the `//`
member. `lib.a(member)` walks the headers; `lib.a((entry))` looks the symbol
up in the `/` member, so no object is read at all. A member's date is whole
seconds, so it is taken as the end of its second: a member is then never older
than the file it was made from. **`ar` dates its members 0 unless given `-U`**,
and make reads 0 as "not there", so a library make keeps up to date wants
`ar rU`.

**Signals.** `^C` is caught. It reaches make where make parks, which is in
`wait_child()`: the child is sent `SIG_INT` (a second `^C` sends `SIG_KILL`),
reaped, and then `intrupt()` does what v7's did — removes `$@` unless
`.PRECIOUS` names it, and exits 2. A `^C` that comes while make is in a call
the kernel cannot abandon, a `stat` or a `spawn`, is only noted, so make asks
`sig_take()` before each wait as well: a long walk is mostly such calls. There is no `SIGQUIT`, and v7's test for an
already-ignored `SIGINT` went with it.

**Bounds.** The BESM port bounded the graph at eleven levels for its 4 KW
stack; `doname()`'s frames are on the heap here and `done = 1` already stops a
cycle, so the bound is gone and `subst()`'s is v7's 100 again. `fatal1()` and
`yyerror()` use `snprintf`, a target name being as long as it likes. Under
`-d`, a dependent that a glob emptied no longer dereferences a null name.

**No buffer has a fixed size.** V7's `INMAX`, `OUTMAX`, `QBUFMAX`, `NLEFTS`,
its 509-slot name table and its 100-byte names all stopped make when a
makefile outgrew them. The table doubles at two-thirds full; `subst()` says
when its buffer is full rather than stopping, and `substa()` expands again
into one twice the size until the text fits; the line, the word, `$?`, a rule
line's targets and `doexec()`'s arguments are sized from what they hold.
Expanding twice is safe because an expansion changes nothing but `used`.
`MAXSUBST` and `MAXINCL`, 100 each, stay: they stop a macro or a file that
refers to itself.

## Tests

[test/cases.mjs](test/cases.mjs) runs each `test/<case>.args` in `/tmp/t`, every
fixture planted beside it, and compares stdout and stderr with
`<case>.expected` and the status with `<case>.status`; `<case>.env` sets its
environment. Twenty-five came from the BESM port, and their expectations moved
only where POSIX moved them — the `No suffix list.` line and the statuses. The
rest are what b6sim could not run (globs over a real directory, commands run
directly and through the shell, times, archives the real `ar` wrote, `^C`) and
what was added beyond V7, each in its own case. The harness clock is frozen, so
each time a case compares is set by the case.

**Against GNU make.** Where the host has GNU make 4, every case in
`GMAKE_CASES` also runs under `-n` on Braam and under the host's `gmake -n`,
over the same files at the same times, and the commands must agree line for
line; the messages, being V7's, are not compared. Without one the run says so
and goes on.

[test/deps.mjs](test/deps.mjs) has `cc` write the dependencies, both ways:
`-MMD -MP` beside each object, and a `%.d: %.c` rule with `-MM`, which make
remakes as makefiles and starts again to read. Either way a moved header
remakes exactly the objects that include it, and with `-MP` a header that
is gone does not stop make.

[test/jobs.mjs](test/jobs.mjs) builds a program of five sources under
`-j 4`, links it only after every object, gets the same bytes as under `-j 1`,
and has a failing compile wait for the one running beside it before make
stops. `^C` under `-j` is `cases.mjs`'s `jobsint`; jobs.mjs sends one while
make walks 200 targets, with `-j 4` and without, and make stops.

[test/gnu.mjs](test/gnu.mjs) builds a program with `cc` on Braam from a
makefile written for GNU make — `$(wildcard)` and `$(patsubst)` for the
objects, a pattern rule, `VPATH` for the sources, an order-only object
directory — makes nothing on the run after, remakes one object when its source
moves, and builds again under `make DEBUG=1`, which an `ifdef` turns into a
flag.

[test/library.mjs](test/library.mjs) builds a library through `.c.a` with
`cc` and `ar` on Braam, links a program against it, makes nothing on the run
after, and remakes one member when its source moves.

[test/build.mjs](test/build.mjs) installs `cc` as devel/cc's tests do and
builds a program of two sources and a header through the built-in `.c.o` rule;
then it is up to date, `touch greet.h` remakes both objects, and a changed
`greet.c` remakes its own alone. [test/portable.mjs](test/portable.mjs) builds
a program of three directories from makefiles in the portable style: `cd dir
&& $(MAKE)`, an included configuration, `.PHONY: all clean`, `$^` and
`$(SRCS:.c=.o)`. Then it is up to date, `make -n` after a header moves names
every command and builds nothing, `make` builds them, `make clean` cleans, and
`make CFLAGS=...` reaches the sub-makes through `MAKEFLAGS`.
