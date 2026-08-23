// Re-coding of advent in C: the hints and the verb handlers -- upstream subr.c.
//
// A handler returns the label its caller must jump back into.
#include "hdr.h"

Task<void> Game::checkhints(void) // 2600 &c
{
    int hint;
    for (hint = i16(Hint::Grate); hint <= hntmax_; hint++) {
        if (hinted_[hint])
            continue;
        if (!bitset(loc_, hint))
            hintlc_[hint] = -1;
        hintlc_[hint]++;
        if (hintlc_[hint] < hints_[hint].turns)
            continue;
        switch (Hint(hint)) {
        case Hint::Grate: // 40400
            if (prop_[grate_] == 0 && !here(keys_))
                goto offer;
            goto reset;
        case Hint::Bird: // 40500
            if (here(bird_) && toting(rod_) && obj_ == bird_)
                goto offer;
            continue;     // 40030, and keep the count
        case Hint::Snake: // 40600
            if (here(snake_) && !here(bird_))
                goto offer;
            goto reset;
        case Hint::Maze: // 40700
            if (atloc_[loc_] == 0 && atloc_[oldloc_] == 0 && atloc_[oldlc2_] == 0 && holdng_ > 1)
                goto offer;
            goto reset;
        case Hint::Plover: // 40800
            if (prop_[emrald_] != -1 && prop_[pyram_] == -1)
                goto offer;
            goto reset;
        case Hint::WittsEnd:
            goto offer; // 40900
        default:
            bug(27);
        }
    offer: // 40010
        hintlc_[hint] = 0;
        if (!co_await yes(hints_[hint].question, Msg::None, Msg::Ok))
            continue;
        adv_printf("I am prepared to give you a hint, but it will ");
        adv_printf("cost you %d points.\n", hints_[hint].cost);
        hinted_[hint] = co_await yes(Msg::WantTheHint, hints_[hint].answer, Msg::Ok);
    reset: // 40020
        hintlc_[hint] = 0;
    }
    co_return;
}

Phase Game::trsay() // 9030
{
    int i;
    if (*wd2_ != 0)
        copystr(wd2_, wd1_);
    i = vocab(wd1_, VOC_USER, 0);
    if (i == i16(Motion::Xyzzy) || i == i16(Motion::Plugh) || i == i16(Motion::Plover) ||
        i == word_of(WordClass::Verb, i16(Verb::Foo))) {
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
    spk_ = Msg::CantBeSerious;
    if (obj_ == plant_ && prop_[plant_] <= 0)
        spk_ = Msg::PlantDeepRoots;
    if (obj_ == bear_ && prop_[bear_] == 1)
        spk_ = Msg::BearStillChained;
    if (obj_ == chain_ && prop_[bear_] != 0)
        spk_ = Msg::ChainStillLocked;
    if (fixed_[obj_] != 0)
        return report();
    if (obj_ == water_ || obj_ == oil_) {
        if (here(bottle_) && liq(0) == obj_) {
            obj_ = bottle_;
            goto pick_up;
        }
        obj_ = bottle_;
        if (toting(bottle_) && prop_[bottle_] == 1)
            return Phase::Fill;
        if (prop_[bottle_] != 1)
            spk_ = Msg::BottleAlreadyFull;
        if (!toting(bottle_))
            spk_ = Msg::NothingToCarryItIn;
        return report();
    }
pick_up: // 9017
    if (holdng_ >= 7) {
        rspeak(Msg::CarryingTooMuch);
        return Phase::EndTurn;
    }
    if (obj_ == bird_) {
        if (prop_[bird_] != 0)
            goto carry_it;
        if (toting(rod_)) {
            rspeak(Msg::BirdBecomesUneasy);
            return Phase::EndTurn;
        }
        if (!toting(cage_)) // 9013
        {
            rspeak(Msg::CantCarryBird);
            return Phase::EndTurn;
        }
        prop_[bird_] = 1; // 9015
    }
carry_it: // 9014
    if ((obj_ == bird_ || obj_ == cage_) && prop_[bird_] != 0)
        carry(bird_ + cage_ - obj_, loc_);
    carry(obj_, loc_);
    k_ = liq(0);
    if (obj_ == bottle_ && k_ != 0)
        place_[k_] = CARRIED;
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
        rspeak(Msg::BirdKillsSnake);
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
        rspeak(Msg::BirdKillsDragon);
        dstroy(bird_);
        prop_[bird_] = 0;
        if (place_[snake_] == plac_[snake_])
            tally2_--;
        return Phase::EndTurn;
    }
    if (obj_ == bear_ && at(troll_)) // 9026
    {
        rspeak(Msg::BearScaresTroll);
        move(troll_, 0);
        move(troll_ + FIXED, 0);
        move(troll2_, plac_[troll_]);
        move(troll2_ + FIXED, fixd_[troll_]);
        juggle(chasm_);
        prop_[troll_] = 2;
        return (dropper());
    }
    if (obj_ != vase_ || loc_ == plac_[pillow_]) // 9027
    {
        rspeak(Msg::Ok);
        return (dropper());
    }
    prop_[vase_] = 2; // 9028
    if (at(pillow_))
        prop_[vase_] = 0;
    pspeak(vase_, prop_[vase_] + 1);
    if (prop_[vase_] != 0)
        fixed_[vase_] = PINNED;
    return (dropper());
}

Phase Game::tropen() // 9040
{
    if (obj_ == clam_ || obj_ == oyster_) {
        k_ = 0; // 9046
        if (obj_ == oyster_)
            k_ = 1;
        spk_ = Msg::PearlFalls + k_;
        if (toting(obj_))
            spk_ = Msg::PutDownClam + k_;
        if (!toting(tridnt_))
            spk_ = Msg::NothingOpensClam + k_;
        if (verb_ == lock_)
            spk_ = Msg::What;
        if (spk_ != Msg::PearlFalls)
            return report();
        dstroy(clam_);
        drop(oyster_, loc_);
        drop(pearl_, Loc::CulDeSac);
        return report();
    }
    if (obj_ == door_)
        spk_ = Msg::DoorRusty;
    if (obj_ == door_ && prop_[door_] == 1)
        spk_ = Msg::Ok;
    if (obj_ == cage_)
        spk_ = Msg::NoLock;
    if (obj_ == keys_)
        spk_ = Msg::CantUnlockKeys;
    if (obj_ == grate_ || obj_ == chain_)
        spk_ = Msg::NoKeys;
    if (spk_ != Msg::NoKeys || !here(keys_))
        return report();
    if (obj_ == chain_) {
        if (verb_ == lock_) {
            spk_ = Msg::ChainLocked; // 9049: lock
            if (prop_[chain_] != 0)
                spk_ = Msg::AlreadyLocked;
            if (loc_ != plac_[chain_])
                spk_ = Msg::NothingToLockChainTo;
            if (spk_ != Msg::ChainLocked)
                return report();
            prop_[chain_] = 2;
            if (toting(chain_))
                drop(chain_, loc_);
            fixed_[chain_] = PINNED;
            return report();
        }
        spk_ = Msg::ChainUnlocked;
        if (prop_[bear_] == 0)
            spk_ = Msg::BearBlocksChain;
        if (prop_[chain_] == 0)
            spk_ = Msg::AlreadyUnlocked;
        if (spk_ != Msg::ChainUnlocked)
            return report();
        prop_[chain_]  = 0;
        fixed_[chain_] = 0;
        if (prop_[bear_] != 3)
            prop_[bear_] = 2;
        fixed_[bear_] = 2 - prop_[bear_];
        return report();
    }
    if (closng_) {
        k_ = i16(Msg::RecordedVoice);
        if (!panic_)
            clock2_ = 15;
        panic_ = TRUE;
        return speak_k();
    }
    k_            = i16(Msg::AlreadyLocked + prop_[grate_]); // 9043
    prop_[grate_] = 1;
    if (verb_ == lock_)
        prop_[grate_] = 0;
    k_ = k_ + 2 * prop_[grate_];
    return speak_k();
}

Task<Phase> Game::trkill() // 9120
{
    int i;
    for (i = 1; i <= NDWARVES; i++)
        if (dloc_[i] == loc_ && dflag_ >= Dwarves::Active)
            break;
    if (i == PIRATE)
        i = 0;
    // Shifting by a hundred rather than adding: a second candidate overflows
    // the object range, and that is the test for "kill what?".
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
        spk_ = Msg::LeaveBirdAlone;
        if (closed_)
            co_return report();
        dstroy(bird_);
        prop_[bird_] = 0;
        if (place_[snake_] == plac_[snake_])
            tally2_++;
        spk_ = Msg::BirdDead;
    }
    if (obj_ == 0)
        spk_ = Msg::NothingToAttack; // 9125
    if (obj_ == clam_ || obj_ == oyster_)
        spk_ = Msg::ShellImpervious;
    if (obj_ == snake_)
        spk_ = Msg::SnakeDangerous;
    if (obj_ == dwarf_)
        spk_ = Msg::BareHands;
    if (obj_ == dwarf_ && closed_)
        co_return Phase::Done3;
    if (obj_ == dragon_)
        spk_ = Msg::DontKnowHow;
    if (obj_ == troll_)
        spk_ = Msg::TrollsTough;
    if (obj_ == bear_)
        spk_ = Msg::BareHandsBear + (prop_[bear_] + 1) / 2;
    if (obj_ != dragon_ || prop_[dragon_] != 0)
        co_return report();
    rspeak(Msg::BareHands);
    verb_ = Verb::None;
    obj_  = 0;
    if (Task<void> t = getin())
        co_await t;
    if (!weq(wd1_, "y") && !weq(wd1_, "yes"))
        co_return Phase::Timers;
    pspeak(dragon_, 1);
    prop_[dragon_] = 2;
    prop_[rug_]    = 0;
    k_             = (plac_[dragon_] + fixd_[dragon_]) / 2;
    move(dragon_ + FIXED, PINNED);
    move(rug_ + FIXED, 0);
    move(dragon_, k_);
    move(rug_, k_);
    for (obj_ = 1; obj_ <= MAXOBJ; obj_++)
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
    if (obj_ >= TREASURE && obj_ <= maxtrs_ && at(troll_)) {
        spk_ = Msg::TrollTakesTreasure; // 9178
        drop(obj_, 0);
        move(troll_, 0);
        move(troll_ + FIXED, 0);
        drop(troll2_, plac_[troll_]);
        drop(troll2_ + FIXED, fixd_[troll_]);
        juggle(chasm_);
        return report();
    }
    if (obj_ == food_ && here(bear_)) {
        obj_ = bear_; // 9177
        return Phase::Feed;
    }
    if (obj_ != axe_)
        return Phase::Drop;
    for (i = 1; i <= NDWARVES; i++) {
        if (dloc_[i] == loc_) {
            spk_ = Msg::DwarfDodges; // 9172
            if (ran(3) == 0 || saved_ != -1)
            drop_axe: { // 9175
                rspeak(spk_);
                drop(axe_, loc_);
                k_ = null_;
                return Phase::Motion;
            }
                dseen_[i] = FALSE;
            dloc_[i] = 0;
            spk_     = Msg::KilledDwarf;
            dkill_++;
            if (dkill_ == 1)
                spk_ = Msg::KilledDwarfCloud;
            goto drop_axe;
        }
    }
    spk_ = Msg::AxeBouncesDragon;
    if (at(dragon_) && prop_[dragon_] == 0)
        goto drop_axe;
    spk_ = Msg::TrollCatchesAxe;
    if (at(troll_))
        goto drop_axe;
    if (here(bear_) && prop_[bear_] == 0) {
        spk_ = Msg::AxeMissesNearBear;
        drop(axe_, loc_);
        fixed_[axe_] = PINNED;
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
        spk_ = Msg::NotHungryFjords;
        return report();
    }
    if (obj_ == snake_ || obj_ == dragon_ || obj_ == troll_) {
        spk_ = Msg::NothingItWantsToEat;
        if (obj_ == dragon_ && prop_[dragon_] != 0)
            spk_ = Msg::DontBeRidiculous;
        if (obj_ == troll_)
            spk_ = Msg::TrollNotGluttony;
        if (obj_ != snake_ || closed_ || !here(bird_))
            return report();
        spk_ = Msg::SnakeEatsBird;
        dstroy(bird_);
        prop_[bird_] = 0;
        tally2_++;
        return report();
    }
    if (obj_ == dwarf_) {
        if (!here(food_))
            return report();
        spk_ = Msg::DwarvesEatCoal;
        dflag_++; // one step angrier
        return report();
    }
    if (obj_ == bear_) {
        if (prop_[bear_] == 0)
            spk_ = Msg::NothingItWantsToEat;
        if (prop_[bear_] == 3)
            spk_ = Msg::DontBeRidiculous;
        if (!here(food_))
            return report();
        dstroy(food_);
        prop_[bear_] = 1;
        fixed_[axe_] = 0;
        prop_[axe_]  = 0;
        spk_         = Msg::BearWolfsFood;
        return report();
    }
    spk_ = Msg::ExplainHow;
    return report();
}

Phase Game::trfill() // 9220
{
    if (obj_ == vase_) {
        spk_ = Msg::NotCarryingIt;
        if (liqloc(loc_) == 0)
            spk_ = Msg::NothingToFillVase;
        if (liqloc(loc_) == 0 || !toting(vase_))
            return report();
        rspeak(Msg::VaseShattered);
        prop_[vase_]  = 2;
        fixed_[vase_] = PINNED;
        return Phase::Drop; // advent/10 goes to 9024
    }
    if (obj_ != 0 && obj_ != bottle_)
        return report();
    if (obj_ == 0 && !here(bottle_))
        return what();
    spk_ = Msg::BottleFullWater;
    if (liqloc(loc_) == 0)
        spk_ = Msg::NothingToFillWith;
    if (liq(0) != 0)
        spk_ = Msg::BottleAlreadyFull;
    if (spk_ != Msg::BottleFullWater)
        return report();
    prop_[bottle_] = bitset(loc_, i16(CondBit::Oil)) * 2; // 0 water, 2 oil
    k_             = liq(0);
    if (toting(bottle_))
        place_[k_] = CARRIED;
    if (k_ == oil_)
        spk_ = Msg::BottleFullOil;
    return report();
}
