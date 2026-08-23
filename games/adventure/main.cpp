// Re-coding of advent in C: closing the cave, and the command loop --
// upstream main.c, less the mode that compiled glorkz into adventure.dat.
//
// Ported to Braam.  exit() does not exist, so the status travels up to
// proc_main; and anything that reads is a coroutine.
#include "hdr.h"

int closing(void) // 10000
{
    int i;
    game.prop[game.grate] = game.prop[game.fissur] = 0;
    for (i = 1; i <= 6; i++) {
        game.dseen[i] = FALSE;
        game.dloc[i]  = 0;
    }
    move(game.troll, 0);
    move(game.troll + 100, 0);
    move(game.troll2, game.plac[game.troll]);
    move(game.troll2 + 100, game.fixd[game.troll]);
    juggle(game.chasm);
    if (game.prop[game.bear] != 3)
        dstroy(game.bear);
    game.prop[game.chain]  = 0;
    game.fixed[game.chain] = 0;
    game.prop[game.axe]    = 0;
    game.fixed[game.axe]   = 0;
    rspeak(129);
    game.clock1 = -1;
    game.closng = TRUE;
    return (19999);
}

int caveclose(void) // 11000
{
    int i;
    game.prop[game.bottle] = put(game.bottle, 115, 1);
    game.prop[game.plant]  = put(game.plant, 115, 0);
    game.prop[game.oyster] = put(game.oyster, 115, 0);
    game.prop[game.lamp]   = put(game.lamp, 115, 0);
    game.prop[game.rod]    = put(game.rod, 115, 0);
    game.prop[game.dwarf]  = put(game.dwarf, 115, 0);
    game.loc               = 115;
    game.oldloc            = 115;
    game.newloc            = 115;

    put(game.grate, 116, 0);
    game.prop[game.snake]  = put(game.snake, 116, 1);
    game.prop[game.bird]   = put(game.bird, 116, 1);
    game.prop[game.cage]   = put(game.cage, 116, 0);
    game.prop[game.rod2]   = put(game.rod2, 116, 0);
    game.prop[game.pillow] = put(game.pillow, 116, 0);

    game.prop[game.mirror]  = put(game.mirror, 115, 0);
    game.fixed[game.mirror] = 116;

    for (i = 1; i <= 100; i++)
        if (toting(i))
            dstroy(i);
    rspeak(132);
    game.closed = TRUE;
    return (2);
}

// The main program.  Upstream's main(), less the mode that compiled glorkz
// into adventure.dat: the data is in the binary and parsed at startup.
static Task<i32> adventure(Args args)
{
    int i;
    int rval;
    struct text *kk;
    Str savfile;

    if (args.size() > 1)
        savfile = args[1];

    rdata(); // read glorkz, build the tables
    linkdata();
    poof();

    if (!savfile.empty()) {
        Result<void> r = Err(Error::NotFound);
        if (Task<Result<void>> t = restore(savfile))
            r = co_await t;
        if (r.is_ok()) {
            if (co_await start(0) == ADV_OVER) // restarting game : 8305
                co_return adv_status;
            game.k = game.null;
            goto l8;
        }
        adv_printf("Your forged file dissappears in a puff of greasy black smoke! (poof)\n");
        if (Task<Result<void>> t = remove_path(savfile, false))
            co_await t;
        co_return 0;
    }
    if (co_await start(0) == ADV_OVER)
        co_return adv_status;
    if (co_await startup() == ADV_OVER) // prepare for a user
        co_return adv_status;
    game.blklin = TRUE;

    for (;;) // main command loop (label 2)
    {
        if (game.newloc < 9 && game.newloc != 0 && game.closng) {
            rspeak(130);            // if closing leave only by
            game.newloc = game.loc; // main office
            if (!game.panic)
                game.clock2 = 15;
            game.panic = TRUE;
        }

        rval = fdwarf(); // dwarf stuff
        if (rval == 99) {
            if (co_await die(99) == ADV_OVER)
                co_return adv_status;
        }

    l2000:
        if (game.loc == 0) { // label 2000
            if (co_await die(99) == ADV_OVER)
                co_return adv_status;
        }
        kk = &game.stext[game.loc];
        if ((game.abb[game.loc] % game.abbnum) == 0 || kk->seekadr == 0)
            kk = &game.ltext[game.loc];
        if (!forced(game.loc) && dark(0)) {
            if (game.wzdark && pct(35)) {
                if (co_await die(90) == ADV_OVER)
                    co_return adv_status;
                goto l2000;
            }
            kk = &game.rtext[16];
        }
        if (toting(game.bear))
            rspeak(141); // 2001
        speak(kk);
        game.k = 1;
        if (forced(game.loc))
            goto l8;
        if (game.loc == 33 && pct(25) && !game.closng)
            rspeak(8);
        if (!dark(0)) {
            game.abb[game.loc]++;
            for (i = game.atloc[game.loc]; i != 0; i = game.plink[i]) // 2004
            {
                game.obj = i;
                if (game.obj > 100)
                    game.obj -= 100;
                if (game.obj == game.steps && toting(game.nugget))
                    continue;
                if (game.prop[game.obj] < 0) {
                    if (game.closed)
                        continue;
                    game.prop[game.obj] = 0;
                    if (game.obj == game.rug || game.obj == game.chain)
                        game.prop[game.obj] = 1;
                    game.tally--;
                    if (game.tally == game.tally2 && game.tally != 0)
                        if (game.limit > 35)
                            game.limit = 35;
                }
                {
                    int pk = game.prop[game.obj]; // 2006
                    if (game.obj == game.steps && game.loc == game.fixed[game.steps])
                        pk = 1;
                    pspeak(game.obj, pk);
                }
            } // 2008
            goto l2012;
        l2009:
            game.k = 54; // 2009
        l2010:
            game.spk = game.k;
        l2011:
            rspeak(game.spk);
        }
    l2012:
        game.verb = 0; // 2012
        game.obj  = 0;
    l2600:
        if (Task<void> t = checkhints()) // to 2600-2602
            co_await t;
        if (game.closed) {
            if (game.prop[game.oyster] < 0 && toting(game.oyster))
                pspeak(game.oyster, 1);
            for (i = 1; i < 100; i++)
                if (toting(i) && game.prop[i] < 0) // 2604
                    game.prop[i] = -1 - game.prop[i];
        }
        game.wzdark = dark(0); // 2605
        if (game.knfloc > 0 && game.knfloc != game.loc)
            game.knfloc = 1;
        if (Task<void> t = getin(&wd1, &wd2))
            co_await t;
        if (delhit) // user typed a DEL
        {
            delhit = 0;           // reset counter
            copystr("quit", wd1); // pretend he's quitting
            *wd2 = 0;
        }
    l2608:
        if ((game.foobar = -game.foobar) > 0)
            game.foobar = 0; // 2608
        // should check here for "magic mode"
        game.turns++;

        if (game.verb == game.say && *wd2 != 0)
            game.verb = 0;
        if (game.verb == game.say)
            goto l4090;
        if (game.tally == 0 && game.loc >= 15 && game.loc != 33)
            game.clock1--;
        if (game.clock1 == 0) {
            closing(); // to 10000
            goto l19999;
        }
        if (game.clock1 < 0)
            game.clock2--;
        if (game.clock2 == 0) {
            caveclose(); // to 11000
            continue;    // back to 2
        }
        if (game.prop[game.lamp] == 1)
            game.limit--;
        if (game.limit <= 30 && here(game.batter) && game.prop[game.batter] == 0 &&
            here(game.lamp)) {
            rspeak(188); // 12000
            game.prop[game.batter] = 1;
            if (toting(game.batter))
                drop(game.batter, game.loc);
            game.limit  = game.limit + 2500;
            game.lmwarn = FALSE;
            goto l19999;
        }
        if (game.limit == 0) {
            game.limit           = -1; // 12400
            game.prop[game.lamp] = 0;
            rspeak(184);
            goto l19999;
        }
        if (game.limit < 0 && game.loc <= 8) {
            rspeak(185); // 12600
            game.gaveup = TRUE;
            co_await done(2); // to 20000
            co_return adv_status;
        }
        if (game.limit <= 30) {
            if (game.lmwarn || !here(game.lamp))
                goto l19999; // 12200
            game.lmwarn = TRUE;
            game.spk    = 187;
            if (game.place[game.batter] == 0)
                game.spk = 183;
            if (game.prop[game.batter] == 1)
                game.spk = 189;
            rspeak(game.spk);
        }
    l19999:
        game.k = 43;
        if (liqloc(game.loc) == game.water)
            game.k = 70;
        if (weq(wd1, "enter") && (weq(wd2, "strea") || weq(wd2, "water")))
            goto l2010;
        if (weq(wd1, "enter") && *wd2 != 0)
            goto l2800;
        if ((!weq(wd1, "water") && !weq(wd1, "oil")) || (!weq(wd2, "plant") && !weq(wd2, "door")))
            goto l2610;
        if (at(vocab(wd2, 1, 0)))
            copystr("pour", wd2);
    l2610:
        if (weq(wd1, "west"))
            if (++game.iwest == 10)
                rspeak(17);
    l2630:
        i = vocab(wd1, -1, 0);
        if (i == -1) {
            game.spk = 60; // 3000
            if (pct(20))
                game.spk = 61;
            if (pct(20))
                game.spk = 13;
            rspeak(game.spk);
            goto l2600;
        }
        game.k  = i % 1000;
        game.kq = i / 1000 + 1;
        switch (game.kq) {
        case 1:
            goto l8;
        case 2:
            goto l5000;
        case 3:
            goto l4000;
        case 4:
            goto l2010;
        default:
            adv_printf("Error 22\n");
            bug(22);
        }

    l8:
        switch (march()) {
        case 2:
            continue; // i.e. goto l2
        case 99:
            rval = co_await die(99);
            switch (rval) {
            case 2000:
                goto l2000;
            case ADV_OVER:
                co_return adv_status;
            default:
                bug(111);
            }
        default:
            bug(110);
        }

    l2800:
        copystr(wd2, wd1);
        *wd2 = 0;
        goto l2610;

    l4000:
        game.verb = game.k;
        game.spk  = game.actspk[game.verb];
        if (*wd2 != 0 && game.verb != game.say)
            goto l2800;
        if (game.verb == game.say)
            game.obj = *wd2;
        if (game.obj != 0)
            goto l4090;

        switch (game.verb) {
        case 1: // take = 8010
            if (game.atloc[game.loc] == 0 || game.plink[game.atloc[game.loc]] != 0)
                goto l8000;
            for (i = 1; i <= 5; i++)
                if (game.dloc[i] == game.loc && game.dflag >= 2)
                    goto l8000;
            game.obj = game.atloc[game.loc];
            goto l9010;
        case 2:
        case 3:
        case 9: // 8000 : drop,say,wave
        case 10:
        case 16:
        case 17: // calm,rub,toss
        case 19:
        case 21:
        case 28: // find,feed,break
        case 29: // wake
        l8000:
            adv_printf("%s what?\n", wd1);
            game.obj = 0;
            goto l2600;
        case 4:
        case 6: // 8040 open,lock
            game.spk = 28;
            if (here(game.clam))
                game.obj = game.clam;
            if (here(game.oyster))
                game.obj = game.oyster;
            if (at(game.door))
                game.obj = game.door;
            if (at(game.grate))
                game.obj = game.grate;
            if (game.obj != 0 && here(game.chain))
                goto l8000;
            if (here(game.chain))
                game.obj = game.chain;
            if (game.obj == 0)
                goto l2011;
            goto l9040;
        case 5:
            goto l2009; // nothing
        case 7:
            goto l9070; // on
        case 8:
            goto l9080; // off
        case 11:
            goto l8000; // walk
        case 12:
            goto l9120; // kill
        case 13:
            goto l9130; // pour
        case 14:        // eat: 8140
            if (!here(game.food))
                goto l8000;
        l8142:
            dstroy(game.food);
            game.spk = 72;
            goto l2011;
        case 15:
            goto l9150; // drink
        case 18:        // quit: 8180
            game.gaveup = co_await yes(22, 54, 54);
            if (game.gaveup) {
                co_await done(2); // 8185
                co_return adv_status;
            }
            goto l2012;
        case 20: // invent=8200
            game.spk = 98;
            for (i = 1; i <= 100; i++) {
                if (i != game.bear && toting(i)) {
                    if (game.spk == 98)
                        rspeak(99);
                    game.blklin = FALSE;
                    pspeak(i, -1);
                    game.blklin = TRUE;
                    game.spk    = 0;
                }
            }
            if (toting(game.bear))
                game.spk = 141;
            goto l2011;
        case 22:
            goto l9220; // fill
        case 23:
            goto l9230; // blast
        case 24:        // score: 8240
            game.scorng = TRUE;
            adv_printf("If you were to quit now, you would score");
            adv_printf(" %d out of a possible ", score());
            adv_printf("%d.", game.mxscor);
            game.scorng = FALSE;
            game.gaveup = co_await yes(143, 54, 54);
            if (game.gaveup) {
                co_await done(2);
                co_return adv_status;
            }
            goto l2012;
        case 25: // foo: 8250
            game.k   = vocab(wd1, 3, 0);
            game.spk = 42;
            if (game.foobar == 1 - game.k)
                goto l8252;
            if (game.foobar != 0)
                game.spk = 151;
            goto l2011;
        l8252:
            game.foobar = game.k;
            if (game.k != 4)
                goto l2009;
            game.foobar = 0;
            if (game.place[game.eggs] == game.plac[game.eggs] ||
                (toting(game.eggs) && game.loc == game.plac[game.eggs]))
                goto l2011;
            if (game.place[game.eggs] == 0 && game.place[game.troll] == 0 &&
                game.prop[game.troll] == 0)
                game.prop[game.troll] = 1;
            game.k = 2;
            if (here(game.eggs))
                game.k = 1;
            if (game.loc == game.plac[game.eggs])
                game.k = 0;
            move(game.eggs, game.plac[game.eggs]);
            pspeak(game.eggs, game.k);
            goto l2012;
        case 26: // brief=8260
            game.spk    = 156;
            game.abbnum = 10000;
            game.detail = 3;
            goto l2011;
        case 27: // read=8270
            if (here(game.magzin))
                game.obj = game.magzin;
            if (here(game.tablet))
                game.obj = game.obj * 100 + game.tablet;
            if (here(game.messag))
                game.obj = game.obj * 100 + game.messag;
            if (game.closed && toting(game.oyster))
                game.obj = game.oyster;
            if (game.obj > 100 || game.obj == 0 || dark(0))
                goto l8000;
            goto l9270;
        case 30: // suspend=8300
            game.spk = 201;
            adv_printf("I can suspend your adventure for you so");
            adv_printf(" you can resume later, but\n");
            adv_printf("you will have to wait at least");
            adv_printf(" %d minutes before continuing.", game.latncy);
            if (!co_await yes(200, 54, 54))
                goto l2012;
            if (Task<void> t = adv_clock())
                co_await t;
            datime(&game.saved, &game.savet);
            if (co_await ciao() == ADV_OVER)
                co_return adv_status;
            continue;
        case 31: // hours=8310
            adv_printf("Colossal cave is closed 9am-5pm Mon ");
            adv_printf("through Fri except holidays.\n");
            goto l2012;
        default:
            bug(23);
        }

    l4090:
        switch (game.verb) {
        case 1: // take = 9010
        l9010:
            switch (trtake()) {
            case 2011:
                goto l2011;
            case 9220:
                goto l9220;
            case 2009:
                goto l2009;
            case 2012:
                goto l2012;
            default:
                bug(102);
            }
        l9020:
        case 2: // drop = 9020
            switch (trdrop()) {
            case 2011:
                goto l2011;
            case 19000:
                co_await done(3);
                co_return adv_status;
            case 2012:
                goto l2012;
            default:
                bug(105);
            }
        case 3:
            switch (trsay()) {
            case 2012:
                goto l2012;
            case 2630:
                goto l2630;
            default:
                bug(107);
            }
        l9040:
        case 4:
        case 6: // open, close
            switch (tropen()) {
            case 2011:
                goto l2011;
            case 2010:
                goto l2010;
            default:
                bug(106);
            }
        case 5:
            goto l2009; // nothing
        case 7:         // on   9070
        l9070:
            if (!here(game.lamp))
                goto l2011;
            game.spk = 184;
            if (game.limit < 0)
                goto l2011;
            game.prop[game.lamp] = 1;
            rspeak(39);
            if (game.wzdark)
                goto l2000;
            goto l2012;

        case 8: // off
        l9080:
            if (!here(game.lamp))
                goto l2011;
            game.prop[game.lamp] = 0;
            rspeak(40);
            if (dark(0))
                rspeak(16);
            goto l2012;

        case 9: // wave
            if ((!toting(game.obj)) && (game.obj != game.rod || !toting(game.rod2)))
                game.spk = 29;
            if (game.obj != game.rod || !at(game.fissur) || !toting(game.obj) || game.closng)
                goto l2011;
            game.prop[game.fissur] = 1 - game.prop[game.fissur];
            pspeak(game.fissur, 2 - game.prop[game.fissur]);
            goto l2012;
        case 10:
        case 11:
        case 18: // calm, walk, quit
        case 24:
        case 25:
        case 26: // score, foo, brief
        case 30:
        case 31: // suspend, hours
            goto l2011;
        l9120:
        case 12: // kill
            rval = co_await trkill();
            switch (rval) {
            case 8000:
                goto l8000;
            case 8:
                goto l8;
            case 2011:
                goto l2011;
            case 2608:
                goto l2608;
            case 19000:
                co_await done(3);
                co_return adv_status;
            default:
                bug(112);
            }
        l9130:
        case 13: // pour
            if (game.obj == game.bottle || game.obj == 0)
                game.obj = liq(0);
            if (game.obj == 0)
                goto l8000;
            if (!toting(game.obj))
                goto l2011;
            game.spk = 78;
            if (game.obj != game.oil && game.obj != game.water)
                goto l2011;
            game.prop[game.bottle] = 1;
            game.place[game.obj]   = 0;
            game.spk               = 77;
            if (!(at(game.plant) || at(game.door)))
                goto l2011;
            if (at(game.door)) {
                game.prop[game.door] = 0; // 9132
                if (game.obj == game.oil)
                    game.prop[game.door] = 1;
                game.spk = 113 + game.prop[game.door];
                goto l2011;
            }
            game.spk = 112;
            if (game.obj != game.water)
                goto l2011;
            pspeak(game.plant, game.prop[game.plant] + 1);
            game.prop[game.plant]  = (game.prop[game.plant] + 2) % 6;
            game.prop[game.plant2] = game.prop[game.plant] / 2;
            game.k                 = game.null;
            goto l8;
        case 14: // 9140 - eat
            if (game.obj == game.food)
                goto l8142;
            if (game.obj == game.bird || game.obj == game.snake || game.obj == game.clam ||
                game.obj == game.oyster || game.obj == game.dwarf || game.obj == game.dragon ||
                game.obj == game.troll || game.obj == game.bear)
                game.spk = 71;
            goto l2011;
        l9150:
        case 15: // 9150 - drink
            if (game.obj == 0 && liqloc(game.loc) != game.water &&
                (liq(0) != game.water || !here(game.bottle)))
                goto l8000;
            if (game.obj != 0 && game.obj != game.water)
                game.spk = 110;
            if (game.spk == 110 || liq(0) != game.water || !here(game.bottle))
                goto l2011;
            game.prop[game.bottle] = 1;
            game.place[game.water] = 0;
            game.spk               = 74;
            goto l2011;
        case 16: // 9160: rub
            if (game.obj != game.lamp)
                game.spk = 76;
            goto l2011;
        case 17: // 9170: throw
            switch (trtoss()) {
            case 2011:
                goto l2011;
            case 9020:
                goto l9020;
            case 9120:
                goto l9120;
            case 8:
                goto l8;
            case 9210:
                goto l9210;
            default:
                bug(113);
            }
        case 19:
        case 20: // 9190: find, invent
            if (at(game.obj) || (liq(0) == game.obj && at(game.bottle)) ||
                game.k == liqloc(game.loc))
                game.spk = 94;
            for (i = 1; i <= 5; i++)
                if (game.dloc[i] == game.loc && game.dflag >= 2 && game.obj == game.dwarf)
                    game.spk = 94;
            if (game.closed)
                game.spk = 138;
            if (toting(game.obj))
                game.spk = 24;
            goto l2011;
        l9210:
        case 21: // feed
            switch (trfeed()) {
            case 2011:
                goto l2011;
            default:
                bug(114);
            }
        l9220:
        case 22: // fill
            switch (trfill()) {
            case 2011:
                goto l2011;
            case 8000:
                goto l8000;
            case 9020:
                goto l9020;
            default:
                bug(115);
            }
        l9230:
        case 23: // blast
            if (game.prop[game.rod2] < 0 || !game.closed)
                goto l2011;
            game.bonus = 133;
            if (game.loc == 115)
                game.bonus = 134;
            if (here(game.rod2))
                game.bonus = 135;
            rspeak(game.bonus);
            co_await done(2);
            co_return adv_status;
        l9270:
        case 27: // read
            if (dark(0))
                goto l5190;
            if (game.obj == game.magzin)
                game.spk = 190;
            if (game.obj == game.tablet)
                game.spk = 196;
            if (game.obj == game.messag)
                game.spk = 191;
            if (game.obj == game.oyster && game.hinted[2] && toting(game.oyster))
                game.spk = 194;
            if (game.obj != game.oyster || game.hinted[2] || !toting(game.oyster) || !game.closed)
                goto l2011;
            game.hinted[2] = co_await yes(192, 193, 54);
            goto l2012;
        case 28: // break
            if (game.obj == game.mirror)
                game.spk = 148;
            if (game.obj == game.vase && game.prop[game.vase] == 0) {
                game.spk = 198;
                if (toting(game.vase))
                    drop(game.vase, game.loc);
                game.prop[game.vase]  = 2;
                game.fixed[game.vase] = -1;
                goto l2011;
            }
            if (game.obj != game.mirror || !game.closed)
                goto l2011;
            rspeak(197);
            co_await done(3);
            co_return adv_status;

        case 29: // wake
            if (game.obj != game.dwarf || !game.closed)
                goto l2011;
            rspeak(199);
            co_await done(3);
            co_return adv_status;

        default:
            bug(24);
        }

    l5000:
        game.obj = game.k;
        if (game.fixed[game.k] != game.loc && !here(game.k))
            goto l5100;
    l5010:
        if (*wd2 != 0)
            goto l2800;
        if (game.verb != 0)
            goto l4090;
        adv_printf("What do you want to do with the %s?\n", wd1);
        goto l2600;
    l5100:
        if (game.k != game.grate)
            goto l5110;
        if (game.loc == 1 || game.loc == 4 || game.loc == 7)
            game.k = game.dprssn;
        if (game.loc > 9 && game.loc < 15)
            game.k = game.entrnc;
        if (game.k != game.grate)
            goto l8;
    l5110:
        if (game.k != game.dwarf)
            goto l5120;
        for (i = 1; i <= 5; i++)
            if (game.dloc[i] == game.loc && game.dflag >= 2)
                goto l5010;
    l5120:
        if ((liq(0) == game.k && here(game.bottle)) || game.k == liqloc(game.loc))
            goto l5010;
        if (game.obj != game.plant || !at(game.plant2) || game.prop[game.plant2] == 0)
            goto l5130;
        game.obj = game.plant2;
        goto l5010;
    l5130:
        if (game.obj != game.knife || game.knfloc != game.loc)
            goto l5140;
        game.knfloc = -1;
        game.spk    = 116;
        goto l2011;
    l5140:
        if (game.obj != game.rod || !here(game.rod2))
            goto l5190;
        game.obj = game.rod2;
        goto l5010;
    l5190:
        if ((game.verb == game.find || game.verb == game.invent) && *wd2 == 0)
            goto l5010;
        adv_printf("I see no %s here\n", wd1);
        goto l2012;
    }
}

Task<i32> proc_main(Args args)
{
    if (Task<void> t = adv_input_init())
        co_await t;

    i32 rc = 0;
    if (Task<i32> t = adventure(args))
        rc = co_await t;
    else
        rc = 1;

    if (Task<void> t = adv_flush())
        co_await t;
    if (Task<void> t = adv_input_done())
        co_await t;
    co_return adv_write_failed() ? 1 : rc;
}
