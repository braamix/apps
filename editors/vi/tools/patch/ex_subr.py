p = 'ex_subr.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

s = s.replace('''void merror1(char *seekpt)
{

	lseek(erfile, (long) seekpt, 0);
	if (read(erfile, linebuf, 128) < 2)
		CP(linebuf, "ERROR");
}''', '''/*
 * Off VMUNIX the messages lived in a file that xstr built and this seeked to
 * one; here, as there, they are compiled in and the "seek pointer" is the
 * string itself.
 */
void merror1(char *seekpt)
{

	strcpy(linebuf, seekpt);
}''')

# morelines: the reservation replaces sbrk. It may not move, so this only
# bumps endcore against the limit taken at startup.
s = s.replace('''	if ((int) sbrk(1024 * sizeof (line)) == -1)
		return (-1);
	endcore += 1024;''', '''	if (endcore + 1024 >= lx_limit)
		return (-1);
	endcore += 1024;''')

s = s.replace('\treturn (i == 1 ? "" : "s");', '\treturn (char *)(i == 1 ? "" : "s");')

s = s.replace('\tint (*OO)();', '\tOutcharFn OO;')
s = s.replace('\tint (*OO)() = Outchar;', '\tOutcharFn OO = Outchar;')

s = s.replace('''	if (c == '\\t') {
		vcntcol += value(TABSTOP) - vcntcol % value(TABSTOP);
		return;
	}
	vcntcol++;
}''', '''	if (c == '\\t') {
		vcntcol += value(TABSTOP) - vcntcol % value(TABSTOP);
		return (0);
	}
	vcntcol++;
	return (0);
}''')

s = s.replace('''int whitecnt(char *str)
{''', '''int whitecnt(char *cp)
{''')

s = s.replace('exbool comment(void)', 'void comment(void)')

# The errno table was Unix's own. The kernel names its errors, one call away.
start = s.index('#define\tstd_nerrs')
end = s.index('#undef\terror') + len('#undef\terror\n')
s = s[:start] + s[end:]

s = s.replace('''void syserror(void)
{
	int e = errno;

	dirtcnt = 0;
	putchar(' ');
	edited = 0;	/* for temp file errors, for example */
	if (e >= 0 && errno <= std_nerrs)
		THROW(error(std_errlist[e]));
	else
		THROW(error("System error %d", e));
}''', '''/*
 * The last system call's complaint. Upstream carried a copy of the errno
 * message list, because perror wrote to standard error and ex wanted the text
 * in a buffer of its own; the kernel names its errors, so this asks.
 */
void syserror(void)
{
	static char buf[48];
	Str m = error_name(Error(errno));
	usize n = m.size() < sizeof buf - 1 ? m.size() : sizeof buf - 1;

	putchar(' ');
	edited = 0;	/* for temp file errors, for example */
	memcpy(buf, m.data(), n);
	buf[n] = 0;
	THROW(error(buf));
}''')
open(p, 'w').write(s)

p = 'ex_subr.cpp'
s = open(p).read()

# tabcol's K&R declaration sat at column 0, so the converter left it.
s = s.replace('''tabcol(col, ts)
int col, ts;
{''', '''int tabcol(int col, int ts)
{''')

s = s.replace('\tint (*OO)() = Outchar;', '\tOutcharFn OO = Outchar;')

# Everything from the overlay-trap handler to the end of the file is signals,
# preserve and job control, none of which survives. What replaces it:
#
#   onemt    an 11/40 overlay bug; there are no overlays
#   onhup    there is no SIGHUP, and no temp file to preserve
#   onintr   SIG_INT is asked for rather than handled: the parked call comes
#            back Err(Intr) and sig_take says which signal it was
#   preserve there is no temp file and no setuid helper to hand it to
#   exit     Sys::Exit records a status and a process ends when its root task
#            returns, so nothing can die where it stands
#   onsusp   SIG_TSTP is not in Braam's catchable set, so ^Z cannot be caught
cut = s.index('''/*
 * The following code is defensive programming against a bug in the
 * pdp-11 overlay implementation.''')
s = s[:cut] + '''/*
 * An interrupt occurred.  Drain any output which
 * is still in the output buffering pipeline.
 * Catch interrupts again.  Unless we are in visual
 * reset the output state (out of -nl mode, e.g).
 * Then like a normal error (with the \\n before Interrupt
 * suppressed in visual mode).
 *
 * Upstream re-armed the handler here, because a caught signal reverted to the
 * default action on delivery. sig_catch is a standing request, so there is
 * nothing to re-arm; what is left is the draining and the message.
 */
void onintr(void)
{

	draino();
	if (!inopen) {
		pstop();
		setlastchar('\\n');
	}
	THROW(error((char *)"\\nInterrupt" + inopen));
}

/*
 * If we are interruptible, enable interrupts again.
 * In some critical sections we turn interrupts off,
 * but not very often.
 *
 * Asking for SIG_INT is what stops it killing the process outright: the
 * default action is death, and a full-screen program that has taken the whole
 * screen has to stay killable until it says otherwise.
 */
Task<void> setrupt(void)
{

	if (ruptible)
		if (Task<Result<void>> t = sig_catch(SIG_INT))
			co_await t;
}

/*
 * Upstream's exit() closed the trace file and called _exit. Sys::Exit only
 * records a status here -- a process ends when its root task returns -- so
 * this records what to end with and unwinds by the same route an error takes.
 */
void ex_exit(int i)
{

	ex_status = i;
	ex_quitting = 1;
	ex_thrown = 1;
}
'''
open(p, 'w').write(s)
print('ok')
