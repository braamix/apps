// Re-coding of advent in C: termination routines -- upstream done.c.
//
// Ported to Braam.  done() and die() return ADV_OVER instead of calling exit().
#include "hdr.h"

int Game::score(void) // sort of like 20000
{
    int scor, i;
    mxscor_ = scor = 0;
    for (i = TREASURE; i <= maxtrs_; i++) {
        if (ptext_[i].txtlen == 0)
            continue;
        k_ = 12;
        if (i == chest_)
            k_ = 14;
        if (i > chest_)
            k_ = 16;
        if (prop_[i] >= 0)
            scor += 2;
        if (place_[i] == Loc::Building && prop_[i] == 0)
            scor += k_ - 2;
        mxscor_ += k_;
    }
    scor += (maxdie_ - numdie_) * 10;
    mxscor_ += maxdie_ * 10;
    if (!(scorng_ || gaveup_))
        scor += 4;
    mxscor_ += 4;
    if (dflag_ != Dwarves::Asleep)
        scor += 25;
    mxscor_ += 25;
    if (closng_)
        scor += 25;
    mxscor_ += 25;
    if (closed_) {
        if (bonus_ == Msg::None)
            scor += 10;
        if (bonus_ == Msg::BlastSplashed)
            scor += 25;
        if (bonus_ == Msg::BlastRepository)
            scor += 30;
        if (bonus_ == Msg::BlastDistant)
            scor += 45;
    }
    mxscor_ += 45;
    if (place_[magzin_] == Loc::WittsEnd)
        scor++;
    mxscor_++;
    scor += 2;
    mxscor_ += 2;
    for (i = 1; i <= hntmax_; i++)
        if (hinted_[i])
            scor -= hints_[i].cost;
    return (scor);
}

// game is over
Task<void> Game::done(Done entry)
{
    int i, sc;
    if (entry == Done::Cave)
        mspeak(Magic::GreenSmoke);
    if (entry == Done::Dwarves)
        rspeak(Msg::DwarvesAwakened);
    adv_printf("\n\n\nYou scored %d out of a ", (sc = score()));
    adv_printf("possible %d using %d turns.\n", mxscor_, turns_);
    status_ = 0;
    for (i = 1; i <= clsses_; i++)
        if (cval_[i] >= sc) {
            speak(&ctext_[i]);
            if (i == clsses_ - 1) {
                adv_printf("To achieve the next higher rating");
                adv_printf(" would be a neat trick!\n\n");
                adv_printf("Congratulations!!\n");
                co_return;
            }
            k_ = cval_[i] + 1 - sc;
            adv_printf("To achieve the next higher rating, you need");
            adv_printf(" %d more point", k_);
            if (k_ == 1)
                adv_printf(".\n");
            else
                adv_printf("s.\n");
            co_return;
        }
    adv_printf("You just went off my scale!!!\n");
    co_return;
}

Task<Move> Game::die( // label 90
    Death entry)
{
    int i, yea;
    if (entry != Death::Elsewhere) {
        rspeak(Msg::FellIntoPit);
        oldlc2_ = loc_;
    }
    if (closng_) // 99
    {
        rspeak(Msg::LooksLikeYoureDead);
        numdie_++;
        co_await done(Done::Score);
        co_return Move::Over;
    }
    yea = co_await yes(Msg::Killed + numdie_ * 2, Msg::ReviveOffer + numdie_ * 2, Msg::Ok);
    numdie_++;
    if (numdie_ == maxdie_ || !yea) {
        co_await done(Done::Score);
        co_return Move::Over;
    }
    place_[water_] = 0;
    place_[oil_]   = 0;
    if (toting(lamp_))
        prop_[lamp_] = 0;
    for (i = MAXOBJ; i >= 1; i--) {
        if (!toting(i))
            continue;
        k_ = oldlc2_;
        if (i == lamp_)
            k_ = 1;
        drop(i, k_);
    }
    loc_    = Loc::Building;
    oldloc_ = loc_;
    co_return Move::Describe;
}
