# Keymap — what a vi user expects, and what is here

[README.md](README.md) says what the *port* changed against *upstream*. This
file asks the other question: a user sits down at this editor with the reflexes
of a 2026 vi or vim, and which of them work? The answer divides in three — what
is here, what could be put here, and what will not be.

The short of it: **the classic vi keymap is complete.** Every keystroke in ex/vi
3.6's visual mode is reachable but one — `=`, the LISP reformat operator — and
on the ex side only `:open`, `:preserve`, `:recover` and `:stop` are gone, each
with the thing it existed for. What is missing is everything vi grew *after*
November 1980, and two commands broken by a mechanical rename.

---

## 1. What is here

### Motions

Every one takes a count, and every one is an operand for an operator.

| Keys | |
| --- | --- |
| `h j k l`, `^H`, space | char left/down/up/right |
| ←, ↓, ↑, → | the same, via `^H`, `^N`, `^P`, space, so `:map` reaches them |
| `0` `^` `$` | line start, first non-blank, end |
| Home, End | `^` and `$` |
| `\|` | to column *n* |
| `w W b B e E` | word forward, back, end; capital is blank-delimited |
| `f F t T` *c*, `;` `,` | find char in line, repeat, reverse |
| `G`, *n*`G` | last line, line *n* |
| `H M L` | screen top, middle, low |
| `+` `-` `<CR>` | first non-blank of next, previous line |
| `_` | the line itself — what makes `dd`, `cc`, `yy` work |
| `(` `)` | sentence back, forward |
| `{` `}` | paragraph, by the `paragraphs` option |
| `[[` `]]` | section, by `sections` and a `{` in column 1 |
| `%` | matching `()`, `[]`, `{}` |
| `` ` ``*x*, `'`*x* | to mark, exact and linewise |
| ``` `` ```, `''` | back to where you were |
| `/` `?` `n` `N` | search — see below |

`operate()` in [ex_voper.cpp](ex_voper.cpp) is the whole table, and it is
upstream's.

### Operators

`d` `c` `y` `<` `>` `!`, each followed by a motion or doubled for the line.
A count on either side: `2d3w` deletes six words.

Shorthands, all present: `dd cc yy << >> !!  D C Y S s x X r R J ~`

`Y` is `yy`, as in vi — not vim's `y$`.

### Entering insert, and insert mode

`i I a A o O`, `R` to overwrite, `C` `S` `s` `cc` through the operator.

| Key | |
| --- | --- |
| `^H`, Backspace | erase a character |
| `^W` | erase a word |
| `^U` | erase the insertion |
| `^V`, `^Q` | quote the next key |
| `^T`, `^D` | shift the line in, out; `^^D` and `0^D` as upstream |
| `<CR>` | new line, with `autoindent` |
| Esc, `^C` | end the insertion |
| ←, →, ↑, ↓, Home, End, Delete | move and keep inserting — a port addition |
| PgUp, PgDn | end the insertion and page |

`wrapmargin` breaks lines, `:ab` expands abbreviations, and `showmatch` checks
the bracket you just closed.

### Undo, repeat, registers, marks

| | |
| --- | --- |
| `u` | undo — one level, and its own inverse, so a second `u` redoes |
| `U` | restore the current line |
| `.` | repeat the last change, with a new count if you give one |
| `m`*x* | set mark *x* |
| `"`*a*–*z* | named register |
| `"`*A*–*Z* | append to it |
| `"1`–`"9` | the delete ring, shifted on each linewise delete |
| `p` `P` | put after, before; linewise or partial as the register holds |
| `y` `Y` `yy` | yank |
| `@`*x* | execute a register as commands |

There is no `"0`: vi has no separate yank register, and `"0` is refused at the
prompt.

### Search

`/` and `?` with `<CR>`, then `n` and `N`. The full ex address tail works from
visual too: `/pat/+2`, `/pat/-`, `/pat/;/pat2/`, `/pat/z`.

Options that govern it: `ignorecase`, `wrapscan`, `magic`.

### Screen

`^F ^B ^D ^U ^E ^Y` — page and scroll; a count sets `scroll` for `^D`/`^U` and
sticks. `z<CR>` `z.` `z-` `z+` `z^`, with a count to resize the window.
`^L` clears and repaints, `^R` repaints. `^G` is `:file`. `^]` takes the word
under the cursor as a tag; `^^` returns to the alternate file.

### Ex commands

The whole 3.6 dispatch, in [ex_cmds.cpp](ex_cmds.cpp):

```
ab args a co cd chd c d e ex f g i j k l map ma m nu n pu p q rew r
se sh so s ta t unm una u ve vi v w wq x y z  *  @  |  "  #  =  !  <  >
```

with `!` on `a c e g j map n q rew ta u v w` where upstream had it, `w>>`,
`w file`, `w !cmd`, `r !cmd`, *addr*`!cmd`, `:s` with `g` `c` `r` and a count,
`:g` and `:v`, `:&` and `:~`.

Addresses: `.` `$` *n* `+`*n* `-`*n* `^`*n* `'`*x* `''` `/re/` `?re?` `\/` `\?`,
joined by `,` or `;`, and `%`.

`:!cmd`, `:sh`, `:r !cmd`, `:w !cmd` and the `!` filter all run — through
`spawn()`, against Braam's `/bin`. `:cd` and `:chdir` change the working
directory for real.

**`:delete` spelled out does not work.** `:d` does; `:de`, `:del`, `:dele`,
`:delete`, `:dp` and `:dl` all fail. See part 2 — it is a one-line bug, not a
design decision.

### Options

All forty of upstream's, in [ex_data.cpp](ex_data.cpp), settable by `:set`,
`:set no`…, `:set` …`?`, `:set` …`=`…, `:set all`, and readable from `EXINIT`
or `~/.exrc`:

```
autoindent autoprint autowrite beautify directory edcompatible errorbells
hardtabs ignorecase lisp list magic mesg number open optimize paragraphs
prompt readonly redraw remap report scroll sections shell shiftwidth
showmatch slowopen tabstop taglength tags term terse timeout ttytype warn
window wrapscan wrapmargin writeany
```

Two defaults moved: `redraw` is on, and `window` and `scroll` are read off the
grid at startup and at every resize unless you have set them yourself.

Six are settable and read by nothing: `directory`, `hardtabs`, `lisp`, `mesg`,
`optimize`, `timeout` — the machinery each governed is gone.

`:set term=` is an error; there is no terminal type.

### Regular expressions

Upstream's BREs, unchanged: `^` `$` `.` `*`, `[...]` with ranges and `[^...]`,
`\(`…`\)` up to nine, `\<` `\>`, `~` for the last replacement, `\/` `\?` `\&`
to reuse the last pattern, and `nomagic`.

On the replacement side: `&`, `~`, `\1`–`\9`, `\u` `\l` `\U` `\L` `\E` `\e`.

### Command line

```
ex [-] [-R] [-v] [-t tag] [-w size] [+cmd] file ...
vi [-] [-R] [-t tag] [-w size] [+cmd] file ...
```

`+`*n* and `+/pat` both work. Several files with `:n`, `:rew`, `:args`.
`EXINIT` and `~/.exrc` are read; `:source` works.

`-r` and `-x` are refused: the first went with the temp file, the second with
the crypt mode. `-l` was never in 3.6.

### What the port added

- The cursor keys work inside an insertion, which upstream's did not.
- `-- INSERT --` on the echo line, which is vim's habit and not vi's.
- Colour: cyan echo line, blue `~`.
- Esc is a key with a code of its own, distinct from the byte `033`.
- `--help` on the command line.

---

## 2. What could be added

Ordered by what it costs. The first five are repairs to code that is already
written.

### Repairs to code that is already written

**`:delete`, and with it `:dp` and `:dl`.** [ex_cmds.cpp:295](ex_cmds.cpp)
passes `tail("exdelete")`. `delete()` was renamed because it is a C++ keyword,
and the rename reached the *string* — which is the command name, not the
function. `tailprim()` seeds `tcommand[0]` from it, so it becomes `'e'`:
`:delete` reports `eelete: Not an editor command`, `:del` reports `eel:`, and
the `tcommand[0] == 'd'` test at
[ex_cmds2.cpp:450](ex_cmds2.cpp) that keeps `dp` and `dl` alive never fires.
The error text names it too, so `:1d ,` says `Extra characters at end of
"exdelete" command`. Pass `"delete"`. **One line**, and a test that types more
than `:d`.

**`6 lines exdeleted`.** The same rename, in `notenam` at
[ex_vops.cpp:348](ex_vops.cpp) and `:459`. **Two lines.**

**The bell.** `obeep()` ([ex_vget.cpp:324](ex_vget.cpp)) ends at
`vputc(CTRL('g'))`, and `vputc` is `((void)0)`. Every refused command aborts
correctly and says nothing at all. A visible bell — invert the echo row for one
frame — is the cheapest, and `vflush()` already decides that row's colour.

**`^@` in insert.** "Insert what you inserted last" is written
([ex_vops2.cpp:173](ex_vops2.cpp)) and unreachable: `key_byte()` never yields 0,
and `getbr()` drops a 0 if it did. Give it a named code the way Esc got `KESC`,
rather than teaching the decoder to pass 0 — 0 means "no key" in three places.

**The six inert options.** Wire or drop. Dropping is three lines each and makes
`:set all` honest.

**`.exrc` in the working directory.** Only `$HOME/.exrc` is read
([ex.cpp:245](ex.cpp)); upstream 3.6 read no more than that either. One more
`source()` call, and a decision about whether that is wise.

### Small new work

**`zz`, `zt`, `zb`** — aliases of `z.`, `z<CR>`, `z-`. Three cases in `vzop()`.

**`gg`** — `g` is unbound and beeps. A prefix in `operate()`'s target switch,
reusing the `G` case with a default of 1. ~15 lines.

**`*` and `#`** — `grabtag()` ([ex_vmain.cpp:1073](ex_vmain.cpp)) already lifts
the identifier under the cursor for `^]`. Wrap it in `\<`…`\>` and hand it to
the search `/` and `?` use. ~30 lines.

**`"0`, the yank register** — refused at the prompt today
([ex_vmain.cpp:117](ex_vmain.cpp)), because `0` is how `kshift()` spells "no
register". Give yank a slot of its own and let `"0` name it. ~20 lines in
[ex_buf.cpp](ex_buf.cpp). This one is a deviation from vi, not a restoration:
vi has no yank register, and `p` after `y` reads the unnamed one.

**`expandtab`** — one option, and the tab in `vgetline()`'s insert path.
Nothing else reads a tab on the way in.

**`showmode`** — `-- INSERT --` is drawn unconditionally by `vflush()`. Making
it an option is one `value()` test.

**`^T`, the tag pop** — not in 3.6; 4.2BSD added it. `tagfind()` already saves
where you were in mark `'t'` ([ex_cmdsub.cpp:508](ex_cmdsub.cpp)); what is
missing is a stack and one case in `vmain()`. ~40 lines.

---

## 3. What cannot

### The platform forbids it

**`^Z` and `:stop`** — `SIG_TSTP` is not in Braam's catchable set, and there is
no job control to stop into.

**`:preserve`, `:recover`, `-r`** — they existed to salvage the temp file, and
there is no temp file: the buffer is memory. A crash takes the session with it.

**The `showmatch` pause** — upstream slept a second on the matching bracket. A
sleep is a syscall, the pause happens inside insert mode, and insert mode cannot
await one. The match is still checked, and an unmatched one is still refused.

**Anything above ASCII, so UTF-8** — `TRIM` is `0177` and a line is a `char`
array, throughout: the buffer, `vtube`, the regular expressions, the column
arithmetic. That is a different editor's data model, not a patch.

**`:e *.c`** — the glob is the shell's and it ran before ex started. Upstream
forked a shell of its own to expand argument words; there is no `fork`. `%`,
`#` and `$` still expand.

**`:open`** — open mode edited one line where it stood, for terminals that
could not address a cursor. A cell grid always can.

### The architecture forbids it cheaply

**`^R`, and multi-level undo.** `undo()` ([ex_cmdsub.cpp:788](ex_cmdsub.cpp))
keeps one saved region and one `undkind`, and is its own inverse — which is
exactly why a second `u` redoes and why there is nothing for `^R` to do. A real
undo stack is not a bigger version of this; it is a different mechanism, and it
lands on an arena that already grows by everything ever typed and never reuses a
byte. Note also that `^R` is taken: it repaints the screen.

**The file size.** About 2 MB, and it is the memory cap, not a constant that
can be raised.

### It is a different editor

Each of these arrived in vim, fifteen to thirty years after this source was
frozen. None is blocked by the platform; all of them are real work, and all of
them are a decision about whether this stays a port of ex/vi 3.6.

**Visual mode — `v`, `V`, `^V`.** The closest of them to reachable. `xdw()`
already works out a from-to region from `wdot`/`wcursor`, and `vatube0` is a
standout plane the highlight could paint into. What is missing is a selection
that survives between keystrokes, and an operator loop that reads one instead
of asking for a motion. Large, but nothing is in the way.

**Text objects — `ciw`, `di"`, `ca(`.** A second operand grammar beside the
motion table. `lskipbal()` and `lmatchp()` in [ex_vops3.cpp](ex_vops3.cpp)
already do the bracket half.

**`gq`, and `=`.** `=` was *deleted*, not never written: it was the LISP
reformat operator, and `LISPCODE` was resolved out at conversion — its comment
still stands at ex_voper.cpp:85 with no `case` under it. Bringing it back means
bringing back `lindent()`, which went with it. `gq` is new work either way.

**`^N` and `^P` completion.** Needs a word index over the buffer, and both keys
are taken: they are the arrow keys.

**`^O` and `^I`, the jump list.** Only the single previous-context mark exists.

**`^A` and `^X`.** Small in isolation, but there is no prefix convention here
to hang them on.

**`:sp`, `:vs`, windows.** `vtube` is one window, and every routine that
touches it assumes so.

**`hlsearch`, `incsearch`, syntax.** The attribute plane is there; the work is
running a match per keystroke and per repaint, and for syntax a lexer per
language.

**The mouse.** The Grid reports no pointer to a program.

**`:reg`, persistent undo, `:earlier`.** Each needs state vi does not keep.
