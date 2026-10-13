# make on Braam

`make` rebuilds files from other files, running only the commands whose
results are out of date. You describe what each file is made from, and how,
in a *makefile*; `make` compares file times and does the rest.

This `make` is POSIX's `make`, with most of GNU make's additions. A makefile
written for either usually works unchanged. This manual covers all of it,
from the first rule to parallel builds.

## 1. A first makefile

In a file called `makefile`:

```make
hello: hello.o greet.o
	cc -o hello hello.o greet.o

hello.o: hello.c greet.h
	cc -c hello.c

greet.o: greet.c greet.h
	cc -c greet.c
```

```sh
make
```

`make` makes the first target, `hello`. To do that it first makes what
`hello` needs — `hello.o` and `greet.o` — and each of those from its `.c`
file. A file is remade only if it is missing or older than something it
needs. Run `make` again and it says:

```text
`hello' is up to date.
```

Change `greet.h` and run `make`: both objects are recompiled and `hello`
relinked. Change `greet.c`: only `greet.o` and `hello`.

**The command lines must start with a tab**, not spaces. This is the most
common mistake in a makefile.

## 2. Running make

```text
make [options] [name=value]... [target]...
```

With no target, `make` makes the *default goal* (§4.3). Each target named
is made in turn. A `name=value` argument defines a macro (§6) that the
makefile cannot change.

`make` reads `makefile`, or `Makefile` if there is no `makefile`, unless
`-f` names others.

| Option | Long form | What it does |
|---|---|---|
| `-f file` | `--file=file`, `--makefile=file` | Read `file` as the makefile; several `-f` read several files in order. `-f -` reads standard input |
| `-C dir` | `--directory=dir` | Change to `dir` first. Several `-C` apply one after another |
| `-n` | `--dry-run`, `--just-print`, `--recon` | Print the commands, run none of them (but see `+`, §5.2) |
| `-s` | `--silent`, `--quiet` | Do not print the commands as they run |
| `-i` | `--ignore-errors` | Go on after a command fails |
| `-k` | `--keep-going` | After a failure, go on making whatever does not need the failed target |
| `-S` | `--stop`, `--no-keep-going` | Undo `-k` |
| `-t` | `--touch` | Do not run commands; update the targets' times instead |
| `-q` | `--question` | Run nothing; exit 0 if everything is up to date, 1 if not |
| `-B` | `--always-make` | Treat every target that has commands as out of date |
| `-o file` | `--old-file=file`, `--assume-old=file` | Treat `file` as very old, and never remake it |
| `-W file` | `--what-if=file`, `--new-file=file`, `--assume-new=file` | Treat `file` as just changed. With `-n`: "what would be rebuilt if I edited this?" |
| `-j [n]` | `--jobs[=n]` | Run up to `n` commands at once; with no `n`, as many as can run (§14) |
| `-O[type]` | `--output-sync[=type]` | With `-j`, keep each target's output together (§14) |
| `-e` | `--environment-overrides` | Environment variables win over the makefile's macros |
| `-r` | `--no-builtin-rules` | Forget the built-in rules (§9.5) |
| `-p` | `--print-data-base` | Print every macro and rule, then make as usual |
| `-d` | `--debug` | Print what is looked at, and the times compared |
| `-w` | `--print-directory` | Print the directory on entering and leaving |
| | `--no-print-directory` | Never print it, not even in a sub-make |
| | `--help` | List the options |

Single-letter options can be combined: `-ks` is `-k -s`.

**Exit status**: 0 when everything went well; 1 from `-q` when something is
out of date; 2 on any error.

## 3. Makefile syntax

A makefile is made of *rules*, *macro definitions*, *directives* (such as
`include` and `ifdef`) and comments.

**Comments** start with `#` and run to the end of the line. In a command
line (one starting with a tab) `#` is not special: the whole line goes to the
shell, which treats `#` as its own comment.

**Long lines** can be split by ending a line with a backslash `\`. In a rule
or macro line, the backslash, the newline and the blanks around them become
one space. In a command line they are passed to the shell as they are, and
the tab at the start of the next line is dropped.

```make
OBJS = main.o parse.o \
       eval.o print.o

install:
	cp prog /usr/local/bin/prog && \
	echo installed
```

Blank lines are ignored.

## 4. Rules

### 4.1 The parts of a rule

```make
targets: prerequisites
	command
	command
```

- **Targets** are the files the rule makes; there can be several, and the
  rule applies to each of them separately.
- **Prerequisites** are the files the targets are made from. They are made
  first, if they need to be.
- **Commands** are the shell lines that make the target. Each starts with a
  tab.

A short command can go on the rule line after a `;`:

```make
clean: ; rm -f *.o prog
```

A target may appear in several rules. Their prerequisites add up, but only
one of the rules may have commands:

```make
main.o: main.c
	cc -c main.c
main.o: config.h          # one more prerequisite, no commands
```

### 4.2 When is a target out of date?

A target is remade when

- the file does not exist, or
- any prerequisite is newer than it, or
- any prerequisite was remade in this run.

A target with no prerequisites is made only if its file is missing. A file
that is not a target of any rule but exists is simply used; one that neither
exists nor has a rule stops make:

```text
Make:  Don't know how to make config.h.  Stop.
```

### 4.3 The default goal

With no target on the command line, `make` makes the first target of the
first rule — skipping targets that start with a dot (special targets, §13),
unless they contain a `/`, and pattern rules (§9.2). A makefile can choose
another by setting `.DEFAULT_GOAL`:

```make
.DEFAULT_GOAL = all
```

### 4.4 Phony targets

A target that is not a file — `all`, `clean`, `install` — should be marked
`.PHONY`. Then its commands always run, and a file that happens to be called
`clean` cannot get in the way:

```make
.PHONY: all clean
all: prog tests
clean:
	rm -f *.o prog
```

### 4.5 Double-colon rules

With `::`, each rule for a target stands alone: its commands run when *its*
prerequisites are newer, whatever the other rules say.

```make
log:: a.txt
	echo a changed >> log
log:: b.txt
	echo b changed >> log
```

A target's rules must be all `:` or all `::`.

### 4.6 Order-only prerequisites

Prerequisites after a `|` are made first, but their times do not matter. The
usual use is a directory, whose time changes whenever a file is added to it:

```make
obj/main.o: main.c | obj
	cc -c -o obj/main.o main.c
obj:
	mkdir obj
```

### 4.7 Wildcards in prerequisites

A prerequisite containing `*`, `?` or `[...]` is replaced by the files that
match it, in the directory's order:

```make
print: *.c
	lpr $?
```

A pattern only matches in its last part: `src/*.c` works, `*/x.c` does not.
If nothing matches, the name is kept as written. In macros and commands use
`$(wildcard)` (§8.2) instead.

## 5. Commands

### 5.1 How commands run

Each command line runs on its own. A line with shell characters in it
(`|`, `;`, `>`, `$`, quotes, `*` and the like) is run by `/bin/sh -e -c
line`; a plain one, such as `cc -c main.c`, is run directly, which is faster.
So a `cd` on one line has no effect on the next:

```make
wrong:
	cd sub
	make            # runs in the old directory

right:
	cd sub && make
```

`make` prints each command before running it. If a command fails — exits
with a status other than 0 — `make` says

```text
*** Error code 1

Stop.
```

and stops. Before running any command of a rule, `make` expands all of
them (§6): macros are replaced, and functions such as `$(shell)` are run.

### 5.2 Command prefixes

A command may start with any of these characters, after the tab:

| Prefix | Meaning |
|---|---|
| `@` | Do not print the command before running it |
| `-` | Ignore a failure: say `(ignored)` and go on |
| `+` | Run it even under `-n`, `-t` and `-q` |

```make
clean:
	-rm *.o           # no error if there are no .o files
	@echo cleaned
```

A command that names `$(MAKE)` or `${MAKE}` counts as `+`, so `make -n`
shows what a sub-make would do instead of skipping it.

### 5.3 Errors

- `-i`, `.IGNORE:` with no prerequisites, or a `-` prefix: failures are
  ignored.
- `.IGNORE: t1 t2`: failures are ignored for those targets only.
- `-k`: after a failure, `make` goes on with everything that does not depend
  on the failed target, then says `` `goal' not remade because of errors ``
  and exits with 2.
- `.DELETE_ON_ERROR:` anywhere in the makefile: when a command fails, a
  target it has already changed is removed, so a half-written file is not
  mistaken for a good one next time.

**Interrupting** with `^C` stops the running command, removes the target it
was making (so it is rebuilt next time), and exits with 2. Targets listed in
`.PRECIOUS` or `.PHONY`, and libraries (§12), are not removed.

### 5.4 `.ONESHELL`

Normally every line is a separate shell. With `.ONESHELL:` anywhere in the
makefile, all the lines of a rule go to one shell as one script, so `cd`
and shell variables carry from line to line. The `@`, `-` and `+` of the
first line apply to the whole script; on later lines they are removed. The
script runs with `sh -c` (no `-e`), so it stops on a failure only if it says
so.

### 5.5 Recursive make

A makefile can run `make` in another directory:

```make
all:
	cd lib && $(MAKE)
	$(MAKE) -C app
```

Always write `$(MAKE)`, not `make`. The sub-make gets the options and the
command-line macros in the environment variable `MAKEFLAGS`, so `make -k
CFLAGS=-g` reaches every level. `-j`, `-C`, `-f`, `-o` and `-W` are not
passed on. A sub-make prints where it works:

```text
make[1]: Entering directory '/home/me/lib'
...
make[1]: Leaving directory '/home/me/lib'
```

`MAKELEVEL` is 0 in the top `make`, 1 in a sub-make, and so on.

## 6. Macros

Macros (also called variables) hold text. They make a makefile shorter and
easier to change.

### 6.1 Defining and using

```make
CC     = cc
CFLAGS = -O
OBJS   = main.o util.o

prog: $(OBJS)
	$(CC) -o prog $(OBJS)
```

- `$(NAME)` and `${NAME}` are the macro's value. A one-character name needs
  no parentheses: `$X`. An undefined macro is empty.
- `$$` is a single `$`, which is how a command passes `$` to the shell:
  `for f in *.c; do echo $$f; done`.
- A name may contain letters, digits, `_`, `.` and `-`; case matters.
- Blanks after `=` are dropped; blanks at the end of the line are kept, and
  so is everything before a `#` comment: in `D = /usr # dir` the value is
  `/usr ` with a trailing blank.

### 6.2 Assignment operators

| Form | Meaning |
|---|---|
| `X = text` | *Recursive*: `text` is stored as it is, and its macros are expanded each time `X` is used |
| `X := text`, `X ::= text` | *Simple*: `text` is expanded now, once; later changes to the macros in it do not matter |
| `X :::= text` | Expanded now, like `:=`, but stored as a recursive macro, every `$` doubled |
| `X += text` | Append a blank and `text`. For a simple `X`, `text` is expanded now |
| `X ?= text` | Set `X` only if it has no value yet |
| `X != command` | Run `command` with `sh -c` now; its output, newlines turned to blanks and the last one dropped, is the value |

The difference between `=` and `:=`:

```make
A = one
B = $(A)          # B is "$(A)", looked up when used
C := $(A)         # C is "one", now and for ever
A = two
# $(B) is "two", $(C) is "one"
```

### 6.3 Where a value comes from

A macro can be set in four places. The first in this list wins:

1. the command line: `make CFLAGS=-g`;
2. the makefile;
3. the environment (with `-e`, the environment is above the makefile);
4. the built-in macros (§9.5).

So a user can always override the makefile from the command line. A
makefile that must win even so says `override`:

```make
override CFLAGS += -Wall      # added even to a command-line CFLAGS
```

`SHELL` and `MAKEFLAGS` are not taken from the environment. Commands always
run with `/bin/sh`.

### 6.4 Multi-line values: `define`

```make
define HEADER
/* generated by make */
#include "config.h"
endef
```

The lines up to `endef` are the value, exactly as written, comments
included. `define NAME :=` (or `+=`, `?=`, `!=` and the rest) uses that
operator. A multi-line value used in a rule's commands becomes one command
per line, each with its own `@`, `-` and `+` — a *canned recipe*:

```make
define compile
@echo compiling $<
$(CC) $(CFLAGS) -c $<
endef

%.o: %.c
	$(compile)
```

### 6.5 Removing a macro: `undefine`

`undefine X` makes `X` undefined again, as if never set. A macro from the
command line is not removed.

### 6.6 The environment of commands: `export`

Commands get `make`'s environment. In addition:

- every macro that came from the environment or the command line is passed,
  at its current value;
- `export X` passes `X` too; `export X = value` defines and exports at once;
- `export` alone passes every macro;
- `unexport X` keeps `X` out, even if it came from the environment.

```make
export PATH := $(PATH):/home/me/tools
```

### 6.7 Target-specific macros

A macro can have a different value while one target is being made:

```make
prog: CFLAGS += -g
prog: main.o util.o
	cc -o prog main.o util.o
```

The value applies to the target, and also to everything made for it, so here
`main.o` and `util.o` are compiled with `-g` too (if they are first made for
`prog`). `override` and `export` may come before the name. A pattern works
the same way:

```make
%.o: CFLAGS += -O2
```

## 7. Automatic macros

Inside a rule's commands, these are set for you:

| Macro | Value |
|---|---|
| `$@` | The target |
| `$<` | The first prerequisite; in an inference rule, the source it found |
| `$^` | All prerequisites, each once |
| `$+` | All prerequisites, in order, repeats kept |
| `$?` | The prerequisites newer than the target |
| `$|` | The order-only prerequisites |
| `$*` | The *stem*: the target without its suffix (`main` for `main.o`), or what `%` matched in a pattern rule |
| `$%` | For a library member `lib.a(x.o)`, the member `x.o`; `$@` is then `lib.a` |

Each has a `D` and an `F` form: `$(@D)` is the target's directory (`.` if it
has none) and `$(@F)` its file name. The same goes for `$(<D)`, `$(^F)` and
the others; for a list, each word is split.

```make
obj/%.o: src/%.c
	@mkdir -p $(@D)
	cc -c -o $@ $<
```

## 8. Expanding text

### 8.1 Substitution references

`$(NAME:old=new)` replaces `old` at the end of each word of `NAME`:

```make
SRCS = main.c util.c
OBJS = $(SRCS:.c=.o)          # main.o util.o
```

With `%`, it is a pattern, `%` standing for any part:

```make
OBJS = $(SRCS:%.c=obj/%.o)    # obj/main.o obj/util.o
```

### 8.2 Functions

A function call is `$(name arguments)`, arguments separated by commas. Blanks
after the name are dropped; other blanks are kept. A comma inside nested
parentheses does not separate.

**Text**

| Function | Result |
|---|---|
| `$(subst from,to,text)` | `text` with every `from` replaced by `to` |
| `$(patsubst pat,repl,words)` | Each word matching `pat` (with `%`) replaced by `repl` |
| `$(strip text)` | Blanks at the ends removed, runs of blanks made one |
| `$(findstring s,text)` | `s` if `text` contains it, else empty |
| `$(filter pats,words)` | The words that match any of the patterns |
| `$(filter-out pats,words)` | The words that match none of them |
| `$(sort words)` | Sorted, repeats removed |
| `$(word n,words)` | The `n`th word, counting from 1 |
| `$(wordlist s,e,words)` | Words `s` to `e` |
| `$(words words)` | How many words |
| `$(firstword words)`, `$(lastword words)` | The first or last word |

**File names**

| Function | Result |
|---|---|
| `$(dir names)` | Each name's directory part, up to the last `/`; `./` if none |
| `$(notdir names)` | Each name without its directory |
| `$(suffix names)` | Each name's suffix: `.c` for `src/a.c` |
| `$(basename names)` | Each name without its suffix |
| `$(addsuffix s,names)`, `$(addprefix p,names)` | `s` added after, or `p` before, each name |
| `$(join list1,list2)` | Word by word: `$(join a b,.c .h)` is `a.c b.h` |
| `$(wildcard pats)` | The existing files that match, sorted. A name starting with `.` matches only a pattern starting with `.` |
| `$(abspath names)` | Each name made absolute, `.` and `..` removed |
| `$(realpath names)` | The same, with links followed; names that do not exist are dropped |

**Conditions and loops**

| Function | Result |
|---|---|
| `$(if cond,then[,else])` | `then` if `cond` is not empty (after stripping), else `else`. Only the chosen part is expanded |
| `$(or a,b,...)` | The first non-empty argument |
| `$(and a,b,...)` | The last argument if none is empty, else empty |
| `$(intcmp a,b[,lt[,eq[,gt]]])` | Compares two integers. `gt` defaults to `eq`; with only `a,b`, the number if equal, else empty |
| `$(foreach v,words,text)` | `text` expanded once per word, `$(v)` being that word; the results joined by blanks |
| `$(let a b,words,text)` | `text` with `$(a)` the first word, `$(b)` the rest |

**Macros**

| Function | Result |
|---|---|
| `$(call v,a1,a2,...)` | The value of `v` expanded with `$(1)`, `$(2)`... set to the arguments, `$(0)` to `v` |
| `$(value v)` | `v`'s value, not expanded |
| `$(origin v)` | Where `v` came from: `undefined`, `default`, `environment`, `environment override`, `file`, `command line`, `override` or `automatic` |
| `$(flavor v)` | `undefined`, `recursive` or `simple` |

**Doing things**

| Function | Result |
|---|---|
| `$(shell command)` | Runs `command` with `sh -c`; its output, newlines turned to blanks, the last one dropped |
| `$(file >name,text)` | Writes `text` and a newline to `name`; `>>name` appends. Empty |
| `$(file <name)` | The contents of `name`, its last newline dropped |
| `$(eval text)` | Reads `text` as makefile lines (rules, macros) |
| `$(info text)` | Prints `text` |
| `$(warning text)` | Prints `text` on the error output, with the line number when in a makefile |
| `$(error text)` | Prints `Make: text.  Stop.` and stops |

`$(call)` with `$(eval)` writes rules from a template:

```make
define PROGRAM
$(1): $(1).o lib.a
	cc -o $(1) $(1).o lib.a
endef

PROGS = cat cp mv
$(foreach p,$(PROGS),$(eval $(call PROGRAM,$(p))))
```

In the template, `$$@` survives `$(call)` as `$@`, for the rule to use.
Rules made by `$(eval)` count for the default goal like any other, so put
`all:` before them.

Points to know:

- Two identical `$(shell ...)` or `$(file ...)` calls in the same line, or
  in the same rule's commands, are run only once.
- A macro set by `$(eval)` is not seen by the rest of the line the `$(eval)`
  is on; it is from the next line on.
- An `$(eval)` in one command is seen by the commands after it.

## 9. Inference rules

Most targets are made the same way — every `.o` from its `.c`. Inference
rules say so once.

### 9.1 Suffix rules

The POSIX way. A rule whose target is two suffixes makes a file with the
second suffix from one with the first:

```make
.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $<
```

A rule with one suffix makes a file with no suffix: `.c:` makes `hello` from
`hello.c`. Only suffixes listed in `.SUFFIXES` count. `.SUFFIXES:` with
nothing after it empties the list, and a later `.SUFFIXES:` line adds to it.

### 9.2 Pattern rules

The GNU way, clearer and more flexible. `%` matches any non-empty part of a
name, the same part on both sides:

```make
%.o: %.c
	$(CC) $(CFLAGS) -c $<

obj/%.o: src/%.c include/config.h
	$(CC) $(CFLAGS) -c -o $@ $<
```

- Pattern rules are tried before suffix rules.
- A rule applies only if each of its prerequisites exists, is mentioned in
  the makefile, or can itself be made by a pattern rule.
- Of the rules that apply, the one where `%` matched the shortest part wins;
  among equals, the first.
- If the target pattern has no `/`, it is matched against the file name
  only, and the directory is put back: `%.o: %.c` makes `src/a.o` from
  `src/a.c`.
- A pattern rule with no commands cancels an earlier one with the same
  target and prerequisites.
- A pattern rule is never the default goal.

### 9.3 Chains and intermediate files

A pattern rule's prerequisite may itself be made by another pattern rule:

```make
%.o: %.c
	cc -c $<
%.c: %.y
	yacc -o $@ $<
```

Here `make prog.o` with only `prog.y` present makes `prog.c` and then
`prog.o`. `prog.c` is an *intermediate* file: it is removed at the end (`make`
prints an `rm` line), and next time it is not remade as long as `prog.o` is
newer than `prog.y`. List a file in `.INTERMEDIATE` to treat it this way
even though it is named in the makefile; `.SECONDARY: files` keeps them
(with no files, all of them), and so does `.PRECIOUS`. Chains go up to ten
rules, each used once.

### 9.4 Static pattern rules

A pattern rule for a given list of targets only:

```make
OBJS = main.o util.o
$(OBJS): %.o: %.c common.h
	$(CC) -c $<
```

Each target must match the target pattern; `$*` is the stem.

### 9.5 Built-in rules

Without `-r`, these are known before any makefile is read:

```make
.SUFFIXES: .o .c .s .a
CC = cc      AS = as      AR = ar      ARFLAGS = -rvU
CFLAGS =     LDFLAGS =

.c:     $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
.s:     $(CC) $(LDFLAGS) -o $@ $<
.c.o:   $(CC) $(CFLAGS) -c $<
.s.o:   $(AS) -o $@ $<
.c.a:   $(CC) -c $(CFLAGS) $<
        $(AR) $(ARFLAGS) $@ $*.o
        rm $*.o
```

So `make hello` builds `hello` from `hello.c` with no makefile at all, and
`make CFLAGS=-O hello` the same with optimisation.

### 9.6 `.DEFAULT`

The commands of a `.DEFAULT:` rule are used for any target that has no rule
of its own and no inference rule; `$<` is then the target's name.

## 10. Searching directories: `VPATH`

Sources need not be in the current directory:

```make
VPATH = src:include          # directories, separated by : or blanks
vpath %.h include            # only for names matching %.h
```

A prerequisite that is not found here is looked for in the `vpath`
directories that match it, then in `VPATH`. When found, `$<`, `$^` and the
other automatic macros give the path where it was found — so commands must
use them, not the bare name:

```make
VPATH = src
%.o: %.c
	cc -c $<            # $< is src/main.c
```

Inference rules find their sources the same way. A *target* found through
`VPATH` that is out of date is rebuilt in the current directory. `vpath
pattern` forgets that pattern's directories, `vpath` alone forgets all.

## 11. Reading other files and choosing lines

### 11.1 `include`

```make
include config.mk $(wildcard rules/*.mk)
-include $(OBJS:.o=.d)
```

The named files are read in place of the line, as if pasted there. Their
names are expanded first, but `*` in them is not: use `$(wildcard)`. A missing file is an error for `include`; `-include`
and `sinclude` skip it silently. A missing included file that some rule can
make is made first (§16).

### 11.2 Conditionals

```make
ifdef DEBUG
CFLAGS += -g
else ifeq ($(MODE),fast)
CFLAGS += -O2
else
CFLAGS += -O
endif
```

| Directive | True when |
|---|---|
| `ifeq (a,b)`, `ifeq "a" "b"`, `ifeq 'a' 'b'` | `a` and `b`, expanded, are the same |
| `ifneq` (same forms) | They differ |
| `ifdef NAME` | `NAME` has a non-empty value (the value is not expanded) |
| `ifndef NAME` | It has not |

Lines in the false branch are skipped entirely. Conditionals nest, and work
around command lines too. In `ifeq (a,b)`, blanks just after `(` and just
before `)` count, and blanks around the comma do not.

## 12. Libraries

`lib.a(member.o)` names a member of an archive. As a prerequisite, its time
is the member's time inside the archive. As a target, it is made by putting
the member in the archive, `$@` being `lib.a` and `$%` the member:

```make
lib.a: lib.a(read.o) lib.a(write.o)

prog: main.o lib.a
	cc -o prog main.o lib.a
```

The built-in `.c.a` rule compiles `read.c`, adds `read.o` to `lib.a` with
`$(AR) $(ARFLAGS)`, and removes `read.o`. In `$?` and `$^` a member appears
by its own name, so `$(AR) $(ARFLAGS) $@ $?` adds exactly the changed
objects.

On Braam, `ar` gives a member a time only with `-U` — which is why the
built-in `ARFLAGS` is `-rvU`. A member with no time looks always out of date.
A library that does not exist yet is fine: all its members are then out of
date.

## 13. Special targets

| Target | Effect |
|---|---|
| `.PHONY: names` | The names are not files: their commands always run (§4.4) |
| `.SUFFIXES: suffixes` | The suffixes suffix rules use; empty clears the list (§9.1) |
| `.DEFAULT:` | Commands for targets with no rule (§9.6) |
| `.PRECIOUS: names` | Never removed on `^C`, on error, or as intermediate |
| `.IGNORE:` | Ignore command failures — everywhere, or for the targets listed |
| `.SILENT:` | Do not print commands — everywhere, or for the targets listed |
| `.DELETE_ON_ERROR:` | Remove a target its failed command changed (§5.3) |
| `.ONESHELL:` | Each rule's lines run as one script (§5.4) |
| `.INTERMEDIATE: names`, `.SECONDARY: names` | See §9.3 |
| `.NOTPARALLEL:` | Ignore `-j`; run one command at a time |
| `.WAIT` | As a prerequisite: under `-j`, nothing after it starts until everything before it is made |
| `.POSIX:`, `.SCCS_GET:` | Accepted, no effect |

## 14. Parallel builds

`make -j 4` runs up to four commands at once, each from a target whose
prerequisites are done. A target's own commands still run one after
another. Prerequisites must be complete for this to be safe: if `prog`
needs `lib.a`, say so, or `-j` may link before the library is built.

```make
all: gen .WAIT build        # gen fully made before build starts
```

- A failure stops new commands from starting; the running ones finish, then
  `make` stops (`*** Waiting for unfinished jobs`). With `-k`, unrelated
  work goes on.
- `^C` stops every running command and removes each target being made.
- Output from parallel commands is mixed together as it comes. `-O` (or
  `-Otarget`) holds each target's output and prints it whole when the target
  is done; `-Oline` prints each command's output when it ends; `-Onone`
  turns it off. Under `-O` a command's normal output comes before its error
  output, and at most 32 commands run at once.
- Sub-makes do not share the `-j`: each runs one command at a time.
- `-n`, `-t`, `-q`, `::` rules and `.NOTPARALLEL` run one command at a time.

## 15. Header dependencies, automatically

An object must be rebuilt when a header it includes changes, but listing
headers by hand is tedious and goes stale. `cc` can write the list for you:

```make
CFLAGS += -MMD -MP
OBJS = main.o util.o

prog: $(OBJS)
	$(CC) -o prog $(OBJS)

-include $(OBJS:.o=.d)
```

`-MMD` makes `cc` write `main.d` beside `main.o`, a rule such as `main.o:
main.c util.h`. The first build has no `.d` files and builds everything
anyway; later builds read them. `-MP` adds an empty rule for each header, so
deleting a header does not break the build.

## 16. Makefiles that are made

Before making anything else, `make` brings its makefiles and the files they
include up to date, as targets. If one was remade, `make` starts over from the
beginning and reads the new version. This is the older way to produce
dependency files:

```make
%.d: %.c
	$(CC) -MM -MF $@ $<

-include $(OBJS:.o=.d)
```

The makefiles are remade even under `-n`, `-t` and `-q`. `MAKE_RESTARTS` says
how many times `make` has started over; it gives up after twenty.

## 17. Macros make sets

| Macro | Value |
|---|---|
| `MAKE` | The name `make` was run by; use it for sub-makes (§5.5) |
| `MAKEFLAGS` | The options and command-line macros, for sub-makes |
| `MAKELEVEL` | 0 at the top, one more in each sub-make |
| `MAKECMDGOALS` | The targets named on the command line |
| `MAKE_RESTARTS` | How many times `make` started over (§16) |
| `.DEFAULT_GOAL` | The default goal; set it to choose another |
| `SHELL` | `/bin/sh` |

## 18. Messages

| Message | Meaning |
|---|---|
| `` `x' is up to date. `` | Nothing needed doing for goal `x` |
| `` `x' not remade because of errors `` | With `-k`: something `x` needs failed |
| `Make:  Don't know how to make x.  Stop.` | `x` does not exist and no rule makes it |
| `*** Error code n` | A command exited with status `n` |
| `Make: Cannot load cmd.  Stop.` | The command `cmd` was not found |
| `Make: Cannot open f.  Stop.` | A makefile or included file is missing |
| `Make: Must be a separator on rules line n.  Stop.` | A line that is neither a rule nor a macro — often a command line indented with spaces instead of a tab |
| `Make: line n: syntax error.  Stop.` | A line `make` cannot read |
| `Warning: X changed after being used` | A macro got a new value after `make` had already used the old one |
| `***  x removed.` | `^C`, or a failure under `.DELETE_ON_ERROR`, removed the half-made `x` |

## 19. Notes for Braam

- File times are in milliseconds, so files made within the same second still
  compare correctly.
- Commands always run with `/bin/sh`, whatever `SHELL` says.
- A directory counts as present, and as old as can be.
- Archives need `ar -U` for member times (§12); `rm` has no `-f`, so the
  built-in `.c.a` rule uses plain `rm`.
