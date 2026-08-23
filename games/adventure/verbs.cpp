// Re-coding of advent in C: the hints and the verb handlers -- upstream subr.c.
//
// A handler returns the label its caller must jump back into.
#include "hdr.h"

Task<void> checkhints(void) // 2600 &c
{
    int hint;
    for (hint = 4; hint <= game.hntmax; hint++) {
        if (game.hinted[hint])
            continue;
        if (!bitset(game.loc, hint))
            game.hintlc[hint] = -1;
        game.hintlc[hint]++;
        if (game.hintlc[hint] < game.hints[hint][1])
            continue;
        switch (hint) {
        case 4: // 40400
            if (game.prop[game.grate] == 0 && !here(game.keys))
                goto l40010;
            goto l40020;
        case 5: // 40500
            if (here(game.bird) && toting(game.rod) && game.obj == game.bird)
                goto l40010;
            continue; // i.e. goto l40030
        case 6:       // 40600
            if (here(game.snake) && !here(game.bird))
                goto l40010;
            goto l40020;
        case 7: // 40700
            if (game.atloc[game.loc] == 0 && game.atloc[game.oldloc] == 0 &&
                game.atloc[game.oldlc2] == 0 && game.holdng > 1)
                goto l40010;
            goto l40020;
        case 8: // 40800
            if (game.prop[game.emrald] != -1 && game.prop[game.pyram] == -1)
                goto l40010;
            goto l40020;
        case 9:
            goto l40010; // 40900
        default:
            bug(27);
        }
    l40010:
        game.hintlc[hint] = 0;
        if (!co_await yes(game.hints[hint][3], 0, 54))
            continue;
        adv_printf("I am prepared to give you a hint, but it will ");
        adv_printf("cost you %d points.\n", game.hints[hint][2]);
        game.hinted[hint] = co_await yes(175, game.hints[hint][4], 54);
    l40020:
        game.hintlc[hint] = 0;
    }
    co_return;
}

int trsay(void) // 9030
{
    int i;
    if (*wd2 != 0)
        copystr(wd2, wd1);
    i = vocab(wd1, -1, 0);
    if (i == 62 || i == 65 || i == 71 || i == 2025) {
        *wd2     = 0;
        game.obj = 0;
        return (2630);
    }
    adv_printf("\nOkay, \"%s\".\n", wd2);
    return (2012);
}

int trtake(void) // 9010
{
    if (toting(game.obj))
        return (2011); // 9010
    game.spk = 25;
    if (game.obj == game.plant && game.prop[game.plant] <= 0)
        game.spk = 115;
    if (game.obj == game.bear && game.prop[game.bear] == 1)
        game.spk = 169;
    if (game.obj == game.chain && game.prop[game.bear] != 0)
        game.spk = 170;
    if (game.fixed[game.obj] != 0)
        return (2011);
    if (game.obj == game.water || game.obj == game.oil) {
        if (here(game.bottle) && liq(0) == game.obj) {
            game.obj = game.bottle;
            goto l9017;
        }
        game.obj = game.bottle;
        if (toting(game.bottle) && game.prop[game.bottle] == 1)
            return (9220);
        if (game.prop[game.bottle] != 1)
            game.spk = 105;
        if (!toting(game.bottle))
            game.spk = 104;
        return (2011);
    }
l9017:
    if (game.holdng >= 7) {
        rspeak(92);
        return (2012);
    }
    if (game.obj == game.bird) {
        if (game.prop[game.bird] != 0)
            goto l9014;
        if (toting(game.rod)) {
            rspeak(26);
            return (2012);
        }
        if (!toting(game.cage)) // 9013
        {
            rspeak(27);
            return (2012);
        }
        game.prop[game.bird] = 1; // 9015
    }
l9014:
    if ((game.obj == game.bird || game.obj == game.cage) && game.prop[game.bird] != 0)
        carry(game.bird + game.cage - game.obj, game.loc);
    carry(game.obj, game.loc);
    game.k = liq(0);
    if (game.obj == game.bottle && game.k != 0)
        game.place[game.k] = -1;
    return (2009);
}

static int dropper(void) // 9021
{
    game.k = liq(0);
    if (game.k == game.obj)
        game.obj = game.bottle;
    if (game.obj == game.bottle && game.k != 0)
        game.place[game.k] = 0;
    if (game.obj == game.cage && game.prop[game.bird] != 0)
        drop(game.bird, game.loc);
    if (game.obj == game.bird)
        game.prop[game.bird] = 0;
    drop(game.obj, game.loc);
    return (2012);
}

int trdrop(void) // 9020
{
    if (toting(game.rod2) && game.obj == game.rod && !toting(game.rod))
        game.obj = game.rod2;
    if (!toting(game.obj))
        return (2011);
    if (game.obj == game.bird && here(game.snake)) {
        rspeak(30);
        if (game.closed)
            return (19000);
        dstroy(game.snake);
        game.prop[game.snake] = 1;
        return (dropper());
    }
    if (game.obj == game.coins && here(game.vend)) // 9024
    {
        dstroy(game.coins);
        drop(game.batter, game.loc);
        pspeak(game.batter, 0);
        return (2012);
    }
    if (game.obj == game.bird && at(game.dragon) && game.prop[game.dragon] == 0) // 9025
    {
        rspeak(154);
        dstroy(game.bird);
        game.prop[game.bird] = 0;
        if (game.place[game.snake] == game.plac[game.snake])
            game.tally2--;
        return (2012);
    }
    if (game.obj == game.bear && at(game.troll)) // 9026
    {
        rspeak(163);
        move(game.troll, 0);
        move(game.troll + 100, 0);
        move(game.troll2, game.plac[game.troll]);
        move(game.troll2 + 100, game.fixd[game.troll]);
        juggle(game.chasm);
        game.prop[game.troll] = 2;
        return (dropper());
    }
    if (game.obj != game.vase || game.loc == game.plac[game.pillow]) // 9027
    {
        rspeak(54);
        return (dropper());
    }
    game.prop[game.vase] = 2; // 9028
    if (at(game.pillow))
        game.prop[game.vase] = 0;
    pspeak(game.vase, game.prop[game.vase] + 1);
    if (game.prop[game.vase] != 0)
        game.fixed[game.vase] = -1;
    return (dropper());
}

int tropen(void) // 9040
{
    if (game.obj == game.clam || game.obj == game.oyster) {
        game.k = 0; // 9046
        if (game.obj == game.oyster)
            game.k = 1;
        game.spk = 124 + game.k;
        if (toting(game.obj))
            game.spk = 120 + game.k;
        if (!toting(game.tridnt))
            game.spk = 122 + game.k;
        if (game.verb == game.lock)
            game.spk = 61;
        if (game.spk != 124)
            return (2011);
        dstroy(game.clam);
        drop(game.oyster, game.loc);
        drop(game.pearl, 105);
        return (2011);
    }
    if (game.obj == game.door)
        game.spk = 111;
    if (game.obj == game.door && game.prop[game.door] == 1)
        game.spk = 54;
    if (game.obj == game.cage)
        game.spk = 32;
    if (game.obj == game.keys)
        game.spk = 55;
    if (game.obj == game.grate || game.obj == game.chain)
        game.spk = 31;
    if (game.spk != 31 || !here(game.keys))
        return (2011);
    if (game.obj == game.chain) {
        if (game.verb == game.lock) {
            game.spk = 172; // 9049: lock
            if (game.prop[game.chain] != 0)
                game.spk = 34;
            if (game.loc != game.plac[game.chain])
                game.spk = 173;
            if (game.spk != 172)
                return (2011);
            game.prop[game.chain] = 2;
            if (toting(game.chain))
                drop(game.chain, game.loc);
            game.fixed[game.chain] = -1;
            return (2011);
        }
        game.spk = 171;
        if (game.prop[game.bear] == 0)
            game.spk = 41;
        if (game.prop[game.chain] == 0)
            game.spk = 37;
        if (game.spk != 171)
            return (2011);
        game.prop[game.chain]  = 0;
        game.fixed[game.chain] = 0;
        if (game.prop[game.bear] != 3)
            game.prop[game.bear] = 2;
        game.fixed[game.bear] = 2 - game.prop[game.bear];
        return (2011);
    }
    if (game.closng) {
        game.k = 130;
        if (!game.panic)
            game.clock2 = 15;
        game.panic = TRUE;
        return (2010);
    }
    game.k                = 34 + game.prop[game.grate]; // 9043
    game.prop[game.grate] = 1;
    if (game.verb == game.lock)
        game.prop[game.grate] = 0;
    game.k = game.k + 2 * game.prop[game.grate];
    return (2010);
}

Task<i32> trkill(void) // 9120
{
    int i;
    for (i = 1; i <= 5; i++)
        if (game.dloc[i] == game.loc && game.dflag >= 2)
            break;
    if (i == 6)
        i = 0;
    if (game.obj == 0) // 9122
    {
        if (i != 0)
            game.obj = game.dwarf;
        if (here(game.snake))
            game.obj = game.obj * 100 + game.snake;
        if (at(game.dragon) && game.prop[game.dragon] == 0)
            game.obj = game.obj * 100 + game.dragon;
        if (at(game.troll))
            game.obj = game.obj * 100 + game.troll;
        if (here(game.bear) && game.prop[game.bear] == 0)
            game.obj = game.obj * 100 + game.bear;
        if (game.obj > 100)
            co_return 8000;
        if (game.obj == 0) {
            if (here(game.bird) && game.verb != game.throw_)
                game.obj = game.bird;
            if (here(game.clam) || here(game.oyster))
                game.obj = 100 * game.obj + game.clam;
            if (game.obj > 100)
                co_return 8000;
        }
    }
    if (game.obj == game.bird) // 9124
    {
        game.spk = 137;
        if (game.closed)
            co_return 2011;
        dstroy(game.bird);
        game.prop[game.bird] = 0;
        if (game.place[game.snake] == game.plac[game.snake])
            game.tally2++;
        game.spk = 45;
    }
    if (game.obj == 0)
        game.spk = 44; // 9125
    if (game.obj == game.clam || game.obj == game.oyster)
        game.spk = 150;
    if (game.obj == game.snake)
        game.spk = 46;
    if (game.obj == game.dwarf)
        game.spk = 49;
    if (game.obj == game.dwarf && game.closed)
        co_return 19000;
    if (game.obj == game.dragon)
        game.spk = 147;
    if (game.obj == game.troll)
        game.spk = 157;
    if (game.obj == game.bear)
        game.spk = 165 + (game.prop[game.bear] + 1) / 2;
    if (game.obj != game.dragon || game.prop[game.dragon] != 0)
        co_return 2011;
    rspeak(49);
    game.verb = 0;
    game.obj  = 0;
    if (Task<void> t = getin(&wd1, &wd2))
        co_await t;
    if (!weq(wd1, "y") && !weq(wd1, "yes"))
        co_return 2608;
    pspeak(game.dragon, 1);
    game.prop[game.dragon] = 2;
    game.prop[game.rug]    = 0;
    game.k                 = (game.plac[game.dragon] + game.fixd[game.dragon]) / 2;
    move(game.dragon + 100, -1);
    move(game.rug + 100, 0);
    move(game.dragon, game.k);
    move(game.rug, game.k);
    for (game.obj = 1; game.obj <= 100; game.obj++)
        if (game.place[game.obj] == game.plac[game.dragon] ||
            game.place[game.obj] == game.fixd[game.dragon])
            move(game.obj, game.k);
    game.loc = game.k;
    game.k   = game.null;
    co_return 8;
}

int trtoss(void) // 9170: throw
{
    int i;
    if (toting(game.rod2) && game.obj == game.rod && !toting(game.rod))
        game.obj = game.rod2;
    if (!toting(game.obj))
        return (2011);
    if (game.obj >= 50 && game.obj <= game.maxtrs && at(game.troll)) {
        game.spk = 159; // 9178
        drop(game.obj, 0);
        move(game.troll, 0);
        move(game.troll + 100, 0);
        drop(game.troll2, game.plac[game.troll]);
        drop(game.troll2 + 100, game.fixd[game.troll]);
        juggle(game.chasm);
        return (2011);
    }
    if (game.obj == game.food && here(game.bear)) {
        game.obj = game.bear; // 9177
        return (9210);
    }
    if (game.obj != game.axe)
        return (9020);
    for (i = 1; i <= 5; i++) {
        if (game.dloc[i] == game.loc) {
            game.spk = 48; // 9172
            if (ran(3) == 0 || game.saved != -1)
            l9175: {
                rspeak(game.spk);
                drop(game.axe, game.loc);
                game.k = game.null;
                return (8);
            }
                game.dseen[i] = FALSE;
            game.dloc[i] = 0;
            game.spk     = 47;
            game.dkill++;
            if (game.dkill == 1)
                game.spk = 149;
            goto l9175;
        }
    }
    game.spk = 152;
    if (at(game.dragon) && game.prop[game.dragon] == 0)
        goto l9175;
    game.spk = 158;
    if (at(game.troll))
        goto l9175;
    if (here(game.bear) && game.prop[game.bear] == 0) {
        game.spk = 164;
        drop(game.axe, game.loc);
        game.fixed[game.axe] = -1;
        game.prop[game.axe]  = 1;
        juggle(game.bear);
        return (2011);
    }
    game.obj = 0;
    return (9120);
}

int trfeed(void) // 9210
{
    if (game.obj == game.bird) {
        game.spk = 100;
        return (2011);
    }
    if (game.obj == game.snake || game.obj == game.dragon || game.obj == game.troll) {
        game.spk = 102;
        if (game.obj == game.dragon && game.prop[game.dragon] != 0)
            game.spk = 110;
        if (game.obj == game.troll)
            game.spk = 182;
        if (game.obj != game.snake || game.closed || !here(game.bird))
            return (2011);
        game.spk = 101;
        dstroy(game.bird);
        game.prop[game.bird] = 0;
        game.tally2++;
        return (2011);
    }
    if (game.obj == game.dwarf) {
        if (!here(game.food))
            return (2011);
        game.spk = 103;
        game.dflag++;
        return (2011);
    }
    if (game.obj == game.bear) {
        if (game.prop[game.bear] == 0)
            game.spk = 102;
        if (game.prop[game.bear] == 3)
            game.spk = 110;
        if (!here(game.food))
            return (2011);
        dstroy(game.food);
        game.prop[game.bear] = 1;
        game.fixed[game.axe] = 0;
        game.prop[game.axe]  = 0;
        game.spk             = 168;
        return (2011);
    }
    game.spk = 14;
    return (2011);
}

int trfill(void) // 9220
{
    if (game.obj == game.vase) {
        game.spk = 29;
        if (liqloc(game.loc) == 0)
            game.spk = 144;
        if (liqloc(game.loc) == 0 || !toting(game.vase))
            return (2011);
        rspeak(145);
        game.prop[game.vase]  = 2;
        game.fixed[game.vase] = -1;
        return (9020); // advent/10 goes to 9024
    }
    if (game.obj != 0 && game.obj != game.bottle)
        return (2011);
    if (game.obj == 0 && !here(game.bottle))
        return (8000);
    game.spk = 107;
    if (liqloc(game.loc) == 0)
        game.spk = 106;
    if (liq(0) != 0)
        game.spk = 105;
    if (game.spk != 107)
        return (2011);
    game.prop[game.bottle] = ((game.cond[game.loc] % 4) / 2) * 2;
    game.k                 = liq(0);
    if (toting(game.bottle))
        game.place[game.k] = -1;
    if (game.k == game.oil)
        game.spk = 108;
    return (2011);
}
