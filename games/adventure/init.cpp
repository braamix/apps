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
    return hints_.resize(HINTSIZ) && travel_.resize(LOCSIZ + 1) && dat_.resize(DATSIZE) &&
           ltext_.resize(LOCSIZ + 1) && stext_.resize(LOCSIZ + 1) && atloc_.resize(LOCSIZ + 1) &&
           cond_.resize(LOCSIZ + 1) && abb_.resize(LOCSIZ + 1) && voc_.resize(HTSIZE) &&
           rtext_.resize(RTXSIZ + 1) && mtext_.resize(MAGSIZ + 1) && ctext_.resize(CLSMAX) &&
           cval_.resize(CLSMAX) && ptext_.resize(OBJSIZ) && plac_.resize(OBJSIZ) &&
           fixd_.resize(OBJSIZ) && fixed_.resize(OBJSIZ) && place_.resize(OBJSIZ) &&
           prop_.resize(OBJSIZ) && plink_.resize(OBJSIZ + MAXOBJ) &&
           actspk_.resize(35) && // upstream's slack above Verb::Hours
           hinted_.resize(HINTSIZ) && hintlc_.resize(HINTSIZ) && dseen_.resize(PIRATE + 1) &&
           dloc_.resize(PIRATE + 1) && odloc_.resize(PIRATE + 1) && tk_.resize(DWARF_EXITS + 1);
}

void Game::linkdata(void) // secondary data manipulation
{
    int i, j;
    // array linkages
    for (i = 1; i <= LOCSIZ; i++)
        if (ltext_[i].seekadr != 0 && !travel_[i].empty())
            if ((travel_[i][0].tverb) == Motion::Forced)
                cond_[i] = COND_FORCED;
    for (j = MAXOBJ; j > 0; j--)
        if (fixd_[j] > 0) {
            drop(j + FIXED, fixd_[j]);
            drop(j, plac_[j]);
        }
    for (j = MAXOBJ; j > 0; j--) {
        fixed_[j] = fixd_[j];
        if (plac_[j] != 0 && fixd_[j] <= 0)
            drop(j, plac_[j]);
    }

    maxtrs_ = 79;
    tally_  = 0;
    tally2_ = 0;

    for (i = TREASURE; i <= maxtrs_; i++) {
        if (ptext_[i].seekadr != 0)
            prop_[i] = -1;
        tally_ -= prop_[i];
    }

    // define mnemonics
    keys_   = vocab("keys", WordClass::Object);
    lamp_   = vocab("lamp", WordClass::Object);
    grate_  = vocab("grate", WordClass::Object);
    cage_   = vocab("cage", WordClass::Object);
    rod_    = vocab("rod", WordClass::Object);
    rod2_   = rod_ + 1;
    steps_  = vocab("steps", WordClass::Object);
    bird_   = vocab("bird", WordClass::Object);
    door_   = vocab("door", WordClass::Object);
    pillow_ = vocab("pillow", WordClass::Object);
    snake_  = vocab("snake", WordClass::Object);
    fissur_ = vocab("fissu", WordClass::Object);
    tablet_ = vocab("table", WordClass::Object);
    clam_   = vocab("clam", WordClass::Object);
    oyster_ = vocab("oyster", WordClass::Object);
    magzin_ = vocab("magaz", WordClass::Object);
    dwarf_  = vocab("dwarf", WordClass::Object);
    knife_  = vocab("knife", WordClass::Object);
    food_   = vocab("food", WordClass::Object);
    bottle_ = vocab("bottl", WordClass::Object);
    water_  = vocab("water", WordClass::Object);
    oil_    = vocab("oil", WordClass::Object);
    plant_  = vocab("plant", WordClass::Object);
    plant2_ = plant_ + 1;
    axe_    = vocab("axe", WordClass::Object);
    mirror_ = vocab("mirro", WordClass::Object);
    dragon_ = vocab("drago", WordClass::Object);
    chasm_  = vocab("chasm", WordClass::Object);
    troll_  = vocab("troll", WordClass::Object);
    troll2_ = troll_ + 1;
    bear_   = vocab("bear", WordClass::Object);
    messag_ = vocab("messa", WordClass::Object);
    vend_   = vocab("vendi", WordClass::Object);
    batter_ = vocab("batte", WordClass::Object);

    nugget_ = vocab("gold", WordClass::Object);
    coins_  = vocab("coins", WordClass::Object);
    chest_  = vocab("chest", WordClass::Object);
    eggs_   = vocab("eggs", WordClass::Object);
    tridnt_ = vocab("tride", WordClass::Object);
    vase_   = vocab("vase", WordClass::Object);
    emrald_ = vocab("emera", WordClass::Object);
    pyram_  = vocab("pyram", WordClass::Object);
    pearl_  = vocab("pearl", WordClass::Object);
    rug_    = vocab("rug", WordClass::Object);
    chain_  = vocab("chain", WordClass::Object);

    back_   = vocab("back", WordClass::Motion);
    look_   = vocab("look", WordClass::Motion);
    cave_   = vocab("cave", WordClass::Motion);
    null_   = vocab("null", WordClass::Motion);
    entrnc_ = vocab("entra", WordClass::Motion);
    dprssn_ = vocab("depre", WordClass::Motion);

    say_    = Verb(vocab("say", WordClass::Verb));
    lock_   = Verb(vocab("lock", WordClass::Verb));
    throw_  = Verb(vocab("throw", WordClass::Verb));
    find_   = Verb(vocab("find", WordClass::Verb));
    invent_ = Verb(vocab("inven", WordClass::Verb));
    // initialize dwarves
    chloc_  = Loc::ChestDeadEnd;
    chloc2_ = Loc::MessageDeadEnd;
    for (i = 1; i <= PIRATE; i++)
        dseen_[i] = FALSE;
    dflag_        = Dwarves::Asleep;
    dloc_[1]      = Loc::HallOfMtKing;
    dloc_[2]      = Loc::WestOfFissure;
    dloc_[3]      = Loc::Y2;
    dloc_[4]      = Loc::MazeAllAlike;
    dloc_[5]      = Loc::ComplexJunction;
    dloc_[PIRATE] = chloc_;
    daltlc_       = Loc::NoteRoom;

    // random flags & ctrs
    turns_  = 0;
    lmwarn_ = FALSE;
    iwest_  = 0;
    knfloc_ = 0;
    detail_ = 0;
    abbnum_ = 5;
    for (i = 0; i <= 4; i++) // the obituaries, in pairs
        if (rtext_[i16(Msg::Killed) + 2 * i].seekadr != 0)
            maxdie_ = i + 1;
    numdie_ = holdng_ = dkill_ = foobar_ = 0;
    bonus_                               = Msg::None;
    clock1_                              = 30;
    clock2_                              = 50;
    saved_                               = 0;
    closng_ = panic_ = closed_ = scorng_ = FALSE;
    check_vocab();
    check_msgs();
    check_hints();
    check_locs();
}

// glorkz decides these numbers; the enums only name them.  Check the naming
// rather than trust it -- a vocabulary that moved would mis-dispatch in
// silence.  Motion::Forced has no word, so it cannot be checked here.
void Game::check_vocab()
{
    struct Named {
        const char *word;
        int want;
    };
    // clang-format off
    static const Named VERBS[] = {
        { "say",   int(Verb::Say)    }, { "lock",  int(Verb::Lock)   },
        { "throw", int(Verb::Throw)  }, { "find",  int(Verb::Find)   },
        { "inven", int(Verb::Invent) }, { "foo",   int(Verb::Foo)    },
    };
    static const Named MOTIONS[] = {
        { "forwa", int(Motion::Forward) }, { "back",  int(Motion::Back)  },
        { "out",   int(Motion::Out)     }, { "crawl", int(Motion::Crawl) },
        { "in",    int(Motion::In)      }, { "null",  int(Motion::Null)  },
        { "up",    int(Motion::Up)      }, { "down",  int(Motion::Down)  },
        { "left",  int(Motion::Left)    }, { "right", int(Motion::Right) },
        { "east",  int(Motion::East)    }, { "west",  int(Motion::West)  },
        { "north", int(Motion::North)   }, { "south", int(Motion::South) },
        { "ne",    int(Motion::NE)      }, { "se",    int(Motion::SE)    },
        { "sw",    int(Motion::SW)      }, { "nw",    int(Motion::NW)    },
        { "look",  int(Motion::Look)    }, { "xyzzy", int(Motion::Xyzzy) },
        { "depre", int(Motion::Depression) }, { "entra", int(Motion::Entrance) },
        { "plugh", int(Motion::Plugh)   }, { "cave",  int(Motion::Cave)  },
        { "plove", int(Motion::Plover)  },
    };
    // clang-format on
    for (const Named &n : VERBS)
        if (vocab(n.word, WordClass::Verb) != n.want)
            bug(35);
    for (const Named &n : MOTIONS)
        if (vocab(n.word, WordClass::Motion) != n.want)
            bug(35);
}

// glorkz decides the room numbers too, and no room has a word to look up.  What
// can be checked is where glorkz puts things and which bits it sets: section 7
// places the grate and the keys, and section 9 marks the rooms a hint or a
// liquid belongs to, one apiece.
void Game::check_locs()
{
    struct Marked {
        i16 loc;
        i16 bit;
    };
    static const Marked MARKED[] = {
        { Loc::OutsideGrate, i16(Hint::Grate) },  { Loc::BirdChamber, i16(Hint::Bird) },
        { Loc::HallOfMtKing, i16(Hint::Snake) },  { Loc::MazeAllAlike, i16(Hint::Maze) },
        { Loc::PloverRoom, i16(Hint::Plover) },   { Loc::WittsEnd, i16(Hint::WittsEnd) },
        { Loc::OilPit, i16(CondBit::Oil) },       { Loc::OilPit, i16(CondBit::Fluid) },
        { Loc::Road, i16(CondBit::Lit) },         { Loc::Building, i16(CondBit::Lit) },
        { Loc::RepositoryNE, i16(CondBit::Lit) },
    };
    for (const Marked &m : MARKED)
        if (!bitset(m.loc, m.bit))
            bug(38);

    if (plac_[grate_] != Loc::OutsideGrate || fixd_[grate_] != Loc::BelowGrate ||
        plac_[keys_] != Loc::Building || plac_[lamp_] != Loc::Building)
        bug(38);

    // Every room the code names is a room, and none of them is a forced move.
    static const i16 NAMED_LOCS[] = {
        Loc::Road,         Loc::Building,     Loc::Valley,          Loc::Slit,
        Loc::OutsideGrate, Loc::BelowGrate,   Loc::BirdChamber,     Loc::HallOfMists,
        Loc::NoteRoom,     Loc::HallOfMtKing, Loc::OilPit,          Loc::WestOfFissure,
        Loc::Y2,           Loc::MazeAllAlike, Loc::ComplexJunction, Loc::Alcove,
        Loc::PloverRoom,   Loc::CulDeSac,     Loc::WittsEnd,        Loc::ChestDeadEnd,
        Loc::RepositoryNE, Loc::RepositorySW, Loc::MessageDeadEnd,
    };
    for (i16 n : NAMED_LOCS)
        if (ltext_[n].seekadr == 0 || forced(n))
            bug(38);
}

// The message names are a summary of glorkz's text and nothing can check that
// at runtime.  What can be checked is that the table under them has not moved:
// every message msg.h claims is there, and the obituary slots glorkz reserves
// but never fills are still empty.
void Game::check_msgs()
{
    for (i16 n = 1; n <= i16(Msg::Last); n++) {
        bool reserved = n > i16(Msg::ReviveRefused) && n < i16(Msg::ObituarySlots);
        if ((rtext_[n].seekadr != 0) == reserved)
            bug(36);
    }
    for (i16 n = 1; n <= i16(Magic::Last); n++)
        if (mtext_[n].seekadr == 0)
            bug(36);
}

// glorkz decides which hint is which; the enum only names them.  Tie each name
// to the messages it asks, so a hint table that moved cannot pass.
void Game::check_hints()
{
    struct Asks {
        Hint hint;
        Msg question, answer;
    };
    static const Asks ASKS[] = {
        { Hint::Grate, Msg::GettingIntoCave, Msg::GrateSolid },
        { Hint::Bird, Msg::CatchingBird, Msg::BirdFrightened },
        { Hint::Snake, Msg::DealWithSnake, Msg::CantKillSnake },
        { Hint::Maze, Msg::HelpWithMaze, Msg::DropThings },
        { Hint::Plover, Msg::ExploreBeyondPlover, Msg::WayToExplore },
        { Hint::WittsEnd, Msg::HelpGettingOut, Msg::DontGoWest },
    };
    for (const Asks &a : ASKS) {
        const HintRule &h = hints_[usize(i16(a.hint))];
        if (h.question != a.question || h.answer != a.answer)
            bug(37);
    }
}

Task<i32> Game::startup(void)
{
    if (Task<void> t = adv_seed()) // random odd seed
        co_await t;
    hinted_[i16(Hint::Instructions)] =
        co_await yes(Msg::WelcomeInstructions, Msg::ColossalCave, Msg::None);
    newloc_ = Loc::Road;
    limit_  = 330;
    if (hinted_[i16(Hint::Instructions)])
        limit_ = 1000; // better batteries if instrucs
    co_return 0;
}
