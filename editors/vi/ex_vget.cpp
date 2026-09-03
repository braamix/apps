/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

#include "kernel/text.h"

/*
 * Input routines for open/visual.
 * We handle upper case only terminals in visual and reading from the
 * echo area here as well as notification on large changes
 * which appears in the echo area.
 */

/*
 * Return the key.
 */
void ungetkey(int c)
{
    if (Peekkey != ATTN)
        Peekkey = c;
}

/*
 * Return a keystroke, but never a ^@.
 */
Task<int> getkey(void)
{
    int c;

    do {
        c = co_await getbr();
        if (c == 0)
            obeep();
    } while (c == 0);
    co_return (c);
}

/*
 * Tell whether next keystroke would be a ^@.
 */
Task<int> peekbr(void)
{
    Peekkey = co_await getbr();
    co_return (Peekkey == 0);
}

short precbksl;

/* The tail of a multi-byte key, waiting to be read one byte at a time. */
static int keyq[4];
static int keyqn;

/*
 * Get a keystroke, including a ^@.
 * If an key was returned with ungetkey, that
 * comes back first.  Next comes unread input (e.g.
 * from repeating commands with .), and finally new
 * keystrokes.
 *
 * The hard work here is in mapping of \ escaped
 * characters on upper case only terminals.
 */
Task<int> getbr(void)
{
    int c;
#define BEEHIVE
    extern short slevel, ttyindes;

    if (Peekkey) {
        c       = Peekkey;
        Peekkey = 0;
        co_return (c);
    }
    if (vglobp) {
        /* Text, not keys: a byte over 0177 must not read as a named key. */
        if (*vglobp)
            co_return (lastvgk = (unsigned char)*vglobp++);
        lastvgk = 0;
        co_return (ESCAPE);
    }
    if (vmacp) {
        if (*vmacp)
            co_return (*vmacp++);
        /* End of a macro or set of nested macros */
        vmacp = 0;
        if (inopen == -1) /* don't screw up undo for esc esc */
            vundkind = VMANY;
        inopen  = 1; /* restore old setting now that macro done */
        vch_mac = VC_NOTINMAC;
    }
    /*
     * The tail of a UTF-8 sequence key_byte() handed over whole. Below the
     * pushbacks, because it is keyboard input and they come first.
     */
    if (keyqn) {
        c = keyq[0];
        for (int i = 1; i < keyqn; i++)
            keyq[i - 1] = keyq[i];
        keyqn--;
        co_return (c);
    }
    /* The frame: the editor stops here, so the screen must be right here. */
    co_await vflush();

again: {
    Result<Key> r = Err(Error::NoMemory);

    if (Task<Result<Key>> t = vscreen->next_key())
        r = co_await t;
    if (r.is_err()) {
        /* Intr is a resize, or ^C, which answers ATTN. */
        if (r.error() == Error::Intr) {
            if (sig_take(SIG_INT))
                co_return (ATTN);
            vresize();
            co_await vflush();
            goto again;
        }
        if (r.error() == Error::Again)
            goto again;
        COTHROWV(0, error("Input read error"));
    }
    c = key_byte(res_of(r));
    if (c == 0)
        goto again;
    /* A codepoint arrives whole; the line takes it a UTF-8 byte at a time. */
    if (c >= 0x80) {
        char b[4];
        usize n = utf8_encode((char32_t)c, b);

        for (usize i = 1; i < n; i++)
            keyq[keyqn++] = (unsigned char)b[i];
        c = (unsigned char)b[0];
    }
}

    lastvgk = 0;
    co_return (c);
}

/*
 * Get a key, but if a exdelete, quit or attention
 * is typed return 0 so we will abort a partial command.
 */
Task<int> getesc(void)
{
    int c;

    c = co_await getkey();
    switch (c) {
    case CTRL('v'):
    case CTRL('q'):
        c = keycmd(co_await getkey());
        co_return (c);

    case ATTN:
    case QUIT:
        ungetkey(c);
        co_return (0);

    /*
     * A cursor key where a character was wanted: abandon the command and
     * let the key be read again as the motion it is.
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
        ungetkey(c);
        co_return (0);

    case KESC:   /* the key */
    case ESCAPE: /* and the byte it would be */
        co_return (0);
    }
    co_return (c);
}

/*
 * Peek at the next keystroke.
 */
Task<int> peekkey(void)
{
    Peekkey = co_await getkey();
    co_return (Peekkey);
}

/*
 * Read a line from the echo area, with single character prompt c.
 * A return value of 1 means the user blewit or blewit away.
 */
Task<int> readecho(int c)
{
    char *sc = cursor;
    PlineFn OP;
    exbool waste;
    int OPeek;

    if (WBOT == WECHO)
        vclean();
    else
        vclrech(0);
    splitw++;
    vgoto(WECHO, 0);
    putchar(c);
    vclreol();
    vgoto(WECHO, 1);
    cursor     = linebuf;
    linebuf[0] = 0;
    genbuf[0]  = c;
    if (co_await peekbr()) {
        if (!INS[0] || (unsigned char)INS[0] == OVERBUF)
            goto blewit;
        vglobp = INS;
    }
    OP    = Pline;
    Pline = normline;
    ignore(co_await vgetline(0, genbuf + 1, &waste, c));
    if (Outchar == termchar)
        putchar('\n');
    vscrap();
    Pline = OP;
    if (Peekkey != ATTN && Peekkey != QUIT && Peekkey != CTRL('h')) {
        cursor = sc;
        vclreol();
        co_return (0);
    }
blewit:
    OPeek   = Peekkey == CTRL('h') ? 0 : Peekkey;
    Peekkey = 0;
    splitw  = 0;
    vclean();
    vshow(dot, NOLINE);
    vnline(sc);
    Peekkey = OPeek;
    co_return (1);
}

/*
 * A complete command has been defined for
 * the purposes of repeat, so copy it from
 * the working to the previous command buffer.
 */
void setLAST(void)
{
    if (vglobp || vmacp)
        return;
    lastreg = vreg;
    lasthad = Xhadcnt;
    lastcnt = Xcnt;
    *lastcp = 0;
    CP(lastcmd, workcmd);
}

/*
 * Gather up some more text from an insert.
 * If the insertion buffer oveflows, then destroy
 * the repeatability of the insert.
 */
void addtext(char *cp)
{
    if (vglobp)
        return;
    addto(INS, cp);
    if ((unsigned char)INS[0] == OVERBUF)
        lastcmd[0] = 0;
}

void setDEL(void)
{
    setBUF(DEL);
}

/*
 * Put text from cursor upto wcursor in BUF.
 */
void setBUF(char *BUF)
{
    int c;
    char *wp = wcursor;

    c      = *wp;
    *wp    = 0;
    BUF[0] = 0;
    addto(BUF, cursor);
    *wp = c;
}

void addto(char *buf, char *str)
{
    if ((unsigned char)buf[0] == OVERBUF)
        return;
    if (strlen(buf) + strlen(str) + 1 >= VBSIZE) {
        buf[0] = OVERBUF;
        return;
    }
    ignore(strcat(buf, str));
}

/*
 * Note a change affecting a lot of lines, or non-visible
 * lines.  If the parameter must is set, then we only want
 * to do this for open modes now; return and save for later
 * notification in visual.
 */
exbool noteit(exbool must)
{
    int sdl = destline, sdc = destcol;

    if (notecnt < 2 || !must && state == VISUAL)
        return (0);
    splitw++;
    if (WBOT == WECHO)
        vmoveitup(1, 1);
    vigoto(WECHO, 0);
    printf("%d %sline", notecnt, notesgn);
    if (notecnt > 1)
        putchar('s');
    if (*notenam) {
        printf(" %s", notenam);
        if (*(strend(notenam) - 1) != 'e')
            putchar('e');
        putchar('d');
    }
    vclreol();
    notecnt = 0;
    if (state != VISUAL)
        vcnt = vcline = 0;
    splitw = 0;
    if (state == ONEOPEN || state == CRTOPEN)
        vup1();
    destline = sdl;
    destcol  = sdc;
    return (1);
}

/*
 * Rrrrringgggggg.
 * If possible, use flash (VB).
 */
void obeep(void)
{
    if (VB)
        vputp(VB, 0);
    else
        vputc(CTRL('g'));
}

/*
 * Map the command input character c,
 * for keypads and labelled keys which do cursor
 * motions.  I.e. on an adm3a we might map ^K to ^P.
 * DM1520 for example has a lot of mappable characters.
 */

Task<int> map(int c, struct maps *maps)
{
    int d;
    char *p;
    int *q;
    int b[10]; /* Assumption: no keypad sends string longer than 10 */

    /*
     * Mapping for special keys on the terminal only.
     * BUG: if there's a long sequence and it matches
     * some chars and then misses, we lose some chars.
     *
     * For this to work, some conditions must be met.
     * 1) Keypad sends SHORT (2 or 3 char) strings
     * 2) All strings sent are same length & similar
     * 3) The user is unlikely to type the first few chars of
     *    one of these strings very fast.
     * Note: some code has been fixed up since the above was laid out,
     * so conditions 1 & 2 are probably not required anymore.
     * However, this hasn't been tested with any first char
     * that means anything else except escape.
     */
    /*
     * If c==0, the char came from getesc typing escape.  Pass it through
     * unchanged.  0 messes up the following code anyway.
     */
    if (c == 0)
        co_return (0);

    b[0] = c;
    b[1] = 0;
    for (d = 0; maps[d].mapto; d++) {
        if (p = maps[d].cap) {
            for (q = b; *p; p++, q++) {
                if (*q == 0) {
                    /*
                     * Is there another char waiting?
                     *
                     * This test is oversimplified, but
                     * should work mostly. It handles the
                     * case where we get an ESCAPE that
                     * wasn't part of a keypad string.
                     */
                    if ((c == '#' ? co_await peekkey() : co_await fastpeekkey()) == 0) {
                        /*
                         * Nothing waiting.  Push back
                         * what we peeked at & return
                         * failure (c).
                         *
                         * We want to be able to undo
                         * commands, but it's nonsense
                         * to undo part of an insertion
                         * so if in input mode don't.
                         */
                        macpushk(&b[1], maps == arrows);
                        co_return (c);
                    }
                    *q   = co_await getkey();
                    q[1] = 0;
                }
                if (*p != *q)
                    goto contin;
            }
            macpush(maps[d].mapto, maps == arrows);
            c = co_await getkey();
            co_return (c); /* first char of map string */
        contin:;
        }
    }
    macpushk(&b[1], 0);
    co_return (c);
}

/*
 * Push st onto the front of vmacp. This is tricky because we have to
 * worry about where vmacp was previously pointing. We also have to
 * check for overflow (which is typically from a recursive macro)
 * Finally we have to set a flag so the whole thing can be undone.
 * canundo is 1 iff we want to be able to undo the macro.  This
 * is false for, for example, pushing back lookahead from fastpeekkey(),
 * since otherwise two fast escapes can clobber our undo.
 */
/* strlen and strcpy, over the key buffer. */
static int maclen(int *p)
{
    int n = 0;

    while (p[n])
        n++;
    return (n);
}

static int *maccpy(int *to, int *from)
{
    while (*to++ = *from++)
        continue;
    return (to - 1);
}

void macpushk(int *st, int canundo)
{
    int tmpbuf[BUFSIZ];

    if (st == 0 || *st == 0)
        return;
#ifdef notdef
    if (!value(UNDOMACRO))
        canundo = 0;
#endif
    if ((vmacp ? maclen(vmacp) : 0) + maclen(st) > BUFSIZ)
        THROW(error("Macro too long@ - maybe recursive?"));
    if (vmacp) {
        maccpy(tmpbuf, vmacp);
        if (!FIXUNDO)
            canundo = 0; /* can't undo inside a macro anyway */
    }
    {
        int *end = maccpy(vmacbuf, st);

        if (vmacp)
            maccpy(end, tmpbuf);
    }
    vmacp = vmacbuf;
    /* arrange to be able to undo the whole macro */
    if (canundo) {
#ifdef notdef
        otchng = tchng;
        vsave();
        saveall();
        inopen   = -1; /* no need to save since it had to be 1 or -1 before */
        vundkind = VMANY;
#endif
        vch_mac = VC_NOCHANGE;
    }
}

/*
 * The same, for text rather than keys: every caller but map() pushes a string
 * out of a buffer, and a byte over 0177 is a byte, not a named key.
 */
void macpush(char *st, int canundo)
{
    int b[BUFSIZ];
    int n = 0;

    if (st == 0 || *st == 0)
        return;
    while (st[n] && n < BUFSIZ - 1) {
        b[n] = (unsigned char)st[n];
        n++;
    }
    b[n] = 0;
    macpushk(b, canundo);
}

/*
 * Get a count from the keyed input stream.
 * A zero count is indistinguishable from no count.
 */
Task<int> vgetcnt(void)
{
    int c, cnt;

    cnt = 0;
    for (;;) {
        c = co_await getkey();
        if (!isdigit(c))
            break;
        cnt *= 10, cnt += c - '0';
    }
    ungetkey(c);
    Xhadcnt = 1;
    Xcnt    = cnt;
    co_return (cnt);
}

/*
 * fastpeekkey is just like peekkey but insists the character come in
 * fast (within 1 second). This will succeed if it is the 2nd char of
 * a machine generated sequence (such as a function pad from an escape
 * flavor terminal) but fail for a human hitting escape then waiting.
 */
Task<int> fastpeekkey(void)
{
    int c;

    /*
     * If the user has set notimeout, we wait forever for a key.
     * If we are in a macro we do too, but since it's already
     * buffered internally it will return immediately.
     * In other cases we force this to die in 1 second.
     * This is pretty reliable (VMUNIX rounds it to .5 - 1.5 secs,
     * but UNIX truncates it to 0 - 1 secs) but due to system delays
     * there are times when arrow keys or very fast typing get counted
     * as separate.  notimeout is provided for people who dislike such
     * nondeterminism.
     */
    /*
     * Upstream waited a second to tell a three-byte arrow key from an ESC
     * typed by hand. KEY_UP arrives as itself, so there is nothing to tell
     * apart and `set notimeout` names a distinction that is gone.
     */
    c = co_await peekkey();
    co_return (c);
}
