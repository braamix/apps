/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"
#include "ex_argv.h"
#include "ex_buf.h"
#include "ex_screen.h"

/*
 * The code for ex is divided as follows:
 *
 * ex.c			Entry point and routines handling interrupt, hangup
 *			signals; initialization code.
 *
 * ex_addr.c		Address parsing routines for command mode decoding.
 *			Routines to set and check address ranges on commands.
 *
 * ex_cmds.c		Command mode command decoding.
 *
 * ex_cmds2.c		Subroutines for command decoding and processing of
 *			file names in the argument list.  Routines to print
 *			messages and reset state when errors occur.
 *
 * ex_cmdsub.c		Subroutines which implement command mode functions
 *			such as append, exdelete, join.
 *
 * ex_data.c		Initialization of options.
 *
 * ex_get.c		Command mode input routines.
 *
 * ex_io.c		General input/output processing: file i/o, unix
 *			escapes, filtering, source commands, preserving
 *			and recovering.
 *
 * ex_put.c		Terminal driving and optimizing routines for low-level
 *			output (cursor-positioning); output line formatting
 *			routines.
 *
 * ex_re.c		Global commands, substitute, regular expression
 *			compilation and execution.
 *
 * ex_set.c		The set command.
 *
 * ex_subr.c		Loads of miscellaneous subroutines.
 *
 * ex_temp.c		Editor buffer routines for main buffer and also
 *			for named buffers (Q registers if you will.)
 *
 * ex_tty.c		Terminal dependent initializations from termcap
 *			data base, grabbing of tty modes (at beginning
 *			and after escapes).
 *
 * ex_unix.c		Routines for the ! command and its variations.
 *
 * ex_v*.c		Visual/open mode routines... see ex_v.c for a
 *			guide to the overall organization.
 */

/*
 * Main procedure.  Process arguments and then
 * transfer control to the main command processing loop
 * in the routine commands.  We are entered as either "ex", "edit", "vi"
 * or "view" and the distinction is made here.  Actually, we are "vi" if
 * there is a 'v' in our name, "view" is there is a 'w', and "edit" if
 * there is a 'd' in our name.  For edit we just diddle options;
 * for vi we actually force an early visual command.
 */
constexpr Str USAGE =
    "Usage:\n"
    "    ex [-] [-R] [-v] [-t tag] [-w size] [+cmd] file ...\n"
    "    vi [-] [-R] [-t tag] [-w size] [+cmd] file ...\n"
    "\n"
    "    -     script mode: no prompt, no autoprint\n"
    "    -R    read only\n"
    "    -v    start in visual mode\n"
    "    -t    edit the file holding a tag\n"
    "    -w    window size for visual\n"
    "    +cmd  run cmd after reading the first file\n";

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
    int ac      = (int)args.size();
    char **av   = avbuf;
    char *bp    = argbuf;
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
        Str w   = args[i];
        usize n = w.size();

        if (bp + n + 1 > argbuf + sizeof argbuf) {
            co_await write_all(SYS_STDERR, "ex: arg list too long\n");
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
        co_await write_all(SYS_STDERR, "ex: out of memory\n");
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
    ivis = any('v', av[0]); /* "vi" */
#endif
    if (any('w', av[0])) /* "view" */
        value(READONLY) = 1;
    if (any('d', av[0])) { /* "edit" */
        value(OPEN)   = 0;
        value(REPORT) = 1;
        value(MAGIC)  = 0;
    }

#ifndef EX_HAVE_VISUAL
    /*
     * The visual half is not built yet. Say so once, here, rather than
     * leaving it to fail at the first :visual: upstream defers the whole
     * of startup into visual when it is entered as vi, and a vi that
     * refused at that point would have read no file and taken no command.
     */
    if (ivis) {
        co_await write_all(SYS_STDERR, "vi: visual mode is not built yet -- this is ex\n");
        ivis = 0;
    }
#endif

    /*
     * Process flag arguments.
     */
    ac--, av++;
    while (ac && av[0][0] == '-') {
        c = av[0][1];
        if (c == 0) {
            hush             = 1;
            value(AUTOPRINT) = 0;
            fast++;
        } else
            switch (c) {
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
                if (av[0][2] == 0)
                    defwind = 3;
                else
                    for (cp = &av[0][2]; isdigit(*cp); cp++)
                        defwind = 10 * defwind + *cp - '0';
                break;

            default:
                smerror((char *)"Unknown option %s\n", av[0]);
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
                co_await source(strcat(strcpy(genbuf, cp), (char *)"/.exrc"), 1);
        }
        ex_thrown = 0;
    }
    init(); /* moved after prev 2 chunks to fix directory option */

    /*
     * Initial processing.  Handle tag and file argument
     * implied next commands.  If going in as 'vi', then don't do
     * anything, just set initev so we will do it later (from within
     * visual).
     */
    if (itag)
        globp = (char *)(ivis ? "tag" : "tag|p");
    else if (argc)
        globp = (char *)"next";
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
        globp = (char *)"visual";
        co_await commands(1, 1);
        if (!ex_quitting)
            ex_thrown = 0;
    }

    /*
     * Clear out trash in state accumulated by startup,
     * and then do the main command loop for a normal edit.
     * If you quit out of a 'vi' command by doing Q or ^\,
     * you also fall through to here.
     */
    if (!ex_quitting) {
        seenprompt = 1;
        ungetchar(0);
        globp  = 0;
        initev = 0;
        setlastchar('\n');
        ex_thrown = 0;
        co_await commands(0, 0);
    }
    cleanup(1);
    co_await exflush();
    co_return (ex_quitting ? ex_status : 0);
}

/*
 * Initialization, before editing a new file.
 * Main thing here is to get a new buffer (in fileinit),
 * rest is peripheral state resetting.
 */
void init(void)
{
    int i;

    fileinit();
    dot = zero = truedol = unddol = dol = fendcore;
    one                                 = zero + 1;
    undkind                             = UNDNONE;
    chng                                = 0;
    edited                              = 0;
    for (i = 0; i <= 'z' - 'a' + 1; i++)
        names[i] = 1;
    anymarks = 0;
}

/*
 * Return last component of unix path name p.
 */
char *tailpath(char *p)
{
    char *r;

    for (r = p; *p; p++)
        if (*p == '/')
            r = p + 1;
    return (r);
}
