// Re-coding of advent in C: termination routines -- upstream done.c.
//
// Ported to Braam.  done() and die() return ADV_OVER instead of calling exit().
#include "hdr.h"

int score(void) // sort of like 20000
{
    int scor, i;
    game.mxscor = scor = 0;
    for (i = 50; i <= game.maxtrs; i++) {
        if (game.ptext[i].txtlen == 0)
            continue;
        game.k = 12;
        if (i == game.chest)
            game.k = 14;
        if (i > game.chest)
            game.k = 16;
        if (game.prop[i] >= 0)
            scor += 2;
        if (game.place[i] == 3 && game.prop[i] == 0)
            scor += game.k - 2;
        game.mxscor += game.k;
    }
    scor += (game.maxdie - game.numdie) * 10;
    game.mxscor += game.maxdie * 10;
    if (!(game.scorng || game.gaveup))
        scor += 4;
    game.mxscor += 4;
    if (game.dflag != 0)
        scor += 25;
    game.mxscor += 25;
    if (game.closng)
        scor += 25;
    game.mxscor += 25;
    if (game.closed) {
        if (game.bonus == 0)
            scor += 10;
        if (game.bonus == 135)
            scor += 25;
        if (game.bonus == 134)
            scor += 30;
        if (game.bonus == 133)
            scor += 45;
    }
    game.mxscor += 45;
    if (game.place[game.magzin] == 108)
        scor++;
    game.mxscor++;
    scor += 2;
    game.mxscor += 2;
    for (i = 1; i <= game.hntmax; i++)
        if (game.hinted[i])
            scor -= game.hints[i][2];
    return (scor);
}

// game is over
// entry=1 means goto 13000, entry=2 means goto 20000, 3=19000
Task<i32> done(int entry)
{
    int i, sc;
    if (entry == 1)
        mspeak(1);
    if (entry == 3)
        rspeak(136);
    adv_printf("\n\n\nYou scored %d out of a ", (sc = score()));
    adv_printf("possible %d using %d turns.\n", game.mxscor, game.turns);
    adv_status = 0;
    for (i = 1; i <= game.clsses; i++)
        if (game.cval[i] >= sc) {
            speak(&game.ctext[i]);
            if (i == game.clsses - 1) {
                adv_printf("To achieve the next higher rating");
                adv_printf(" would be a neat trick!\n\n");
                adv_printf("Congratulations!!\n");
                co_return ADV_OVER;
            }
            game.k = game.cval[i] + 1 - sc;
            adv_printf("To achieve the next higher rating, you need");
            adv_printf(" %d more point", game.k);
            if (game.k == 1)
                adv_printf(".\n");
            else
                adv_printf("s.\n");
            co_return ADV_OVER;
        }
    adv_printf("You just went off my scale!!!\n");
    co_return ADV_OVER;
}

Task<i32> die( // label 90
    int entry)
{
    int i, yea;
    if (entry != 99) {
        rspeak(23);
        game.oldlc2 = game.loc;
    }
    if (game.closng) // 99
    {
        rspeak(131);
        game.numdie++;
        co_return co_await done(2);
    }
    yea = co_await yes(81 + game.numdie * 2, 82 + game.numdie * 2, 54);
    game.numdie++;
    if (game.numdie == game.maxdie || !yea)
        co_return co_await done(2);
    game.place[game.water] = 0;
    game.place[game.oil]   = 0;
    if (toting(game.lamp))
        game.prop[game.lamp] = 0;
    for (i = 100; i >= 1; i--) {
        if (!toting(i))
            continue;
        game.k = game.oldlc2;
        if (i == game.lamp)
            game.k = 1;
        drop(i, game.k);
    }
    game.loc    = 3;
    game.oldloc = game.loc;
    co_return 2000;
}
