p = 'ex_get.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_vis.h"', 1)

# --- getach: the read is gone from here.
s = s.replace('''	flush();
	if (intty) {
		c = read(0, incurs, sizeof incurs - 4);
		if (c < 0)
			return (lastc = EOF);
		if (c == 0 || incurs[c-1] != '\\n')
			incurs[c++] = CTRL('d');
		if (incurs[c-1] == '\\n')
			noteinp();
		incurs[c] = 0;
		for (c--; c >= 0; c--)
			if (incurs[c] == 0)
				incurs[c] = QUOTE;
		input = incurs;
		goto top;
	}
	if (read(0, (char *) &lastc, 1) != 1)
		lastc = EOF;
	return (lastc);
}''', '''	/*
	 * Upstream read here, and that is the one thing this cannot do: a read
	 * is a syscall, a syscall must be awaited, and this is called from the
	 * bottom of the address parser. ex_readline() below fills the buffer
	 * instead, from the three places that can await -- the top of the
	 * command loop, gettty(), and a substitute's confirmation.
	 *
	 * Running dry is therefore end of input, and that is safe because a
	 * whole line is always buffered before any of it is parsed: a command
	 * stops at its newline and never asks for the one after it.
	 */
	return (lastc = EOF);
}

/*
 * A line of command input. Loops until it holds a newline, because everything
 * below getach() assumes a complete line and answers EOF rather than waiting.
 *
 * On a terminal the kernel hands over a line at a time, so one read usually
 * does. Off one, this reads a byte at a time, which is what upstream did and
 * for the same reason: what is not read stays readable, so a child spawned by
 * :! or :r ! sees the rest of the script rather than finding it eaten.
 */
Task<Result<void>> ex_readline(void)
{
	static char inbuf[LBSIZE + 4];
	int n = 0;
	exbool sawnl = 0;

	while (n < (int) sizeof inbuf - 4) {
		Result<String> r = Err(Error::NoMemory);
		if (Task<Result<String>> t = read_some(SYS_STDIN,
		    intty ? (u32) (sizeof inbuf - 4 - n) : 1u))
			r = co_await t;
		if (r.is_err()) {
			if (r.error() == Error::Closed)
				break;
			errno = int(r.error());
			co_return Err(r.error());
		}
		Str got = r.value().str();
		if (got.empty())
			break;
		for (usize i = 0; i < got.size() && n < (int) sizeof inbuf - 4; i++) {
			inbuf[n++] = got.data()[i];
			if (got.data()[i] == '\\n')
				sawnl = 1;
		}
		if (sawnl)
			break;
	}

	/*
	 * Upstream's own coding of the buffer: a short read with no newline is
	 * a ^D, an embedded NUL becomes QUOTE so that the string stays one
	 * string, and a completed line moves the notional cursor down.
	 */
	if (n == 0 || inbuf[n-1] != '\\n')
		inbuf[n++] = CTRL('d');
	if (inbuf[n-1] == '\\n')
		noteinp();
	inbuf[n] = 0;
	for (n--; n >= 0; n--)
		if (inbuf[n] == 0)
			inbuf[n] = QUOTE;
	input = inbuf;
	co_return {};
}''')

# --- gettty: the two K&R-era declarations, and isatty
s = s.replace('''	int numbline();
	extern int (*Pline)();
	int offset = Pline == numbline ? 8 : 0;''',
'''	int offset = Pline == numbline ? 8 : 0;''')

s = s.replace('''				if (lastin == 0 && isatty(0) == -1) {''',
'''				if (lastin == 0) {''')

# gettty must be able to fill the buffer, since append() reads line after line.
s = s.replace('''	if (intty && !inglobal) {
		if (offset) {''', '''	if (input == 0 && peekc == 0)
		if ((co_await ex_readline()).is_err())
			co_return (EOF);
	if (intty && !inglobal) {
		if (offset) {''')

# --- smunch: its K&R parameters were (ocp) with col implicit.
s = s.replace('''void smunch(int c, char *cp)
{
	char *cp;

	cp = ocp;''', '''int smunch(int col, char *ocp)
{
	char *cp;

	cp = ocp;''')

# --- checkjunk wrote the warning straight to standard error.
s = s.replace('''void checkjunk(int c)
{

	if (junkbs == 0 && c == '\\b') {
		write(2, cntrlhm, 13);
		junkbs = 1;
	}
}''', '''void checkjunk(int c)
{

	if (junkbs == 0 && c == '\\b') {
		putS(cntrlhm);
		junkbs = 1;
	}
}''')

s = s.replace('''line *setin(line *addr)
{

	if (addr == zero)
		lastin = 0;
	else
		getline(*addr), lastin = smunch(0, linebuf);
}''', '''void setin(line *addr)
{

	if (addr == zero)
		lastin = 0;
	else
		getline(*addr), lastin = smunch(0, linebuf);
}''')
s = s.replace('\t\tStr got = r.value().str();', '\t\tStr got = res_of(r).str();')
open(p, 'w').write(s)


p = 'ex_get.cpp'
s = open(p).read()

s = s.replace('''Task<Result<void>> ex_readline(void)''', '''/*
 * Is the command input buffer empty?
 *
 * A drained buffer still points at its own terminating NUL: getach() is what
 * turns that into a null pointer, and it does so only when it next runs. So
 * "input == 0" is not the question -- asking it was worth one evening.
 */
exbool need_input(void)
{

	return (peekc == 0 && globp == 0 && (input == 0 || *input == 0));
}

Task<Result<void>> ex_readline(void)''')

s = s.replace('''	if (input == 0 && peekc == 0)
		if ((co_await ex_readline()).is_err())
			co_return (EOF);''',
'''	if (need_input())
		if ((co_await ex_readline()).is_err())
			co_return (EOF);''')
open(p, 'w').write(s)
