// Re-coding of advent in C: the statement functions -- upstream subr.c.
#include "hdr.h"

// Statement functions

int toting(int objj)
{
    if (game.place[objj] == -1)
        return (TRUE);
    else
        return (FALSE);
}

int here(int objj)
{
    if (game.place[objj] == game.loc || toting(objj))
        return (TRUE);
    else
        return (FALSE);
}

int at(int objj)
{
    if (game.place[objj] == game.loc || game.fixed[objj] == game.loc)
        return (TRUE);
    else
        return (FALSE);
}

static int liq2(int pbotl)
{
    return ((1 - pbotl) * game.water + (pbotl / 2) * (game.water + game.oil));
}

int liq(int foo)
{
    int i;
    (void)foo;
    i = game.prop[game.bottle];
    if (i > -1 - i)
        return (liq2(i));
    else
        return (liq2(-1 - i));
}

int liqloc(int locc) // may want to clean this one up a bit
{
    int i, j, l;
    i = game.cond[locc] / 2;
    j = ((i * 2) % 8) - 5;
    l = game.cond[locc] / 4;
    l = l % 2;
    return (liq2(j * l + 1));
}

int bitset(int l, int n)
{
    if (game.cond[l] & setbit[n])
        return (TRUE);
    return (FALSE);
}

int forced(int locc)
{
    if (game.cond[locc] == 2)
        return (TRUE);
    return (FALSE);
}

int dark(int foo)
{
    (void)foo;
    if ((game.cond[game.loc] % 2) == 0 && (game.prop[game.lamp] == 0 || !here(game.lamp)))
        return (TRUE);
    return (FALSE);
}

int pct(int n)
{
    if (ran(100) < n)
        return (TRUE);
    return (FALSE);
}
