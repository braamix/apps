p = 'ex_vmain.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

s = s.replace('\t\tTSYNC();\n', '')

# KR was the string the right-arrow key sent, and this asked whether that
# string happened to be ^L, so that ^L could not both redraw and move. A named
# key arrives as itself and sends no string at all.
s = s.replace("if (c == CTRL('l') || (KR && *KR==CTRL('l'))) {",
              "if (c == CTRL('l')) {")

# The four CATCH blocks in vmain. Each guarded body stays where it is: vcatch
# says an error here comes back to me, and excatch() is the landing that
# upstream reached by longjmp.
s = s.replace("\t\t\tCATCH\n"
              "\t\t\t\tchar tmpbuf[BUFSIZ];\n"
              "\n"
              "\t\t\t\tco_await regbuf(c,tmpbuf,sizeof(vmacbuf));\n"
              "\t\t\t\tmacpush(tmpbuf, 1);\n"
              "\t\t\tONERR\n"
              "\t\t\t\tlastmac = 0;\n"
              "\t\t\t\tsplitw = 0;\n"
              "\t\t\t\tgetDOT();\n"
              "\t\t\t\tvrepaint(cursor);\n"
              "\t\t\t\tcontinue;\n"
              "\t\t\tENDCATCH",
              "\t\t\tvcatch = 1;\n"
              "\t\t\tco_await regbuf(c, vmactmp, sizeof(vmacbuf));\n"
              "\t\t\tif (!ex_thrown)\n"
              "\t\t\t\tmacpush(vmactmp, 1);\n"
              "\t\t\tvcatch = 0;\n"
              "\t\t\tif (excatch()) {\n"
              "\t\t\t\tlastmac = 0;\n"
              "\t\t\t\tsplitw = 0;\n"
              "\t\t\t\tgetDOT();\n"
              "\t\t\t\tvrepaint(cursor);\n"
              "\t\t\t\tcontinue;\n"
              "\t\t\t}")

s = s.replace('''			CATCH
				co_await vremote(1, vreg ? putreg : put, vreg);
			ONERR
				if (vreg == -1) {
					splitw = 0;
					if (op == 'P')
						dot++, vcline++;
					goto pfixup;
				}
			ENDCATCH''', '''			vcatch = 1;
			co_await vremote(1, vreg ? op_putreg : op_put, vreg);
			vcatch = 0;
			if (excatch()) {
				if (vreg == -1) {
					splitw = 0;
					if (op == 'P')
						dot++, vcline++;
					goto pfixup;
				}
			}''')

s = s.replace('vremote(cnt, join, 0)', 'vremote(cnt, op_join, 0)')
s = s.replace('vremote(1, yank, 0)', 'vremote(1, op_yank, 0)')

# Upstream's `char tmpbuf[BUFSIZ]` lived in the CATCH block above; vmain is a
# coroutine and a frame past 512 bytes costs a whole 64 KiB span.
s = s.replace('char\tworkcmd[5];\n', 'char\tworkcmd[5];\n')
s = s.replace('\nTask<void> vmain(void)',
              '\n/* See the note in ex_buf.cpp: a kilobyte does not go in a frame. */\n'
              'static char vmactmp[BUFSIZ];\n\nTask<void> vmain(void)')

# vremote applies one of eight functions to a range, and three of them await.
s = s.replace('''Task<void> vremote(int cnt, Vopf f, int arg)''',
              '''Task<void> vremote(int cnt, Vopf f, int arg)''')
s = s.replace('\t(*f)(arg);', '\tco_await (*f)(arg);')

# The wrappers. Upstream stored any of these in an `int (*)()`; five of them
# do not await, and are here rather than being made to for the table's sake.
s = s.replace('''/*
 * Save the current contents of linebuf, if it has changed.
 */''', '''Task<void> op_move(int c)
{

	(void) c;
	vmove();
	co_return;
}

Task<void> op_beep(int c)
{

	(void) c;
	obeep();
	co_return;
}

Task<void> op_yank(int c)
{

	(void) c;
	yank();
	co_return;
}

Task<void> op_delete(int c)
{

	exdelete(c);
	co_return;
}

Task<void> op_shift(int c)
{

	(void) c;
	vshift();
	co_return;
}

Task<void> op_yankreg(int c)
{

	YANKreg(c);
	co_return;
}

Task<void> op_put(int c)
{

	(void) c;
	co_await put();
}

Task<void> op_putreg(int c)
{

	co_await putreg(c);
}

Task<void> op_join(int c)
{

	co_await join(c);
}

Task<void> op_filter(int c)
{

	co_await filter(c);
}

/*
 * Save the current contents of linebuf, if it has changed.
 */''')
open(p, 'w').write(s)

p = 'ex_vmain.cpp'
s = open(p).read()

# The degenerate kind: print the message and carry on, which is what a CATCH
# with no ONERR meant.
s = s.replace("\t\t\t\tCATCH\n"
              "\t\t\t\t\tCOTHROW(error(\"Q gets ex command mode, :q leaves vi\"));\n"
              "\t\t\t\tENDCATCH",
              "\t\t\t\terror((char *) \"Q gets ex command mode, :q leaves vi\");\n"
              "\t\t\t\t(void) excatch();")

# The : escape into command mode. This is the one that matters most: an error
# in the command must come back here, to visual, rather than throwing the
# session out to the command loop -- which is exactly what upstream's vcatch
# was for.
s = s.replace("\t\t\tCATCH\n", "\t\t\tvcatch = 1;\n\t\t\t{\n", 1)
s = s.replace("\t\t\t\tco_await commands(1, 1);\n"
              "\t\t\t\tif (dot == zero && dol > zero)\n"
              "\t\t\t\t\tdot = one;\n"
              "\t\t\tONERR\n"
              "\t\t\t\tcopy(esave, vtube[WECHO], TUBECOLS);\n"
              "\t\t\tENDCATCH",
              "\t\t\t\tco_await commands(1, 1);\n"
              "\t\t\t\tif (dot == zero && dol > zero)\n"
              "\t\t\t\t\tdot = one;\n"
              "\t\t\t}\n"
              "\t\t\tvcatch = 0;\n"
              "\t\t\tif (excatch())\n"
              "\t\t\t\tcopy(esave, vtube[WECHO], TUBECOLS);")

s = s.replace('\t\t\tint (*OPline)(), (*OPutchar)();', '\t\t\tPlineFn OPline;\n\t\t\tOutcharFn OPutchar;')
open(p, 'w').write(s)

p = 'ex_vmain.cpp'
s = open(p).read()
s = s.replace('\tint onumber, olist, (*OPline)(), (*OPutchar)();',
              '\tint onumber, olist;\n\tPlineFn OPline;\n\tOutcharFn OPutchar;')

# The Z arm for open mode, which cannot be reached: state is always VISUAL.
# ostop() put the terminal back; the screen is claimed once, in vop().
s = s.replace('\t\tostop(normf);\n\t\tsetoutt();', '\t\tsetoutt();')
open(p, 'w').write(s)

p = 'ex_vmain.cpp'
s = open(p).read()

# vzop's own forbid, which returns from a coroutine.
s = s.replace('#define\tforbid(a)\tif (a) { obeep(); return; }',
              '#define\tforbid(a)\tif (a) { obeep(); co_return; }')

# The Z arm for open mode again: termreset re-synced a cursor nobody can see,
# and ostart put the terminal into raw mode, which vop() did once.
s = s.replace('\t\tputNFL();\n\t\ttermreset();\n\t\tOutchar = vputchar;\n\t\tignore(ostart());',
              '\t\tputNFL();\n\t\tOutchar = vputchar;')
open(p, 'w').write(s)

p = 'ex_vmain.cpp'
s = open(p).read()
i = s.index('Task<void> grabtag(void)')
j = s.index('\n}\n', i)
s = s[:j] + '\n\tco_return;' + s[j:]
open(p, 'w').write(s)

p = 'ex_vmain.cpp'
s = open(p).read()
# :q and :wq from the : escape record a status and unwind; upstream's exit()
# ended the process where it stood, so there was nothing to notice.
s = s.replace('''	for (;;) {
		/*
		 * Decode a visual command.''', '''	for (;;) {
		if (ex_quitting)
			co_return;
		/*
		 * Decode a visual command.''')
open(p, 'w').write(s)
