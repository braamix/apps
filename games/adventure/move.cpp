// Re-coding of advent in C: the dwarves and the travel table -- upstream subr.c.
#include "hdr.h"

int Game::fdwarf(void) // 71
{
    int i, j;

    if (newloc_ != loc_ && !forced(loc_) && !bitset(loc_, 3)) {
        for (i = 1; i <= 5; i++) {
            if (odloc_[i] != newloc_ || !dseen_[i])
                continue;
            newloc_ = loc_;
            rspeak(Msg::DwarfBlocks);
            break;
        }
    }
    loc_ = newloc_; // 74
    if (loc_ == 0 || forced(loc_) || bitset(newloc_, 3))
        return (2000);
    if (dflag_ == 0) {
        if (loc_ >= 15)
            dflag_ = 1;
        return (2000);
    }
    if (dflag_ == 1) // 6000
    {
        if (loc_ < 15 || pct(95))
            return (2000);
        dflag_ = 2;
        for (i = 1; i <= 2; i++) {
            j = 1 + ran(5);
            if (pct(50) && saved_ == -1)
                dloc_[j] = 0; // 6001
        }
        for (i = 1; i <= 5; i++) {
            if (dloc_[i] == loc_)
                dloc_[i] = daltlc_;
            odloc_[i] = dloc_[i]; // 6002
        }
        rspeak(Msg::DwarfThrowsAxe);
        drop(axe_, loc_);
        return (2000);
    }
    dtotal_ = attack_ = stick_ = 0; // 6010
    for (i = 1; i <= 6; i++)        // loop to 6030
    {
        if (dloc_[i] == 0)
            continue;
        j                     = 1;
        const Vec<Travel> &tl = travel_[dloc_[i]];
        for (usize x = 0; x < tl.size(); x++) {
            newloc_ = tl[x].tloc;
            if (newloc_ > 300 || newloc_ < 15 || newloc_ == odloc_[i] ||
                (j > 1 && newloc_ == tk_[j - 1]) || j >= 20 || newloc_ == dloc_[i] ||
                forced(newloc_) || (i == 6 && bitset(newloc_, 3)) || tl[x].conditions == 100)
                continue;
            tk_[j++] = newloc_;
        }
        tk_[j] = odloc_[i]; // 6016
        if (j >= 2)
            j--;
        j         = 1 + ran(j);
        odloc_[i] = dloc_[i];
        dloc_[i]  = tk_[j];
        dseen_[i] = (dseen_[i] && loc_ >= 15) || (dloc_[i] == loc_ || odloc_[i] == loc_);
        if (!dseen_[i])
            continue; // 6030, next dwarf
        dloc_[i] = loc_;
        if (i == 6) // pirate's spotted him
        {
            if (loc_ == chloc_ || prop_[chest_] >= 0)
                continue;
            k_ = 0;
            for (j = 50; j <= maxtrs_; j++) // loop to 6020
            {
                if (j == pyram_ && (loc_ == plac_[pyram_] || loc_ == plac_[emrald_]))
                    goto seen_here;
                if (toting(j))
                    goto rob;
            seen_here: // 6020
                if (here(j))
                    k_ = 1;
            } // 6020
            if (tally_ == tally2_ + 1 && k_ == 0 && place_[chest_] == 0 && here(lamp_) &&
                prop_[lamp_] == 1)
                goto steal_chest;
            if (odloc_[6] != dloc_[6] && pct(20))
                rspeak(Msg::FaintRustling);
            continue; // 6030, next dwarf
        rob:          // 6022
            rspeak(Msg::PiratePounces);
            if (place_[messag_] == 0)
                move(chest_, chloc_);
            move(messag_, chloc2_);
            for (j = 50; j <= maxtrs_; j++) // loop to 6023
            {
                if (j == pyram_ && (loc_ == plac_[pyram_] || loc_ == plac_[emrald_]))
                    continue;
                if (at(j) && fixed_[j] == 0)
                    carry(j, loc_);
                if (toting(j))
                    drop(j, chloc_);
            }
        hide_chest: // 6024
            dloc_[6] = odloc_[6] = chloc_;
            dseen_[6]            = FALSE;
            continue;
        steal_chest: // 6025
            rspeak(Msg::RustlingThenPirate);
            move(chest_, chloc_);
            move(messag_, chloc2_);
            goto hide_chest;
        }
        dtotal_++; // 6027
        if (odloc_[i] != dloc_[i])
            continue;
        attack_++;
        if (knfloc_ >= 0)
            knfloc_ = loc_;
        if (ran(1000) < 95 * (dflag_ - 2))
            stick_++;
    } // 6030
    if (dtotal_ == 0)
        return (2000);
    if (dtotal_ != 1) {
        adv_printf("There are %d threatening little dwarves ", dtotal_);
        adv_printf("in the room with you.\n");
    } else
        rspeak(Msg::DwarfInRoom);
    if (attack_ == 0)
        return (2000);
    if (dflag_ == 2)
        dflag_ = 3;
    if (saved_ != -1)
        dflag_ = 20;
    if (attack_ != 1) {
        adv_printf("%d of them throw knives at you!\n", attack_);
        k_ = i16(Msg::NoneHitYou);
    hits:                // 82
        if (stick_ <= 1) // 82
        {
            rspeak(Msg(k_) + stick_);
            if (stick_ == 0)
                return (2000);
        } else
            adv_printf("%d of them get you!\n", stick_); // 83
        oldlc2_ = loc_;
        return (99);
    }
    rspeak(Msg::KnifeThrown);
    k_ = i16(Msg::KnifeMisses);
    goto hits;
}

int Game::mback(void) // 20
{
    i32 tk2;
    int ll;
    if (forced(k_ = oldloc_))
        k_ = oldlc2_; // k=location
    oldlc2_ = oldloc_;
    oldloc_ = loc_;
    tk2     = -1; // upstream's null travel entry
    if (k_ == loc_) {
        rspeak(Msg::DontRememberHow);
        return (2);
    }
    for (; !tk_end(); tkk_++) // 21
    {
        ll = tk().tloc;
        if (ll == k_) {
            k_ = i16(tk().tverb); // k back to verb
            tk_to(loc_);
            return (9);
        }
        // Dead as written, and kept that way: the cursor starts at entry 0, so
        // k_ == j[0].tloc would have matched ll == k_ on the first pass and
        // returned.  Upstream reads travel[ll] here, not travel[loc]; see the
        // note in README.md.
        if (ll <= 300) {
            const Vec<Travel> &j = travel_[loc_];
            if (forced(ll) && k_ == j[0].tloc)
                tk2 = tkk_;
        }
    }
    tkk_ = tk2 < 0 ? i32(travel_[tkk_loc_].size()) : tk2; // 23
    if (!tk_end()) {
        k_ = i16(tk().tverb);
        tk_to(loc_);
        return (9);
    }
    rspeak(Msg::CantGetThereFromHere);
    return (2);
}

int Game::badmove(void) // 20
{
    Motion m = Motion(k_);
    spk_     = Msg::WordNotHere;
    if (m >= Motion::East && m <= Motion::NW)
        spk_ = Msg::NoWayThatWay;
    if (m == Motion::Up || m == Motion::Down)
        spk_ = Msg::NoWayThatWay;
    if (m == Motion::Forward || m == Motion::Left || m == Motion::Right)
        spk_ = Msg::UnsureFacing;
    if (m == Motion::Out || m == Motion::In)
        spk_ = Msg::UnsureInOut;
    if (verb_ == find_ || verb_ == invent_)
        spk_ = Msg::OnlyTellWhatYouSee;
    if (m == Motion::Xyzzy || m == Motion::Plugh)
        spk_ = Msg::NothingHappens;
    if (m == Motion::Crawl)
        spk_ = Msg::WhichWay;
    rspeak(spk_);
    return (2);
}

int Game::trbridge(void) // 30300
{
    if (prop_[troll_] == 1) {
        pspeak(troll_, 1);
        prop_[troll_] = 0;
        move(troll2_, 0);
        move(troll2_ + 100, 0);
        move(troll_, plac_[troll_]);
        move(troll_ + 100, fixd_[troll_]);
        juggle(chasm_);
        newloc_ = loc_;
        return (2);
    }
    newloc_ = plac_[troll_] + fixd_[troll_] - loc_; // 30310
    if (prop_[troll_] == 0)
        prop_[troll_] = 1;
    if (!toting(bear_))
        return (2);
    rspeak(Msg::BridgeBuckles);
    prop_[chasm_] = 1;
    prop_[troll_] = 2;
    drop(bear_, newloc_);
    fixed_[bear_] = -1;
    prop_[bear_]  = 3;
    if (prop_[spices_] < 0)
        tally2_++;
    oldlc2_ = newloc_;
    return (99);
}

int Game::specials(void) // 30000
{
    switch (newloc_ -= 300) {
    case 1: // 30100
        newloc_ = 99 + 100 - loc_;
        if (holdng_ == 0 || (holdng_ == 1 && toting(emrald_)))
            return (2);
        newloc_ = loc_;
        rspeak(Msg::WontFitThroughTunnel);
        return (2);
    case 2: // 30200
        drop(emrald_, loc_);
        return (12);
    case 3: // to 30300
        return (trbridge());
    default:
        bug(29);
    }
}

int Game::march(void) // label 8
{
    int ll1, ll2;

    tk_to(newloc_ = loc_);
    if (tk_end())
        bug(26);
    if (k_ == null_)
        return (2);
    if (k_ == cave_) // 40
    {
        if (loc_ < 8)
            rspeak(Msg::NoStreamOnSurface);
        if (loc_ >= 8)
            rspeak(Msg::NeedDetail);
        return (2);
    }
    if (k_ == look_) // 30
    {
        if (detail_++ < 3)
            rspeak(Msg::NoMoreDetail);
        wzdark_    = FALSE;
        abb_[loc_] = 0;
        return (2);
    }
    if (k_ == back_) // 20
    {
        switch (mback()) {
        case 2:
            return (2);
        case 9:
            goto find_exit;
        default:
            bug(100);
        }
    }
    oldlc2_ = oldloc_;
    oldloc_ = loc_;
find_exit: // 9
    for (; !tk_end(); tkk_++)
        if (tk().tverb == Motion::Forced || tk().tverb == Motion(k_))
            break;
    if (tk_end()) {
        badmove();
        return (2);
    }
try_entry:                     // 11
    ll1     = tk().conditions; // 11
    ll2     = tk().tloc;
    newloc_ = ll1;           // newloc=conditions
    k_      = newloc_ % 100; // k used for prob
    if (newloc_ <= 300) {
        if (newloc_ <= 100) // 13
        {
            if (newloc_ != 0 && !pct(newloc_))
                goto next_entry; // 14
        take_exit:               // 16
            newloc_ = ll2;       // newloc=location
            if (newloc_ <= 300)
                return (2);
            if (newloc_ <= 500)
                switch (specials()) // to 30000
                {
                case 2:
                    return (2);
                case 12:
                    goto next_entry;
                case 99:
                    return (99);
                default:
                    bug(101);
                }
            rspeak(Msg(newloc_ - 500));
            newloc_ = loc_;
            return (2);
        }
        if (toting(k_) || (newloc_ > 200 && at(k_)))
            goto take_exit;
        goto next_entry;
    }
    if (prop_[k_] != (newloc_ / 100) - 3)
        goto take_exit; // newloc still conditions
next_entry:             // 12, the alternative to a probability move
    for (; !tk_end(); tkk_++)
        if (tk().tloc != ll2 || tk().conditions != ll1)
            break;
    if (tk_end())
        bug(25);
    goto try_entry;
}
