p = 'ex_cmds2.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"', 1)

# --- error(): the K&R declaration was #ifdef'd, so it kept its old header.
s = s.replace('''/*VARARGS2*/
error(str, i)
#ifdef lint
	char *str;
#else
	int str;
#endif
	int i;
{

	error0();
	merror(str, i);
	if (writing) {
		serror(" [Warning - %s is incomplete]", file);
		writing = 0;
	}
	error1(str);
}''', '''/*VARARGS2*/
void error(char *str, int i)
{

	error0();
	merror(str, i);
	if (writing) {
		serror(" [Warning - %s is incomplete]", file);
		writing = 0;
	}
	error_end(str != 0);
}''')

# --- error0(): the temp-file sync and the tty restore.
s = s.replace('''	if (laste) {
#ifdef VMUNIX
		tlaste();
#endif
		laste = 0;
		sync();
	}''', '''	if (laste) {
		laste = 0;
		sync();
	}''')

s = s.replace('''		COLUMNS = OCOLUMNS;
		undvis();
		ostop(normf);
		/* ostop should be doing this
		putpad(VE);
		putpad(KE);
		*/
		putnl();''', '''		COLUMNS = OCOLUMNS;
		undvis();
		putnl();''')

# --- error1() -> error_end(). This is the whole of the port's control flow.
s = s.replace('''error1(str)
	char *str;
{
	exbool die;

	if (io > 0) {
		close(io);
		io = -1;
	}
	die = (getpid() != ppid);	/* Only children die */
	inappend = inglobal = 0;
	globp = vglobp = vmacp = 0;
	if (vcatch && !die) {
		inopen = 1;
		vcatch = 0;
		if (str)
			noonl();
		fixol();
		longjmp(vreslab,1);
	}
	if (str && !vcatch)
		putNFL();
	if (die)
		exit(1);
	lseek(0, 0L, 2);
	if (inglobal)
		setlastchar('\\n');
	while (lastchar() != '\\n' && lastchar() != EOF)
		ignchar();
	ungetchar(0);
	endline = 1;
	reset();
}''', '''/*
 * Post error printing processing.
 * Close the i/o file if left open.
 *
 * This is where upstream threw: to a visual CATCH if one was set, and to the
 * top of the command loop otherwise. Neither throw exists -- there is no
 * setjmp here and none can be written, because wasm keeps its call stack
 * outside linear memory. So this records, and the THROW macros unwind one
 * frame at a time until a landing is reached: excatch() below for the visual
 * side, ex_reset() for the command loop.
 *
 * The one ordering that matters: ex_thrown goes up last. Output is a buffer
 * now, putch() drops on the way out, and the message above has to reach it.
 *
 * `die` is gone with it. Upstream compared getpid() against the pid it started
 * with, because filter() forked a second copy of the editor to feed a command
 * and that copy had to exit rather than unwind; nothing forks here.
 */
void error_end(exbool had_msg)
{

	if (io > 0) {
		ex_close(io);
		io = -1;
	}
	inappend = inglobal = 0;
	globp = vglobp = vmacp = 0;
	if (vcatch) {
		ex_thrown_msg = had_msg;
		ex_thrown = 1;
		return;
	}
	if (had_msg)
		putNFL();
	ex_thrown_msg = had_msg;
	ex_thrown = 1;
}

/*
 * The visual landing. Upstream ran this at the throw, just before the longjmp;
 * it runs at the catch now, because nothing unwinds by itself. Answers whether
 * there was an error to catch, so a CATCH block reads as an if.
 */
exbool excatch(void)
{

	if (!ex_thrown || ex_quitting)
		return (0);
	ex_thrown = 0;
	inopen = 1;
	vcatch = 0;
	if (ex_thrown_msg)
		noonl();
	fixol();
	return (1);
}

/*
 * The command mode landing, at the top of commands()'s loop. What upstream did
 * on the way to reset(): throw away the rest of the line the bad command was
 * on, so the next one starts clean.
 */
void ex_reset(void)
{

	ex_thrown = 0;
	if (inglobal)
		setlastchar('\\n');
	while (lastchar() != '\\n' && lastchar() != EOF)
		ignchar();
	ungetchar(0);
	endline = 1;
}''')
s = s.replace('\terror1(str);', '\terror_end(str != 0);')
open(p, 'w').write(s)
print('ok')
p = 'ex_cmds2.cpp'
s = open(p).read()

# error_end() is a plain function and a close is a syscall, so the descriptor
# is handed to the landing, which can await.
s = s.replace('''	if (io > 0) {
		ex_close(io);
		io = -1;
	}''', '''	if (io > 0) {
		/*
		 * Upstream closed it here. A close is a syscall and this is a
		 * plain function, so the descriptor goes to whoever lands --
		 * the command loop, or the visual catch -- which can await it.
		 */
		ex_pendclose = io;
		io = -1;
	}''')

s = s.replace('''void quickly(void)
{

	if (exclam())
		return (1);''', '''exbool quickly(void)
{

	if (exclam())
		return (1);''')

s = s.replace('''		THROWV(0, error("No write@since last change (:%s! overrides)", Command));''',
              '''		THROWV(0, serror("No write@since last change (:%s! overrides)",
				 Command));''')

s = s.replace('''serror(str, cp)
#ifdef lint
	char *str;
#else
	int str;
#endif
	char *cp;
{

	error0();
	smerror(str, cp);
	error1(str);
}''', '''void serror(char *str, char *cp)
{

	error0();
	smerror(str, cp);
	error_end(str != 0);
}''')
open(p, 'w').write(s)

p = 'ex_cmds2.cpp'
s = open(p).read()

s = s.replace('COTHROW(merror("[Hit co_return to continue] "));',
              'merror((char *) "[Hit return to continue] ");')

# The tty-mode calls. ostart/ostop/tostart/tostop/vraw/vcook/termreset put the
# terminal in and out of raw mode and re-synced the cursor; the screen is
# claimed once, in vop(), and there is nothing to re-sync -- a repaint sends
# whatever changed.
s = s.replace('''			if (state == CRTOPEN) {
				termreset();
				vgoto(WECHO, 0);
			}''', '''			if (state == CRTOPEN)
				vgoto(WECHO, 0);''')

s = s.replace('''#ifndef CBREAK
		vraw();
#endif
		if (ask) {''', '''		if (ask) {''')

s = s.replace('''		vclrech(1);
		if (Peekkey != ':') {
			putpad(TI);
			tostart();
			/* replaced by ostart.
			putpad(VS);
			putpad(KS);
			*/
		}''', '''		vclrech(1);''')

s = s.replace('''		vgoto(WECHO, 0);
		vclrbyte(vtube[WECHO], WCOLS);
		tostop();
		/* replaced by the ostop above
		putpad(VE);
		putpad(KE);
		*/
	}''', '''		vgoto(WECHO, 0);
		vclrbyte(vtube[WECHO], WCOLS);
	}''')
open(p, 'w').write(s)

p = 'ex_cmds2.cpp'
s = open(p).read()
s = s.replace('void tailprim(char *comm, int comfewest, exbool comdiff)',
              'void tailprim(char *comm, int i, exbool notinvis)')
open(p, 'w').write(s)

p = 'ex_cmds2.cpp'
s = open(p).read()
# Upstream defined pflag, nflag and poffset here *and* in ex_cmds.c. C merged
# the two into one common symbol; C++ has none, so ex_cmds.cpp keeps them.
s = s.replace('exbool\tpflag, nflag;\nint\tpoffset;\n\n', '')
open(p, 'w').write(s)
