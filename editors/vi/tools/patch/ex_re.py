p = 'ex_re.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"\n#include "ex_re.h"\n',
              '#include "ex.h"\n#include "ex_re.h"\n#include "ex_screen.h"\n'
              '#include "ex_vis.h"\n')

s = s.replace('''	flush();
	ch = c = co_await getkey();
again:
	if (c == '\\r')
		c = '\\n';
	if (inopen)
		putchar(c), flush();
	if (c != '\\n' && c != EOF) {
		c = co_await getkey();
		goto again;
	}''', '''	flush();
	/*
	 * getkey() reads a raw key, which needs a claimed keyboard, and command
	 * mode has not claimed one -- upstream reached the terminal directly
	 * and so could ask either way. getch(), which upstream left written but
	 * never called, is the other half.
	 */
	ch = c = inopen ? co_await getkey() : co_await getch();
again:
	if (c == '\\r')
		c = '\\n';
	if (inopen)
		putchar(c), flush();
	if (c != '\\n' && c != EOF) {
		c = inopen ? co_await getkey() : co_await getch();
		goto again;
	}''')

s = s.replace('''Task<int> getch(void)
{
	char c;

	if (read(2, &c, 1) != 1)
		co_return (EOF);
	co_return (c & TRIM);
}''', '''/*
 * One character of a substitute's confirmation. Upstream read it from file
 * descriptor 2, which was the terminal even when the commands came from a
 * script; here it comes from the ordinary command input, so a scripted s///c
 * is answered by the script.
 */
Task<int> getch(void)
{
	int c;

	if (need_input())
		if ((co_await ex_readline()).is_err())
			co_return (EOF);
	c = getcd();
	co_return (c == EOF ? EOF : (c & TRIM));
}''')
open(p, 'w').write(s)
print('ok')

p = 'ex_re.cpp'
s = open(p).read()
# Two implicit-int functions whose value nothing used. C left the return
# register undefined; C++ makes falling off the end undefined behaviour, and
# clang turns that into a trap.
s = s.replace('''endrhs:
	*rp++ = 0;
}''', '''endrhs:
	*rp++ = 0;
}''')
open(p, 'w').write(s)
