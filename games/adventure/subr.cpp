// Re-coding of advent in C: the statement functions -- upstream subr.c.
#include "hdr.h"

// Statement functions

int Game::toting(int objj)
{
    if (place_[objj] == -1)
        return (TRUE);
    else
        return (FALSE);
}

int Game::here(int objj)
{
    if (place_[objj] == loc_ || toting(objj))
        return (TRUE);
    else
        return (FALSE);
}

int Game::at(int objj)
{
    if (place_[objj] == loc_ || fixed_[objj] == loc_)
        return (TRUE);
    else
        return (FALSE);
}

int Game::liq2(int pbotl)
{
    return ((1 - pbotl) * water_ + (pbotl / 2) * (water_ + oil_));
}

int Game::liq(int foo)
{
    int i;
    (void)foo;
    i = prop_[bottle_];
    if (i > -1 - i)
        return (liq2(i));
    else
        return (liq2(-1 - i));
}

int Game::liqloc(int locc) // may want to clean this one up a bit
{
    int i, j, l;
    i = cond_[locc] / 2;
    j = ((i * 2) % 8) - 5;
    l = cond_[locc] / 4;
    l = l % 2;
    return (liq2(j * l + 1));
}

int Game::bitset(int l, int n)
{
    if (cond_[l] & SETBIT[n])
        return (TRUE);
    return (FALSE);
}

int Game::forced(int locc)
{
    if (cond_[locc] == 2)
        return (TRUE);
    return (FALSE);
}

int Game::dark(int foo)
{
    (void)foo;
    if ((cond_[loc_] % 2) == 0 && (prop_[lamp_] == 0 || !here(lamp_)))
        return (TRUE);
    return (FALSE);
}

int Game::pct(int n)
{
    if (ran(100) < n)
        return (TRUE);
    return (FALSE);
}
