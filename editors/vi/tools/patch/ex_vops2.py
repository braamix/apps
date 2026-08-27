p = 'ex_vops2.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

s = s.replace('\t\taddtext(gobblebl ? " " : "\\n");',
              '\t\taddtext((char *)(gobblebl ? " " : "\\n"));')
s = s.replace('vremote(1, yank, 0)', 'vremote(1, op_yank, 0)')
s = s.replace('\tint (*OO)() = Outchar;', '\tOutcharFn OO = Outchar;')

# The tty's erase and kill characters, which the line discipline used to hand
# over and which the user could change with stty. There is no line discipline:
# a key arrives as itself, so erase is the backspace key -- which the shim in
# ex_screen.cpp already answers as ^H -- and there is no kill character at all.
# ^U is what everything since has used for it.
s = s.replace('\t\t\tif (c == tty.sg_erase)\n\t\t\t\tc = CTRL(\'h\');\n'
              '\t\t\telse if (c == tty.sg_kill)\n\t\t\t\tc = -1;',
              '\t\t\tif (c == CTRL(\'u\'))\n\t\t\t\tc = -1;')
s = s.replace("if (c == tty.sg_erase || c == tty.sg_kill)",
              "if (c == CTRL('h') || c == CTRL('u'))")
open(p, 'w').write(s)

p = 'ex_vops2.cpp'
s = open(p).read()

# vdoappend. append() is a coroutine because most of what is handed to it
# reads a file or a terminal; vgetsplit reads nothing and answers once. Making
# vdoappend await would take fixzero with it and vrepaint after that, which is
# called from twenty places that need not await. So the one-line case is here.
old = s[s.index('void vdoappend(char *lp)'):s.index('/*\n * Vmaxrep determines')]
s = s.replace(old, '''void vdoappend(char *lp)
{
	line *a1, *a2, *rdot;
	int oing = inglobal;

	inglobal = 1;
	strcLIN(lp);
	if (truedol >= endcore && morelines() < 0) {
		inglobal = oing;
		THROW(error("Out of memory@- too many lines in file"));
	}
	/*
	 * append()'s loop body, once. The undo bookkeeping it does first is
	 * guarded by !inopen and this is only ever called from open or visual,
	 * so it does not apply.
	 */
	a1 = truedol + 1;
	a2 = a1 + 1;
	dot++;
	undap2++;
	dol++;
	unddol++;
	truedol++;
	for (rdot = dot; a1 > rdot;)
		*--a2 = *--a1;
	*rdot = 0;
	putmark(rdot);
	inglobal = oing;
}

''')
open(p, 'w').write(s)

p = 'ex_vops2.cpp'
s = open(p).read()
# Upstream declared these here and in ex_vops.c both; C merged the two into
# one common symbol. ex_vops.cpp keeps them, and ex_vis.h declares them.
s = s.replace('char\t*vUA1, *vUA2;\nchar\t*vUD1, *vUD2;\n', '')
open(p, 'w').write(s)
