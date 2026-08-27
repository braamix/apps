/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_argv.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

/*
 * Subroutines for major command loop.
 */

/*
 * Is there a single letter indicating a named buffer next?
 */
int cmdreg(void)
{
    int c  = 0;
    int wh = skipwh();

    if (wh && isalpha(peekchar()))
        c = getchar();
    return (c);
}

/*
 * Tell whether the character ends a command
 */
int endcmd(int ch)
{
    switch (ch) {
    case '\n':
    case EOF:
        endline = 1;
        return (1);

    case '|':
    case '"':
        endline = 0;
        return (1);
    }
    return (0);
}

/*
 * Insist on the end of the command.
 */
void eol(void)
{
    if (!skipend())
        THROW(error("Extra chars|Extra characters at end of command"));
    ignnEOF();
}

/*
 * Print out the message in the error message file at str,
 * with i an integer argument to printf.
 */
/*VARARGS2*/
void error(char *str, int i)
{
    error0();
    merror(str, i);
    if (writing) {
        THROW(serror(" [Warning - %s is incomplete]", file));
        writing = 0;
    }
    error_end(str != 0);
}

/*
 * Rewind the argument list.
 */
void erewind(void)
{
    argc = argc0;
    argv = argv0;
    args = args0;
    if (argc > 1 && !hush) {
        printf(mesg("%d files@to edit"), argc);
        if (inopen)
            putchar(' ');
        else
            putNFL();
    }
}

/*
 * Guts of the pre-printing error processing.
 * If in visual and catching errors, then we dont mung up the internals,
 * just fixing up the echo area for the print.
 * Otherwise we reset a number of externals, and discard unused input.
 */
void error0(void)
{
    if (laste) {
        laste = 0;
        sync();
    }
    if (vcatch) {
        if (splitw == 0)
            fixech();
        if (!SO || !SE)
            dingdong();
        return;
    }
    if (input) {
        input = strend(input) - 1;
        if (*input == '\n')
            setlastchar('\n');
        input = 0;
    }
    setoutt();
    flush();
    resetflav();
    if (!SO || !SE)
        dingdong();
    if (inopen) {
        /*
         * We are coming out of open/visual ungracefully.
         * Restore COLUMNS, undo, and fix tty mode.
         */
        COLUMNS = OCOLUMNS;
        undvis();
        putnl();
    }
    inserting = 0;
    inopen    = 0;
    holdcm    = 0;
}

/*
 * Post error printing processing.
 * Close the i/o file if left open.
 * If catching in visual then throw to the visual catch,
 * else if a child after a fork, then exit.
 * Otherwise, in the normal command mode error case,
 * finish state reset, and throw to top.
 */
/*
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
        /*
         * Upstream closed it here. A close is a syscall and this is a
         * plain function, so the descriptor goes to whoever lands --
         * the command loop, or the visual catch -- which can await it.
         */
        ex_pendclose = io;
        io           = -1;
    }
    inappend = inglobal = 0;
    globp = vglobp = 0;
    vmacp = 0;
    if (vcatch) {
        ex_thrown_msg = had_msg;
        ex_thrown     = 1;
        return;
    }
    if (had_msg)
        putNFL();
    ex_thrown_msg = had_msg;
    ex_thrown     = 1;
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
    inopen    = 1;
    vcatch    = 0;
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
        setlastchar('\n');
    while (lastchar() != '\n' && lastchar() != EOF)
        ignchar();
    ungetchar(0);
    endline = 1;
}

void fixol(void)
{
    if (Outchar != vputchar) {
        flush();
        if (state == ONEOPEN || state == HARDOPEN)
            outline = destline = 0;
        Outchar = vputchar;
        vcontin(1);
    } else {
        if (destcol)
            vclreol();
        vclean();
    }
}

/*
 * Does an ! character follow in the command stream?
 */
int exclam(void)
{
    if (peekchar() == '!') {
        ignchar();
        return (1);
    }
    return (0);
}

/*
 * Make an argument list for e.g. next.
 */
void makargs(void)
{
    glob(&frob);
    argc0 = frob.argc0;
    argv0 = frob.argv;
    args0 = argv0[0];
    erewind();
}

/*
 * Advance to next file in argument list.
 */
Task<void> next(void)
{
    extern short isalt; /* defined in ex_io.c */

    if (argc == 0)
        COTHROW(error("No more files@to edit"));
    morargc = argc;
    isalt   = (strcmp(altfile, args) == 0) + 1;
    if (savedfile[0])
        CP(altfile, savedfile);
    CP(savedfile, args);
    argc--;
    args = argv ? *++argv : strend(args) + 1;
}

/*
 * Eat trailing flags and offsets after a command,
 * saving for possible later post-command prints.
 */
void newline(void)
{
    int c;

    resetflav();
    for (;;) {
        c = getchar();
        switch (c) {
        case '^':
        case '-':
            poffset--;
            break;

        case '+':
            poffset++;
            break;

        case 'l':
            listf++;
            break;

        case '#':
            nflag++;
            break;

        case 'p':
            listf = 0;
            break;

        case ' ':
        case '\t':
            continue;

        case '"':
            comment();
            setflav();
            return;

        default:
            if (!endcmd(c))
                THROW(serror("Extra chars|Extra characters at end of \"%s\" command", Command));
            if (c == EOF)
                ungetchar(c);
            setflav();
            return;
        }
        pflag++;
    }
}

/*
 * Before quit or respec of arg list, check that there are
 * no more files in the arg list.
 */
void nomore(void)
{
    if (argc == 0 || morargc == argc)
        return;
    morargc = argc;
    merror("%d more file", argc);
    THROW(serror("%s@to edit", plural((long)argc)));
}

/*
 * Before edit of new file check that either an ! follows
 * or the file has not been changed.
 */
exbool quickly(void)
{
    if (exclam())
        return (1);
    if (chng && dol > zero) {
        /*
                        chng = 0;
        */
        xchng = 0;
        THROWV(0, serror("No write@since last change (:%s! overrides)", Command));
    }
    return (0);
}

/*
 * Reset the flavor of the output to print mode with no numbering.
 */
void resetflav(void)
{
    if (inopen)
        return;
    listf   = 0;
    nflag   = 0;
    pflag   = 0;
    poffset = 0;
    setflav();
}

/*
 * Print an error message with a %s type argument to printf.
 * Message text comes from error message file.
 */
void serror(char *str, char *cp)
{
    error0();
    smerror(str, cp);
    error_end(str != 0);
}

/*
 * Set the flavor of the output based on the flags given
 * and the number and list options to either number or not number lines
 * and either use normally decoded (ARPAnet standard) characters or list mode,
 * where end of lines are marked and tabs print as ^I.
 */
void setflav(void)
{
    if (inopen)
        return;
    setnumb(nflag || value(NUMBER));
    setlist(listf || value(LIST));
    setoutt();
}

/*
 * Skip white space and tell whether command ends then.
 */
int skipend(void)
{
    pastwh();
    return (endcmd(peekchar()) && peekchar() != '"');
}

/*
 * Set the command name for non-word commands.
 */
void tailspec(int c)
{
    static char foocmd[2];

    foocmd[0] = c;
    Command   = foocmd;
}

/*
 * Try to read off the rest of the command word.
 * If alphabetics follow, then this is not the command we seek.
 */
void tail(char *comm)
{
    tailprim(comm, 1, 0);
}

void tail2of(char *comm)
{
    tailprim(comm, 2, 0);
}

char tcommand[20];

void tailprim(char *comm, int i, exbool notinvis)
{
    char *cp;
    int c;

    Command = comm;
    for (cp = tcommand; i > 0; i--)
        *cp++ = *comm++;
    while (*comm && peekchar() == *comm)
        *cp++ = getchar(), comm++;
    c = peekchar();
    if (notinvis || isalpha(c)) {
        /*
         * Of the trailing lp funny business, only dl and dp
         * survive the move from ed to ex.
         */
        if (tcommand[0] == 'd' && any(c, "lp"))
            goto ret;
        if (tcommand[0] == 's' && any(c, "gcr"))
            goto ret;
        while (cp < &tcommand[19] && isalpha(peekchar()))
            *cp++ = getchar();
        *cp = 0;
        if (notinvis)
            THROW(serror("What?|%s: No such command from open/visual", tcommand));
        else
            THROW(serror("What?|%s: Not an editor command", tcommand));
    }
ret:
    *cp = 0;
}

/*
 * Continue after a : command from open/visual.
 */
Task<void> vcontin(exbool ask)
{
    if (vcnt > 0)
        vcnt = -vcnt;
    if (inopen) {
        if (state != VISUAL) {
            /*
             * We don't know what a shell command may have left on
             * the screen, so we move the cursor to the right place
             * and then put out a newline.  But this makes an extra
             * blank line most of the time so we only do it for :sh
             * since the prompt gets left on the screen.
             *
             * BUG: :!echo longer than current line \\c
             * will screw it up, but be reasonable!
             */
            if (state == CRTOPEN)
                vgoto(WECHO, 0);
            if (!ask) {
                putch('\r');
                putch('\n');
            }
            co_return;
        }
        if (ask) {
            merror("[Hit return to continue] ");
            flush();
        }
        if (ask) {
            if (co_await getkey() == ':') {
                /* Ugh. Extra newlines, but no other way */
                putch('\n');
                outline = WECHO;
                ungetkey(':');
            }
        }
        vclrech(1);
    }
}

/*
 * Put out a newline (before a shell escape)
 * if in open/visual.
 */
void vnfl(void)
{
    if (inopen) {
        if (state != VISUAL && state != CRTOPEN && destline <= WECHO)
            vclean();
        else
            vmoveitup(1, 0);
        vgoto(WECHO, 0);
        vclrbyte(vtube[WECHO], WCOLS);
    }
    flush();
}
