p = 'ex_cmdsub.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

s = s.replace('''exbool	endline = 1;
line	*tad1;
static	jnoop();''', '''line	*tad1;
static Task<int> jnoop(void);''')

# append() reads through a function pointer, and every one of them is a Task
# now: getsub and getREG reach a register, getfile a descriptor, gettty the
# terminal.
s = s.replace('	while ((*f)() == 0) {', '	while (co_await (*f)() == 0) {')

# The temp file's dirty-block sync went with the file.
s = s.replace('''		if (f == gettty) {
			dirtcnt++;
			TSYNC();
		}
	}''', '''	}''')

# The two SIGINT blockades. Upstream shut interrupts off across the pointer
# shuffle, because a handler that longjmp'd out of the middle would leave the
# buffer half moved. A signal is delivered where a process parks, and nothing
# in the shuffle parks, so it is already atomic.
s = s.replace('''		change();
		dsavint = signal(SIGINT, SIG_IGN);
		undkind = UNDCHANGE;''', '''		change();
		undkind = UNDCHANGE;''')
s = s.replace('''		pkill[0] = pkill[1] = 0;
		signal(SIGINT, dsavint);''', '''		pkill[0] = pkill[1] = 0;''')
s = s.replace('\tint (*dsavint)();\n', '')

s = s.replace('static\tint jcount, jnoop();', 'static\tint jcount;')
s = s.replace('	ignore(append(jnoop, --addr1));',
              '	ignore(co_await append(jnoop, --addr1));')
s = s.replace('''static
jnoop()
{

	return(--jcount);
}''', '''static Task<int> jnoop(void)
{

	co_return (--jcount);
}''')

s = s.replace('int\tgetcopy();\n', '')

# fkey() answered the termcap string a function key sends. Braam's named keys
# do not send a string -- they arrive as KEY_F1..KEY_F12 -- so there is nothing
# for #1..#9 to match against and the map is stored under its own name.
s = s.replace('\t\tchar *fkey();\n', '')
s = s.replace('\t\tfnkey = fkey(lhs[1] - \'0\');', '\t\tfnkey = 0;')
s = s.replace('int\tgetput();\n', '')

s = s.replace('''static
Task<void> splitit(void)''', '''Task<void> splitit(void)''')

# somechange() answers whether undo would do anything; every arm of its switch
# returned an implicit zero.
s = s.replace('''	switch (undkind) {

	case UNDMOVE:
		return;

	case UNDCHANGE:
		if (undap1 == undap2 && dol == unddol)
			break;
		return;

	case UNDPUT:
		if (undap1 != undap2)
			return;
		break;

	case UNDALL:
		if (unddol - dol != lineDOL())
			return;
		for (ip = one, jp = dol + 1; ip <= dol; ip++, jp++)
			if ((*ip &~ 01) != (*jp &~ 01))
				return;''', '''	switch (undkind) {

	case UNDMOVE:
		return (0);

	case UNDCHANGE:
		if (undap1 == undap2 && dol == unddol)
			break;
		return (0);

	case UNDPUT:
		if (undap1 != undap2)
			return (0);
		break;

	case UNDALL:
		if (unddol - dol != lineDOL())
			return (0);
		for (ip = one, jp = dol + 1; ip <= dol; ip++, jp++)
			if ((*ip &~ 01) != (*jp &~ 01))
				return (0);''')

# The tag file: open, scan, close. Upstream's other arm used stdio to binary
# search a sorted tags file; there is no stdio, and a linear scan of a file
# that is already in memory costs nothing worth the code.
s = s.replace('		io = open(fn, 0);', '		io = co_await ex_open(fn, 0);')
s = s.replace('''			close(io);
			/* Rest of tag if abbreviated */''', '''			co_await ex_close(io);
			io = -1;
			/* Rest of tag if abbreviated */''')
s = s.replace('''		close(io);
	}	/* end of "for each file in path" */''', '''		co_await ex_close(io);
		io = -1;
	}	/* end of "for each file in path" */''')

# `goto badtag` jumped into the arm of an if, past the co_await the THROW
# expands to, which C++ will not allow in a coroutine.
s = s.replace('''		if (!endcmd(peekchar()))
badtag:
			COTHROW(error("Bad tag|Give one tag per line"));
	} else if (lasttag[0] == 0)
		COTHROW(error("No previous tag"));
	c = getchar();
	if (!endcmd(c))
		goto badtag;''', '''		if (!endcmd(peekchar()))
			COTHROW(error("Bad tag|Give one tag per line"));
	} else if (lasttag[0] == 0)
		COTHROW(error("No previous tag"));
	c = getchar();
	if (!endcmd(c))
		COTHROW(error("Bad tag|Give one tag per line"));''')
open(p, 'w').write(s)
print('ok')

p = 'ex_cmdsub.cpp'
s = open(p).read()
# Falling off the end of a coroutine is well defined -- it calls return_void --
# but clang warns as if it were an ordinary function, and the warning is worth
# not having to read past.
s = s.replace('''	for (l = COLUMNS > 80 ? 40 : COLUMNS / 2; l > 0; l--)
		putchar('-');
	putnl();
}''', '''	for (l = COLUMNS > 80 ? 40 : COLUMNS / 2; l > 0; l--)
		putchar('-');
	putnl();
	co_return;
}''')
s = s.replace('''		if (movedot)
			dot = addr;
	}
}''', '''		if (movedot)
			dot = addr;
	}
	co_return;
}''')
open(p, 'w').write(s)
