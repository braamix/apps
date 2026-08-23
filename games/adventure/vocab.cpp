// Re-coding of advent in C: data structure routines -- upstream vocab.c.
#include "hdr.h"

void Game::dstroy(int object)
{
    move(object, 0);
}

void Game::juggle(int object)
{
    int i, j;
    i = place_[object];
    j = fixed_[object];
    move(object, i);
    move(object + 100, j);
}

void Game::move(int object, int where)
{
    int from;
    if (object <= 100)
        from = place_[object];
    else
        from = fixed_[object - 100];
    if (from > 0 && from <= 300)
        carry(object, from);
    drop(object, where);
}

int Game::put(int object, int where, int pval)
{
    move(object, where);
    return (-1 - pval);
}

void Game::carry(int object, int where)
{
    int temp;
    if (object <= 100) {
        if (place_[object] == -1)
            return;
        place_[object] = -1;
        holdng_++;
    }
    if (atloc_[where] == object) {
        atloc_[where] = plink_[object];
        return;
    }
    for (temp = atloc_[where]; plink_[temp] != object; temp = plink_[temp])
        ;
    plink_[temp] = plink_[object];
}

void Game::drop(int object, int where)
{
    if (object > 100)
        fixed_[object - 100] = where;
    else {
        if (place_[object] == -1)
            holdng_--;
        place_[object] = where;
    }
    if (where <= 0)
        return;
    plink_[object] = atloc_[where];
    atloc_[where]  = object;
}

// Good hash function.
// (C) 2006 Serge Vakulenko
static unsigned int rot13_hash(const char *str)
{
    unsigned int len, hash, c;

    // Max 5 utf8 characters.
    len = 5;
    for (hash = 0; len > 0; str++) {
        c = (unsigned char)*str;
        if (c == 0)
            break;
        if (!(c & 0x80))
            len--;
        hash += (unsigned char)*str;
        hash -= (hash << 13) | (hash >> 19);
    }
    return hash;
}

int Game::vocab(                // look up or store a word
    const char *word, int type, // -2 for store, -1 for user word, >=0 for canned lookup
    int value)                  // used for storing only
{
    int adr;
    unsigned int hash32, hash;
    HashTab *h;

    hash32 = rot13_hash(word); // some kind of hash
    hash   = hash32 % HTSIZE;  // put it into range of table

    for (adr = hash;; adr++) { // look for entry in table
        if (adr == HTSIZE)
            adr = 0;    // wrap around
        h = &voc_[adr]; // point at the entry
        switch (type) {
        case -2:        // fill in entry
            if (h->val) // already got an entry?
                goto exitloop2;
            h->val  = value;
            h->hash = hash32;
            return 0; // entry unused

        case -1:             // looking up user word
            if (h->val == 0) // not found
                return (-1);
            if ((unsigned int)h->hash != hash32)
                goto exitloop2;
            return h->val; // the word matched o.k.

        default: // looking up known word
            if (h->val == 0) {
                adv_printf("Unable to find %s in vocab\n", word);
                bug(33);
            }
            if ((unsigned int)h->hash != hash32)
                goto exitloop2;
            // the word matched o.k.
            if (h->val / 1000 != type)
                continue;
            return h->val % 1000;
        }
    exitloop2: // hashed entry does not match
        if (adr + 1 == (int)hash || (adr == HTSIZE && hash == 0)) {
            adv_printf("Hash table overflow\n");
            bug(34);
        }
    }
}

void copystr( // copy one string to another
    const char *w1, char *w2)
{
    const char *s;
    char *t;
    for (s = w1, t = w2; *s;)
        *t++ = *s++;
    *t = 0;
}

int weq(                            // compare words
    const char *w1, const char *w2) // w1 is user, w2 is system
{
    const char *s, *t;
    int i;
    s = w1;
    t = w2;
    for (i = 0; i < 5; i++) // compare at most 5 chars
    {
        if (*t == 0 && *s == 0)
            return (TRUE);
        if (*s++ != *t++)
            return (FALSE);
    }
    return (TRUE);
}

int length( // includes 0 at end
    const char *str)
{
    const char *s;
    int n;
    for (n = 0, s = str; *s++;)
        n++;
    return (n + 1);
}
