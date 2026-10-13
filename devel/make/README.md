# make — maintain program groups

**This is Stuart Feldman's make from Seventh Edition Unix**, version 2.58 of
14 March 1979 ([ident.cpp](ident.cpp)), Caldera's licence in
[COPYRIGHT](COPYRIGHT). It came here by way of the BESM-6 port in
`~/Project/Besm-6/v7besm/cmd/make/`, which had already turned it into C11 and
fixed six bounds v7 did not check; that port's README is the account of those,
and what follows is only what Braam changed on top of them.

```
make [-f makefile] [-inpqrstkS] [name=value]... [target]...
```

`makefile` then `Makefile` is read when no `-f` names one, and `-f -` reads
standard input. The built-in rules are `cc`'s and `as`'s:

```
.SUFFIXES : .out .o .c .s
.c.o            $(CC) $(CFLAGS) -c $<
.s.o            $(AS) -o $@ $<
.c.out .s.out .o.out
                $(CC) $(CFLAGS) $< $(LOADLIBES) -o $@
```

v7's yacc and lex rules are gone, as the BESM port's f77 rules went before
them: neither program is on Braam. `-r` leaves them all out.

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
message and returns the status. `-q`'s `exit(-1)` is a status of 255, as it was.
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
the last as before. The description file is read whole first, so the parser
under it never waits.

**Times are milliseconds.** `TIMETYPE` is 64 bits — a `long` is 32 here — and
`exists()` asks `stat_of()`, not `st_mtime`'s whole seconds, since a build here
finishes inside a second. `prestime()` is `clock_now()`.

**Directories.** A listing here is taken whole in one call and does not change
after it, so where v7 rewound an open directory to see what a command had made
since, `srchdir()` reads it again. `-d` prints a directory's name and no
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
`.PRECIOUS` names it, and exits 2. There is no `SIGQUIT`, and v7's test for an
already-ignored `SIGINT` went with it.

**Bounds.** The BESM port bounded the graph at eleven levels for its 4 KW
stack; `doname()`'s frames are on the heap here and `done = 1` already stops a
cycle, so the bound is gone and `subst()`'s is v7's 100 again. `fatal1()` and
`yyerror()` use `snprintf`, a target name being as long as it likes. Under
`-d`, a dependent that a glob emptied no longer dereferences a null name.

## Tests

[test/cases.mjs](test/cases.mjs) runs each `test/<case>.args` in `/tmp/t`, every
fixture planted beside it, and compares stdout and stderr with
`<case>.expected` and the status with `<case>.status`. Twenty-five are the
BESM port's, passing with their expectations unchanged; its `toodeep` is
`deep` here and now builds all sixteen levels. The rest are what b6sim could
not run: a `*.c` glob over a real directory, an implicit rule finding a source
nobody named, a command through the shell and one spawned directly, a command
that is not there, `*** Error code 1` and `(ignored)`, stale and fresh decided
by times, `-t` creating a target, a long-named member, an archive written by
the real `ar`, and `^C` removing a target and `.PRECIOUS` keeping one. The
harness clock is frozen, so each time a case compares is set by the case.

[test/build.mjs](test/build.mjs) installs `cc` as devel/cc's tests do and
builds a program of two sources and a header through the built-in `.c.o` rule;
then it is up to date, `touch greet.h` remakes both objects, and a changed
`greet.c` remakes its own alone.
