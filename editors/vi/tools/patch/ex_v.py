p = 'ex_v.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

# oop() -- open mode. It existed for terminals that could not address a cursor,
# and edited one line where it stood; a cell grid always can address one. The
# whole of it goes, and with it the four refusals in vop() that fell back to it.
i = s.index('/*\n * Enter open mode\n */')
s = s[:i] + s[s.index('void ovbeg(void)'):]

# vop(). Upstream refused four times over -- no addressable cursor, no clear
# screen, overstriking, no scrolling -- and fell back to open mode for each.
# The grid has all four. What replaces them is the pair of claims.
old = s[s.index('Task<void> vop(void)'):s.index('/*\n * Hack to allow entry to visual')]
new = '''Task<void> vop(void)
{
	int c;

	ovbeg();
	COCHK;
	bastate = VISUAL;
	c = 0;
	if (any(peekchar(), "+-^."))
		c = getchar();
	pastwh();
	vsetsiz(isdigit(peekchar()) ? getnum() : value(WINDOW));
	newline();
	COCHK;

	/* Taken here, given back in ovend(): command mode wants neither. */
	if ((co_await vscreen_take()).is_err())
		COTHROW(error("Visual needs a terminal"));

	setwind();
	vok(atube);
	if (!inglobal)
		savevis();
	Outchar = vputchar;
	vmoving = 0;
	if (initev == 0) {
		vcontext(dot, c);
		vnline(NOSTR);
	}
	co_await vmain();
	Command = (char *) "visual";
	co_await ovend();
}

'''
s = s.replace(old, new)

# ovend(). The last frame goes out first: dropping the claim restores the
# shell's screen, so anything unsent by then is never sent.
s = s.replace('''	holdcm = 0;
	splitw = 0;
	ostop(f);
	setoutt();''', '''	holdcm = 0;
	splitw = 0;
	co_await vflush();
	co_await vscreen_give();
	setoutt();''')

# Signals. Upstream re-armed SIGINT on every delivery and swapped handlers
# entering and leaving visual. sig_catch is standing and there is no handler:
# ^C reaches getbr(), which answers ATTN -- vintr()\'s whole job.
s = s.replace('\tif (ruptible)\n\t\tsignal(SIGINT, onintr);\n', '')
s = s.replace('\tinopen = 1;\n\tsignal(SIGINT, vintr);\n', '\tinopen = 1;\n')
i = s.index('vintr()')
j = s.index('/*\n * Set the size of the screen', i)
s = s[:i] + s[j:]
open(p, 'w').write(s)
