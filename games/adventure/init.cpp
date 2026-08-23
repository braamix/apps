// Re-coding of advent in C: the shared state and its setup -- upstream init.c.
#include "hdr.h"

const i16 Game::SETBIT[16] = {
    1,    2,     4,     010,   020,    040,    0100,   0200,
    0400, 01000, 02000, 04000, 010000, 020000, 040000, i16(0100000),
};

// The tables are 1-based, FORTRAN's legacy, and two of them are read one past
// the size they are named for: rdesc() lets a message number equal RTXSIZ or
// MAGSIZ through, and linkdata() below runs its location loop to i <= LOCSIZ.
// resize() value-initialises, which is the zero .bss used to provide.
bool Game::alloc()
{
    if (!hints_.resize(HINTSIZ))
        return false;
    for (i32 i = 0; i < HINTSIZ; i++)
        if (!hints_[i].resize(5))
            return false;

    return ltext_.resize(LOCSIZ + 1) && stext_.resize(LOCSIZ + 1) && atloc_.resize(LOCSIZ + 1) &&
           cond_.resize(LOCSIZ + 1) && abb_.resize(LOCSIZ + 1) && voc_.resize(HTSIZE) &&
           rtext_.resize(RTXSIZ + 1) && mtext_.resize(MAGSIZ + 1) && ctext_.resize(CLSMAX) &&
           cval_.resize(CLSMAX) && ptext_.resize(OBJSIZ) && plac_.resize(OBJSIZ) &&
           fixd_.resize(OBJSIZ) && fixed_.resize(OBJSIZ) && place_.resize(OBJSIZ) &&
           prop_.resize(OBJSIZ) && plink_.resize(201) && actspk_.resize(35) &&
           hinted_.resize(HINTSIZ) && hintlc_.resize(HINTSIZ) && dseen_.resize(7) &&
           dloc_.resize(7) && odloc_.resize(7) && tk_.resize(21);
}

void Game::linkdata(void) // secondary data manipulation
{
    int i, j;
    // array linkages
    for (i = 1; i <= LOCSIZ; i++)
        if (ltext_[i].seekadr != 0 && travel_[i] != 0)
            if ((travel_[i]->tverb) == 1)
                cond_[i] = 2;
    for (j = 100; j > 0; j--)
        if (fixd_[j] > 0) {
            drop(j + 100, fixd_[j]);
            drop(j, plac_[j]);
        }
    for (j = 100; j > 0; j--) {
        fixed_[j] = fixd_[j];
        if (plac_[j] != 0 && fixd_[j] <= 0)
            drop(j, plac_[j]);
    }

    maxtrs_ = 79;
    tally_  = 0;
    tally2_ = 0;

    for (i = 50; i <= maxtrs_; i++) {
        if (ptext_[i].seekadr != 0)
            prop_[i] = -1;
        tally_ -= prop_[i];
    }

    // define mnemonics
    keys_   = vocab("keys", 1, 0);
    lamp_   = vocab("lamp", 1, 0);
    grate_  = vocab("grate", 1, 0);
    cage_   = vocab("cage", 1, 0);
    rod_    = vocab("rod", 1, 0);
    rod2_   = rod_ + 1;
    steps_  = vocab("steps", 1, 0);
    bird_   = vocab("bird", 1, 0);
    door_   = vocab("door", 1, 0);
    pillow_ = vocab("pillow", 1, 0);
    snake_  = vocab("snake", 1, 0);
    fissur_ = vocab("fissu", 1, 0);
    tablet_ = vocab("table", 1, 0);
    clam_   = vocab("clam", 1, 0);
    oyster_ = vocab("oyster", 1, 0);
    magzin_ = vocab("magaz", 1, 0);
    dwarf_  = vocab("dwarf", 1, 0);
    knife_  = vocab("knife", 1, 0);
    food_   = vocab("food", 1, 0);
    bottle_ = vocab("bottl", 1, 0);
    water_  = vocab("water", 1, 0);
    oil_    = vocab("oil", 1, 0);
    plant_  = vocab("plant", 1, 0);
    plant2_ = plant_ + 1;
    axe_    = vocab("axe", 1, 0);
    mirror_ = vocab("mirro", 1, 0);
    dragon_ = vocab("drago", 1, 0);
    chasm_  = vocab("chasm", 1, 0);
    troll_  = vocab("troll", 1, 0);
    troll2_ = troll_ + 1;
    bear_   = vocab("bear", 1, 0);
    messag_ = vocab("messa", 1, 0);
    vend_   = vocab("vendi", 1, 0);
    batter_ = vocab("batte", 1, 0);

    nugget_ = vocab("gold", 1, 0);
    coins_  = vocab("coins", 1, 0);
    chest_  = vocab("chest", 1, 0);
    eggs_   = vocab("eggs", 1, 0);
    tridnt_ = vocab("tride", 1, 0);
    vase_   = vocab("vase", 1, 0);
    emrald_ = vocab("emera", 1, 0);
    pyram_  = vocab("pyram", 1, 0);
    pearl_  = vocab("pearl", 1, 0);
    rug_    = vocab("rug", 1, 0);
    chain_  = vocab("chain", 1, 0);

    back_   = vocab("back", 0, 0);
    look_   = vocab("look", 0, 0);
    cave_   = vocab("cave", 0, 0);
    null_   = vocab("null", 0, 0);
    entrnc_ = vocab("entra", 0, 0);
    dprssn_ = vocab("depre", 0, 0);

    say_    = vocab("say", 2, 0);
    lock_   = vocab("lock", 2, 0);
    throw_  = vocab("throw", 2, 0);
    find_   = vocab("find", 2, 0);
    invent_ = vocab("inven", 2, 0);
    // initialize dwarves
    chloc_  = 114;
    chloc2_ = 140;
    for (i = 1; i <= 6; i++)
        dseen_[i] = FALSE;
    dflag_   = 0;
    dloc_[1] = 19;
    dloc_[2] = 27;
    dloc_[3] = 33;
    dloc_[4] = 44;
    dloc_[5] = 64;
    dloc_[6] = chloc_;
    daltlc_  = 18;

    // random flags & ctrs
    turns_  = 0;
    lmwarn_ = FALSE;
    iwest_  = 0;
    knfloc_ = 0;
    detail_ = 0;
    abbnum_ = 5;
    for (i = 0; i <= 4; i++)
        if (rtext_[2 * i + 81].seekadr != 0)
            maxdie_ = i + 1;
    numdie_ = holdng_ = dkill_ = foobar_ = bonus_ = 0;
    clock1_                                       = 30;
    clock2_                                       = 50;
    saved_                                        = 0;
    closng_ = panic_ = closed_ = scorng_ = FALSE;
}

Task<i32> Game::startup(void)
{
    if (Task<void> t = adv_seed()) // random odd seed
        co_await t;
    hinted_[3] = co_await yes(65, 1, 0);
    newloc_    = 1;
    limit_     = 330;
    if (hinted_[3])
        limit_ = 1000; // better batteries if instrucs
    co_return 0;
}
