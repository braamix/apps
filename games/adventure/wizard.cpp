// Re-coding of advent in C: privileged operations -- upstream wizard.c.
//
// Ported to Braam.  exit() does not exist: ciao() returns ADV_OVER and the
// status travels up to proc_main.
#include "hdr.h"

void poof(void)
{
    magic       = "dwarf";
    game.latncy = 15; // originally 45 minutes
}

static Task<i32> wizard(void) // not as complex as advent/10 (for now)
{
    char *word, *x;
    if (!co_await yesm(16, 0, 7))
        co_return FALSE;
    mspeak(17);
    if (Task<void> t = getin(&word, &x))
        co_await t;
    if (!weq(word, magic)) {
        mspeak(20);
        co_return FALSE;
    }
    mspeak(19);
    co_return TRUE;
}

Task<i32> start(int n)
{
    int d, t, delay;

    (void)n;
    if (Task<void> c = adv_clock())
        co_await c;
    datime(&d, &t);
    delay = (d - game.saved) * 1440 + (t - game.savet); // good for about a month
    if (delay >= game.latncy || delay < 0) {
        game.saved = -1;
        co_return 0;
    }
    adv_printf("This adventure was suspended a mere %d minutes ago.", delay);
    if (delay <= game.latncy / 3) {
        mspeak(2);
        adv_status = 0;
        co_return ADV_OVER;
    }
    mspeak(8);
    if (!co_await wizard()) {
        mspeak(9);
        adv_status = 0;
        co_return ADV_OVER;
    }
    game.saved = -1;
    co_return 0;
}

Task<i32> ciao(void)
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
            adv_status = 0;
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
    adv_status = 0;
    co_return ADV_OVER;
}
