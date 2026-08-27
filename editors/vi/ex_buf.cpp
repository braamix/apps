/* Copyright (c) 1980 Regents of the University of California */
/* @(#)ex_temp.c	6.3 11/3/80 */

/*
 * The editor buffer, and the named buffers with it. This was ex_temp.c, whose
 * whole subject was a file in /tmp; the text lives in memory now, and what is
 * left is the arithmetic that was always in memory anyway.
 *
 * What survives untouched is the important half. A `line` handle packs a block
 * number and an offset, and on the VMUNIX arm SHFT is 0 and OFFBTS is 10
 * against a BUFSIZ of 1024 -- so the packing is the identity and a handle
 * simply *is* a byte offset, with bit 0 left clear for global's mark. getline()
 * and putline() are therefore upstream's own source, and so is the property
 * they rest on: tline only ever increases, so a handle names one line forever,
 * which is what makes marks and the general undo correct.
 *
 * What is gone is everything the file needed. getblock() was eighty lines of
 * two-buffer LRU over three BUFSIZ buffers -- worth about a quarter of the
 * reads, upstream measured -- and blocks are contiguous now, so there is
 * nothing left to choose between. With it went blkio, tflush, tlaste, the
 * MAXDIRT sync, and struct header, which existed so that expreserve could
 * reconstruct a buffer after a crash.
 */
#include "ex.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

#include "kernel/alloc.h"

/*
 * The arena doubles from TX_INIT and stops at TX_MAX. Growth moves it, which
 * is safe precisely because no `line` is a pointer -- a handle is an offset,
 * so nothing needs relocating.
 */
exbool tx_reserve(int want)
{
    int n;
    char *p;

    if (want <= tx_size)
        return (1);
    if (want > TX_MAX)
        return (0);
    n = tx_size ? tx_size : TX_INIT;
    while (n < want)
        n *= 2;
    if (n > TX_MAX)
        n = TX_MAX;
    p = (char *)heap_alloc(n);
    if (p == 0)
        return (0);
    if (tx_arena) {
        memcpy(p, tx_arena, tx_size);
        heap_free(tx_arena);
    }
    memset(p + tx_size, 0, n - tx_size);
    tx_arena = p;
    tx_size  = n;
    return (1);
}

/*
 * The line pointer array, which sbrk used to hand out. This one may not move:
 * dot, dol, addr1, addr2, unddol, truedol, undap1, undap2, unddel, undadot,
 * wdot, names[] and vUNDdot are all raw line * into it. So it is taken once
 * and morelines() only bumps endcore against the limit.
 */
exbool lx_init(void)
{
    line *p = (line *)heap_alloc(LX_SLOTS * sizeof(line));

    if (p == 0)
        return (0);
    fendcore = p;
    lx_limit = p + LX_SLOTS;
    endcore  = fendcore - 2;
    return (1);
}

/*
 * Start a new buffer. Upstream discarded the temp file and created another,
 * which is exactly what winding tline back to the beginning does: no handle
 * from the old file is reachable, and the storage is reused wholesale.
 */
void fileinit(void)
{
    if (tline == INCRMT * 2)
        return;
    tline = INCRMT * 2;
    nleft = 0;
}

void cleanup(exbool all)
{
    if (all)
        flush();
}

void getline(line tl)
{
    char *bp, *lp;
    int nl;

    if (ex_thrown) {
        linebuf[0] = 0;
        return;
    }
    lp = linebuf;
    bp = getblock(tl, 0);
    nl = nleft;
    tl &= ~OFFMSK;
    while (*lp++ = *bp++)
        if (--nl == 0) {
            bp = getblock(tl += INCRMT, 0);
            nl = nleft;
        }
}

/*
 * A line handle that names an empty line and is never handed out twice. What
 * putline() answers while an error is pending: appending would grow the arena
 * on the way out, and answering a live handle would let two lines share one,
 * which is precisely what breaks a mark.
 */
static line tx_sentinel;

line putline(void)
{
    char *bp, *lp;
    int nl;
    line tl;

    if (ex_thrown)
        return (tx_sentinel);
    lp = linebuf;
    change();
    tl = tline;
    bp = getblock(tl, 1);
    if (ex_thrown)
        return (tx_sentinel);
    nl = nleft;
    tl &= ~OFFMSK;
    while (*bp = *lp++) {
        if (*bp++ == '\n') {
            *--bp  = 0;
            linebp = lp;
            break;
        }
        if (--nl == 0) {
            bp = getblock(tl += INCRMT, 1);
            if (ex_thrown)
                return (tx_sentinel);
            nl = nleft;
        }
    }
    tl = tline;
    tline += (((lp - linebuf) + BNDRY - 1) >> SHFT) & 077776;
    return (tl);
}

/*
 * The block a handle names, and how much of it is left. Upstream chose between
 * two read buffers and wrote a third back; here the blocks are one array, so
 * this is address arithmetic and a bounds check. nleft is still set, because
 * getline() and putline() walk block boundaries by counting it down.
 */
char *getblock(line atl, int iof)
{
    int off, bno;

    (void)iof;
    off   = (int)atl & OFFMSK;
    bno   = ((int)atl >> OFFBTS) & BLKMSK;
    nleft = BUFSIZ - off;
    if (bno >= NMBLKS)
        THROWV(tx_arena, error("Tmp file too large"));
    if (!tx_reserve((bno + 1) * BUFSIZ))
        THROWV(tx_arena, error(" Out of memory"));
    return (tx_arena + bno * BUFSIZ + off);
}

/*
 * Named buffer routines.
 * These are implemented differently than the main buffer.
 * Each named buffer has a chain of blocks in the register file.
 * Each block contains roughly 508 chars of text,
 * and a previous and next block number.  We also have information
 * about which blocks came from deletes of multiple partial lines,
 * e.g. deleting a sentence or a LISP object.
 *
 * We maintain a free map for the temp file.  To free the blocks
 * in a register we must read the blocks to find how they are chained
 * together.
 *
 * BUG:		The default savind of deleted lines in numbered
 *		buffers may be rather inefficient; it hasn't been profiled.
 */
static char *rblocks;

/*
 * Upstream's callers each kept a `struct rbuf arbuf` on the stack and pointed
 * rbuf at it. That is a kilobyte, and three of those callers are coroutines
 * now, where a frame past 512 bytes costs a whole 64 KiB span -- so the
 * scratch block is here instead. Nothing nests: no two of them are live at
 * once.
 */
static struct rbuf arbuf;

/*
 * One register block, in or out. Upstream seeked a second temp file and read
 * or wrote it, choosing the direction by comparing the function pointer it was
 * handed against read; the direction is a flag now, and the seek is an index.
 */
void regio(int b, exbool writing)
{
    if (rblocks == 0) {
        rblocks = (char *)heap_alloc(RNBLKS * BUFSIZ);
        if (rblocks == 0)
            THROW(error(" Out of memory"));
        memset(rblocks, 0, RNBLKS * BUFSIZ);
    }
    if (b < 0 || b >= RNBLKS)
        THROW(error("Out of register space (ugh)"));
    if (writing)
        memcpy(rblocks + b * BUFSIZ, (char *)rbuf, BUFSIZ);
    else
        memcpy((char *)rbuf, rblocks + b * BUFSIZ, BUFSIZ);
    rblock = b;
}

line REGblk(void)
{
    int i, j, m;

    for (i = 0; i < (int)(sizeof rused / sizeof rused[0]); i++) {
        m = (rused[i] ^ 0177777) & 0177777;
        if (i == 0)
            m &= ~1;
        if (m != 0) {
            j = 0;
            while ((m & 1) == 0)
                j++, m >>= 1;
            rused[i] |= (1 << j);
            return (i * 16 + j);
        }
    }
    THROWV(0, error("Out of register space (ugh)"));
}

struct strreg *mapreg(int c)
{
    if (isupper(c))
        c = tolower(c);
    return (isdigit(c) ? &strregs[('z' - 'a' + 1) + (c - '0')] : &strregs[c - 'a']);
}

void KILLreg(int c)
{
    struct strreg *sp;

    rbuf         = &arbuf;
    sp           = mapreg(c);
    rblock       = sp->rg_first;
    sp->rg_first = sp->rg_last = 0;
    sp->rg_flags = sp->rg_nleft = 0;
    while (rblock != 0) {
        rused[rblock / 16] &= ~(1 << (rblock % 16));
        regio(rblock, 0);
        CHK;
        rblock = rbuf->rb_next;
    }
}

Task<void> putreg(int c)
{
    line *odot = dot;
    line *odol = dol;
    int cnt;

    deletenone();
    appendnone();
    rbuf   = &arbuf;
    rnleft = 0;
    rblock = 0;
    rnext  = mapreg(c)->rg_first;
    if (rnext == 0) {
        if (inopen) {
            splitw++;
            vclean();
            vgoto(WECHO, 0);
        }
        vreg = -1;
        COTHROW(error("Nothing in register %c", c));
    }
    if (inopen && partreg(c)) {
        if (!FIXUNDO) {
            splitw++;
            vclean();
            vgoto(WECHO, 0);
            vreg = -1;
            COTHROW(error("Can't put partial line inside macro"));
        }
        squish();
        addr1 = addr2 = dol;
    }
    cnt = co_await append(getREG, addr2);
    COCHK;
    if (inopen && partreg(c)) {
        unddol = dol;
        dol    = odol;
        dot    = odot;
        pragged(0);
    }
    killcnt(cnt);
    notecnt = cnt;
}

int partreg(int c)
{
    return (mapreg(c)->rg_flags);
}

void notpart(int c)
{
    if (c)
        mapreg(c)->rg_flags = 0;
}

Task<int> getREG(void)
{
    char *lp = linebuf;
    int c;

    for (;;) {
        if (rnleft == 0) {
            if (rnext == 0)
                co_return (EOF);
            regio(rnext, 0);
            COCHKV(EOF);
            rnext  = rbuf->rb_next;
            rbufcp = rbuf->rb_text;
            rnleft = sizeof rbuf->rb_text;
        }
        c = *rbufcp;
        if (c == 0)
            co_return (EOF);
        rbufcp++, --rnleft;
        if (c == '\n') {
            *lp++ = 0;
            co_return (0);
        }
        *lp++ = c;
    }
}

/* Upstream kept this line image on the stack; see the note above arbuf. */
static char savelb[LBSIZE];

void YANKreg(int c)
{
    line *addr;
    struct strreg *sp;

    if (isdigit(c))
        kshift();
    if (islower(c))
        KILLreg(c);
    CHK;
    strp = sp    = mapreg(c);
    sp->rg_flags = inopen && cursor && wcursor;
    rbuf         = &arbuf;
    if (sp->rg_last) {
        regio(sp->rg_last, 0);
        CHK;
        rnleft = sp->rg_nleft;
        rbufcp = &rbuf->rb_text[sizeof rbuf->rb_text - rnleft];
    } else {
        rblock = 0;
        rnleft = 0;
    }
    CP(savelb, linebuf);
    for (addr = addr1; addr <= addr2; addr++) {
        getline(*addr);
        if (sp->rg_flags) {
            if (addr == addr2)
                *wcursor = 0;
            if (addr == addr1)
                strcpy(linebuf, cursor);
        }
        YANKline();
        CHK;
    }
    rbflush();
    killed();
    CP(linebuf, savelb);
}

void kshift(void)
{
    int i;

    KILLreg('9');
    for (i = '8'; i >= '0'; i--)
        copy(mapreg(i + 1), mapreg(i), sizeof(struct strreg));
}

void YANKline(void)
{
    char *lp        = linebuf;
    struct rbuf *rp = rbuf;
    int c;

    do {
        c = *lp++;
        if (c == 0)
            c = '\n';
        if (rnleft == 0) {
            rp->rb_next = REGblk();
            CHK;
            rbflush();
            CHK;
            rblock      = rp->rb_next;
            rp->rb_next = 0;
            rp->rb_prev = rblock;
            rnleft      = sizeof rp->rb_text;
            rbufcp      = rp->rb_text;
        }
        *rbufcp++ = c;
        --rnleft;
    } while (c != '\n');
    if (rnleft)
        *rbufcp = 0;
}

void rbflush(void)
{
    struct strreg *sp = strp;

    if (rblock == 0)
        return;
    regio(rblock, 1);
    CHK;
    if (sp->rg_first == 0)
        sp->rg_first = rblock;
    sp->rg_last  = rblock;
    sp->rg_nleft = rnleft;
}

/* Register c to char buffer buf of size buflen */
Task<char *> regbuf(int c, char *buf, int buflen)
{
    char *p, *lp;

    rbuf   = &arbuf;
    rnleft = 0;
    rblock = 0;
    rnext  = mapreg(c)->rg_first;
    if (rnext == 0) {
        *buf = 0;
        COTHROWV(buf, error("Nothing in register %c", c));
    }
    p = buf;
    while (co_await getREG() == 0) {
        for (lp = linebuf; *lp;) {
            if (p >= &buf[buflen])
                COTHROWV(buf, error("Register too long@to fit in memory"));
            *p++ = *lp++;
        }
        *p++ = '\n';
    }
    COCHKV(buf);
    if (partreg(c))
        p--;
    *p = '\0';
    getDOT();
    co_return (buf);
}
