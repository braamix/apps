// Re-coding of advent in C: the dwarves and the travel table -- upstream subr.c.
#include "hdr.h"

int fdwarf(void) // 71
{
    int i, j;
    struct travlist *kk;

    if (game.newloc != game.loc && !forced(game.loc) && !bitset(game.loc, 3)) {
        for (i = 1; i <= 5; i++) {
            if (game.odloc[i] != game.newloc || !game.dseen[i])
                continue;
            game.newloc = game.loc;
            rspeak(2);
            break;
        }
    }
    game.loc = game.newloc; // 74
    if (game.loc == 0 || forced(game.loc) || bitset(game.newloc, 3))
        return (2000);
    if (game.dflag == 0) {
        if (game.loc >= 15)
            game.dflag = 1;
        return (2000);
    }
    if (game.dflag == 1) // 6000
    {
        if (game.loc < 15 || pct(95))
            return (2000);
        game.dflag = 2;
        for (i = 1; i <= 2; i++) {
            j = 1 + ran(5);
            if (pct(50) && game.saved == -1)
                game.dloc[j] = 0; // 6001
        }
        for (i = 1; i <= 5; i++) {
            if (game.dloc[i] == game.loc)
                game.dloc[i] = game.daltlc;
            game.odloc[i] = game.dloc[i]; // 6002
        }
        rspeak(3);
        drop(game.axe, game.loc);
        return (2000);
    }
    game.dtotal = game.attack = game.stick = 0; // 6010
    for (i = 1; i <= 6; i++)                    // loop to 6030
    {
        if (game.dloc[i] == 0)
            continue;
        j = 1;
        for (kk = game.travel[game.dloc[i]]; kk != 0; kk = kk->next) {
            game.newloc = kk->tloc;
            if (game.newloc > 300 || game.newloc < 15 || game.newloc == game.odloc[i] ||
                (j > 1 && game.newloc == game.tk[j - 1]) || j >= 20 ||
                game.newloc == game.dloc[i] || forced(game.newloc) ||
                (i == 6 && bitset(game.newloc, 3)) || kk->conditions == 100)
                continue;
            game.tk[j++] = game.newloc;
        }
        game.tk[j] = game.odloc[i]; // 6016
        if (j >= 2)
            j--;
        j             = 1 + ran(j);
        game.odloc[i] = game.dloc[i];
        game.dloc[i]  = game.tk[j];
        game.dseen[i] = (game.dseen[i] && game.loc >= 15) ||
                        (game.dloc[i] == game.loc || game.odloc[i] == game.loc);
        if (!game.dseen[i])
            continue; // i.e. goto 6030
        game.dloc[i] = game.loc;
        if (i == 6) // pirate's spotted him
        {
            if (game.loc == game.chloc || game.prop[game.chest] >= 0)
                continue;
            game.k = 0;
            for (j = 50; j <= game.maxtrs; j++) // loop to 6020
            {
                if (j == game.pyram &&
                    (game.loc == game.plac[game.pyram] || game.loc == game.plac[game.emrald]))
                    goto l6020;
                if (toting(j))
                    goto l6022;
            l6020:
                if (here(j))
                    game.k = 1;
            } // 6020
            if (game.tally == game.tally2 + 1 && game.k == 0 && game.place[game.chest] == 0 &&
                here(game.lamp) && game.prop[game.lamp] == 1)
                goto l6025;
            if (game.odloc[6] != game.dloc[6] && pct(20))
                rspeak(127);
            continue; // to 6030
        l6022:
            rspeak(128);
            if (game.place[game.messag] == 0)
                move(game.chest, game.chloc);
            move(game.messag, game.chloc2);
            for (j = 50; j <= game.maxtrs; j++) // loop to 6023
            {
                if (j == game.pyram &&
                    (game.loc == game.plac[game.pyram] || game.loc == game.plac[game.emrald]))
                    continue;
                if (at(j) && game.fixed[j] == 0)
                    carry(j, game.loc);
                if (toting(j))
                    drop(j, game.chloc);
            }
        l6024:
            game.dloc[6] = game.odloc[6] = game.chloc;
            game.dseen[6]                = FALSE;
            continue;
        l6025:
            rspeak(186);
            move(game.chest, game.chloc);
            move(game.messag, game.chloc2);
            goto l6024;
        }
        game.dtotal++; // 6027
        if (game.odloc[i] != game.dloc[i])
            continue;
        game.attack++;
        if (game.knfloc >= 0)
            game.knfloc = game.loc;
        if (ran(1000) < 95 * (game.dflag - 2))
            game.stick++;
    } // 6030
    if (game.dtotal == 0)
        return (2000);
    if (game.dtotal != 1) {
        adv_printf("There are %d threatening little dwarves ", game.dtotal);
        adv_printf("in the room with you.\n");
    } else
        rspeak(4);
    if (game.attack == 0)
        return (2000);
    if (game.dflag == 2)
        game.dflag = 3;
    if (game.saved != -1)
        game.dflag = 20;
    if (game.attack != 1) {
        adv_printf("%d of them throw knives at you!\n", game.attack);
        game.k = 6;
    l82:
        if (game.stick <= 1) // 82
        {
            rspeak(game.k + game.stick);
            if (game.stick == 0)
                return (2000);
        } else
            adv_printf("%d of them get you!\n", game.stick); // 83
        game.oldlc2 = game.loc;
        return (99);
    }
    rspeak(5);
    game.k = 52;
    goto l82;
}

static int mback(void) // 20
{
    struct travlist *tk2, *j;
    int ll;
    if (forced(game.k = game.oldloc))
        game.k = game.oldlc2; // k=location
    game.oldlc2 = game.oldloc;
    game.oldloc = game.loc;
    tk2         = 0;
    if (game.k == game.loc) {
        rspeak(91);
        return (2);
    }
    for (; tkk != 0; tkk = tkk->next) // 21
    {
        ll = tkk->tloc;
        if (ll == game.k) {
            game.k = tkk->tverb; // k back to verb
            tkk    = game.travel[game.loc];
            return (9);
        }
        if (ll <= 300) {
            j = game.travel[game.loc];
            if (forced(ll) && game.k == j->tloc)
                tk2 = tkk;
        }
    }
    tkk = tk2; // 23
    if (tkk != 0) {
        game.k = tkk->tverb;
        tkk    = game.travel[game.loc];
        return (9);
    }
    rspeak(140);
    return (2);
}

static int badmove(void) // 20
{
    game.spk = 12;
    if (game.k >= 43 && game.k <= 50)
        game.spk = 9;
    if (game.k == 29 || game.k == 30)
        game.spk = 9;
    if (game.k == 7 || game.k == 36 || game.k == 37)
        game.spk = 10;
    if (game.k == 11 || game.k == 19)
        game.spk = 11;
    if (game.verb == game.find || game.verb == game.invent)
        game.spk = 59;
    if (game.k == 62 || game.k == 65)
        game.spk = 42;
    if (game.k == 17)
        game.spk = 80;
    rspeak(game.spk);
    return (2);
}

static int trbridge(void) // 30300
{
    if (game.prop[game.troll] == 1) {
        pspeak(game.troll, 1);
        game.prop[game.troll] = 0;
        move(game.troll2, 0);
        move(game.troll2 + 100, 0);
        move(game.troll, game.plac[game.troll]);
        move(game.troll + 100, game.fixd[game.troll]);
        juggle(game.chasm);
        game.newloc = game.loc;
        return (2);
    }
    game.newloc = game.plac[game.troll] + game.fixd[game.troll] - game.loc; // 30310
    if (game.prop[game.troll] == 0)
        game.prop[game.troll] = 1;
    if (!toting(game.bear))
        return (2);
    rspeak(162);
    game.prop[game.chasm] = 1;
    game.prop[game.troll] = 2;
    drop(game.bear, game.newloc);
    game.fixed[game.bear] = -1;
    game.prop[game.bear]  = 3;
    if (game.prop[game.spices] < 0)
        game.tally2++;
    game.oldlc2 = game.newloc;
    return (99);
}

static int specials(void) // 30000
{
    switch (game.newloc -= 300) {
    case 1: // 30100
        game.newloc = 99 + 100 - game.loc;
        if (game.holdng == 0 || (game.holdng == 1 && toting(game.emrald)))
            return (2);
        game.newloc = game.loc;
        rspeak(117);
        return (2);
    case 2: // 30200
        drop(game.emrald, game.loc);
        return (12);
    case 3: // to 30300
        return (trbridge());
    default:
        bug(29);
    }
}

int march(void) // label 8
{
    int ll1, ll2;

    tkk = game.travel[game.newloc = game.loc];
    if (tkk == 0)
        bug(26);
    if (game.k == game.null)
        return (2);
    if (game.k == game.cave) // 40
    {
        if (game.loc < 8)
            rspeak(57);
        if (game.loc >= 8)
            rspeak(58);
        return (2);
    }
    if (game.k == game.look) // 30
    {
        if (game.detail++ < 3)
            rspeak(15);
        game.wzdark        = FALSE;
        game.abb[game.loc] = 0;
        return (2);
    }
    if (game.k == game.back) // 20
    {
        switch (mback()) {
        case 2:
            return (2);
        case 9:
            goto l9;
        default:
            bug(100);
        }
    }
    game.oldlc2 = game.oldloc;
    game.oldloc = game.loc;
l9:
    for (; tkk != 0; tkk = tkk->next)
        if (tkk->tverb == 1 || tkk->tverb == game.k)
            break;
    if (tkk == 0) {
        badmove();
        return (2);
    }
l11:
    ll1         = tkk->conditions; // 11
    ll2         = tkk->tloc;
    game.newloc = ll1;               // newloc=conditions
    game.k      = game.newloc % 100; // k used for prob
    if (game.newloc <= 300) {
        if (game.newloc <= 100) // 13
        {
            if (game.newloc != 0 && !pct(game.newloc))
                goto l12; // 14
        l16:
            game.newloc = ll2; // newloc=location
            if (game.newloc <= 300)
                return (2);
            if (game.newloc <= 500)
                switch (specials()) // to 30000
                {
                case 2:
                    return (2);
                case 12:
                    goto l12;
                case 99:
                    return (99);
                default:
                    bug(101);
                }
            rspeak(game.newloc - 500);
            game.newloc = game.loc;
            return (2);
        }
        if (toting(game.k) || (game.newloc > 200 && at(game.k)))
            goto l16;
        goto l12;
    }
    if (game.prop[game.k] != (game.newloc / 100) - 3)
        goto l16; // newloc still conditions
l12:              // alternative to probability move
    for (; tkk != 0; tkk = tkk->next)
        if (tkk->tloc != ll2 || tkk->conditions != ll1)
            break;
    if (tkk == 0)
        bug(25);
    goto l11;
}
