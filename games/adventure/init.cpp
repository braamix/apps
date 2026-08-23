// Re-coding of advent in C: the shared state and its setup -- upstream init.c.
#include "hdr.h"

int delhit;
int adv_status; // what proc_main will return
char *wd1, *wd2;
struct game game;
struct travlist *tkk;
const char *magic;

const short setbit[16] = {
    1,    2,     4,     010,   020,    040,    0100,   0200,
    0400, 01000, 02000, 04000, 010000, 020000, 040000, short(0100000),
};

void linkdata(void) // secondary data manipulation
{
    int i, j;
    // array linkages
    for (i = 1; i <= LOCSIZ; i++)
        if (game.ltext[i].seekadr != 0 && game.travel[i] != 0)
            if ((game.travel[i]->tverb) == 1)
                game.cond[i] = 2;
    for (j = 100; j > 0; j--)
        if (game.fixd[j] > 0) {
            drop(j + 100, game.fixd[j]);
            drop(j, game.plac[j]);
        }
    for (j = 100; j > 0; j--) {
        game.fixed[j] = game.fixd[j];
        if (game.plac[j] != 0 && game.fixd[j] <= 0)
            drop(j, game.plac[j]);
    }

    game.maxtrs = 79;
    game.tally  = 0;
    game.tally2 = 0;

    for (i = 50; i <= game.maxtrs; i++) {
        if (game.ptext[i].seekadr != 0)
            game.prop[i] = -1;
        game.tally -= game.prop[i];
    }

    // define mnemonics
    game.keys   = vocab("keys", 1, 0);
    game.lamp   = vocab("lamp", 1, 0);
    game.grate  = vocab("grate", 1, 0);
    game.cage   = vocab("cage", 1, 0);
    game.rod    = vocab("rod", 1, 0);
    game.rod2   = game.rod + 1;
    game.steps  = vocab("steps", 1, 0);
    game.bird   = vocab("bird", 1, 0);
    game.door   = vocab("door", 1, 0);
    game.pillow = vocab("pillow", 1, 0);
    game.snake  = vocab("snake", 1, 0);
    game.fissur = vocab("fissu", 1, 0);
    game.tablet = vocab("table", 1, 0);
    game.clam   = vocab("clam", 1, 0);
    game.oyster = vocab("oyster", 1, 0);
    game.magzin = vocab("magaz", 1, 0);
    game.dwarf  = vocab("dwarf", 1, 0);
    game.knife  = vocab("knife", 1, 0);
    game.food   = vocab("food", 1, 0);
    game.bottle = vocab("bottl", 1, 0);
    game.water  = vocab("water", 1, 0);
    game.oil    = vocab("oil", 1, 0);
    game.plant  = vocab("plant", 1, 0);
    game.plant2 = game.plant + 1;
    game.axe    = vocab("axe", 1, 0);
    game.mirror = vocab("mirro", 1, 0);
    game.dragon = vocab("drago", 1, 0);
    game.chasm  = vocab("chasm", 1, 0);
    game.troll  = vocab("troll", 1, 0);
    game.troll2 = game.troll + 1;
    game.bear   = vocab("bear", 1, 0);
    game.messag = vocab("messa", 1, 0);
    game.vend   = vocab("vendi", 1, 0);
    game.batter = vocab("batte", 1, 0);

    game.nugget = vocab("gold", 1, 0);
    game.coins  = vocab("coins", 1, 0);
    game.chest  = vocab("chest", 1, 0);
    game.eggs   = vocab("eggs", 1, 0);
    game.tridnt = vocab("tride", 1, 0);
    game.vase   = vocab("vase", 1, 0);
    game.emrald = vocab("emera", 1, 0);
    game.pyram  = vocab("pyram", 1, 0);
    game.pearl  = vocab("pearl", 1, 0);
    game.rug    = vocab("rug", 1, 0);
    game.chain  = vocab("chain", 1, 0);

    game.back   = vocab("back", 0, 0);
    game.look   = vocab("look", 0, 0);
    game.cave   = vocab("cave", 0, 0);
    game.null   = vocab("null", 0, 0);
    game.entrnc = vocab("entra", 0, 0);
    game.dprssn = vocab("depre", 0, 0);

    game.say    = vocab("say", 2, 0);
    game.lock   = vocab("lock", 2, 0);
    game.throw_ = vocab("throw", 2, 0);
    game.find   = vocab("find", 2, 0);
    game.invent = vocab("inven", 2, 0);
    // initialize dwarves
    game.chloc  = 114;
    game.chloc2 = 140;
    for (i = 1; i <= 6; i++)
        game.dseen[i] = FALSE;
    game.dflag   = 0;
    game.dloc[1] = 19;
    game.dloc[2] = 27;
    game.dloc[3] = 33;
    game.dloc[4] = 44;
    game.dloc[5] = 64;
    game.dloc[6] = game.chloc;
    game.daltlc  = 18;

    // random flags & ctrs
    game.turns  = 0;
    game.lmwarn = FALSE;
    game.iwest  = 0;
    game.knfloc = 0;
    game.detail = 0;
    game.abbnum = 5;
    for (i = 0; i <= 4; i++)
        if (game.rtext[2 * i + 81].seekadr != 0)
            game.maxdie = i + 1;
    game.numdie = game.holdng = game.dkill = game.foobar = game.bonus = 0;
    game.clock1                                                       = 30;
    game.clock2                                                       = 50;
    game.saved                                                        = 0;
    game.closng = game.panic = game.closed = game.scorng = FALSE;
}

Task<i32> startup(void)
{
    if (Task<void> t = adv_seed()) // random odd seed
        co_await t;
    game.hinted[3] = co_await yes(65, 1, 0);
    game.newloc    = 1;
    game.limit     = 330;
    if (game.hinted[3])
        game.limit = 1000; // better batteries if instrucs
    co_return 0;
}
