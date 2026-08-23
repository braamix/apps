// Re-coding of advent in C: user i/o -- upstream io.c, less the glorkz parser,
// which is in data.cpp.
#include "hdr.h"

Task<void> Game::getin() // get command from user
{
    String line;

    wd1_[0] = 0;
    wd2_[0] = 0; // in case it isn't set here

    if (Task<void> t = adv_flush())
        co_await t;

    AdvEnd how = AdvEnd::Eof;
    if (Task<AdvEnd> t = adv_readline(line))
        how = co_await t;

    if (how == AdvEnd::Interrupt) // ^C, which was DEL on a VAX
    {
        delhit_++;
        co_return;
    }
    if (how == AdvEnd::Eof) // nothing more will be typed
    {
        copystr("quit", wd1_);
        co_return;
    }

    // Two words out of the line, as the character loop used to do it: blanks
    // separate them, the rest of the line is thrown away, and anything past
    // MAXSTR is a complaint rather than an overrun.
    Str s      = line.str();
    usize i    = 0;
    char *dest = wd1_;
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
            dest          = wd2_;
            continue;
        }
        if (++numch >= MAXSTR) // string too long
        {
            adv_printf("Give me a break!!\n");
            wd1_[0] = wd2_[0] = 0;
            co_return;
        }
        dest[numch - 1] = c;
        dest[numch]     = 0;
    }
    co_return;
}

Task<i32> Game::yes(int x, int y, int z) // confirm with rspeak
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

Task<i32> Game::yesm(int x, int y, int z) // confirm with mspeak
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

void Game::rspeak(int msg)
{
    if (msg != 0)
        speak(&rtext_[msg]);
}

void Game::mspeak(int msg)
{
    if (msg != 0)
        speak(&mtext_[msg]);
}

// The longest message in glorkz is 1460 bytes; upstream used alloca.
static char tbuf[2048];

void Game::speak(Text *msg) // read, decrypt, and print a message (not ptext)
{                           // msg is a pointer to seek address and length of mess
    char *s, nonfirst;
    const char *tape;
    if (msg->seekadr + msg->txtlen > DATSIZE || msg->txtlen >= sizeof tbuf) {
        adv_printf("Corrupted dat file!\n");
        return;
    }
    __builtin_memcpy(tbuf, dat_.data() + msg->seekadr, msg->txtlen);
    s        = tbuf;
    nonfirst = 0;
    while (s - tbuf < msg->txtlen) // read a line at a time
    {
        tape = IOTAPE; // restart decryption tape
        while ((*s++ ^ *tape++) != TAB)
            ; // read past loc num
        // assume tape is longer than location number
        // plus the lookahead put together
        if ((*s ^ *tape) == '>' && (*(s + 1) ^ *(tape + 1)) == '$' &&
            (*(s + 2) ^ *(tape + 2)) == '<')
            break;
        if (blklin_ && !nonfirst++)
            adv_putc('\n');
        do {
            if (*tape == 0)
                tape = IOTAPE; // rewind decryp tape
            adv_putc(*s ^ *tape);
        } while ((*s++ ^ *tape++) != LF); // better end with LF
    }
}

void Game::pspeak(int msg, // read, decrypt an print a ptext message
                  int skip)
// msg is the number of all the p msgs for this place
// assumes object 1 doesn't have prop 1, obj 2 no prop 2 &c
{
    char *s, nonfirst;
    const char *tape;
    char *numst;
    int lstr;
    lstr = ptext_[msg].txtlen;
    if (ptext_[msg].seekadr + lstr > DATSIZE || usize(lstr) >= sizeof tbuf) {
        adv_printf("Corrupted dat file!\n");
        return;
    }
    __builtin_memcpy(tbuf, dat_.data() + ptext_[msg].seekadr, lstr);
    s        = tbuf;
    nonfirst = 0;
    while (s - tbuf < lstr) // read a line at a time
    {
        tape = IOTAPE; // restart decryption tape
        for (numst = s; (*s ^= *tape++) != TAB; s++)
            ;     // get number
        *s++ = 0; // decrypting number within the string
        if (atoi(numst) != 100 * skip && skip >= 0) {
            while ((*s++ ^ *tape++) != LF) // flush the line
                if (*tape == 0)
                    tape = IOTAPE;
            continue;
        }
        if ((*s ^ *tape) == '>' && (*(s + 1) ^ *(tape + 1)) == '$' &&
            (*(s + 2) ^ *(tape + 2)) == '<')
            break;
        if (blklin_ && !nonfirst++)
            adv_putc('\n');
        do {
            if (*tape == 0)
                tape = IOTAPE;
            adv_putc(*s ^ *tape);
        } while ((*s++ ^ *tape++) != LF); // better end with LF
        if (skip < 0)
            break;
    }
}
