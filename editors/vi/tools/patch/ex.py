p = 'ex.cpp'
s = open(p).read()

s = s.replace('#include "ex.h"', '#include "ex.h"\n#include "ex_buf.h"\n'
              '#include "ex_screen.h"\n#include "ex_vis.h"', 1)

# main() is replaced whole. Everything it did before falling into commands()
# was setup for a machine that is not here: grabbing the tty modes, opening the
# file the error messages were compiled into, and installing five signal
# handlers, of which two signals do not exist and one was an 11/40 overlay bug.
# The four setexit() guards each bracketed a stage of startup so that an error
# in it landed back here rather than killing the editor; each is a clearing of
# the flag now, and the stage after it runs either way, as it did.
old = s[s.index('main(ac, av)'):s.index('/*\n * Initialization, before editing a new file.')]
new = '''constexpr Str USAGE =
	"Usage:\\n"
	"    ex [-] [-R] [-v] [-t tag] [-w size] [+cmd] file ...\\n"
	"    vi [-] [-R] [-t tag] [-w size] [+cmd] file ...\\n"
	"\\n"
	"    -     script mode: no prompt, no autoprint\\n"
	"    -R    read only\\n"
	"    -v    start in visual mode\\n"
	"    -t    edit the file holding a tag\\n"
	"    -w    window size for visual\\n"
	"    +cmd  run cmd after reading the first file\\n";

/* argv, copied out of the Args views once so that everything below sees the
 * NUL-terminated words it was written against. */
static char *avbuf[NARGS + 2];
static char argbuf[NCARGS];

Task<i32> proc_main(Args args)
{
	char *cp;
	int c;
	exbool ivis;
	exbool itag = 0;
	exbool fast = 0;
	int ac = (int) args.size();
	char **av = avbuf;
	char *bp = argbuf;
	int i;

	/*
	 * ex had no --help; the tree's programs answer one, so this does too,
	 * and nothing below ever sees the word.
	 */
	for (i = 1; i < ac; i++)
		if (args[i] == Str("--help")) {
			co_await write_all(SYS_STDOUT, USAGE);
			co_return 0;
		}

	if (ac > NARGS)
		ac = NARGS;
	for (i = 0; i < ac; i++) {
		Str w = args[i];
		usize n = w.size();

		if (bp + n + 1 > argbuf + sizeof argbuf) {
			co_await write_all(SYS_STDERR, "ex: arg list too long\\n");
			co_return 1;
		}
		av[i] = bp;
		memcpy(bp, w.data(), n);
		bp += n;
		*bp++ = 0;
	}
	av[ac] = 0;

	/*
	 * The line pointer array, which sbrk used to hand out. It is taken once
	 * and never moved, because dot, dol, the marks and the undo bounds are
	 * all raw pointers into it.
	 */
	if (!lx_init()) {
		co_await write_all(SYS_STDERR, "ex: out of memory\\n");
		co_return 1;
	}

	/*
	 * Figure out how we were invoked: ex, edit, vi, view. A link named for
	 * any of them still works; the two binaries this tree builds carry the
	 * answer in a define as well, so vi is visual even when it is reached
	 * by a path with no v in it.
	 */
	av[0] = tailpath(av[0]);
#ifdef EX_DEFAULT_VISUAL
	ivis = 1;
#else
	ivis = any('v', av[0]);	/* "vi" */
#endif
	if (any('w', av[0]))	/* "view" */
		value(READONLY) = 1;
	if (any('d', av[0])) {	/* "edit" */
		value(OPEN) = 0;
		value(REPORT) = 1;
		value(MAGIC) = 0;
	}

	/*
	 * Process flag arguments.
	 */
	ac--, av++;
	while (ac && av[0][0] == '-') {
		c = av[0][1];
		if (c == 0) {
			hush = 1;
			value(AUTOPRINT) = 0;
			fast++;
		} else switch (c) {

		case 'R':
			value(READONLY) = 1;
			break;

		case 't':
			if (ac > 1 && av[1][0] != '-') {
				ac--, av++;
				itag = 1;
				/* BUG: should check for too long tag. */
				CP(lasttag, av[0]);
			}
			break;

		case 'v':
			ivis = 1;
			break;

		case 'w':
			defwind = 0;
			if (av[0][2] == 0) defwind = 3;
			else for (cp = &av[0][2]; isdigit(*cp); cp++)
				defwind = 10*defwind + *cp - '0';
			break;

		default:
			smerror((char *) "Unknown option %s\\n", av[0]);
			break;
		}
		ac--, av++;
	}

	if (ac && av[0][0] == '+') {
		firstpat = &av[0][1];
		ac--, av++;
	}

	/*
	 * Initialize the argument list.
	 */
	argv0 = av;
	argc0 = ac;
	args0 = av[0];
	erewind();

	/*
	 * Set up the terminal environment and read user startup commands.
	 *
	 * There is no terminal type to look up: the screen is an array of
	 * cells, so the capabilities are fixed and ex_screen.h names them. The
	 * geometry is the one thing that is not, and it rides on every terminal
	 * reply, so it is asked for where the screen is claimed.
	 */
	co_await setrupt();
	{
		Result<TtyInfo> t = Err(Error::NoMemory);

		if (Task<Result<TtyInfo>> k = tty_of(SYS_STDIN))
			t = co_await k;
		intty = t.is_ok() && res_of(t).console;
	}
	value(PROMPT) = intty;
	if ((cp = getenv("SHELL")) != 0)
		CP(shell, cp);
	if (fast)
		intty = 0;
	ex_thrown = 0;

	if (!fast && intty) {
		if ((globp = getenv("EXINIT")) != 0 && *globp)
			co_await commands(1, 1);
		else {
			globp = 0;
			if ((cp = getenv("HOME")) != 0 && *cp)
				co_await source(strcat(strcpy(genbuf, cp),
						       (char *) "/.exrc"), 1);
		}
		ex_thrown = 0;
	}
	init();	/* moved after prev 2 chunks to fix directory option */

	/*
	 * Initial processing.  Handle tag and file argument
	 * implied next commands.  If going in as 'vi', then don't do
	 * anything, just set initev so we will do it later (from within
	 * visual).
	 */
	if (itag)
		globp = (char *)(ivis ? "tag" : "tag|p");
	else if (argc)
		globp = (char *) "next";
	if (ivis)
		initev = globp;
	else if (globp) {
		inglobal = 1;
		co_await commands(1, 1);
		inglobal = 0;
	}
	ex_thrown = 0;

	/*
	 * Vi command... go into visual.
	 * Strange... everything in vi usually happens
	 * before we ever "start".
	 */
	if (ivis && !ex_quitting) {
		/*
		 * Don't have to be upward compatible with stupidity
		 * of starting editing at line $.
		 */
		if (dol > zero)
			dot = one;
		globp = (char *) "visual";
		co_await commands(1, 1);
		if (!ex_quitting)
			ex_thrown = 0;
	}

	/*
	 * Clear out trash in state accumulated by startup,
	 * and then do the main command loop for a normal edit.
	 * If you quit out of a 'vi' command by doing Q or ^\\,
	 * you also fall through to here.
	 */
	if (!ex_quitting) {
		seenprompt = 1;
		ungetchar(0);
		globp = 0;
		initev = 0;
		setlastchar('\\n');
		ex_thrown = 0;
		co_await commands(0, 0);
	}
	cleanup(1);
	co_await exflush();
	co_return (ex_quitting ? ex_status : 0);
}

'''
s = s.replace(old, new)
open(p, 'w').write(s)
print('ok')

p = 'ex.cpp'
s = open(p).read()

open(p, 'w').write(s)

p = 'ex.cpp'
s = open(p).read()
# Upstream set this from what SIGINT's disposition was on entry -- a shell that
# started ex in the background left it ignored, and then ex left it ignored.
# There is no such thing here: a signal is asked for or it is not.
s = s.replace('\tco_await setrupt();', '\truptible = 1;\n\tco_await setrupt();')
open(p, 'w').write(s)
