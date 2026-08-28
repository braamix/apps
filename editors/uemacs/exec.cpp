/*	exec.c
 *
 *	This file is for functions dealing with execution of
 *	commands, command lines, buffers, files and startup files.
 *
 *	written 1986 by Daniel Lawrence
 *	modified by Petri Kutvonen
 */

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"

#include "proc/io.h"

/*
 * Give the native stack back.
 *
 * A co_await is a call here and not a tail call - the wasm tail-call feature
 * is off - so entering a task and returning from it each leave a frame
 * behind, and a loop that awaits without ever suspending grows the stack
 * until it overflows.  Suspending on a syscall is what unwinds it: the kernel
 * re-enters at _resume, from the top.  So every so many turns round such a
 * loop, park on the cheapest syscall there is.
 *
 * The counter is one for the whole editor rather than one per loop, because
 * the stack is one: a macro three levels deep is three loops sharing it.
 */
#define EXEC_YIELD 32

Task<void> exec_yield(void)
{
    static int turns;

    if (++turns < EXEC_YIELD)
        co_return;
    turns = 0;
    if (Task<Result<void>> t = sleep_for(0))
        co_await t;
}

/*
 * Execute a named command even if it is not bound.
 */
Task<int> cmd_execute_named_command(int f, int n)
{
    fn_t kfunc; /* ptr to the requexted function to bind to */

    /* prompt the user to type a named command */
    msg_printf(": ");

    /* and now get the function name to execute */
    kfunc = co_await getname();
    if (kfunc == NULL) {
        msg_printf("(No such function)");
        co_return FALSE;
    }

    /* and then execute the command */
    co_return co_await kfunc(f, n);
}

/*
 * execcmd:
 *	Execute a command line command to be typed in
 *	by the user
 *
 * int f, n;		default Flag and Numeric argument
 */
Task<int> cmd_execute_command_line(int f, int n)
{
    int status;           /* status return */
    char cmdstr[NSTRING]; /* string holding command to execute */

    /* get the line wanted */
    if ((status = co_await ask_string(": ", cmdstr, NSTRING)) != TRUE)
        co_return status;

    if_level = 0;
    co_return co_await docmd(cmdstr);
}

/*
 * docmd:
 *	take a passed string as a command line and translate
 * 	it to be executed as a command. This function will be
 *	used by execute-command-line and by all source and
 *	startup files. Lastflag/thisflag is also updated.
 *
 *	format of the command line is:
 *
 *		{# arg} <command-name> {<argument string(s)>}
 *
 * char *cline;		command line to execute
 */
Task<int> docmd(char *cline)
{
    int f;             /* default argument flag */
    int n;             /* numeric repeat value */
    fn_t fnc;          /* function to execute */
    int status;        /* return status of function */
    int oldcle;        /* old contents of clexec flag */
    char *oldestr;     /* original exec string */
    char tkn[NSTRING]; /* next token off of command line */

    /* if we are scanning and not executing..go back here */
    if (if_level)
        co_return TRUE;

    oldestr        = command_string; /* save last ptr to string to execute */
    command_string = cline;          /* and set this one as current */

    /* first set up the default command values */
    f        = FALSE;
    n        = 1;
    lastflag = thisflag;
    thisflag = 0;

    if ((status = co_await macarg(tkn)) != TRUE) { /* and grab the first token */
        command_string = oldestr;
        co_return status;
    }

    /* process leadin argument */
    if (token_type(tkn) != TKCMD) {
        f = TRUE;
        co_await getval(tkn, tkn, sizeof(tkn));
        n = atoi(tkn);

        /* and now get the command to execute */
        if ((status = co_await macarg(tkn)) != TRUE) {
            command_string = oldestr;
            co_return status;
        }
    }

    /* and match the token to see if it exists */
    if ((fnc = fncmatch(tkn)) == NULL) {
        msg_printf("(No such Function)");
        command_string = oldestr;
        co_return FALSE;
    }

    /* save the arguments and go execute the command */
    oldcle                 = executing_command_line; /* save old clexec flag */
    executing_command_line = TRUE;                   /* in cline execution */
    status                 = co_await (*fnc)(f, n);  /* call the function */
    command_status         = status;                 /* save the status */
    executing_command_line = oldcle;                 /* restore clexec flag */
    command_string         = oldestr;
    co_return status;
}

/*
 * token:
 *	chop a token off a string
 *	return a pointer past the token
 *
 * char *src, *tok;	source string, destination token string
 * int size;		maximum size of token
 */
char *token(char *src, char *tok, int size)
{
    int quotef; /* is the current string quoted? */
    char c;     /* temporary character */

    /* first scan past any whitespace in the source string */
    while (*src == ' ' || *src == '\t')
        ++src;

    /* scan through the source string */
    quotef = FALSE;
    while (*src) {
        /* process special characters */
        if (*src == '~') {
            ++src;
            if (*src == 0)
                break;
            switch (*src++) {
            case 'r':
                c = 13;
                break;
            case 'n':
                c = 10;
                break;
            case 't':
                c = 9;
                break;
            case 'b':
                c = 8;
                break;
            case 'f':
                c = 12;
                break;
            default:
                c = *(src - 1);
            }
            if (--size > 0) {
                *tok++ = c;
            }
        } else {
            /* check for the end of the token */
            if (quotef) {
                if (*src == '"')
                    break;
            } else {
                if (*src == ' ' || *src == '\t')
                    break;
            }

            /* set quote mode if quote found */
            if (*src == '"')
                quotef = TRUE;

            /* record the character */
            c = *src++;
            if (--size > 0)
                *tok++ = c;
        }
    }

    /* terminate the token and exit */
    if (*src)
        ++src;
    *tok = 0;
    return src;
}

/*
 * get a macro line argument
 *
 * char *tok;		buffer to place argument
 */
Task<int> macarg(char *tok)
{
    int savcle; /* buffer to store original clexec */
    int status;

    savcle                 = executing_command_line; /* save execution mode */
    executing_command_line = TRUE;                   /* get the argument */
    status                 = co_await nextarg("", tok, NSTRING, char_to_keycode('\n'));
    executing_command_line = savcle; /* restore execution mode */
    co_return status;
}

/*
 * nextarg:
 *	get the next argument
 *
 * char *prompt;		prompt to use if we must be interactive
 * char *buffer;		buffer to put token into
 * int size;			size of the buffer
 * int terminator;		terminating char to be used on interactive fetch
 */
Task<int> nextarg(char *prompt, char *buffer, int size, int terminator)
{
    /* if we are interactive, go get it! */
    if (executing_command_line == FALSE)
        co_return co_await getstring(prompt, buffer, size, terminator);

    /* grab token and advance past */
    command_string = token(command_string, buffer, size);

    /* evaluate it */
    co_await getval(buffer, buffer, size);
    co_return TRUE;
}

/*
 * storemac:
 *	Set up a macro buffer and flag to store all
 *	executed command lines there
 *
 * int f;		default flag
 * int n;		macro number to use
 */
Task<int> cmd_store_macro(int f, int n)
{
    struct buffer *bp; /* pointer to macro buffer */
    char bname[NBUFN]; /* name of buffer to use */

    /* must have a numeric argument to this function */
    if (f == FALSE) {
        msg_printf("No macro specified");
        co_return FALSE;
    }

    /* range check the macro number */
    if (n < 1 || n > 40) {
        msg_printf("Macro number out of range");
        co_return FALSE;
    }

    /* construct the macro buffer name */
    strcpy(bname, "*Macro xx*");
    bname[7] = '0' + (n / 10);
    bname[8] = '0' + (n % 10);

    /* set up the new macro buffer */
    if ((bp = find_buffer(bname, TRUE, BFINVS)) == NULL) {
        msg_printf("Can not create macro");
        co_return FALSE;
    }

    /* and make sure it is empty */
    co_await clear_buffer(bp);

    /* and set the macro store pointers to it */
    storing_macro = TRUE;
    store_buffer  = bp;
    co_return TRUE;
}

/*
 * storeproc:
 *	Set up a procedure buffer and flag to store all
 *	executed command lines there
 *
 * int f;		default flag
 * int n;		macro number to use
 */
Task<int> cmd_store_procedure(int f, int n)
{
    struct buffer *bp; /* pointer to macro buffer */
    int status;        /* return status */
    char bname[NBUFN]; /* name of buffer to use */

    /* a numeric argument means its a numbered macro */
    if (f == TRUE)
        co_return co_await cmd_store_macro(f, n);

    /* get the name of the procedure */
    if ((status = co_await ask_string("Procedure name: ", &bname[1], NBUFN - 2)) != TRUE)
        co_return status;

    /* construct the macro buffer name */
    bname[0] = '*';
    strcat(bname, "*");

    /* set up the new macro buffer */
    if ((bp = find_buffer(bname, TRUE, BFINVS)) == NULL) {
        msg_printf("Can not create macro");
        co_return FALSE;
    }

    /* and make sure it is empty */
    co_await clear_buffer(bp);

    /* and set the macro store pointers to it */
    storing_macro = TRUE;
    store_buffer  = bp;
    co_return TRUE;
}

/*
 * execproc:
 *	Execute a procedure
 *
 * int f, n;		default flag and numeric arg
 */
Task<int> cmd_execute_procedure(int f, int n)
{
    struct buffer *bp;    /* ptr to buffer to execute */
    int status;           /* status return */
    char bufn[NBUFN + 2]; /* name of buffer to execute */

    /* find out what buffer the user wants to execute */
    if ((status = co_await ask_string("Execute procedure: ", &bufn[1], NBUFN)) != TRUE)
        co_return status;

    /* construct the buffer name */
    bufn[0] = '*';
    strcat(bufn, "*");

    /* find the pointer to that buffer */
    if ((bp = find_buffer(bufn, FALSE, 0)) == NULL) {
        msg_printf("No such procedure");
        co_return FALSE;
    }

    /* and now execute it as asked */
    while (n-- > 0)
        if ((status = co_await dobuf(bp)) != TRUE)
            co_return status;
    co_return TRUE;
}

/*
 * execbuf:
 *	Execute the contents of a buffer of commands
 *
 * int f, n;		default flag and numeric arg
 */
Task<int> cmd_execute_buffer(int f, int n)
{
    struct buffer *bp;  /* ptr to buffer to execute */
    int status;         /* status return */
    char bufn[NSTRING]; /* name of buffer to execute */

    /* find out what buffer the user wants to execute */
    if ((status = co_await ask_string("Execute buffer: ", bufn, NBUFN)) != TRUE)
        co_return status;

    /* find the pointer to that buffer */
    if ((bp = find_buffer(bufn, FALSE, 0)) == NULL) {
        msg_printf("No such buffer");
        co_return FALSE;
    }

    /* and now execute it as asked */
    while (n-- > 0)
        if ((status = co_await dobuf(bp)) != TRUE)
            co_return status;
    co_return TRUE;
}

/*
 * dobuf:
 *	execute the contents of the buffer pointed to
 *	by the passed BP
 *
 *	Directives start with a "!" and include:
 *
 *	!endm		End a macro
 *	!if (cond)	conditional execution
 *	!else
 *	!endif
 *	!return		Return (terminating current macro)
 *	!goto <label>	Jump to a label in the current macro
 *	!force		Force macro to continue...even if command fails
 *	!while (cond)	Execute a loop if the condition is true
 *	!endwhile
 *
 *	Line Labels begin with a "*" as the first nonblank char, like:
 *
 *	*LBL01
 *
 * struct buffer *bp;		buffer to execute
 */
Task<int> dobuf(struct buffer *bp)
{
    int status;                  /* status return */
    struct line *lp;             /* pointer to line to execute */
    struct line *hlp;            /* pointer to line header */
    struct line *glp;            /* line to goto */
    struct line *mp;             /* Macro line storage temp */
    int dirnum;                  /* directive index */
    int linlen;                  /* length of line to execute */
    int i;                       /* index */
    int force;                   /* force TRUE result? */
    struct window *wp;           /* ptr to windows to scan */
    struct while_block *whlist;  /* ptr to !WHILE list */
    struct while_block *scanner; /* ptr during scan */
    struct while_block *whtemp;  /* temporary ptr to a struct while_block */
    char *einit;                 /* initial value of eline */
    char *eline;                 /* text of line to execute */
    char tkn[NSTRING];           /* buffer to evaluate an expresion in */

    /* clear IF level flags/while ptr */
    if_level = 0;
    whlist   = NULL;
    scanner  = NULL;

    /* scan the buffer to execute, building WHILE header blocks */
    hlp = bp->b_linep;
    lp  = hlp->l_fp;
    while (lp != hlp) {
        /* scan the current line */
        eline = lp->l_text;
        i     = lp->l_used;

        /* trim leading whitespace */
        while (i-- > 0 && (*eline == ' ' || *eline == '\t'))
            ++eline;

        /* if theres nothing here, don't bother */
        if (i <= 0)
            goto nxtscan;

        /* if is a while directive, make a block... */
        if (eline[0] == '!' && eline[1] == 'w' && eline[2] == 'h') {
            whtemp = (struct while_block *)malloc(sizeof(struct while_block));
            if (whtemp == NULL) {
            noram:
                msg_printf("%%Out of memory during while scan");
            failexit:
                freewhile(scanner);
                freewhile(whlist);
                co_return FALSE;
            }
            whtemp->w_begin = lp;
            whtemp->w_type  = BTWHILE;
            whtemp->w_next  = scanner;
            scanner         = whtemp;
        }

        /* if is a BREAK directive, make a block... */
        if (eline[0] == '!' && eline[1] == 'b' && eline[2] == 'r') {
            if (scanner == NULL) {
                msg_printf("%%!BREAK outside of any !WHILE loop");
                goto failexit;
            }
            whtemp = (struct while_block *)malloc(sizeof(struct while_block));
            if (whtemp == NULL)
                goto noram;
            whtemp->w_begin = lp;
            whtemp->w_type  = BTBREAK;
            whtemp->w_next  = scanner;
            scanner         = whtemp;
        }

        /* if it is an endwhile directive, record the spot... */
        if (eline[0] == '!' && strncmp(&eline[1], "endw", 4) == 0) {
            if (scanner == NULL) {
                msg_printf("%%!ENDWHILE with no preceding !WHILE in '%s'", bp->b_bname);
                goto failexit;
            }
            /* move top records from the scanner list to the
               whlist until we have moved all BREAK records
               and one WHILE record */
            do {
                scanner->w_end = lp;
                whtemp         = whlist;
                whlist         = scanner;
                scanner        = scanner->w_next;
                whlist->w_next = whtemp;
            } while (whlist->w_type == BTBREAK);
        }

    nxtscan: /* on to the next line */
        lp = lp->l_fp;
    }

    /* while and endwhile should match! */
    if (scanner != NULL) {
        msg_printf("%%!WHILE with no matching !ENDWHILE in '%s'", bp->b_bname);
        goto failexit;
    }

    /* let the first command inherit the flags from the last one.. */
    thisflag = lastflag;

    /* starting at the beginning of the buffer */
    hlp = bp->b_linep;
    lp  = hlp->l_fp;
    while (lp != hlp) {
        co_await exec_yield();

        /* allocate eline and copy macro line to it */
        linlen = lp->l_used;
        if ((einit = eline = (char *)malloc(linlen + 1)) == NULL) {
            msg_printf("%%Out of Memory during macro execution");
            freewhile(whlist);
            co_return FALSE;
        }
        strncpy(eline, lp->l_text, linlen);
        eline[linlen] = 0; /* make sure it ends */

        /* trim leading whitespace */
        while (*eline == ' ' || *eline == '\t')
            ++eline;

        /* dump comments and blank lines */
        if (*eline == ';' || *eline == 0)
            goto onward;

        /* Parse directives here.... */
        dirnum = -1;
        if (*eline == '!') {
            /* Find out which directive this is */
            ++eline;
            for (dirnum = 0; dirnum < NUMDIRS; dirnum++)
                if (strncmp(eline, directive_names[dirnum], strlen(directive_names[dirnum])) == 0)
                    break;

            /* and bitch if it's illegal */
            if (dirnum == NUMDIRS) {
                msg_printf("%%Unknown Directive");
                freewhile(whlist);
                co_return FALSE;
            }

            /* service only the !ENDM macro here */
            if (dirnum == DENDM) {
                storing_macro = FALSE;
                store_buffer  = NULL;
                goto onward;
            }

            /* restore the original eline.... */
            --eline;
        }

        /* if macro store is on, just salt this away */
        if (storing_macro) {
            /* allocate the space for the line */
            linlen = strlen(eline);
            if ((mp = line_alloc(linlen)) == NULL) {
                msg_printf("Out of memory while storing macro");
                co_return FALSE;
            }

            /* copy the text into the new line */
            for (i = 0; i < linlen; ++i)
                lputc(mp, i, eline[i]);

            /* attach the line to the end of the buffer */
            store_buffer->b_linep->l_bp->l_fp = mp;
            mp->l_bp                          = store_buffer->b_linep->l_bp;
            store_buffer->b_linep->l_bp       = mp;
            mp->l_fp                          = store_buffer->b_linep;
            goto onward;
        }

        force = FALSE;

        /* dump comments */
        if (*eline == '*')
            goto onward;

        /* now, execute directives */
        if (dirnum != -1) {
            /* skip past the directive */
            while (*eline && *eline != ' ' && *eline != '\t')
                ++eline;
            command_string = eline;

            switch (dirnum) {
            case DIF: /* IF directive */
                /* grab the value of the logical exp */
                if (if_level == 0) {
                    if (co_await macarg(tkn) != TRUE)
                        goto eexec;
                    if (truth_value(tkn) == FALSE)
                        ++if_level;
                } else
                    ++if_level;
                goto onward;

            case DWHILE: /* WHILE directive */
                /* grab the value of the logical exp */
                if (if_level == 0) {
                    if (co_await macarg(tkn) != TRUE)
                        goto eexec;
                    if (truth_value(tkn) == TRUE)
                        goto onward;
                }
                /* drop down and act just like !BREAK */

            case DBREAK: /* BREAK directive */
                if (dirnum == DBREAK && if_level)
                    goto onward;

                /* jump down to the endwhile */
                /* find the right while loop */
                whtemp = whlist;
                while (whtemp) {
                    if (whtemp->w_begin == lp)
                        break;
                    whtemp = whtemp->w_next;
                }

                if (whtemp == NULL) {
                    msg_printf("%%Internal While loop error");
                    freewhile(whlist);
                    co_return FALSE;
                }

                /* reset the line pointer back.. */
                lp = whtemp->w_end;
                goto onward;

            case DELSE: /* ELSE directive */
                if (if_level == 1)
                    --if_level;
                else if (if_level == 0)
                    ++if_level;
                goto onward;

            case DENDIF: /* ENDIF directive */
                if (if_level)
                    --if_level;
                goto onward;

            case DGOTO: /* GOTO directive */
                /* .....only if we are currently executing */
                if (if_level == 0) {
                    /* grab label to jump to */
                    eline  = token(eline, goto_label, NPAT);
                    linlen = strlen(goto_label);
                    glp    = hlp->l_fp;
                    while (glp != hlp) {
                        if (*glp->l_text == '*' &&
                            (strncmp(&glp->l_text[1], goto_label, linlen) == 0)) {
                            lp = glp;
                            goto onward;
                        }
                        glp = glp->l_fp;
                    }
                    msg_printf("%%No such label");
                    freewhile(whlist);
                    co_return FALSE;
                }
                goto onward;

            case DRETURN: /* RETURN directive */
                if (if_level == 0)
                    goto eexec;
                goto onward;

            case DENDWHILE: /* ENDWHILE directive */
                if (if_level) {
                    --if_level;
                    goto onward;
                } else {
                    /* find the right while loop */
                    whtemp = whlist;
                    while (whtemp) {
                        if (whtemp->w_type == BTWHILE && whtemp->w_end == lp)
                            break;
                        whtemp = whtemp->w_next;
                    }

                    if (whtemp == NULL) {
                        msg_printf("%%Internal While loop error");
                        freewhile(whlist);
                        co_return FALSE;
                    }

                    /* reset the line pointer back.. */
                    lp = whtemp->w_begin->l_bp;
                    goto onward;
                }

            case DFORCE: /* FORCE directive */
                force = TRUE;
            }
        }

        /* execute the statement */
        status = co_await docmd(eline);
        if (force) /* force the status */
            status = TRUE;

        /* check for a command error */
        if (status != TRUE) {
            /* look if buffer is showing */
            wp = window_head;
            while (wp != NULL) {
                if (wp->w_bufp == bp) {
                    /* and point it */
                    wp->w_dotp = lp;
                    wp->w_doto = 0;
                    wp->w_flag |= WFHARD;
                }
                wp = wp->w_wndp;
            }
            /* in any case set the buffer . */
            bp->b_dotp = lp;
            bp->b_doto = 0;
            free(einit);
            if_level = 0;
            freewhile(whlist);
            co_return status;
        }

    onward: /* on to the next line */
        free(einit);
        lp = lp->l_fp;
    }

eexec: /* exit the current function */
    if_level = 0;
    freewhile(whlist);
    co_return TRUE;
}

/*
 * free a list of while block pointers
 *
 * struct while_block *wp;		head of structure to free
 */
void freewhile(struct while_block *wp)
{
    if (wp == NULL)
        return;
    if (wp->w_next)
        freewhile(wp->w_next);
    free(wp);
}

/*
 * execute a series of commands in a file
 *
 * int f, n;		default flag and numeric arg to pass on to file
 */
Task<int> cmd_execute_file(int f, int n)
{
    int status;          /* return status of name query */
    char fname[NSTRING]; /* name of file to execute */
    char *fspec;         /* full file spec */

    if ((status = co_await ask_string("File to execute: ", fname, NSTRING - 1)) != TRUE)
        co_return status;

    /* look up the path for the file */
    fspec = co_await lookup_file(fname, FALSE); /* used to by TRUE, P.K. */

    /* if it isn't around */
    if (fspec == NULL)
        co_return FALSE;

    /* otherwise, execute it */
    while (n-- > 0)
        if ((status = co_await dofile(fspec)) != TRUE)
            co_return status;

    co_return TRUE;
}

/*
 * dofile:
 *	yank a file into a buffer and execute it
 *	if there are no errors, delete the buffer on exit
 *
 * char *fname;		file name to execute
 */
Task<int> dofile(char *fname)
{
    struct buffer *bp; /* buffer to place file to exeute */
    struct buffer *cb; /* temp to hold current buf while we read */
    int status;        /* results of various calls */
    char bname[NBUFN]; /* name of buffer */

    makename(bname, fname);                         /* derive the name of the buffer */
    unique_buffer_name(bname);                      /* make sure we don't stomp things */
    if ((bp = find_buffer(bname, TRUE, 0)) == NULL) /* get the needed buffer */
        co_return FALSE;

    bp->b_mode = MDVIEW; /* mark the buffer as read only */
    cb         = curbp;  /* save the old buffer */
    curbp      = bp;     /* make this one current */
    /* and try to read in the file to execute */
    if ((status = co_await readin(fname, FALSE)) != TRUE) {
        curbp = cb; /* restore the current buffer */
        co_return status;
    }

    /* go execute it! */
    curbp = cb; /* restore the current buffer */
    if ((status = co_await dobuf(bp)) != TRUE)
        co_return status;

    /* if not displayed, remove the now unneeded buffer and exit */
    if (bp->b_nwnd == 0)
        co_await destroy_buffer(bp);
    co_return TRUE;
}

/*
 * execute_numbered_macro:
 *	Execute the contents of a numbered buffer
 *
 * int f, n;		default flag and numeric arg
 * int number;		number of buffer to execute
 */
Task<int> execute_numbered_macro(int f, int n, int number)
{
    struct buffer *bp; /* ptr to buffer to execute */
    int status;        /* status return */
    static char bufname[] = "*Macro xx*";

    /* make the buffer name */
    bufname[7] = '0' + (number / 10);
    bufname[8] = '0' + (number % 10);

    /* find the pointer to that buffer */
    if ((bp = find_buffer(bufname, FALSE, 0)) == NULL) {
        msg_printf("Macro not defined");
        co_return FALSE;
    }

    /* and now execute it as asked */
    while (n-- > 0)
        if ((status = co_await dobuf(bp)) != TRUE)
            co_return status;
    co_return TRUE;
}

Task<int> cmd_execute_macro_1(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 1);
}

Task<int> cmd_execute_macro_2(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 2);
}

Task<int> cmd_execute_macro_3(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 3);
}

Task<int> cmd_execute_macro_4(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 4);
}

Task<int> cmd_execute_macro_5(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 5);
}

Task<int> cmd_execute_macro_6(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 6);
}

Task<int> cmd_execute_macro_7(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 7);
}

Task<int> cmd_execute_macro_8(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 8);
}

Task<int> cmd_execute_macro_9(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 9);
}

Task<int> cmd_execute_macro_10(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 10);
}

Task<int> cmd_execute_macro_11(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 11);
}

Task<int> cmd_execute_macro_12(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 12);
}

Task<int> cmd_execute_macro_13(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 13);
}

Task<int> cmd_execute_macro_14(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 14);
}

Task<int> cmd_execute_macro_15(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 15);
}

Task<int> cmd_execute_macro_16(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 16);
}

Task<int> cmd_execute_macro_17(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 17);
}

Task<int> cmd_execute_macro_18(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 18);
}

Task<int> cmd_execute_macro_19(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 19);
}

Task<int> cmd_execute_macro_20(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 20);
}

Task<int> cmd_execute_macro_21(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 21);
}

Task<int> cmd_execute_macro_22(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 22);
}

Task<int> cmd_execute_macro_23(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 23);
}

Task<int> cmd_execute_macro_24(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 24);
}

Task<int> cmd_execute_macro_25(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 25);
}

Task<int> cmd_execute_macro_26(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 26);
}

Task<int> cmd_execute_macro_27(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 27);
}

Task<int> cmd_execute_macro_28(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 28);
}

Task<int> cmd_execute_macro_29(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 29);
}

Task<int> cmd_execute_macro_30(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 30);
}

Task<int> cmd_execute_macro_31(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 31);
}

Task<int> cmd_execute_macro_32(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 32);
}

Task<int> cmd_execute_macro_33(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 33);
}

Task<int> cmd_execute_macro_34(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 34);
}

Task<int> cmd_execute_macro_35(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 35);
}

Task<int> cmd_execute_macro_36(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 36);
}

Task<int> cmd_execute_macro_37(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 37);
}

Task<int> cmd_execute_macro_38(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 38);
}

Task<int> cmd_execute_macro_39(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 39);
}

Task<int> cmd_execute_macro_40(int f, int n)
{
    co_return co_await execute_numbered_macro(f, n, 40);
}
