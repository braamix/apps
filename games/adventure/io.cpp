// Re-coding of advent in C: file i/o and user i/o
//
// Ported to Braam.  There is no seek syscall, so the message text the parser
// used to write to adventure.dat is written to a heap block instead and
// speak() indexes it.  glorkz itself is in the binary, and rdata() reads it
// from there rather than from a stream.
#include "hdr.h"

// Generated from glorkz by mkdata.py.
extern const char GLORKZ[];
extern const u32 GLORKZ_LEN;

int seekhere = 1; // initial seek for output file

static const char *inbuf; // the next byte of glorkz
static const char *inend;
static char *datbuf; // the decrypted-text block
static u32 outadr;   // where the next byte goes

int adrptr;    // current seek adr ptr
int outsw = 0; // putting stuff to data file?

const char iotape[] = "Ax3F'tt$8hqer*hnGKrX:!l";
const char *tape    = iotape; // pointer to encryption tape

char breakch; // tell which char ended rnum

char nbf[12];

// Upstream leaned on atoi from stdlib; there is none here.
static int atoi(const char *s)
{
    int sign = 1, v = 0;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return v * sign;
}

int next(void) // next char frm file, bump adr
{
    char ch;
    adrptr++; // seek address in file
    ch = inbuf < inend ? *inbuf++ : LF;
    if (outsw) { // putting data in tmp file
        if (*tape == 0)
            tape = iotape; // rewind encryption tape
        if (outadr < DATSIZE)
            datbuf[outadr++] = ch ^ *tape; // encrypt & output char
        tape++;
    }
    return (ch);
}

static void putdat(char ch) // the leading 'X', so seekadr > 0
{
    if (outadr < DATSIZE)
        datbuf[outadr++] = ch;
}

int rnum(void) // read initial location num
{
    char *s;
    tape = iotape; // restart encryption tape
    for (s = nbf, *s = 0;; s++)
        if ((*s = next()) == TAB || *s == '\n' || *s == LF)
            break;
    breakch = *s; // save char for rtrav()
    *s      = 0;  // got the number as ascii
    if (nbf[0] == '-')
        return (-1);    // end of data
    return (atoi(nbf)); // convert it to integer
}

static void rtrav(void) // read travel table
{
    int locc;
    struct travlist *t = 0;
    char *s;
    char buf[12];
    int len, m, n, entries = 0;
    for (game.oldloc = -1;;) // get another line
    {
        if ((locc = rnum()) != game.oldloc && game.oldloc >= 0) // end of entry
        {
            t->next = 0; // terminate the old entry
        }
        if (locc == -1)
            return;
        if (locc != game.oldloc) // getting a new entry
        {
            t = heap_new<struct travlist>();
            if (!t)
                bug(31);
            game.travel[locc] = t;
            entries           = 0;
            game.oldloc       = locc;
        }
        for (s = buf, *s = 0;; s++) // get the newloc number /ASCII
            if ((*s = next()) == TAB || *s == LF)
                break;
        *s  = 0;
        len = length(buf) - 1; // quad long number handling
        if (len < 4)           // no "m" conditions
        {
            m = 0;
            n = atoi(buf); // newloc mod 1000 = newloc
        } else             // a long integer
        {
            n            = atoi(buf + len - 3);
            buf[len - 3] = 0; // terminate newloc/1000
            m            = atoi(buf);
        }
        while (breakch != LF) // only do one line at a time
        {
            if (entries++) {
                t->next = heap_new<struct travlist>();
                if (!t->next)
                    bug(31);
                t = t->next;
            }
            t->tverb      = rnum(); // get verb from the file
            t->tloc       = n;      // table entry mod 1000
            t->conditions = m;      // table entry / 1000
        }
    }
}

static void rdesc(int sect) // read description-format msgs
{
    int locc;
    int seekstart, maystart;
    outsw = 1; // these msgs go into tmp file
    if (sect == 1)
        putdat('X'); // so seekadr > 0
    adrptr = 0;
    for (game.oldloc = -1, seekstart = seekhere;;) {
        maystart = adrptr;                                     // maybe starting new entry
        if ((locc = rnum()) != game.oldloc && game.oldloc >= 0 // finished msg
            && !(sect == 5 && (locc == 0 || locc >= 100)))     // unless sect 5
        {
            switch (sect) // now put it into right table
            {
            case 1: // long descriptions
                game.ltext[game.oldloc].seekadr = seekhere;
                game.ltext[game.oldloc].txtlen  = maystart - seekstart;
                break;
            case 2: // short descriptions
                game.stext[game.oldloc].seekadr = seekhere;
                game.stext[game.oldloc].txtlen  = maystart - seekstart;
                break;
            case 5: // object descriptions
                game.ptext[game.oldloc].seekadr = seekhere;
                game.ptext[game.oldloc].txtlen  = maystart - seekstart;
                break;
            case 6: // random messages
                if (game.oldloc > RTXSIZ) {
                    adv_printf("Too many random msgs\n");
                    bug(32);
                }
                game.rtext[game.oldloc].seekadr = seekhere;
                game.rtext[game.oldloc].txtlen  = maystart - seekstart;
                break;
            case 10: // class messages
                game.ctext[game.clsses].seekadr = seekhere;
                game.ctext[game.clsses].txtlen  = maystart - seekstart;
                game.cval[game.clsses++]        = game.oldloc;
                break;
            case 12: // magic messages
                if (game.oldloc > MAGSIZ) {
                    adv_printf("Too many magic msgs\n");
                    bug(32);
                }
                game.mtext[game.oldloc].seekadr = seekhere;
                game.mtext[game.oldloc].txtlen  = maystart - seekstart;
                break;
            default:
                adv_printf("rdesc called with bad section\n");
                bug(32);
            }
            seekhere += maystart - seekstart;
        }
        if (locc < 0) {
            outsw = 0;     // turn off output
            seekhere += 3; // -1<delimiter>
            return;
        }
        if (sect != 5 || (locc > 0 && locc < 100)) {
            if (game.oldloc != locc) // starting a new message
                seekstart = maystart;
            game.oldloc = locc;
        }
        FLUSHLF; // scan the line
    }
}

Task<void> getin(char **wrd1, char **wrd2) // get command from user
{
    static char wd1buf[MAXSTR], wd2buf[MAXSTR];
    String line;

    *wrd1     = wd1buf; // return ptr to internal string
    *wrd2     = wd2buf;
    wd1buf[0] = 0;
    wd2buf[0] = 0; // in case it isn't set here

    if (Task<void> t = adv_flush())
        co_await t;

    AdvEnd how = AdvEnd::Eof;
    if (Task<AdvEnd> t = adv_readline(line))
        how = co_await t;

    if (how == AdvEnd::Interrupt) // ^C, which was DEL on a VAX
    {
        delhit++;
        co_return;
    }
    if (how == AdvEnd::Eof) // nothing more will be typed
    {
        copystr("quit", wd1buf);
        co_return;
    }

    // Two words out of the line, as the character loop used to do it: blanks
    // separate them, the rest of the line is thrown away, and anything past
    // MAXSTR is a complaint rather than an overrun.
    Str s      = line.str();
    usize i    = 0;
    char *dest = wd1buf;
    int first = 1, numch = 0;
    while (i < s.size()) {
        char c = s[i++];
        if (c >= 'A' && c <= 'Z')
            c = c - ('A' - 'a'); // convert to upper case
        if (c == ' ' || c == TAB) {
            if (numch == 0)
                continue; // initial blank
            if (!first)
                break; // finished 2nd word
            first = numch = 0;
            dest          = wd2buf;
            continue;
        }
        if (++numch >= MAXSTR) // string too long
        {
            adv_printf("Give me a break!!\n");
            wd1buf[0] = wd2buf[0] = 0;
            co_return;
        }
        dest[numch - 1] = c;
        dest[numch]     = 0;
    }
    co_return;
}

Task<i32> yes(int x, int y, int z) // confirm with rspeak
{
    int result = -1;
    String line;
    for (;;) {
        rspeak(x); // tell him what we want
        if (Task<void> t = adv_flush())
            co_await t;
        AdvEnd how = AdvEnd::Eof;
        if (Task<AdvEnd> t = adv_readline(line))
            how = co_await t;
        if (how != AdvEnd::Enter) // ^C or an end of input answers
        {
            result = TRUE;
            break;
        }
        char ch = line.empty() ? 0 : line.str()[0];
        if (ch == 'y' || ch == 'Y')
            result = TRUE;
        else if (ch == 'n' || ch == 'N')
            result = FALSE;
        if (result >= 0)
            break;
        adv_printf("Please answer the question.\n");
    }
    if (result == TRUE)
        rspeak(y);
    if (result == FALSE)
        rspeak(z);
    co_return result;
}

Task<i32> yesm(int x, int y, int z) // confirm with mspeak
{
    int result = -1;
    String line;
    for (;;) {
        mspeak(x); // tell him what we want
        if (Task<void> t = adv_flush())
            co_await t;
        AdvEnd how = AdvEnd::Eof;
        if (Task<AdvEnd> t = adv_readline(line))
            how = co_await t;
        if (how != AdvEnd::Enter) {
            result = TRUE;
            break;
        }
        char ch = line.empty() ? 0 : line.str()[0];
        if (ch == 'y' || ch == 'Y')
            result = TRUE;
        else if (ch == 'n' || ch == 'N')
            result = FALSE;
        if (result >= 0)
            break;
        adv_printf("Please answer the question.\n");
    }
    if (result == TRUE)
        mspeak(y);
    if (result == FALSE)
        mspeak(z);
    co_return result;
}

static void rvoc(void)
{
    char *s; // read the vocabulary
    int index;
    char buf[6];
    for (;;) {
        index = rnum();
        if (index < 0)
            break;
        for (s = buf, *s = 0;; s++) // get the word
            if ((*s = next()) == TAB || *s == '\n' || *s == LF || *s == ' ')
                break;
        // terminate word with newline, LF, tab, blank
        if (*s != '\n' && *s != LF)
            FLUSHLF; // can be comments
        *s = 0;
        vocab(buf, -2, index);
    }
}

static void rlocs(void) // initial object locations
{
    for (;;) {
        if ((game.obj = rnum()) < 0)
            break;
        game.plac[game.obj] = rnum(); // initial loc for this obj
        if (breakch == TAB)           // there's another entry
            game.fixd[game.obj] = rnum();
        else
            game.fixd[game.obj] = 0;
    }
}

static void rdflt(void) // default verb messages
{
    for (;;) {
        if ((game.verb = rnum()) < 0)
            break;
        game.actspk[game.verb] = rnum();
    }
}

static void rliq(void) // liquid assets &c: cond bits
{
    int bitnum;
    for (;;) // read new bit list
    {
        if ((bitnum = rnum()) < 0)
            break;
        for (;;) // read locs for bits
        {
            game.cond[rnum()] |= setbit[bitnum];
            if (breakch == LF)
                break;
        }
    }
}

static void rhints(void)
{
    int hintnum, i;
    game.hntmax = 0;
    for (;;) {
        if ((hintnum = rnum()) < 0)
            break;
        for (i = 1; i < 5; i++)
            game.hints[hintnum][i] = rnum();
        if (hintnum > game.hntmax)
            game.hntmax = hintnum;
    }
}

void rdata(void) // read all data from orig file
{
    int sect;
    char ch;
    inbuf  = GLORKZ; // all the data lives in here
    inend  = GLORKZ + GLORKZ_LEN;
    datbuf = static_cast<char *>(heap_alloc(DATSIZE)); // the text lines go here
    if (!datbuf)
        bug(30);
    outadr      = 0;
    game.clsses = 1;
    for (;;) // read data sections
    {
        sect = next() - '0';     // 1st digit of section number
        if ((ch = next()) != LF) // is there a second digit?
        {
            FLUSHLF;
            sect = 10 * sect + ch - '0';
        }
        switch (sect) {
        case 0: // finished reading database
            return;
        case 1: // long form descriptions
            rdesc(1);
            break;
        case 2: // short form descriptions
            rdesc(2);
            break;
        case 3: // travel table
            rtrav();
            break;
        case 4: // vocabulary
            rvoc();
            break;
        case 5: // object descriptions
            rdesc(5);
            break;
        case 6: // arbitrary messages
            rdesc(6);
            break;
        case 7: // object locations
            rlocs();
            break;
        case 8: // action defaults
            rdflt();
            break;
        case 9: // liquid assets
            rliq();
            break;
        case 10: // class messages
            rdesc(10);
            break;
        case 11: // hints
            rhints();
            break;
        case 12: // magic messages
            rdesc(12);
            break;
        default:
            adv_printf("Invalid data section number: %d\n", sect);
            bug(32);
        }
        if (breakch != LF) // routines return after "-1"
            FLUSHLF;
    }
}

void rspeak(int msg)
{
    if (msg != 0)
        speak(&game.rtext[msg]);
}

void mspeak(int msg)
{
    if (msg != 0)
        speak(&game.mtext[msg]);
}

// The longest message in glorkz is 1460 bytes; upstream used alloca.
static char tbuf[2048];

void speak(struct text *msg) // read, decrypt, and print a message (not ptext)
{                            // msg is a pointer to seek address and length of mess
    char *s, nonfirst;
    if (msg->seekadr + msg->txtlen > DATSIZE || msg->txtlen >= sizeof tbuf) {
        adv_printf("Corrupted dat file!\n");
        return;
    }
    __builtin_memcpy(tbuf, datbuf + msg->seekadr, msg->txtlen);
    s        = tbuf;
    nonfirst = 0;
    while (s - tbuf < msg->txtlen) // read a line at a time
    {
        tape = iotape; // restart decryption tape
        while ((*s++ ^ *tape++) != TAB)
            ; // read past loc num
        // assume tape is longer than location number
        // plus the lookahead put together
        if ((*s ^ *tape) == '>' && (*(s + 1) ^ *(tape + 1)) == '$' &&
            (*(s + 2) ^ *(tape + 2)) == '<')
            break;
        if (game.blklin && !nonfirst++)
            adv_putc('\n');
        do {
            if (*tape == 0)
                tape = iotape; // rewind decryp tape
            adv_putc(*s ^ *tape);
        } while ((*s++ ^ *tape++) != LF); // better end with LF
    }
}

void pspeak(int msg, // read, decrypt an print a ptext message
            int skip)
// msg is the number of all the p msgs for this place
// assumes object 1 doesn't have prop 1, obj 2 no prop 2 &c
{
    char *s, nonfirst;
    char *numst;
    int lstr;
    lstr = game.ptext[msg].txtlen;
    if (game.ptext[msg].seekadr + lstr > DATSIZE || usize(lstr) >= sizeof tbuf) {
        adv_printf("Corrupted dat file!\n");
        return;
    }
    __builtin_memcpy(tbuf, datbuf + game.ptext[msg].seekadr, lstr);
    s        = tbuf;
    nonfirst = 0;
    while (s - tbuf < lstr) // read a line at a time
    {
        tape = iotape; // restart decryption tape
        for (numst = s; (*s ^= *tape++) != TAB; s++)
            ;     // get number
        *s++ = 0; // decrypting number within the string
        if (atoi(numst) != 100 * skip && skip >= 0) {
            while ((*s++ ^ *tape++) != LF) // flush the line
                if (*tape == 0)
                    tape = iotape;
            continue;
        }
        if ((*s ^ *tape) == '>' && (*(s + 1) ^ *(tape + 1)) == '$' &&
            (*(s + 2) ^ *(tape + 2)) == '<')
            break;
        if (game.blklin && !nonfirst++)
            adv_putc('\n');
        do {
            if (*tape == 0)
                tape = iotape;
            adv_putc(*s ^ *tape);
        } while ((*s++ ^ *tape++) != LF); // better end with LF
        if (skip < 0)
            break;
    }
}
