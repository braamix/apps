/*	input.c
 *
 *	Various input routines
 *
 *	written by Daniel Lawrence 5/9/86
 *	modified by Petri Kutvonen
 */

#include "estruct.h"
#include "globals.h"
#include "efunc.h"

#include "proc/io.h"

/*
 * Ask a yes or no question in the message line. Return either TRUE, FALSE, or
 * ABORT. The ABORT status is returned if the user bumps out of the question
 * with a ^G. Used any time a confirmation is required.
 */
Task<int> ask_yesno(char *prompt)
{
    char c;         /* input character */
    char buf[NPAT]; /* prompt to user */

    for (;;) {
        /* build and prompt the user */
        strcpy(buf, prompt);
        strcat(buf, " (y/n)? ");
        msg_puts(buf);

        /* get the responce */
        c = co_await tgetc();

        if (c == keycode_to_char(abort_char)) /* Bail out! */
            co_return ABORT;

        if (c == 'y' || c == 'Y')
            co_return TRUE;

        if (c == 'n' || c == 'N')
            co_return FALSE;
    }
}

/*
 * Write a prompt into the message line, then read back a response. Keep
 * track of the physical position of the cursor. If we are in a keyboard
 * macro throw the prompt away, and return the remembered response. This
 * lets macros run at full speed. The reply is always terminated by a carriage
 * return. Handle erase, kill, and abort keys.
 */

Task<int> ask_string(char *prompt, char *buf, int nbuf)
{
    co_return co_await nextarg(prompt, buf, nbuf, char_to_keycode('\n'));
}

Task<int> ask_string_until(char *prompt, char *buf, int nbuf, int eolchar)
{
    co_return co_await nextarg(prompt, buf, nbuf, eolchar);
}

/*
 * keycode_to_char:
 *	expanded character to character
 *	collapse the CONTROL and SPEC flags back into an ascii code
 */
int keycode_to_char(int c)
{
    if (c & CONTROL)
        c = c & ~(CONTROL | 0x40);
    if (c & SPEC)
        c = c & 255;
    return c;
}

/*
 * char_to_keycode:
 *	character to extended character
 *	pull out the CONTROL and SPEC prefixes (if possible)
 */
int char_to_keycode(int c)
{
    if (c >= 0x00 && c <= 0x1F)
        c = CONTROL | (c + '@');
    return c;
}

/*
 * get a command name from the command line. Command completion means
 * that pressing a <SPACE> will attempt to complete an unfinished command
 * name if it is unique.
 */
Task<fn_t> getname(void)
{
    int cpos; /* current column on screen output */
    int c;
    char *sp;               /* pointer to string for output */
    struct name_bind *ffp;  /* first ptr to entry in name binding table */
    struct name_bind *cffp; /* current ptr to entry in name binding table */
    struct name_bind *lffp; /* last ptr to entry in name binding table */
    char buf[NSTRING];      /* buffer to hold tentative command name */

    /* starting at the beginning of the string buffer */
    cpos = 0;

    /* if we are executing a command line get the next arg and match it */
    if (executing_command_line) {
        if (co_await macarg(buf) != TRUE)
            co_return NULL;
        co_return fncmatch(&buf[0]);
    }

    /* build a name string from the keyboard */
    while (TRUE) {
        c = co_await tgetc();

        /* if we are at the end, just match it */
        if (c == 0x0d) {
            buf[cpos] = 0;

            /* and match it off */
            co_return fncmatch(&buf[0]);

        } else if (c == keycode_to_char(abort_char)) { /* Bell, abort */
            co_await cmd_abort_command(FALSE, 0);
            ttflush();
            co_return NULL;

        } else if (c == 0x7F || c == 0x08) { /* rubout/erase */
            if (cpos != 0) {
                ttputc('\b');
                ttputc(' ');
                ttputc('\b');
                --shown_col;
                --cpos;
                ttflush();
            }

        } else if (c == 0x15) { /* C-U, kill */
            while (cpos != 0) {
                ttputc('\b');
                ttputc(' ');
                ttputc('\b');
                --cpos;
                --shown_col;
            }

            ttflush();

        } else if (c == ' ' || c == 0x1b || c == 0x09) {
            /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
            /* attempt a completion */
            buf[cpos] = 0;         /* terminate it for us */
            ffp       = &names[0]; /* scan for matches */
            while (ffp->n_func != NULL) {
                if (strncmp(buf, ffp->n_name, strlen(buf)) == 0) {
                    /* a possible match! More than one? */
                    if ((ffp + 1)->n_func == NULL ||
                        (strncmp(buf, (ffp + 1)->n_name, strlen(buf)) != 0)) {
                        /* no...we match, print it */
                        sp = ffp->n_name + cpos;
                        while (*sp)
                            ttputc(*sp++);
                        ttflush();
                        co_return ffp->n_func;
                    } else {
                        /* << << << << << << << << << << << << << << << << << */
                        /* try for a partial match against the list */

                        /* first scan down until we no longer match the current input */
                        lffp = (ffp + 1);
                        while ((lffp + 1)->n_func != NULL) {
                            if (strncmp(buf, (lffp + 1)->n_name, strlen(buf)) != 0)
                                break;
                            ++lffp;
                        }

                        /* and now, attempt to partial complete the string, char at a time */
                        while (TRUE) {
                            /* add the next char in */
                            buf[cpos] = ffp->n_name[cpos];

                            /* scan through the candidates */
                            cffp = ffp + 1;
                            while (cffp <= lffp) {
                                if (cffp->n_name[cpos] != buf[cpos])
                                    goto onward;
                                ++cffp;
                            }

                            /* add the character */
                            ttputc(buf[cpos++]);
                        }
                        /* << << << << << << << << << << << << << << << << << */
                    }
                }
                ++ffp;
            }

            /* no match.....beep and onward */
            tcapbeep();
        onward:;
            ttflush();
            /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
        } else {
            if (cpos < NSTRING - 1 && c > ' ') {
                buf[cpos++] = c;
                ttputc(c);
            }

            ++shown_col;
            ttflush();
        }
    }
}

/*	tgetc:	Get a key from the terminal driver, resolve any keyboard
                macro action					*/

Task<int> tgetc(void)
{
    int c; /* fetched character */

    /* if we are playing a keyboard macro back, */
    if (keyboard_macro_mode == PLAY) {
        /* if there is some left... */
        if (keyboard_macro_pos < keyboard_macro_end)
            co_return (int) * keyboard_macro_pos++;

        /* at the end of last repitition? */
        if (--keyboard_macro_repeat < 1) {
            keyboard_macro_mode = STOP;
            /* force a screen update after all is done */
            co_await update();
        } else {
            /* reset the macro to the begining for the next rep */
            keyboard_macro_pos = &keyboard_macro[0];
            co_return (int) * keyboard_macro_pos++;
        }
    }

    /* fetch a character from the terminal driver */
    c = co_await ttgetc();

    /* record it for $lastkey */
    last_key = c;

    /* save it if we need to */
    if (keyboard_macro_mode == RECORD) {
        *keyboard_macro_pos++ = c;
        keyboard_macro_end    = keyboard_macro_pos;

        /* don't overrun the buffer */
        if (keyboard_macro_pos == &keyboard_macro[NKBDM - 1]) {
            keyboard_macro_mode = STOP;
            tcapbeep();
        }
    }

    /* and finally give the char back */
    co_return c;
}

/*	GET1KEY:	Get one keystroke. The only prefixs legal here
                        are the SPEC and CONTROL prefixes.
                                                                */

Task<int> get1key(void)
{
    int c;

    /* get a keystroke */
    c = co_await tgetc();

    if (c >= 0x00 && c <= 0x1F) /* C0 control -> C-     */
        c = CONTROL | (c + '@');
    co_return c;
}

/*	GETCMD:	Get a command from the keyboard. Process all applicable
                prefix keys
                                                        */
Task<int> getcmd(void)
{
    int c; /* fetched keystroke */
    int d; /* second character P.K. */
    int cmask = 0;
    /* get initial character */
    c = co_await get1key();

proc_metac:
    if (c == 128 + 27) /* CSI */
        goto handle_CSI;
    /* process META prefix */
    if (c == (CONTROL | '[')) {
        c = co_await get1key();
        if (c == '[' || c == 'O') { /* CSI P.K. */
        handle_CSI:
            c = co_await get1key();
            if (c >= 'A' && c <= 'D')
                co_return SPEC | c | cmask;
            if (c >= 'E' && c <= 'z' && c != 'i' && c != 'c')
                co_return SPEC | c | cmask;
            d = co_await get1key();
            if (d == '~') /* ESC [ n ~   P.K. */
                co_return SPEC | c | cmask;
            switch (c) { /* ESC [ n n ~ P.K. */
            case '1':
                c = d + 32;
                break;
            case '2':
                c = d + 48;
                break;
            case '3':
                c = d + 64;
                break;
            default:
                c = '?';
                break;
            }
            if (d != '~') /* eat tilde P.K. */
                co_await get1key();
            if (c == 'i') { /* DO key    P.K. */
                c = ctlx_char;
                goto proc_ctlxc;
            } else if (c == 'c') /* ESC key   P.K. */
                c = co_await get1key();
            else
                co_return SPEC | c | cmask;
        }
        if (c == (CONTROL | '[')) {
            cmask = META;
            goto proc_metac;
        }
        if (islower(c)) /* Force to upper */
            c ^= DIFCASE;
        if (c >= 0x00 && c <= 0x1F) /* control key */
            c = CONTROL | (c + '@');
        co_return META | c;
    } else if (c == meta_char) {
        c = co_await get1key();
        if (c == (CONTROL | '[')) {
            cmask = META;
            goto proc_metac;
        }
        if (islower(c)) /* Force to upper */
            c ^= DIFCASE;
        if (c >= 0x00 && c <= 0x1F) /* control key */
            c = CONTROL | (c + '@');
        co_return META | c;
    }

proc_ctlxc:
    /* process CTLX prefix */
    if (c == ctlx_char) {
        c = co_await get1key();
        if (c == (CONTROL | '[')) {
            cmask = CTLX;
            goto proc_metac;
        }
        if (c >= 'a' && c <= 'z') /* Force to upper */
            c -= 0x20;
        if (c >= 0x00 && c <= 0x1F) /* control key */
            c = CONTROL | (c + '@');
        co_return CTLX | c;
    }

    /* otherwise, just return it */
    co_return c;
}

/*
 * The nth name in the directory `path` names that begins with the last
 * component of `path`, written into `out` with that directory back on the
 * front.  Answers its length, or 0 when there is no nth one.
 *
 * Upstream forked a shell to echo the glob into a temporary file and read the
 * words back out of it.  list_dir() answers the same names with neither, so
 * the temporary file, mkstemp and the shell's globbing are all gone; the one
 * thing lost with them is a * or ? typed into the prompt, which was the
 * shell's to expand and is nobody's now.
 */
static Task<int> complete_name(char *path, int nth, char *out, int outsize)
{
    char *slash             = strrchr(path, '/');
    const char *stem        = slash ? slash + 1 : path;
    usize stemlen           = strlen(stem);
    int lead                = slash ? (int)(slash - path) + 1 : 0;
    Str dir                 = slash ? Str(path, (usize)(slash - path)) : Str(".", 1);
    Result<Vec<DirEntry>> r = Err(Error::NoMemory);

    if (slash == path) /* "/foo" is the root, not "" */
        dir = Str("/", 1);
    if (Task<Result<Vec<DirEntry>>> t = list_dir(dir))
        r = co_await t;
    if (r.is_err())
        co_return 0;

    for (const DirEntry &e : r.value()) {
        Str nm = e.name.str();
        int k  = lead;
        usize i;

        if (nm.size() < stemlen || memcmp(nm.data(), stem, stemlen) != 0)
            continue;
        if (nth-- > 0)
            continue;
        for (i = 0; i < nm.size() && k < outsize - 1; i++)
            out[k++] = nm.data()[i];
        out[k] = 0;
        co_return k;
    }
    co_return 0;
}

/*	A more generalized prompt/reply function allowing the caller
        to specify the proper terminator. If the terminator is not
        a return ('\n') it will echo as "<NL>"
                                                        */
Task<int> getstring(char *prompt, char *buf, int nbuf, int eolchar)
{
    int cpos; /* current character position in string */
    int c;
    int quotef; /* are we quoting the next char? */
    int ffile, ocpos, nskip = 0, didtry = 0;
    char stem[NFILEN]; /* what was typed, to fall back on */

    ffile = (strcmp(prompt, "Find file: ") == 0 || strcmp(prompt, "View file: ") == 0 ||
             strcmp(prompt, "Insert file: ") == 0 || strcmp(prompt, "Write file: ") == 0 ||
             strcmp(prompt, "Read file: ") == 0 || strcmp(prompt, "File to execute: ") == 0);

    cpos   = 0;
    quotef = FALSE;

    /* prompt the user for the input string */
    msg_puts(prompt);

    for (;;) {
        if (!didtry)
            nskip = -1;
        didtry = 0;
        /* get a character from the user */
        c = co_await get1key();

        /* If it is a <ret>, change it to a <NL> */
        if (c == (CONTROL | 0x4d) && !quotef)
            c = CONTROL | 0x40 | '\n';

        /* if they hit the line terminate, wrap it up */
        if (c == eolchar && quotef == FALSE) {
            buf[cpos++] = 0;

            /* clear the message line */
            msg_printf("");
            ttflush();

            /* if we default the buffer, return FALSE */
            if (buf[0] == 0)
                co_return FALSE;

            co_return TRUE;
        }

        /* change from command form back to character form */
        c = keycode_to_char(c);

        if (c == keycode_to_char(abort_char) && quotef == FALSE) {
            /* Abort the input? */
            co_await cmd_abort_command(FALSE, 0);
            ttflush();
            co_return ABORT;
        } else if ((c == 0x7F || c == 0x08) && quotef == FALSE) {
            /* rubout/erase */
            if (cpos != 0) {
                outstring("\b \b");
                --shown_col;

                if (buf[--cpos] < 0x20) {
                    outstring("\b \b");
                    --shown_col;
                }
                if (buf[cpos] == '\n') {
                    outstring("\b\b  \b\b");
                    shown_col -= 2;
                }

                ttflush();
            }

        } else if (c == 0x15 && quotef == FALSE) {
            /* C-U, kill */
            while (cpos != 0) {
                outstring("\b \b");
                --shown_col;

                if (buf[--cpos] < 0x20) {
                    outstring("\b \b");
                    --shown_col;
                }
                if (buf[cpos] == '\n') {
                    outstring("\b\b  \b\b");
                    shown_col -= 2;
                }
            }
            ttflush();

        } else if ((c == 0x09 || c == ' ') && quotef == FALSE && ffile) {
            /* TAB, complete file name */
            int n;

            didtry = 1;
            ocpos  = cpos;
            while (cpos != 0) {
                outstring("\b \b");
                --shown_col;

                if (buf[--cpos] < 0x20) {
                    outstring("\b \b");
                    --shown_col;
                }
                if (buf[cpos] == '\n') {
                    outstring("\b\b  \b\b");
                    shown_col -= 2;
                }
            }
            ttflush();

            /*
             * The first TAB after anything else was typed takes
             * what is there as the stem; the ones after it offer
             * the next name against that same stem rather than
             * against the name they just put up.
             */
            if (nskip < 0) {
                buf[ocpos] = 0;
                strcpy(stem, buf);
                nskip = 0;
            }

            /*
             * Offer the nskip'th name and count on; when they run
             * out, beep, put the stem back, and start over at the
             * first on the next press.
             */
            strcpy(buf, stem);
            n = co_await complete_name(stem, nskip++, buf, nbuf);
            if (n == 0) {
                tcapbeep();
                nskip = 0;
                strcpy(buf, stem);
                n = strlen(buf);
            }
            cpos = n;

            for (n = 0; n < cpos; n++) {
                c = buf[n];
                if ((c < ' ') && (c != '\n')) {
                    outstring("^");
                    ++shown_col;
                    c ^= 0x40;
                }

                if (c != '\n') {
                    if (display_input)
                        ttputc(c);
                } else { /* put out <NL> for <ret> */
                    outstring("<NL>");
                    shown_col += 3;
                }
                ++shown_col;
            }
            ttflush();

        } else if ((c == quote_char || c == 0x16) && quotef == FALSE) {
            quotef = TRUE;
        } else {
            quotef = FALSE;
            if (cpos < nbuf - 1) {
                buf[cpos++] = c;

                if ((c < ' ') && (c != '\n')) {
                    outstring("^");
                    ++shown_col;
                    c ^= 0x40;
                }

                if (c != '\n') {
                    if (display_input)
                        ttputc(c);
                } else { /* put out <NL> for <ret> */
                    outstring("<NL>");
                    shown_col += 3;
                }
                ++shown_col;
                ttflush();
            }
        }
    }
}

/*
 * output a string of characters
 *
 * char *s;		string to output
 */
void outstring(char *s)
{
    if (display_input)
        while (*s)
            ttputc(*s++);
}

/*
 * output a string of output characters
 *
 * char *s;		string to output
 */
void ostring(char *s)
{
    if (display_commands)
        while (*s)
            ttputc(*s++);
}
