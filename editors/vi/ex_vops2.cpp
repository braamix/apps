/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

/*
 * Low level routines for operations sequences,
 * and mostly, insert mode (and a subroutine
 * to read an input line, including in the echo area.)
 */

/*
 * Obleeperate characters in hardcopy
 * open with \'s.
 */
void bleep(int i, char *cp)
{
    i -= column(cp);
    do
        putchar('\\' | QUOTE);
    while (--i >= 0);
    rubble = 1;
}

/*
 * Common code for middle part of exdelete
 * and change operating on parts of lines.
 */
int vdcMID(void)
{
    char *cp;

    squish();
    setLAST();
    if (FIXUNDO)
        vundkind = VCHNG, CP(vutmp, linebuf);
    if (wcursor < cursor)
        cp = wcursor, wcursor = cursor, cursor = cp;
    vUD1 = vUA1 = vUA2 = cursor;
    vUD2               = wcursor;
    return (column(wcursor - 1));
}

/*
 * Take text from linebuf and stick it
 * in the VBSIZE buffer BUF.  Used to save
 * deleted text of part of line.
 */
void takeout(char *BUF)
{
    char *cp;

    if (wcursor < linebuf)
        wcursor = linebuf;
    if (cursor == wcursor) {
        obeep();
        return;
    }
    if (wcursor < cursor) {
        cp      = wcursor;
        wcursor = cursor;
        cursor  = cp;
    }
    setBUF(BUF);
    if ((unsigned char)BUF[0] == OVERBUF)
        obeep();
}

/*
 * Are we at the end of the printed representation of the
 * line?  Used internally in hardcopy open.
 */
exbool ateopr(void)
{
    int i, c;
    int *cp = vtube[destline] + destcol;

    for (i = WCOLS - destcol; i > 0; i--) {
        c = *cp++;
        if (c == 0)
            return (1);
        if (c != ' ' && (c & QUOTE) == 0)
            return (0);
    }
    return (1);
}

/*
 * Append.
 *
 * This routine handles the top level append, doing work
 * as each new line comes in, and arranging repeatability.
 * It also handles append with repeat counts, and calculation
 * of autoindents for new lines.
 */
exbool vaifirst;
exbool gobbled;
char *ogcursor;

Task<void> vappend(int ch, int cnt, int indent)
{
    int i;
    char *gcursor;
    exbool escape;
    int repcnt, savedoomed;
    short oldhold = hold;

    /*
     * Before a move in hardopen when the line is dirty
     * or we are in the middle of the printed representation,
     * we retype the line to the left of the cursor so the
     * insert looks clean.
     */
    if (ch != 'o' && state == HARDOPEN && (rubble || !ateopr())) {
        rubble   = 1;
        gcursor  = cursor;
        i        = *gcursor;
        *gcursor = ' ';
        wcursor  = gcursor;
        vmove();
        *gcursor = i;
    }
again:
    vaifirst = indent == 0;
    /*
     * The mode line, up before the ^@ peek below parks: that is where the
     * editor waits after i is pressed and nothing has been typed yet. r takes
     * one character and is over, so it is not a mode.
     */
    inserting = ch != 'r';

    /*
     * Handle replace character by (eventually)
     * limiting the number of input characters allowed
     * in the vgetline routine.
     */
    if (ch == 'r')
        repcnt = 2;
    else
        repcnt = 0;

    /*
     * If an autoindent is specified, then
     * generate a mixture of blanks to tabs to implement
     * it and place the cursor after the indent.
     * Text read by the vgetline routine will be placed in genbuf,
     * so the indent is generated there.
     */
    if (value(AUTOINDENT) && indent != 0) {
        gcursor  = genindent(indent);
        *gcursor = 0;
        vgotoCL(qcolumn(cursor - 1, genbuf));
    } else {
        gcursor  = genbuf;
        *gcursor = 0;
        if (ch == 'o')
            vfixcurs();
    }

    /*
     * Prepare for undo.  Pointers delimit inserted portion of line.
     */
    vUA1 = vUA2 = cursor;

    /*
     * If we are not in a repeated command and a ^@ comes in
     * then this means the previous inserted text.
     * If there is none or it was too long to be saved,
     * then obeep() and also arrange to undo any damage done
     * so far (e.g. if we are a change.)
     */
    if ((vglobp && *vglobp == 0) || co_await peekbr()) {
        if ((unsigned char)INS[0] == OVERBUF) {
            obeep();
            if (!splitw)
                ungetkey('u');
            doomed    = 0;
            hold      = oldhold;
            inserting = 0;
            co_return;
        }
        /*
         * Unread input from INS.
         * An escape will be generated at end of string.
         * Hold off n^^2 type update on dumb terminals.
         */
        vglobp = INS;
        hold |= HOLDQIK;
    } else if (vglobp == 0)
        /*
         * Not a repeated command, get
         * a new inserted text for repeat.
         */
        INS[0] = 0;

    /*
     * For wrapmargin to hack away second space after a '.'
     * when the first space caused a line break we keep
     * track that this happened in gobblebl, which says
     * to gobble up a blank silently.
     */
    gobblebl = 0;

    /*
     * Text gathering loop.
     * New text goes into genbuf starting at gcursor.
     * cursor preserves place in linebuf where text will eventually go.
     */
    if (*cursor == 0 || state == CRTOPEN)
        hold |= HOLDROL;
    for (;;) {
        if (ch == 'r' && repcnt == 0)
            escape = 0;
        else {
            gcursor = co_await vgetline(repcnt, gcursor, &escape, ch);

            /*
             * After an append, stick information
             * about the ^D's and ^^D's and 0^D's in
             * the repeated text buffer so repeated
             * inserts of stuff indented with ^D as backtab's
             * can work.
             */
            if (HADUP)
                addtext("^");
            else if (HADZERO)
                addtext("0");
            while (CDCNT > 0)
                addtext("\204"), CDCNT--;
            if (gobbled)
                addtext(" ");
            addtext(ogcursor);
        }
        repcnt = 0;

        /*
         * Smash the generated and preexisting indents together
         * and generate one cleanly made out of tabs and spaces
         * if we are using autoindent.
         */
        if (!vaifirst && value(AUTOINDENT)) {
            i = fixindent(indent);
            if (!HADUP)
                indent = i;
            gcursor = strend(genbuf);
        }

        /*
         * Limit the repetition count based on maximum
         * possible line length; do output implied
         * by further count (> 1) and cons up the new line
         * in linebuf.
         */
        cnt = vmaxrep(ch, cnt);
        CP(gcursor + 1, cursor);
        do {
            CP(cursor, genbuf);
            if (cnt > 1) {
                int oldhold = hold;

                Outchar = vinschar;
                hold |= HOLDQIK;
                printf("%s", genbuf);
                hold    = oldhold;
                Outchar = vputchar;
            }
            cursor += gcursor - genbuf;
        } while (--cnt > 0);
        endim();
        vUA2 = cursor;
        if (escape != '\n')
            CP(cursor, gcursor + 1);

        /*
         * If doomed characters remain, clobber them,
         * and reopen the line to get the display exact.
         */
        if (state != HARDOPEN) {
            DEPTH(vcline) = 0;
            savedoomed    = doomed;
            if (doomed > 0) {
                int cind = cindent();

                physdc(cind, cind + doomed);
                doomed = 0;
            }
            i = vreopen(LINE(vcline), lineDOT(), vcline);
            if (ch == 'R')
                doomed = savedoomed;
        }

        /*
         * All done unless we are continuing on to another line.
         */
        if (escape != '\n')
            break;

        /*
         * Set up for the new line.
         * First save the current line, then construct a new
         * first image for the continuation line consisting
         * of any new autoindent plus the pushed ahead text.
         */
        killU();
        addtext((char *)(gobblebl ? " " : "\n"));
        vsave();
        cnt = 1;
        if (value(AUTOINDENT)) {
            if (!HADUP && vaifirst)
                indent = whitecnt(linebuf);
            vaifirst = 0;
            strcLIN(vpastwh(gcursor + 1));
            gcursor  = genindent(indent);
            *gcursor = 0;
            if (gcursor + strlen(linebuf) > &genbuf[LBSIZE - 2])
                gcursor = genbuf;
            CP(gcursor, linebuf);
        } else {
            CP(genbuf, gcursor + 1);
            gcursor = genbuf;
        }

        /*
         * If we started out as a single line operation and are now
         * turning into a multi-line change, then we had better yank
         * out dot before it changes so that undo will work
         * correctly later.
         */
        if (FIXUNDO && vundkind == VCHNG) {
            co_await vremote(1, op_yank, 0);
            undap1--;
        }

        /*
         * Now do the append of the new line in the buffer,
         * and update the display.  If slowopen
         * we don't do very much.
         */
        vdoappend(genbuf);
        vundkind = VMANYINS;
        vcline++;
        if (state != VISUAL)
            vshow(dot, NOLINE);
        else {
            i += LINE(vcline - 1);
            vopen(dot, i);
            if (value(SLOWOPEN))
                vscrap();
            else
                vsync1(LINE(vcline));
        }
        strcLIN(gcursor);
        *gcursor = 0;
        cursor   = linebuf;
        vgotoCL(qcolumn(cursor - 1, genbuf));
    }

    /*
     * All done with insertion, position the cursor
     * and sync the screen.
     */
    hold = oldhold;
    /* The mode line stays up across a motion: the insertion is not over. */
    inserting = insmotion != 0;
    /*
     * The caret drops back onto the last character inserted -- unless a
     * motion is waiting, which starts from where the next character would
     * have gone.
     */
    if (cursor > linebuf && !insmotion)
        cursor--;
    if (state != HARDOPEN)
        vsyncCL();
    else if (cursor > linebuf)
        back1();
    doomed  = 0;
    wcursor = cursor;
    vmove();

    /*
     * A cursor key or a backspace ended it: move or rub out, and open another
     * insertion there. What follows is vmain's own preamble for an insert
     * command, less the parts that only a typed one needs.
     */
    if (insmotion && co_await vinsmove()) {
        if (ch != 'R')
            ch = 'i';
        cnt    = 1;
        indent = 0;
        lastcp = workcmd;
        /* '.' repeats the resumed insertion, not the command that began it. */
        *lastcp++ = ch;
        setLAST();
        vcursat(cursor);
        prepapp();
        vnoapp();
        doomed = ch == 'R' ? 10000 : 0;
        if (FIXUNDO)
            vundkind = VCHNG;
        vmoving = 0;
        CP(vutmp, linebuf);
        oldhold = hold;
        goto again;
    }
    inserting = 0;
}

/*
 * The motion a key means, between one insertion and the next. The ones that
 * stay on the line are pointer moves; the ones that need the buffer or the
 * window are the command they answer. Zero to leave insert mode.
 */
Task<exbool> vinsmove(void)
{
    int c = insmotion;

    insmotion = 0;
    switch (c) {
    /*
     * Backspace past the start of the insertion. Not operate('X'), whose
     * margin() beeps where an append leaves the cursor -- on the terminator.
     */
    case CTRL('h'): {
        int at = cursor - linebuf - 1;

        /* At column 0 the character before the cursor is the line break. */
        if (cursor == linebuf) {
            if (dot == one) {
                obeep();
                break;
            }
            co_await operate(CTRL('p'), 1); /* onto the line to join to */
            vsave();
            co_await vmacchng(1);
            setLAST();
            cursor = strend(linebuf);        /* the seam, and the insert point */
            co_await vremote(2, op_join, 1); /* 1: no space between */
            notenam = "join";
            vmoving = 0;
            killU();
            vreplace(vcline, 2, 1);
            if (notecnt == 2)
                notecnt = 0;
            vrepaint(cursor);
            break;
        }
        wdot    = NOLINE;
        wcursor = cursor - 1;
        co_await vmacchng(1);
        co_await vdelete(0);
        cursor  = linebuf + at; /* vdelete leaves it on a character */
        vmoving = 0;
        break;
    }

    case KLEFT:
        if (cursor > linebuf)
            cursor--;
        else
            obeep();
        break;

    case KRIGHT:
        if (*cursor)
            cursor++;
        else
            obeep();
        break;

    case KHOME:
        cursor = linebuf;
        break;

    case KEND:
        cursor = strend(linebuf);
        break;

    case KUP:
    case KDOWN:
    case KDEL:
        co_await operate(keycmd(c), 1);
        break;

    default: /* a page leaves the insertion, as an escape would */
        ungetkey(keycmd(c));
        co_return (0);
    }
    co_return (1);
}

/*
 * Subroutine for vgetline to back up a single character position,
 * backwards around end of lines (vgoto can't hack columns which are
 * less than 0 in general).
 */
void back1(void)
{
    vgoto(destline - 1, WCOLS + destcol - 1);
}

/*
 * Get a line into genbuf after gcursor.
 * Cnt limits the number of input characters
 * accepted and is used for handling the replace
 * single character command.  Aescaped is the location
 * where we stick a termination indicator (whether we
 * ended with an ESCAPE or a newline/return.
 *
 * We do erase-kill type processing here and also
 * are careful about the way we do this so that it is
 * repeatable.  (I.e. so that your kill doesn't happen,
 * when you repeat an insert if it was escaped with \ the
 * first time you did it.  commch is the command character
 * involved, including the prompt for readline.
 */
Task<char *> vgetline(int cnt, char *gcursor, exbool *aescaped, int commch)
{
    int c, ch;
    char *cp;
    int x, y, iwhite, backsl = 0;
    char *iglobp;
    char cstr[2];
    OutcharFn OO = Outchar;

    /*
     * Clear the output state and counters
     * for autoindent backwards motion (counts of ^D, etc.)
     * Remember how much white space at beginning of line so
     * as not to allow backspace over autoindent.
     */
    *aescaped = 0;
    insmotion = 0;
    ogcursor  = gcursor;
    flusho();
    CDCNT   = 0;
    HADUP   = 0;
    HADZERO = 0;
    gobbled = 0;
    iwhite  = whitecnt(genbuf);
    iglobp  = vglobp;

    /*
     * Carefully avoid using vinschar in the echo area.
     */
    if (splitw)
        Outchar = vputchar;
    else {
        Outchar = vinschar;
        vprepins();
    }
    for (;;) {
        backsl = 0;
        if (gobblebl)
            gobblebl--;
        if (cnt != 0) {
            cnt--;
            if (cnt == 0)
                goto vadone;
        }
        c = co_await getkey();
        if (c >= 0)
            c &= (QUOTE | TRIM);
        ch        = c;
        maphopcnt = 0;
        if (vglobp == 0 && Peekkey == 0 && commch != 'r')
            while ((ch = co_await map(c, immacs)) != c) {
                c = ch;
                if (!value(REMAP))
                    break;
                if (++maphopcnt > 256)
                    COTHROWV(0, error("Infinite macro loop"));
            }
        if (!iglobp) {
            /*
             * Erase-kill type processing.
             * Only happens if we were not reading
             * from untyped input when we started.
             * Map users erase to ^H, kill to -1 for switch.
             */
            if (c == CTRL('u'))
                c = -1;
            switch (c) {
            /*
             * ^?		Interrupt drops you back to visual
             *		command mode with an unread interrupt
             *		still in the input buffer.
             *
             * ^\		Quit does the same as interrupt.
             *		If you are a ex command rather than
             *		a vi command this will drop you
             *		back to command mode for sure.
             */
            case ATTN:
            case QUIT:
                ungetkey(c);
                goto vadone;

            /*
             * A cursor key. End the insertion here so that the line is whole
             * to move around in; vappend does the motion and opens another
             * insertion where it lands.
             *
             * Not in the echo area, where ending it would submit a half
             * typed command line, and not under r, which takes one character
             * only. The key travels in insmotion rather than through
             * ungetkey, so readecho and vmain never see it.
             */
            case KUP:
            case KDOWN:
            case KLEFT:
            case KRIGHT:
            case KHOME:
            case KEND:
            case KPGUP:
            case KPGDN:
            case KDEL:
                if (splitw || commch == 'r') {
                    obeep();
                    continue;
                }
                insmotion = c;
                goto vadone;

            /*
             * ^H		Backs up a character in the input.
             *
             * BUG:		Can't back around line boundaries.
             *		This is hard because stuff has
             *		already been saved for repeat.
             */
            case CTRL('h'):
            bakchar:
                cp = gcursor - 1;
                if (cp < ogcursor) {
                    if (splitw) {
                        /*
                         * Backspacing over readecho
                         * prompt. Pretend exdelete but
                         * don't obeep.
                         */
                        ungetkey(c);
                        goto vadone;
                    }
                    /* The autoindent is in genbuf too: lower the floor. */
                    if (ogcursor > genbuf) {
                        ogcursor = genbuf;
                        goto vbackup;
                    }
                    /* The line is whole only between insertions: end this
                     * one and let vinsmove rub the character out. */
                    if (commch != 'r') {
                        insmotion = c;
                        goto vadone;
                    }
                    obeep();
                    continue;
                }
                goto vbackup;

            /*
             * ^W		Back up a white/non-white word.
             */
            case CTRL('w'):
                wdkind = 1;
                for (cp = gcursor; cp > ogcursor && isspace(cp[-1]); cp--)
                    continue;
                for (c = wordch(cp - 1); cp > ogcursor && wordof(c, cp - 1); cp--)
                    continue;
                goto vbackup;

            /*
             * users kill	Kill input on this line, back to
             *		the autoindent.
             */
            case -1:
                cp = ogcursor;
            vbackup:
                if (cp == gcursor) {
                    obeep();
                    continue;
                }
                endim();
                *cp = 0;
                c   = cindent();
                vgotoCL(qcolumn(cursor - 1, genbuf));
                if (doomed >= 0)
                    doomed += c - cindent();
                gcursor = cp;
                continue;

            /*
             * \		Followed by erase or kill
             *		maps to just the erase or kill.
             */
            case '\\':
                x = destcol, y = destline;
                putchar('\\');
                vcsync();
                c = co_await getkey();
                if (c == CTRL('h') || c == CTRL('u')) {
                    vgoto(y, x);
                    if (doomed >= 0)
                        doomed++;
                    goto def;
                }
                ungetkey(c), c = '\\';
                backsl = 1;
                break;

            /*
             * ^Q		Super quote following character
             *		Only ^@ is verboten (trapped at
             *		a lower level) and \n forces a line
             *		split so doesn't really go in.
             *
             * ^V		Synonym for ^Q
             */
            case CTRL('q'):
            case CTRL('v'):
                x = destcol, y = destline;
                putchar('^');
                vgoto(y, x);
                c = keycmd(co_await getkey()); /* a quoted cursor key is its byte */
                if (c != NL) {
                    if (doomed >= 0)
                        doomed++;
                    goto def;
                }
                break;
            }
        }

        /*
         * If we get a blank not in the echo area
         * consider splitting the window in the wrapmargin.
         */
        if (c != NL && c != KESC && !splitw) {
            if (c == ' ' && gobblebl) {
                gobbled = 1;
                continue;
            }
            if (value(WRAPMARGIN) &&
                (outcol >= OCOLUMNS - value(WRAPMARGIN) || backsl && outcol == 0) &&
                commch != 'r') {
                /*
                 * At end of word and hit wrapmargin.
                 * Move the word to next line and keep going.
                 */
                wdkind     = 1;
                *gcursor++ = c;
                if (backsl)
                    *gcursor++ = keycmd(co_await getkey());
                *gcursor = 0;
                /*
                 * Find end of previous word if we are past it.
                 */
                for (cp = gcursor; cp > ogcursor && isspace(cp[-1]); cp--)
                    ;
                if (outcol + (backsl ? OCOLUMNS : 0) - (gcursor - cp) >=
                    OCOLUMNS - value(WRAPMARGIN)) {
                    /*
                     * Find beginning of previous word.
                     */
                    for (; cp > ogcursor && !isspace(cp[-1]); cp--)
                        ;
                    if (cp <= ogcursor) {
                        /*
                         * There is a single word that
                         * is too long to fit.  Just
                         * let it pass, but obeep for
                         * each new letter to warn
                         * the luser.
                         */
                        c        = *--gcursor;
                        *gcursor = 0;
                        obeep();
                        goto dontbreak;
                    }
                    /*
                     * Save it for next line.
                     */
                    macpush(cp, 0);
                    cp--;
                }
                macpush("\n", 0);
                /*
                 * Erase white space before the word.
                 */
                while (cp > ogcursor && isspace(cp[-1]))
                    cp--; /* skip blank */
                gobblebl = 3;
                goto vbackup;
            }
        dontbreak:;
        }

        /*
         * Word abbreviation mode.
         */
        cstr[0] = c;
        if (anyabbrs && gcursor > ogcursor && !wordch(cstr) && wordch(gcursor - 1)) {
            int wdtype, abno;

            cstr[1] = 0;
            wdkind  = 1;
            cp      = gcursor - 1;
            for (wdtype = wordch(cp - 1); cp > ogcursor && wordof(wdtype, cp - 1); cp--)
                ;
            *gcursor = 0;
            for (abno = 0; abbrevs[abno].mapto; abno++) {
                if (eq(cp, abbrevs[abno].cap)) {
                    macpush(cstr, 0);
                    macpush(abbrevs[abno].mapto);
                    goto vbackup;
                }
            }
        }

        switch (c) {
        /*
         * ^M		Except in repeat maps to \n.
         */
        case CR:
            if (vglobp)
                goto def;
            c = '\n';
            /* presto chango ... */

        /*
         * \n		Start new line.
         */
        case NL:
            *aescaped = c;
            goto vadone;

        /*
         * The escape key always ends the insertion. Pressing it is not the
         * only way a 033 gets here -- getbr() answers one when replayed input
         * runs out, and lastvgk says that is where this came from -- and that
         * one goes in as text when there is more of the repeat to come.
         */
        case KESC:
            goto vadone;

        /*
         * escape	End insert unless repeat and more to repeat.
         */
        case ESCAPE:
            if (lastvgk)
                goto def;
            goto vadone;

        /*
         * ^D		Backtab.
         * ^T		Software forward tab.
         *
         *		Unless in repeat where this means these
         *		were superquoted in.
         */
        case CTRL('d'):
        case CTRL('t'):
            if (vglobp)
                goto def;
            /* fall into ... */

        /*
         * ^D|QUOTE	Is a backtab (in a repeated command).
         */
        case CTRL('d') | QUOTE:
            *gcursor = 0;
            cp       = vpastwh(genbuf);
            c        = whitecnt(genbuf);
            if (ch == CTRL('t')) {
                /*
                 * ^t just generates new indent replacing
                 * current white space rounded up to soft
                 * tab stop increment.
                 */
                if (cp != gcursor)
                    /*
                     * BUG:		Don't hack ^T except
                     *		right after initial
                     *		white space.
                     */
                    continue;
                cp       = genindent(iwhite = backtab(c + value(SHIFTWIDTH) + 1));
                ogcursor = cp;
                goto vbackup;
            }
            /*
             * ^D works only if we are at the (end of) the
             * generated autoindent.  We count the ^D for repeat
             * purposes.
             */
            if (c == iwhite && c != 0)
                if (cp == gcursor) {
                    iwhite = backtab(c);
                    CDCNT++;
                    ogcursor = cp = genindent(iwhite);
                    goto vbackup;
                } else if (&cp[1] == gcursor && (*cp == '^' || *cp == '0')) {
                    /*
                     * ^^D moves to margin, then back
                     * to current indent on next line.
                     *
                     * 0^D moves to margin and then
                     * stays there.
                     */
                    HADZERO  = *cp == '0';
                    ogcursor = cp = genbuf;
                    HADUP         = 1 - HADZERO;
                    CDCNT         = 1;
                    endim();
                    back1();
                    vputchar(' ');
                    goto vbackup;
                }
            if (vglobp && vglobp - iglobp >= 2 && (vglobp[-2] == '^' || vglobp[-2] == '0') &&
                gcursor == ogcursor + 1)
                goto bakchar;
            continue;

        default:
            /*
             * Possibly discard control inputs.
             */
            if (!vglobp && junk(c)) {
                obeep();
                continue;
            }
        def:
            if (!backsl) {
                putchar(c);
                flush();
            }
            if (gcursor > &genbuf[LBSIZE - 2])
                COTHROWV(0, error("Line too long"));
            *gcursor++ = c & TRIM;
            vcsync();
            if (value(SHOWMATCH) && !iglobp)
                if (c == ')' || c == '}')
                    lsmatch(gcursor);
            continue;
        }
    }
vadone:
    *gcursor = 0;
    if (Outchar != termchar)
        Outchar = OO;
    endim();
    co_return (gcursor);
}

int vgetsplit();
char *vsplitpt;

/*
 * Append the line in buffer at lp
 * to the buffer after dot.
 */
void vdoappend(char *lp)
{
    line *a1, *a2, *rdot;
    int oing = inglobal;

    inglobal = 1;
    strcLIN(lp);
    if (truedol >= endcore && morelines() < 0) {
        inglobal = oing;
        THROW(error("Out of memory@- too many lines in file"));
    }
    /*
     * append()'s loop body, once. The undo bookkeeping it does first is
     * guarded by !inopen and this is only ever called from open or visual,
     * so it does not apply.
     */
    a1 = truedol + 1;
    a2 = a1 + 1;
    dot++;
    undap2++;
    dol++;
    unddol++;
    truedol++;
    for (rdot = dot; a1 > rdot;)
        *--a2 = *--a1;
    *rdot = 0;
    putmark(rdot);
    inglobal = oing;
}

/*
 * Vmaxrep determines the maximum repetitition factor
 * allowed that will yield total line length less than
 * LBSIZE characters and also does hacks for the R command.
 */
int vmaxrep(int ch, int cnt)
{
    int len, replen;

    if (cnt > LBSIZE - 2)
        cnt = LBSIZE - 2;
    replen = strlen(genbuf);
    if (ch == 'R') {
        len = strlen(cursor);
        if (replen < len)
            len = replen;
        CP(cursor, cursor + len);
        vUD2 += len;
    }
    len = strlen(linebuf);
    if (len + cnt * replen <= LBSIZE - 2)
        return (cnt);
    cnt = (LBSIZE - 2 - len) / replen;
    if (cnt == 0) {
        vsave();
        THROWV(0, error("Line too long"));
    }
    return (cnt);
}
