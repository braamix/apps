// Re-coding of advent in C: closing the cave, and the command loop --
// upstream main.c, less the mode that compiled glorkz into adventure.dat.
//
// Ported to Braam.  exit() does not exist, so the status travels up to
// proc_main; and anything that reads is a coroutine.
#include "data.h"

Phase Game::closing() // 10000
{
    int i;
    prop_[grate_] = prop_[fissur_] = 0;
    for (i = 1; i <= 6; i++) {
        dseen_[i] = FALSE;
        dloc_[i]  = 0;
    }
    move(troll_, 0);
    move(troll_ + 100, 0);
    move(troll2_, plac_[troll_]);
    move(troll2_ + 100, fixd_[troll_]);
    juggle(chasm_);
    if (prop_[bear_] != 3)
        dstroy(bear_);
    prop_[chain_]  = 0;
    fixed_[chain_] = 0;
    prop_[axe_]    = 0;
    fixed_[axe_]   = 0;
    rspeak(129);
    clock1_ = -1;
    closng_ = TRUE;
    return Phase::Lamp;
}

Phase Game::caveclose() // 11000
{
    int i;
    prop_[bottle_] = put(bottle_, 115, 1);
    prop_[plant_]  = put(plant_, 115, 0);
    prop_[oyster_] = put(oyster_, 115, 0);
    prop_[lamp_]   = put(lamp_, 115, 0);
    prop_[rod_]    = put(rod_, 115, 0);
    prop_[dwarf_]  = put(dwarf_, 115, 0);
    loc_           = 115;
    oldloc_        = 115;
    newloc_        = 115;

    put(grate_, 116, 0);
    prop_[snake_]  = put(snake_, 116, 1);
    prop_[bird_]   = put(bird_, 116, 1);
    prop_[cage_]   = put(cage_, 116, 0);
    prop_[rod2_]   = put(rod2_, 116, 0);
    prop_[pillow_] = put(pillow_, 116, 0);

    prop_[mirror_]  = put(mirror_, 115, 0);
    fixed_[mirror_] = 116;

    for (i = 1; i <= 100; i++)
        if (toting(i))
            dstroy(i);
    rspeak(132);
    closed_ = TRUE;
    return Phase::Dwarves;
}

// The main program.  Upstream's main(), less the mode that compiled glorkz
// into adventure.dat: the data is in the binary and parsed at startup.
//
// The turn is a state machine.  Upstream's statement labels are methods and
// each returns the label to go to next; play() at the bottom dispatches.
Task<i32> Game::play(Args args)
{
    Str savfile;
    Phase p = Phase::Dwarves;

    if (args.size() > 1)
        savfile = args[1];

    DataReader(*this).read(); // read glorkz, build the tables
    linkdata();
    poof();

    if (!savfile.empty()) {
        Result<void> r = Err(Error::NotFound);
        if (Task<Result<void>> t = restore(savfile))
            r = co_await t;
        if (r.is_err()) {
            adv_printf("Your forged file dissappears in a puff of greasy black smoke! (poof)\n");
            if (Task<Result<void>> t = remove_path(savfile, false))
                co_await t;
            co_return 0;
        }
        if (co_await start(0) == ADV_OVER) // restarting game : 8305
            co_return status_;
        k_ = null_;
        p  = Phase::Motion;
    } else {
        if (co_await start(0) == ADV_OVER)
            co_return status_;
        if (co_await startup() == ADV_OVER) // prepare for a user
            co_return status_;
        blklin_ = TRUE;
    }

    while (p != Phase::Over) {
        switch (p) {
        case Phase::Dwarves:
            p = co_await dwarves();
            break;
        case Phase::Describe:
            p = co_await describe();
            break;
        case Phase::EndTurn:
            p = end_turn();
            break;
        case Phase::Hints:
            p = co_await hints_then_read();
            break;
        case Phase::Timers:
            p = co_await timers();
            break;
        case Phase::Lamp:
            p = lamp();
            break;
        case Phase::West:
            p = west();
            break;
        case Phase::Lookup:
            p = lookup();
            break;
        case Phase::Motion:
            p = co_await motion();
            break;
        case Phase::Shift:
            p = shift();
            break;
        case Phase::VerbOnly:
            p = co_await verb_only();
            break;
        case Phase::VerbObject:
            p = co_await verb_object();
            break;
        case Phase::ObjectOnly:
            p = object_only();
            break;
        case Phase::Named:
            p = named();
            break;

        // The four a tr* handler re-enters with another verb still in verb_.
        case Phase::Drop:
            p = trdrop();
            break;
        case Phase::Kill:
            p = co_await trkill();
            break;
        case Phase::Feed:
            p = trfeed();
            break;
        case Phase::Fill:
            p = trfill();
            break;

        case Phase::Done3:
            co_await done(3);
            p = Phase::Over;
            break;
        case Phase::Over:
            break;
        }
    }
    co_return status_;
}

Task<Phase> Game::dwarves() // 2
{
    if (newloc_ < 9 && newloc_ != 0 && closng_) {
        rspeak(130);    // if closing leave only by
        newloc_ = loc_; // main office
        if (!panic_)
            clock2_ = 15;
        panic_ = TRUE;
    }

    if (fdwarf() == 99) // dwarf stuff
        if (co_await die(99) == ADV_OVER)
            co_return Phase::Over;
    co_return Phase::Describe;
}

Task<Phase> Game::describe() // 2000
{
    int i;
    Text *kk;

    for (;;) {
        if (loc_ == 0) { // label 2000
            if (co_await die(99) == ADV_OVER)
                co_return Phase::Over;
        }
        kk = &stext_[loc_];
        if ((abb_[loc_] % abbnum_) == 0 || kk->seekadr == 0)
            kk = &ltext_[loc_];
        if (!forced(loc_) && dark(0)) {
            if (wzdark_ && pct(35)) {
                if (co_await die(90) == ADV_OVER)
                    co_return Phase::Over;
                continue; // i.e. goto l2000
            }
            kk = &rtext_[16];
        }
        break;
    }
    if (toting(bear_))
        rspeak(141); // 2001
    speak(kk);
    k_ = 1;
    if (forced(loc_))
        co_return Phase::Motion;
    if (loc_ == 33 && pct(25) && !closng_)
        rspeak(8);
    if (!dark(0)) {
        abb_[loc_]++;
        for (i = atloc_[loc_]; i != 0; i = plink_[i]) // 2004
        {
            obj_ = i;
            if (obj_ > 100)
                obj_ -= 100;
            if (obj_ == steps_ && toting(nugget_))
                continue;
            if (prop_[obj_] < 0) {
                if (closed_)
                    continue;
                prop_[obj_] = 0;
                if (obj_ == rug_ || obj_ == chain_)
                    prop_[obj_] = 1;
                tally_--;
                if (tally_ == tally2_ && tally_ != 0)
                    if (limit_ > 35)
                        limit_ = 35;
            }
            {
                int pk = prop_[obj_]; // 2006
                if (obj_ == steps_ && loc_ == fixed_[steps_])
                    pk = 1;
                pspeak(obj_, pk);
            }
        } // 2008
    }
    co_return Phase::EndTurn;
}

Phase Game::end_turn() // 2012
{
    verb_ = 0;
    obj_  = 0;
    return Phase::Hints;
}

Task<Phase> Game::hints_then_read() // 2600
{
    int i;
    if (Task<void> t = checkhints()) // to 2600-2602
        co_await t;
    if (closed_) {
        if (prop_[oyster_] < 0 && toting(oyster_))
            pspeak(oyster_, 1);
        for (i = 1; i < 100; i++)
            if (toting(i) && prop_[i] < 0) // 2604
                prop_[i] = -1 - prop_[i];
    }
    wzdark_ = dark(0); // 2605
    if (knfloc_ > 0 && knfloc_ != loc_)
        knfloc_ = 1;
    if (Task<void> t = getin())
        co_await t;
    if (delhit_) // user typed a DEL
    {
        delhit_ = 0;           // reset counter
        copystr("quit", wd1_); // pretend he's quitting
        *wd2_ = 0;
    }
    co_return Phase::Timers;
}

Task<Phase> Game::timers() // 2608
{
    if ((foobar_ = -foobar_) > 0)
        foobar_ = 0; // 2608
    // should check here for "magic mode"
    turns_++;

    if (verb_ == say_ && *wd2_ != 0)
        verb_ = 0;
    if (verb_ == say_)
        co_return Phase::VerbObject;
    if (tally_ == 0 && loc_ >= 15 && loc_ != 33)
        clock1_--;
    if (clock1_ == 0)
        co_return closing(); // to 10000
    if (clock1_ < 0)
        clock2_--;
    if (clock2_ == 0)
        co_return caveclose(); // to 11000
    if (prop_[lamp_] == 1)
        limit_--;
    if (limit_ <= 30 && here(batter_) && prop_[batter_] == 0 && here(lamp_)) {
        rspeak(188); // 12000
        prop_[batter_] = 1;
        if (toting(batter_))
            drop(batter_, loc_);
        limit_  = limit_ + 2500;
        lmwarn_ = FALSE;
        co_return Phase::Lamp;
    }
    if (limit_ == 0) {
        limit_       = -1; // 12400
        prop_[lamp_] = 0;
        rspeak(184);
        co_return Phase::Lamp;
    }
    if (limit_ < 0 && loc_ <= 8) {
        rspeak(185); // 12600
        gaveup_ = TRUE;
        co_await done(2); // to 20000
        co_return Phase::Over;
    }
    if (limit_ <= 30) {
        if (lmwarn_ || !here(lamp_))
            co_return Phase::Lamp; // 12200
        lmwarn_ = TRUE;
        spk_    = 187;
        if (place_[batter_] == 0)
            spk_ = 183;
        if (prop_[batter_] == 1)
            spk_ = 189;
        rspeak(spk_);
    }
    co_return Phase::Lamp;
}

Phase Game::lamp() // 19999
{
    k_ = 43;
    if (liqloc(loc_) == water_)
        k_ = 70;
    if (weq(wd1_, "enter") && (weq(wd2_, "strea") || weq(wd2_, "water")))
        return speak_k();
    if (weq(wd1_, "enter") && *wd2_ != 0)
        return Phase::Shift;
    if ((!weq(wd1_, "water") && !weq(wd1_, "oil")) || (!weq(wd2_, "plant") && !weq(wd2_, "door")))
        return Phase::West;
    if (at(vocab(wd2_, 1, 0)))
        copystr("pour", wd2_);
    return Phase::West;
}

Phase Game::west() // 2610
{
    if (weq(wd1_, "west"))
        if (++iwest_ == 10)
            rspeak(17);
    return Phase::Lookup;
}

Phase Game::lookup() // 2630
{
    int i = vocab(wd1_, -1, 0);
    if (i == -1) {
        spk_ = 60; // 3000
        if (pct(20))
            spk_ = 61;
        if (pct(20))
            spk_ = 13;
        rspeak(spk_);
        return Phase::Hints;
    }
    k_  = i % 1000;
    kq_ = i / 1000 + 1;
    switch (kq_) {
    case 1:
        return Phase::Motion;
    case 2:
        return Phase::ObjectOnly;
    case 3:
        return Phase::VerbOnly;
    case 4:
        return speak_k();
    default:
        adv_printf("Error 22\n");
        bug(22);
    }
}

Task<Phase> Game::motion() // 8
{
    switch (march()) {
    case 2:
        co_return Phase::Dwarves; // i.e. goto l2
    case 99:
        switch (co_await die(99)) {
        case 2000:
            co_return Phase::Describe;
        case ADV_OVER:
            co_return Phase::Over;
        default:
            bug(111);
        }
    default:
        bug(110);
    }
}

Phase Game::shift() // 2800
{
    copystr(wd2_, wd1_);
    *wd2_ = 0;
    return Phase::West;
}

Phase Game::what() // 8000
{
    adv_printf("%s what?\n", wd1_);
    obj_ = 0;
    return Phase::Hints;
}

Task<Phase> Game::verb_only() // 4000
{
    int i;

    verb_ = k_;
    spk_  = actspk_[verb_];
    if (*wd2_ != 0 && verb_ != say_)
        co_return Phase::Shift;
    if (verb_ == say_)
        obj_ = *wd2_;
    if (obj_ != 0)
        co_return Phase::VerbObject;

    switch (verb_) {
    case 1: // take = 8010
        if (atloc_[loc_] == 0 || plink_[atloc_[loc_]] != 0)
            co_return what();
        for (i = 1; i <= 5; i++)
            if (dloc_[i] == loc_ && dflag_ >= 2)
                co_return what();
        obj_ = atloc_[loc_];
        co_return Phase::VerbObject; // 9010
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
        co_return what();
    case 4:
    case 6: // 8040 open,lock
        spk_ = 28;
        if (here(clam_))
            obj_ = clam_;
        if (here(oyster_))
            obj_ = oyster_;
        if (at(door_))
            obj_ = door_;
        if (at(grate_))
            obj_ = grate_;
        if (obj_ != 0 && here(chain_))
            co_return what();
        if (here(chain_))
            obj_ = chain_;
        if (obj_ == 0)
            co_return report();
        co_return Phase::VerbObject; // 9040
    case 5:
        co_return nothing(); // nothing
    case 7:
    case 8: // on, off
        co_return Phase::VerbObject;
    case 11:
        co_return what(); // walk
    case 12:
    case 13: // kill, pour
        co_return Phase::VerbObject;
    case 14: // eat: 8140
        if (!here(food_))
            co_return what();
        co_return eat_food();
    case 15:
        co_return Phase::VerbObject; // drink
    case 18:                         // quit: 8180
        gaveup_ = co_await yes(22, 54, 54);
        if (gaveup_) {
            co_await done(2); // 8185
            co_return Phase::Over;
        }
        co_return Phase::EndTurn;
    case 20: // invent=8200
        spk_ = 98;
        for (i = 1; i <= 100; i++) {
            if (i != bear_ && toting(i)) {
                if (spk_ == 98)
                    rspeak(99);
                blklin_ = FALSE;
                pspeak(i, -1);
                blklin_ = TRUE;
                spk_    = 0;
            }
        }
        if (toting(bear_))
            spk_ = 141;
        co_return report();
    case 22:
    case 23: // fill, blast
        co_return Phase::VerbObject;
    case 24: // score: 8240
        scorng_ = TRUE;
        adv_printf("If you were to quit now, you would score");
        adv_printf(" %d out of a possible ", score());
        adv_printf("%d.", mxscor_);
        scorng_ = FALSE;
        gaveup_ = co_await yes(143, 54, 54);
        if (gaveup_) {
            co_await done(2);
            co_return Phase::Over;
        }
        co_return Phase::EndTurn;
    case 25: // foo: 8250
        k_   = vocab(wd1_, 3, 0);
        spk_ = 42;
        if (foobar_ != 1 - k_) {
            if (foobar_ != 0)
                spk_ = 151;
            co_return report();
        }
        foobar_ = k_; // 8252
        if (k_ != 4)
            co_return nothing();
        foobar_ = 0;
        if (place_[eggs_] == plac_[eggs_] || (toting(eggs_) && loc_ == plac_[eggs_]))
            co_return report();
        if (place_[eggs_] == 0 && place_[troll_] == 0 && prop_[troll_] == 0)
            prop_[troll_] = 1;
        k_ = 2;
        if (here(eggs_))
            k_ = 1;
        if (loc_ == plac_[eggs_])
            k_ = 0;
        move(eggs_, plac_[eggs_]);
        pspeak(eggs_, k_);
        co_return Phase::EndTurn;
    case 26: // brief=8260
        spk_    = 156;
        abbnum_ = 10000;
        detail_ = 3;
        co_return report();
    case 27: // read=8270
        if (here(magzin_))
            obj_ = magzin_;
        if (here(tablet_))
            obj_ = obj_ * 100 + tablet_;
        if (here(messag_))
            obj_ = obj_ * 100 + messag_;
        if (closed_ && toting(oyster_))
            obj_ = oyster_;
        if (obj_ > 100 || obj_ == 0 || dark(0))
            co_return what();
        co_return Phase::VerbObject; // 9270
    case 30:                         // suspend=8300
        spk_ = 201;
        adv_printf("I can suspend your adventure for you so");
        adv_printf(" you can resume later, but\n");
        adv_printf("you will have to wait at least");
        adv_printf(" %d minutes before continuing.", latncy_);
        if (!co_await yes(200, 54, 54))
            co_return Phase::EndTurn;
        if (Task<void> t = adv_clock())
            co_await t;
        datime(&saved_, &savet_);
        if (co_await ciao() == ADV_OVER)
            co_return Phase::Over;
        co_return Phase::Dwarves;
    case 31: // hours=8310
        adv_printf("Colossal cave is closed 9am-5pm Mon ");
        adv_printf("through Fri except holidays.\n");
        co_return Phase::EndTurn;
    default:
        bug(23);
    }
}

Task<Phase> Game::verb_object() // 4090
{
    int i;

    switch (verb_) {
    case 1: // take = 9010
        co_return trtake();
    case 2: // drop = 9020
        co_return trdrop();
    case 3:
        co_return trsay();
    case 4:
    case 6: // open, close = 9040
        co_return tropen();
    case 5:
        co_return nothing(); // nothing
    case 7:                  // on   9070
        if (!here(lamp_))
            co_return report();
        spk_ = 184;
        if (limit_ < 0)
            co_return report();
        prop_[lamp_] = 1;
        rspeak(39);
        if (wzdark_)
            co_return Phase::Describe;
        co_return Phase::EndTurn;

    case 8: // off
        if (!here(lamp_))
            co_return report();
        prop_[lamp_] = 0;
        rspeak(40);
        if (dark(0))
            rspeak(16);
        co_return Phase::EndTurn;

    case 9: // wave
        if ((!toting(obj_)) && (obj_ != rod_ || !toting(rod2_)))
            spk_ = 29;
        if (obj_ != rod_ || !at(fissur_) || !toting(obj_) || closng_)
            co_return report();
        prop_[fissur_] = 1 - prop_[fissur_];
        pspeak(fissur_, 2 - prop_[fissur_]);
        co_return Phase::EndTurn;
    case 10:
    case 11:
    case 18: // calm, walk, quit
    case 24:
    case 25:
    case 26: // score, foo, brief
    case 30:
    case 31: // suspend, hours
        co_return report();
    case 12: // kill = 9120
        co_return co_await trkill();
    case 13: // pour = 9130
        if (obj_ == bottle_ || obj_ == 0)
            obj_ = liq(0);
        if (obj_ == 0)
            co_return what();
        if (!toting(obj_))
            co_return report();
        spk_ = 78;
        if (obj_ != oil_ && obj_ != water_)
            co_return report();
        prop_[bottle_] = 1;
        place_[obj_]   = 0;
        spk_           = 77;
        if (!(at(plant_) || at(door_)))
            co_return report();
        if (at(door_)) {
            prop_[door_] = 0; // 9132
            if (obj_ == oil_)
                prop_[door_] = 1;
            spk_ = 113 + prop_[door_];
            co_return report();
        }
        spk_ = 112;
        if (obj_ != water_)
            co_return report();
        pspeak(plant_, prop_[plant_] + 1);
        prop_[plant_]  = (prop_[plant_] + 2) % 6;
        prop_[plant2_] = prop_[plant_] / 2;
        k_             = null_;
        co_return Phase::Motion;
    case 14: // 9140 - eat
        if (obj_ == food_)
            co_return eat_food();
        if (obj_ == bird_ || obj_ == snake_ || obj_ == clam_ || obj_ == oyster_ || obj_ == dwarf_ ||
            obj_ == dragon_ || obj_ == troll_ || obj_ == bear_)
            spk_ = 71;
        co_return report();
    case 15: // 9150 - drink
        if (obj_ == 0 && liqloc(loc_) != water_ && (liq(0) != water_ || !here(bottle_)))
            co_return what();
        if (obj_ != 0 && obj_ != water_)
            spk_ = 110;
        if (spk_ == 110 || liq(0) != water_ || !here(bottle_))
            co_return report();
        prop_[bottle_] = 1;
        place_[water_] = 0;
        spk_           = 74;
        co_return report();
    case 16: // 9160: rub
        if (obj_ != lamp_)
            spk_ = 76;
        co_return report();
    case 17: // 9170: throw
        co_return trtoss();
    case 19:
    case 20: // 9190: find, invent
        if (at(obj_) || (liq(0) == obj_ && at(bottle_)) || k_ == liqloc(loc_))
            spk_ = 94;
        for (i = 1; i <= 5; i++)
            if (dloc_[i] == loc_ && dflag_ >= 2 && obj_ == dwarf_)
                spk_ = 94;
        if (closed_)
            spk_ = 138;
        if (toting(obj_))
            spk_ = 24;
        co_return report();
    case 21: // feed = 9210
        co_return trfeed();
    case 22: // fill = 9220
        co_return trfill();
    case 23: // blast = 9230
        if (prop_[rod2_] < 0 || !closed_)
            co_return report();
        bonus_ = 133;
        if (loc_ == 115)
            bonus_ = 134;
        if (here(rod2_))
            bonus_ = 135;
        rspeak(bonus_);
        co_await done(2);
        co_return Phase::Over;
    case 27: // read = 9270
        if (dark(0))
            co_return no_see();
        if (obj_ == magzin_)
            spk_ = 190;
        if (obj_ == tablet_)
            spk_ = 196;
        if (obj_ == messag_)
            spk_ = 191;
        if (obj_ == oyster_ && hinted_[2] && toting(oyster_))
            spk_ = 194;
        if (obj_ != oyster_ || hinted_[2] || !toting(oyster_) || !closed_)
            co_return report();
        hinted_[2] = co_await yes(192, 193, 54);
        co_return Phase::EndTurn;
    case 28: // break
        if (obj_ == mirror_)
            spk_ = 148;
        if (obj_ == vase_ && prop_[vase_] == 0) {
            spk_ = 198;
            if (toting(vase_))
                drop(vase_, loc_);
            prop_[vase_]  = 2;
            fixed_[vase_] = -1;
            co_return report();
        }
        if (obj_ != mirror_ || !closed_)
            co_return report();
        rspeak(197);
        co_await done(3);
        co_return Phase::Over;

    case 29: // wake
        if (obj_ != dwarf_ || !closed_)
            co_return report();
        rspeak(199);
        co_await done(3);
        co_return Phase::Over;

    default:
        bug(24);
    }
}

Phase Game::object_only() // 5000
{
    int i;

    obj_ = k_;
    if (fixed_[k_] == loc_ || here(k_))
        return Phase::Named;
    if (k_ == grate_) { // 5100
        if (loc_ == 1 || loc_ == 4 || loc_ == 7)
            k_ = dprssn_;
        if (loc_ > 9 && loc_ < 15)
            k_ = entrnc_;
        if (k_ != grate_)
            return Phase::Motion;
    }
    if (k_ == dwarf_) // 5110
        for (i = 1; i <= 5; i++)
            if (dloc_[i] == loc_ && dflag_ >= 2)
                return Phase::Named;
    if ((liq(0) == k_ && here(bottle_)) || k_ == liqloc(loc_)) // 5120
        return Phase::Named;
    if (obj_ == plant_ && at(plant2_) && prop_[plant2_] != 0) {
        obj_ = plant2_;
        return Phase::Named;
    }
    if (obj_ == knife_ && knfloc_ == loc_) { // 5130
        knfloc_ = -1;
        spk_    = 116;
        return report();
    }
    if (obj_ == rod_ && here(rod2_)) { // 5140
        obj_ = rod2_;
        return Phase::Named;
    }
    return no_see();
}

Phase Game::named() // 5010
{
    if (*wd2_ != 0)
        return Phase::Shift;
    if (verb_ != 0)
        return Phase::VerbObject;
    adv_printf("What do you want to do with the %s?\n", wd1_);
    return Phase::Hints;
}

Phase Game::no_see() // 5190
{
    if ((verb_ == find_ || verb_ == invent_) && *wd2_ == 0)
        return Phase::Named;
    adv_printf("I see no %s here\n", wd1_);
    return Phase::EndTurn;
}

Task<i32> proc_main(Args args)
{
    if (Task<void> t = adv_input_init())
        co_await t;

    // On the heap: the state is ten kilobytes, and once it holds a Vec it is
    // not trivially destructible, which a global here must be.
    i32 rc     = 1;
    Game *game = heap_new<Game>();
    if (!game || !game->alloc())
        adv_printf("adventure: out of memory\n");
    else {
        struct Free {
            ~Free() { heap_delete(game); }
            Game *game;
        } free_game{ game };

        if (Task<i32> t = game->play(args))
            rc = co_await t;
    }

    if (Task<void> t = adv_flush())
        co_await t;
    if (Task<void> t = adv_input_done())
        co_await t;
    co_return adv_write_failed() ? 1 : rc;
}
