p = 'ex_cmds.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"\n'
              '#include "ex_screen.h"\n#include "ex_vis.h"', 1)

s = s.replace('''		if (inglobal == 0) {
			flush();''', '''		if (inglobal == 0) {
			flush();''')

# The temp file's periodic sync, and the "was that the last of the input"
# checks that went with reading it a descriptor at a time.
s = s.replace('			TSYNC();\n', '')

# commands() answers a status now: upstream fell off the end of main, and this
# has to say whether it was left by an error, a quit, or the end of input.
s = s.replace('''				if (c == EOF)
					co_return;
				continue;''', '''				if (c == EOF)
					co_return (0);
				continue;''')

# :cd. cwd_set is a syscall, so the chdir is awaited.
s = s.replace('''					if (chdir(p) < 0)
						COTHROWV(0, filioerr(p));''',
'''					{
						Result<String> r =
						    Err(Error::NoMemory);

						if (Task<Result<String>> t =
						    cwd_set(Str(p, strlen(p))))
							r = co_await t;
						if (r.is_err()) {
							errno = int(r.error());
							COTHROWV(0, filioerr(p));
						}
					}''')

# :preserve and :recover existed for the temp file, which is gone: there is
# nothing on disk to preserve and no setuid helper to hand it to.
s = s.replace('''				if (peekchar() == 'e') {
/* preserve */
					tail2of("preserve");
					eol();
					if (preserve() == 0)
						COTHROWV(0, error("Preserve failed!"));
					else
						COTHROWV(0, error("File preserved."));
				}''', '''				if (peekchar() == 'e') {
/* preserve */
					/*
					 * Preserve wrote the temp file somewhere
					 * a setuid helper could find it after a
					 * crash. There is no temp file: the
					 * buffer is in memory, so :w is the
					 * whole of what preserving means.
					 */
					tail2of("preserve");
					eol();
					COTHROWV(0, error("Preserve is not supported - use :w"));
				}''')

# Leaving. Upstream put the terminal back and called exit(); the screen is
# given back by ~Proc whatever happens, and exit() records a status here.
s = s.replace('''			if (inopen) {
				vgoto(WECHO, 0);
				if (!ateopr())
					vnfl();
				else {
					tostop();
				}
				flush();
				setty(normf);
			}
			cleanup(1);
			exit(0);''', '''			if (inopen) {
				vgoto(WECHO, 0);
				if (!ateopr())
					vnfl();
				flush();
			}
			cleanup(1);
			ex_exit(0);
			co_return (0);''')

s = s.replace('''			if (inopen) {
				vgoto(WECHO, 0);
				if (!ateopr())
					vnfl();
				else
					tostop();
				flush();
				setty(normf);
			}
			cleanup(1);
			exit(0);''', '''			if (inopen) {
				vgoto(WECHO, 0);
				if (!ateopr())
					vnfl();
				flush();
			}
			cleanup(1);
			ex_exit(0);
			co_return (0);''')

# :open. Open mode edited one line at a time on terminals that could not
# address a cursor. A cell grid always can, so this is visual's business.
s = s.replace('''			oop();''', '''			COTHROWV(0, error("Open mode is not supported - use visual"));''')
open(p, 'w').write(s)
print('ok')

p = 'ex_cmds.cpp'
s = open(p).read()

# tail() takes a char *; a conditional between two literals is const char *.
s = s.replace('tail(peekchar() == \'x\' ? "ex" : "edit");',
              'tail((char *)(peekchar() == \'x\' ? "ex" : "edit"));')
s = s.replace('tail(c == \'q\' ? "wq" : "write");',
              'tail((char *)(c == \'q\' ? "wq" : "write"));')

s = s.replace('''			if (!c)
quit:
				nomore();''', '''			if (!c)
				nomore();
quit:''')

# :recover. The temp file it recovered from does not exist, and the setuid
# helper that reconstructed a buffer from one went with it.
old = s[s.index('					init();\n					addr2 = zero;\n					laste++;'):
        s.index('					nochng();\n					continue;\n				}')]
s = s.replace(old, '''					COTHROWV(0, error("Recover is not supported"));
''')

# :sh. There is no terminal-end sequence and no descriptor to hand it.
s = s.replace('''				vnfl();
				putpad(TE);
				flush();
				co_await unixwt(1, co_await unixex("-i", (char *) 0, 0, 0));''',
'''				vnfl();
				flush();
				co_await vspawn_begin();
				co_await unixex((char *) "-i", (char *) "", 0, 0);
				co_await vspawn_end();
				co_await unixwt(1, 0);''')

# The end of input. Upstream asked whether descriptor 0 was still a terminal,
# because a chtty could turn one into /dev/null underneath it, and treated
# that as a hangup. There is no chtty and no SIGHUP.
s = s.replace('''			if (exitoneof) {
				if (addr2 != 0)
					dot = addr2;
				co_return;
			}
			if (!isatty(0)) {
				if (intty)
					/*
					 * Chtty sys call at UCB may cause a
					 * input which was a tty to suddenly be
					 * turned into /dev/null.
					 */
					onhup();
				co_return;
			}''', '''			if (exitoneof) {
				if (addr2 != 0)
					dot = addr2;
				co_return (0);
			}
			if (!intty)
				co_return (0);''')
open(p, 'w').write(s)

p = 'ex_cmds.cpp'
s = open(p).read()

# ateopr() answers whether the cursor is already at the end of the last line;
# upstream's was an implicit int.
s = s.replace('''				if (!ateopr())
					vnfl();
				flush();''', '''				if (!ateopr())
					vnfl();
				flush();''')

# The ! command. Same shape as :sh: the claims go back, the shell runs, they
# come back.
s = s.replace('''				co_await unix0(1);
				pofix();
				putpad(TE);
				flush();
				co_await unixwt(1, co_await unixex("-c", uxb, 0, 0));''',
'''				co_await unix0(1);
				COCHKV(0);
				pofix();
				flush();
				co_await vspawn_begin();
				co_await unixex((char *) "-c", uxb, 0, 0);
				co_await vspawn_end();
				co_await unixwt(1, 0);''')
open(p, 'w').write(s)

p = 'ex_cmds.cpp'
s = open(p).read()

# The top of the command loop is where the port's control flow lives: it is
# the landing an error unwinds to, and the one place in command mode that
# reads. Everything below getach() runs off the buffer this fills, which is
# what keeps the address parser, the regular expressions and the substitute
# ordinary synchronous code.
s = s.replace('''	resetflav();
	nochng();
	for (;;) {
		/*
		 * If dot at last command''', '''	resetflav();
	nochng();
	for (;;) {
		/*
		 * The landing. Upstream arrived here by longjmp from error();
		 * nothing unwinds by itself now, so the flag says whether the
		 * last command ended badly, and ex_reset() does what error1()
		 * did on the way: throw away the rest of that command's line.
		 */
		if (ex_thrown) {
			if (ex_quitting)
				co_return (ex_status);
			ex_reset();
		}
		if (ex_pendclose > 0) {
			co_await ex_close(ex_pendclose);
			ex_pendclose = -1;
		}

		/*
		 * If dot at last command''')

# Read where upstream prompted. The prompt has to reach the terminal before
# the read parks, which is the reason exflush() is here and not anywhere else.
s = s.replace('''			if (!hush && value(PROMPT) && !globp && !noprompt && endline) {
				putchar(':');
				hadpr = 1;
			}
		}''', '''			if (!hush && value(PROMPT) && !globp && !noprompt && endline) {
				putchar(':');
				hadpr = 1;
			}
		}

		/*
		 * A whole line of input, before any of it is parsed. getach()
		 * answers EOF rather than waiting, so the line has to be here
		 * in full: a command stops at its newline and never asks for
		 * the one after it.
		 */
		if (need_input()) {
			co_await exflush();
			if ((co_await ex_readline()).is_err())
				co_return (0);
		}''')

# Listing a large file must not have to hold all of it at once.
s = s.replace('''			co_await plines(addr1, addr2, 1);''',
              '''			co_await plines(addr1, addr2, 1);
			if (out_pending())
				co_await exflush();''')
open(p, 'w').write(s)

p = 'ex_cmds.cpp'
s = open(p).read()

# The one CHK the loop cannot do without. address() answers 0 when it throws,
# and upstream never came back from there at all; here it returns, and the
# address list would go on gathering and then run a command on the addresses a
# failed parse left behind.
s = s.replace('''		if (addr1 == 0)
			addr1 = addr2;
		if (c == ':')
			c = getchar();''', '''		if (ex_thrown)
			continue;
		if (addr1 == 0)
			addr1 = addr2;
		if (c == ':')
			c = getchar();''')

# And one after each command has run, so that the trailing print flags and the
# autoprint at the top of the loop do not act on a command that failed.
s = s.replace('''		switch (c) {

		case 'a':''', '''		if (ex_thrown)
			continue;

		switch (c) {

		case 'a':''')
open(p, 'w').write(s)

import re
p = 'ex_cmds.cpp'
s = open(p).read()

# Every throw in this file is inside commands()' own for(;;), and upstream's
# landed at the top of it. co_return would leave the editor rather than the
# command, which is a memorable way to lose a session to a typo.
s = re.sub(r'COTHROWV\(0, (.*?)\);\n', r'THROWC(\1);\n', s, flags=re.S)
s = re.sub(r'COTHROW\((.*?)\);\n', r'THROWC(\1);\n', s, flags=re.S)
open(p, 'w').write(s)

p = 'ex_cmds.cpp'
s = open(p).read()

# The CHK pass. Upstream's longjmp meant that when one of these refused the
# command line -- a bad count, a zero address, a trailing character -- nothing
# after it in the case body ran. They return now, so each is followed by the
# check that says so. THROWC's `continue` is the same landing the longjmp had.
PARSE = ('setcount', 'newline', 'eol', 'nonzero', 'setdot', 'setdot1',
         'setall', 'setNAEOL', 'setCNL', 'setnoaddr', 'getone', 'notempty',
         'donewline', 'setcaddr')
out = []
for line in s.split('\n'):
    out.append(line)
    t = line.strip()
    for f in PARSE:
        if t == f + '();':
            indent = line[:len(line) - len(line.lstrip())]
            out.append(indent + 'if (ex_thrown)')
            out.append(indent + '\tcontinue;')
            break
s = '\n'.join(out)
open(p, 'w').write(s)
