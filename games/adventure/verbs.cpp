// Re-coding of advent in C: the hints and the verb handlers -- upstream subr.c.
//
// A handler returns the label its caller must jump back into.
#include "hdr.h"

Task<void> Game::checkhints(void) // 2600 &c
{
    int hint;
    for (hint = 4; hint <= hntmax_; hint++) {
        if (hinted_[hint])
            continue;
        if (!bitset(loc_, hint))
            hintlc_[hint] = -1;
        hintlc_[hint]++;
        if (hintlc_[hint] < hints_[hint][1])
            continue;
        switch (hint) {
        case 4: // 40400
            if (prop_[grate_] == 0 && !here(keys_))
                goto l40010;
            goto l40020;
        case 5: // 40500
            if (here(bird_) && toting(rod_) && obj_ == bird_)
                goto l40010;
            continue; // i.e. goto l40030
        case 6:       // 40600
            if (here(snake_) && !here(bird_))
                goto l40010;
            goto l40020;
        case 7: // 40700
            if (atloc_[loc_] == 0 && atloc_[oldloc_] == 0 && atloc_[oldlc2_] == 0 && holdng_ > 1)
                goto l40010;
            goto l40020;
        case 8: // 40800
            if (prop_[emrald_] != -1 && prop_[pyram_] == -1)
                goto l40010;
            goto l40020;
        case 9:
            goto l40010; // 40900
        default:
            bug(27);
        }
    l40010:
        hintlc_[hint] = 0;
        if (!co_await yes(hints_[hint][3], 0, 54))
            continue;
        adv_printf("I am prepared to give you a hint, but it will ");
        adv_printf("cost you %d points.\n", hints_[hint][2]);
        hinted_[hint] = co_await yes(175, hints_[hint][4], 54);
    l40020:
        hintlc_[hint] = 0;
    }
    co_return;
}

Phase Game::trsay() // 9030
{
    int i;
    if (*wd2_ != 0)
        copystr(wd2_, wd1_);
    i = vocab(wd1_, -1, 0);
    if (i == 62 || i == 65 || i == 71 || i == 2025) {
        *wd2_ = 0;
        obj_  = 0;
        return Phase::Lookup;
    }
    adv_printf("\nOkay, \"%s\".\n", wd2_);
    return Phase::EndTurn;
}

Phase Game::trtake() // 9010
{
    if (toting(obj_))
        return report(); // 9010
    spk_ = 25;
    if (obj_ == plant_ && prop_[plant_] <= 0)
        spk_ = 115;
    if (obj_ == bear_ && prop_[bear_] == 1)
        spk_ = 169;
    if (obj_ == chain_ && prop_[bear_] != 0)
        spk_ = 170;
    if (fixed_[obj_] != 0)
        return report();
    if (obj_ == water_ || obj_ == oil_) {
        if (here(bottle_) && liq(0) == obj_) {
            obj_ = bottle_;
            goto l9017;
        }
        obj_ = bottle_;
        if (toting(bottle_) && prop_[bottle_] == 1)
            return Phase::Fill;
        if (prop_[bottle_] != 1)
            spk_ = 105;
        if (!toting(bottle_))
            spk_ = 104;
        return report();
    }
l9017:
    if (holdng_ >= 7) {
        rspeak(92);
        return Phase::EndTurn;
    }
    if (obj_ == bird_) {
        if (prop_[bird_] != 0)
            goto l9014;
        if (toting(rod_)) {
            rspeak(26);
            return Phase::EndTurn;
        }
        if (!toting(cage_)) // 9013
        {
            rspeak(27);
            return Phase::EndTurn;
        }
        prop_[bird_] = 1; // 9015
    }
l9014:
    if ((obj_ == bird_ || obj_ == cage_) && prop_[bird_] != 0)
        carry(bird_ + cage_ - obj_, loc_);
    carry(obj_, loc_);
    k_ = liq(0);
    if (obj_ == bottle_ && k_ != 0)
        place_[k_] = -1;
    return nothing();
}

Phase Game::dropper() // 9021
{
    k_ = liq(0);
    if (k_ == obj_)
        obj_ = bottle_;
    if (obj_ == bottle_ && k_ != 0)
        place_[k_] = 0;
    if (obj_ == cage_ && prop_[bird_] != 0)
        drop(bird_, loc_);
    if (obj_ == bird_)
        prop_[bird_] = 0;
    drop(obj_, loc_);
    return Phase::EndTurn;
}

Phase Game::trdrop() // 9020
{
    if (toting(rod2_) && obj_ == rod_ && !toting(rod_))
        obj_ = rod2_;
    if (!toting(obj_))
        return report();
    if (obj_ == bird_ && here(snake_)) {
        rspeak(30);
        if (closed_)
            return Phase::Done3;
        dstroy(snake_);
        prop_[snake_] = 1;
        return (dropper());
    }
    if (obj_ == coins_ && here(vend_)) // 9024
    {
        dstroy(coins_);
        drop(batter_, loc_);
        pspeak(batter_, 0);
        return Phase::EndTurn;
    }
    if (obj_ == bird_ && at(dragon_) && prop_[dragon_] == 0) // 9025
    {
        rspeak(154);
        dstroy(bird_);
        prop_[bird_] = 0;
        if (place_[snake_] == plac_[snake_])
            tally2_--;
        return Phase::EndTurn;
    }
    if (obj_ == bear_ && at(troll_)) // 9026
    {
        rspeak(163);
        move(troll_, 0);
        move(troll_ + 100, 0);
        move(troll2_, plac_[troll_]);
        move(troll2_ + 100, fixd_[troll_]);
        juggle(chasm_);
        prop_[troll_] = 2;
        return (dropper());
    }
    if (obj_ != vase_ || loc_ == plac_[pillow_]) // 9027
    {
        rspeak(54);
        return (dropper());
    }
    prop_[vase_] = 2; // 9028
    if (at(pillow_))
        prop_[vase_] = 0;
    pspeak(vase_, prop_[vase_] + 1);
    if (prop_[vase_] != 0)
        fixed_[vase_] = -1;
    return (dropper());
}

Phase Game::tropen() // 9040
{
    if (obj_ == clam_ || obj_ == oyster_) {
        k_ = 0; // 9046
        if (obj_ == oyster_)
            k_ = 1;
        spk_ = 124 + k_;
        if (toting(obj_))
            spk_ = 120 + k_;
        if (!toting(tridnt_))
            spk_ = 122 + k_;
        if (verb_ == lock_)
            spk_ = 61;
        if (spk_ != 124)
            return report();
        dstroy(clam_);
        drop(oyster_, loc_);
        drop(pearl_, 105);
        return report();
    }
    if (obj_ == door_)
        spk_ = 111;
    if (obj_ == door_ && prop_[door_] == 1)
        spk_ = 54;
    if (obj_ == cage_)
        spk_ = 32;
    if (obj_ == keys_)
        spk_ = 55;
    if (obj_ == grate_ || obj_ == chain_)
        spk_ = 31;
    if (spk_ != 31 || !here(keys_))
        return report();
    if (obj_ == chain_) {
        if (verb_ == lock_) {
            spk_ = 172; // 9049: lock
            if (prop_[chain_] != 0)
                spk_ = 34;
            if (loc_ != plac_[chain_])
                spk_ = 173;
            if (spk_ != 172)
                return report();
            prop_[chain_] = 2;
            if (toting(chain_))
                drop(chain_, loc_);
            fixed_[chain_] = -1;
            return report();
        }
        spk_ = 171;
        if (prop_[bear_] == 0)
            spk_ = 41;
        if (prop_[chain_] == 0)
            spk_ = 37;
        if (spk_ != 171)
            return report();
        prop_[chain_]  = 0;
        fixed_[chain_] = 0;
        if (prop_[bear_] != 3)
            prop_[bear_] = 2;
        fixed_[bear_] = 2 - prop_[bear_];
        return report();
    }
    if (closng_) {
        k_ = 130;
        if (!panic_)
            clock2_ = 15;
        panic_ = TRUE;
        return speak_k();
    }
    k_            = 34 + prop_[grate_]; // 9043
    prop_[grate_] = 1;
    if (verb_ == lock_)
        prop_[grate_] = 0;
    k_ = k_ + 2 * prop_[grate_];
    return speak_k();
}

Task<Phase> Game::trkill() // 9120
{
    int i;
    for (i = 1; i <= 5; i++)
        if (dloc_[i] == loc_ && dflag_ >= 2)
            break;
    if (i == 6)
        i = 0;
    if (obj_ == 0) // 9122
    {
        if (i != 0)
            obj_ = dwarf_;
        if (here(snake_))
            obj_ = obj_ * 100 + snake_;
        if (at(dragon_) && prop_[dragon_] == 0)
            obj_ = obj_ * 100 + dragon_;
        if (at(troll_))
            obj_ = obj_ * 100 + troll_;
        if (here(bear_) && prop_[bear_] == 0)
            obj_ = obj_ * 100 + bear_;
        if (obj_ > 100)
            co_return what();
        if (obj_ == 0) {
            if (here(bird_) && verb_ != throw_)
                obj_ = bird_;
            if (here(clam_) || here(oyster_))
                obj_ = 100 * obj_ + clam_;
            if (obj_ > 100)
                co_return what();
        }
    }
    if (obj_ == bird_) // 9124
    {
        spk_ = 137;
        if (closed_)
            co_return report();
        dstroy(bird_);
        prop_[bird_] = 0;
        if (place_[snake_] == plac_[snake_])
            tally2_++;
        spk_ = 45;
    }
    if (obj_ == 0)
        spk_ = 44; // 9125
    if (obj_ == clam_ || obj_ == oyster_)
        spk_ = 150;
    if (obj_ == snake_)
        spk_ = 46;
    if (obj_ == dwarf_)
        spk_ = 49;
    if (obj_ == dwarf_ && closed_)
        co_return Phase::Done3;
    if (obj_ == dragon_)
        spk_ = 147;
    if (obj_ == troll_)
        spk_ = 157;
    if (obj_ == bear_)
        spk_ = 165 + (prop_[bear_] + 1) / 2;
    if (obj_ != dragon_ || prop_[dragon_] != 0)
        co_return report();
    rspeak(49);
    verb_ = 0;
    obj_  = 0;
    if (Task<void> t = getin())
        co_await t;
    if (!weq(wd1_, "y") && !weq(wd1_, "yes"))
        co_return Phase::Timers;
    pspeak(dragon_, 1);
    prop_[dragon_] = 2;
    prop_[rug_]    = 0;
    k_             = (plac_[dragon_] + fixd_[dragon_]) / 2;
    move(dragon_ + 100, -1);
    move(rug_ + 100, 0);
    move(dragon_, k_);
    move(rug_, k_);
    for (obj_ = 1; obj_ <= 100; obj_++)
        if (place_[obj_] == plac_[dragon_] || place_[obj_] == fixd_[dragon_])
            move(obj_, k_);
    loc_ = k_;
    k_   = null_;
    co_return Phase::Motion;
}

Phase Game::trtoss() // 9170: throw
{
    int i;
    if (toting(rod2_) && obj_ == rod_ && !toting(rod_))
        obj_ = rod2_;
    if (!toting(obj_))
        return report();
    if (obj_ >= 50 && obj_ <= maxtrs_ && at(troll_)) {
        spk_ = 159; // 9178
        drop(obj_, 0);
        move(troll_, 0);
        move(troll_ + 100, 0);
        drop(troll2_, plac_[troll_]);
        drop(troll2_ + 100, fixd_[troll_]);
        juggle(chasm_);
        return report();
    }
    if (obj_ == food_ && here(bear_)) {
        obj_ = bear_; // 9177
        return Phase::Feed;
    }
    if (obj_ != axe_)
        return Phase::Drop;
    for (i = 1; i <= 5; i++) {
        if (dloc_[i] == loc_) {
            spk_ = 48; // 9172
            if (ran(3) == 0 || saved_ != -1)
            l9175: {
                rspeak(spk_);
                drop(axe_, loc_);
                k_ = null_;
                return Phase::Motion;
            }
                dseen_[i] = FALSE;
            dloc_[i] = 0;
            spk_     = 47;
            dkill_++;
            if (dkill_ == 1)
                spk_ = 149;
            goto l9175;
        }
    }
    spk_ = 152;
    if (at(dragon_) && prop_[dragon_] == 0)
        goto l9175;
    spk_ = 158;
    if (at(troll_))
        goto l9175;
    if (here(bear_) && prop_[bear_] == 0) {
        spk_ = 164;
        drop(axe_, loc_);
        fixed_[axe_] = -1;
        prop_[axe_]  = 1;
        juggle(bear_);
        return report();
    }
    obj_ = 0;
    return Phase::Kill;
}

Phase Game::trfeed() // 9210
{
    if (obj_ == bird_) {
        spk_ = 100;
        return report();
    }
    if (obj_ == snake_ || obj_ == dragon_ || obj_ == troll_) {
        spk_ = 102;
        if (obj_ == dragon_ && prop_[dragon_] != 0)
            spk_ = 110;
        if (obj_ == troll_)
            spk_ = 182;
        if (obj_ != snake_ || closed_ || !here(bird_))
            return report();
        spk_ = 101;
        dstroy(bird_);
        prop_[bird_] = 0;
        tally2_++;
        return report();
    }
    if (obj_ == dwarf_) {
        if (!here(food_))
            return report();
        spk_ = 103;
        dflag_++;
        return report();
    }
    if (obj_ == bear_) {
        if (prop_[bear_] == 0)
            spk_ = 102;
        if (prop_[bear_] == 3)
            spk_ = 110;
        if (!here(food_))
            return report();
        dstroy(food_);
        prop_[bear_] = 1;
        fixed_[axe_] = 0;
        prop_[axe_]  = 0;
        spk_         = 168;
        return report();
    }
    spk_ = 14;
    return report();
}

Phase Game::trfill() // 9220
{
    if (obj_ == vase_) {
        spk_ = 29;
        if (liqloc(loc_) == 0)
            spk_ = 144;
        if (liqloc(loc_) == 0 || !toting(vase_))
            return report();
        rspeak(145);
        prop_[vase_]  = 2;
        fixed_[vase_] = -1;
        return Phase::Drop; // advent/10 goes to 9024
    }
    if (obj_ != 0 && obj_ != bottle_)
        return report();
    if (obj_ == 0 && !here(bottle_))
        return what();
    spk_ = 107;
    if (liqloc(loc_) == 0)
        spk_ = 106;
    if (liq(0) != 0)
        spk_ = 105;
    if (spk_ != 107)
        return report();
    prop_[bottle_] = ((cond_[loc_] % 4) / 2) * 2;
    k_             = liq(0);
    if (toting(bottle_))
        place_[k_] = -1;
    if (k_ == oil_)
        spk_ = 108;
    return report();
}
