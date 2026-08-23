// Re-coding of advent in C: privileged operations -- upstream wizard.c.
//
// Ported to Braam.  exit() does not exist: ciao() returns ADV_OVER and the
// status travels up to proc_main.
#include "hdr.h"

void Game::poof(void)
{
    magic_  = "dwarf";
    latncy_ = 15; // originally 45 minutes
}

Task<i32> Game::wizard(void) // not as complex as advent/10 (for now)
{
    if (!co_await yesm(Magic::AreYouAWizard, Magic::None, Magic::VeryWell))
        co_return FALSE;
    mspeak(Magic::ProveIt);
    if (Task<void> t = getin())
        co_await t;
    if (!weq(wd1_, magic_)) {
        mspeak(Magic::Charlatan);
        co_return FALSE;
    }
    mspeak(Magic::ReallyAWizard);
    co_return TRUE;
}

Task<i32> Game::start(int n)
{
    int d, t, delay;

    (void)n;
    if (Task<void> c = adv_clock())
        co_await c;
    datime(&d, &t);
    delay = (d - saved_) * 1440 + (t - savet_); // good for about a month
    if (delay >= latncy_ || delay < 0) {
        saved_ = -1;
        co_return 0;
    }
    adv_printf("This adventure was suspended a mere %d minutes ago.", delay);
    if (delay <= latncy_ / 3) {
        mspeak(Magic::WizardsWait);
        status_ = 0;
        co_return ADV_OVER;
    }
    mspeak(Magic::WizardMayContinue);
    if (!co_await wizard()) {
        mspeak(Magic::ResumeLater);
        status_ = 0;
        co_return ADV_OVER;
    }
    saved_ = -1;
    co_return 0;
}

Task<i32> Game::ciao(void)
{
    String fname;

    for (;;) {
        adv_printf("What would you like to call the saved version?\n");
        if (Task<void> t = adv_flush())
            co_await t;
        AdvEnd how = AdvEnd::Eof;
        if (Task<AdvEnd> t = adv_readline(fname))
            how = co_await t;
        if (how != AdvEnd::Enter || fname.empty()) {
            status_ = 0;
            co_return ADV_OVER;
        }
        Result<FileInfo> st = Err(Error::NotFound);
        if (Task<Result<FileInfo>> t = stat_of(fname.str()))
            st = co_await t;
        if (st.is_err())
            break;
        adv_printf("I can't use that one.\n");
        co_return 0;
    }
    if (Task<Result<void>> t = save(fname.str()))
        co_await t;
    adv_printf("                    ^\n");
    adv_printf("That should do it.  Gis revido.\n");
    status_ = 0;
    co_return ADV_OVER;
}
