p = 'ex_set.cpp'
s = open(p).read()

s = s.replace('#include "ex_screen.h"\n', '#include "ex_screen.h"\n#include "ex_vis.h"\n')

s = s.replace('\textern short ospeed;\n', '')

# w300, w1200 and w9600 picked a window size from the line's speed, and there
# is no line: the screen is a shared array, so this is always the w9600 case
# and the other two are ignored as they were on a fast terminal.
s = s.replace('''		/* Implement w300, w1200, and w9600 specially */
		if (eq(cp, "w300")) {
			if (ospeed >= B1200) {
dontset:
				ignore(getchar());	/* = */
				ignore(getnum());	/* value */
				continue;
			}
			cp = "window";
		} else if (eq(cp, "w1200")) {
			if (ospeed < B1200 || ospeed >= B2400)
				goto dontset;
			cp = "window";
		} else if (eq(cp, "w9600")) {
			if (ospeed < B2400)
				goto dontset;
			cp = "window";
		}''', '''		/*
		 * Implement w300, w1200, and w9600 specially.
		 * These chose a window size from the line's speed. There is no
		 * line: the screen is an array both sides can see, so this is
		 * always the fast case, and the slow two are ignored exactly as
		 * they were on a terminal that was already fast.
		 */
		if (eq(cp, "w300") || eq(cp, "w1200")) {
			ignore(getchar());	/* = */
			ignore(getnum());	/* value */
			continue;
		} else if (eq(cp, "w9600")) {
			cp = "window";
		}''')

s = s.replace('''				if (inopen)
THROW(error("Can't change type of terminal from within open/visual"));
				setterm(optname);''',
'''				/*
				 * There is no terminal type to set: the screen
				 * is an array of cells, so a name would name
				 * nothing. :set term is answered, not obeyed.
				 */
				THROW(error("Terminal type is not settable"));''')

s = s.replace('void setend(void)\n{\n\n\treturn (', 'exbool setend(void)\n{\n\n\treturn (')
open(p, 'w').write(s)

