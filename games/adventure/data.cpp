// Re-coding of advent in C: the glorkz parser -- upstream io.c's rdata() and
// what it calls.
//
// Ported to Braam.  There is no seek syscall, so the message text upstream
// wrote to adventure.dat goes into Game::dat_ instead, which outlives this
// object because speak() indexes it at play time.  glorkz itself is in the
// binary rather than a file.
#include "data.h"

// Generated from glorkz by mkdata.py.
extern const char GLORKZ[];
extern const u32 GLORKZ_LEN;

#define FLUSHLF          \
    while (next() != LF) \
        ;

int DataReader::next(void) // next char frm file, bump adr
{
    char ch;
    adrptr++; // seek address in file
    ch = inbuf < inend ? *inbuf++ : LF;
    if (outsw) { // putting data in tmp file
        if (*tape == 0)
            tape = IOTAPE; // rewind encryption tape
        if (outadr < DATSIZE)
            g_.dat_[outadr++] = ch ^ *tape; // encrypt & output char
        tape++;
    }
    return (ch);
}

void DataReader::putdat(char ch) // the leading 'X', so seekadr > 0
{
    if (outadr < DATSIZE)
        g_.dat_[outadr++] = ch;
}

int DataReader::rnum(void) // read initial location num
{
    char *s;
    tape = IOTAPE; // restart encryption tape
    for (s = nbf, *s = 0;; s++)
        if ((*s = next()) == TAB || *s == '\n' || *s == LF)
            break;
    breakch = *s; // save char for rtrav()
    *s      = 0;  // got the number as ascii
    if (nbf[0] == '-')
        return (-1);    // end of data
    return (atoi(nbf)); // convert it to integer
}

void DataReader::rtrav(void) // read travel table
{
    int locc;
    char *s;
    char buf[12];
    int len, m, n;
    for (g_.oldloc_ = -1;;) // get another line
    {
        if ((locc = rnum()) == -1)
            return;
        if (locc != g_.oldloc_) // getting a new entry
        {
            g_.travel_[locc].clear(); // upstream's fresh head node
            g_.oldloc_ = locc;
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
            Travel t;
            t.tverb      = Motion(rnum()); // get verb from the file
            t.tloc       = n;              // table entry mod 1000
            t.conditions = m;              // table entry / 1000
            if (!g_.travel_[locc].push(t))
                bug(31);
        }
    }
}

void DataReader::rdesc(Section sect) // read description-format msgs
{
    int locc;
    int seekstart, maystart;
    outsw = 1; // these msgs go into tmp file
    if (sect == Section::Long)
        putdat('X'); // so seekadr > 0
    adrptr = 0;
    for (g_.oldloc_ = -1, seekstart = seekhere;;) {
        maystart = adrptr;                                   // maybe starting new entry
        if ((locc = rnum()) != g_.oldloc_ && g_.oldloc_ >= 0 // finished msg
            && !(sect == Section::Objects && (locc == 0 || locc >= 100))) // unless sect 5
        {
            switch (sect) // now put it into right table
            {
            case Section::Long: // long descriptions
                g_.ltext_[g_.oldloc_].seekadr = seekhere;
                g_.ltext_[g_.oldloc_].txtlen  = maystart - seekstart;
                break;
            case Section::Short: // short descriptions
                g_.stext_[g_.oldloc_].seekadr = seekhere;
                g_.stext_[g_.oldloc_].txtlen  = maystart - seekstart;
                break;
            case Section::Objects: // object descriptions
                g_.ptext_[g_.oldloc_].seekadr = seekhere;
                g_.ptext_[g_.oldloc_].txtlen  = maystart - seekstart;
                break;
            case Section::Messages: // random messages
                if (g_.oldloc_ > RTXSIZ) {
                    adv_printf("Too many random msgs\n");
                    bug(32);
                }
                g_.rtext_[g_.oldloc_].seekadr = seekhere;
                g_.rtext_[g_.oldloc_].txtlen  = maystart - seekstart;
                break;
            case Section::Classes: // class messages
                g_.ctext_[g_.clsses_].seekadr = seekhere;
                g_.ctext_[g_.clsses_].txtlen  = maystart - seekstart;
                g_.cval_[g_.clsses_++]        = g_.oldloc_;
                break;
            case Section::Magic: // magic messages
                if (g_.oldloc_ > MAGSIZ) {
                    adv_printf("Too many magic msgs\n");
                    bug(32);
                }
                g_.mtext_[g_.oldloc_].seekadr = seekhere;
                g_.mtext_[g_.oldloc_].txtlen  = maystart - seekstart;
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
        if (sect != Section::Objects || (locc > 0 && locc < 100)) {
            if (g_.oldloc_ != locc) // starting a new message
                seekstart = maystart;
            g_.oldloc_ = locc;
        }
        FLUSHLF; // scan the line
    }
}

void DataReader::rvoc(void)
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
        g_.vocab(buf, VOC_STORE, index);
    }
}

void DataReader::rlocs(void) // initial object locations
{
    for (;;) {
        if ((g_.obj_ = rnum()) < 0)
            break;
        g_.plac_[g_.obj_] = rnum(); // initial loc for this obj
        if (breakch == TAB)         // there's another entry
            g_.fixd_[g_.obj_] = rnum();
        else
            g_.fixd_[g_.obj_] = 0;
    }
}

void DataReader::rdflt(void) // default verb messages
{
    int v;
    for (;;) {
        v        = rnum();
        g_.verb_ = Verb(v); // upstream parsed into the global, the -1 too
        if (v < 0)
            break;
        g_.actspk_[usize(v)] = Msg(rnum());
    }
}

void DataReader::rliq(void) // liquid assets &c: cond bits
{
    int bitnum;
    for (;;) // read new bit list
    {
        if ((bitnum = rnum()) < 0)
            break;
        for (;;) // read locs for bits
        {
            g_.cond_[rnum()] |= Game::SETBIT[bitnum];
            if (breakch == LF)
                break;
        }
    }
}

void DataReader::rhints(void)
{
    int hintnum;
    g_.hntmax_ = 0;
    for (;;) {
        if ((hintnum = rnum()) < 0)
            break;
        HintRule &h = g_.hints_[usize(hintnum)]; // the four columns, in order
        h.turns     = i16(rnum());
        h.cost      = i16(rnum());
        h.question  = Msg(rnum());
        h.answer    = Msg(rnum());
        if (hintnum > g_.hntmax_)
            g_.hntmax_ = hintnum;
    }
}

void DataReader::read() // upstream rdata()
{
    int sect;
    char ch;
    inbuf      = GLORKZ; // all the data lives in here
    inend      = GLORKZ + GLORKZ_LEN;
    outadr     = 0;
    g_.clsses_ = 1;
    for (;;) // read data sections
    {
        sect = next() - '0';     // 1st digit of section number
        if ((ch = next()) != LF) // is there a second digit?
        {
            FLUSHLF;
            sect = 10 * sect + ch - '0';
        }
        switch (Section(sect)) {
        case Section::End: // finished reading database
            return;
        case Section::Long: // long form descriptions
            rdesc(Section::Long);
            break;
        case Section::Short: // short form descriptions
            rdesc(Section::Short);
            break;
        case Section::Travel:
            rtrav();
            break;
        case Section::Vocab:
            rvoc();
            break;
        case Section::Objects: // object descriptions
            rdesc(Section::Objects);
            break;
        case Section::Messages: // arbitrary messages
            rdesc(Section::Messages);
            break;
        case Section::Places: // object locations
            rlocs();
            break;
        case Section::Defaults: // action defaults
            rdflt();
            break;
        case Section::Liquid: // liquid assets
            rliq();
            break;
        case Section::Classes: // class messages
            rdesc(Section::Classes);
            break;
        case Section::Hints:
            rhints();
            break;
        case Section::Magic: // magic messages
            rdesc(Section::Magic);
            break;
        default:
            adv_printf("Invalid data section number: %d\n", sect);
            bug(32);
        }
        if (breakch != LF) // routines return after "-1"
            FLUSHLF;
    }
}
